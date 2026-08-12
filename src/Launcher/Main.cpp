/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2023-2026 Mifan Bang <https://debug.tw>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LaussDef.h"

#include "Debug.h"

#include <DllInjector.h>
#include <Handle.h>
#include <ModuleList.h>
#include <ProcessList.h>

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>


using namespace std::literals;


namespace
{

std::wstring GetCurrentDirectoryW()
{
	const DWORD lengthNeeded = ::GetCurrentDirectoryW(0, nullptr);

	std::wstring result(static_cast<size_t>(lengthNeeded - 1), L'\\');
	[[maybe_unused]] const auto lengthWritten = ::GetCurrentDirectoryW(lengthNeeded, result.data());
	assert(lengthWritten + 1 == lengthNeeded);

	return result;
}

std::optional<bool> IsProcessStillAlive(gan::WinHandle proc)
{
	gan::WinDword exitCode;
	const auto getExitCodeResult = ::GetExitCodeProcess(proc, &exitCode);

	return getExitCodeResult ?
		std::make_optional(exitCode == STILL_ACTIVE)
		: std::nullopt;
}


class LineProcessHelper
{
public:
	enum class InjectionError
	{
		AlreadyActive,
		ProcessNotFound,
		HookError,
		SystemCallError,

		Count
	};
	constexpr static std::array<std::string_view, std::to_underlying(InjectionError::Count)> k_ResultStrings
	{
		"AlreadyActive"sv,
		"ProcessNotFound"sv,
		"HookError"sv,
		"SystemCallError"sv
	};

	static std::expected<gan::AutoWinHandle, InjectionError> InjectPayload(uint32_t pid)
	{
		auto hProc = OpenTargetProcess(pid);
		assert(hProc);
		if (!hProc)
		{
			// Either process not accessible or terminated
			return std::unexpected{ InjectionError::ProcessNotFound };
		}
		else
		{
			const auto payloadFound = PayloadExistsIn(*hProc);
			if (!payloadFound)
				return std::unexpected{ InjectionError::ProcessNotFound };  // Error during module enumeration
			else if (payloadFound.value())
				return std::unexpected{ InjectionError::AlreadyActive };
		}

		const auto tid = GetTargetThreadId(pid);
		if (!tid)
			return std::unexpected{ InjectionError::ProcessNotFound };  // Likely process terminated
		auto hThread = OpenTargetThread(*tid);
		assert(hThread);
		if (!hThread)
			return std::unexpected{ InjectionError::SystemCallError };  // Note: could also be process terminated

		const auto path = GetCurrentDirectoryW() + L"\\" + PayloadName();
		const auto hMod = ::LoadLibraryW(path.c_str());
		assert(hMod);
		if (hMod == nullptr)
			return std::unexpected{ InjectionError::HookError };
		const auto hHookProc = reinterpret_cast<HOOKPROC>(::GetProcAddress(hMod, "Dummy"));
		assert(hHookProc);
		const auto hHook = ::SetWindowsHookExW(WH_GETMESSAGE, hHookProc, hMod, ::GetThreadId(*hThread));
		if (hHook == nullptr)
			return std::unexpected{ InjectionError::HookError };

		if (!WaitForPayload(*hProc, 5s))
			return std::unexpected{ InjectionError::SystemCallError };

		::UnhookWindowsHookEx(hHook);
		return hProc;
	}

	static auto GetTargetProcessList()
	{
		return gan::ProcessEnumerator{}().value_or({ })
			| std::views::filter([](const auto& proc) { return ::StrStrIW(proc.imageName.c_str(), LineImageName()); });
	}

	static gan::AutoWinHandle OpenTargetProcess(uint32_t pid)
	{
		constexpr BOOL k_notInheritable = FALSE;
		constexpr DWORD k_procAccessFlags =
			PROCESS_VM_OPERATION
			| PROCESS_VM_WRITE
			| PROCESS_SUSPEND_RESUME
			| SYNCHRONIZE;
		return gan::AutoWinHandle{ ::OpenProcess(k_procAccessFlags, k_notInheritable, pid) };
	}

	static gan::AutoWinHandle OpenTargetThread(uint32_t tid)
	{
		constexpr BOOL k_notInheritable = FALSE;
		constexpr DWORD k_threadAccessFlags =
			THREAD_QUERY_INFORMATION
			| THREAD_SET_INFORMATION
			| THREAD_SUSPEND_RESUME
			| THREAD_GET_CONTEXT
			| THREAD_SET_CONTEXT
			| SYNCHRONIZE;
		return gan::AutoWinHandle{ ::OpenThread(k_threadAccessFlags, k_notInheritable, tid) };
	}

private:
	static std::optional<uint32_t> GetTargetThreadId(uint32_t pid)
	{
		if (const auto threadList = gan::ThreadEnumerator{}(pid);
			threadList && threadList->size() > 0)
		{
			return (*threadList)[0].tid;  // Any thread should do. Just return the first one.
		}
		return std::nullopt;
	}

	static std::optional<bool> PayloadExistsIn(gan::WinHandle process)
	{
		if (const auto modList = gan::ModuleEnumerator{}(process))
		{
			const auto itr = std::find_if(
				modList->begin(),
				modList->end(),
				[](const auto& mod) -> bool { return ::StrStrIW(mod.imageName.c_str(), PayloadName()); }
			);
			return std::make_optional(itr != modList->end());
		}
		return std::nullopt;
	}

	static bool WaitForPayload(gan::WinHandle process, std::chrono::seconds timeout)
	{
		constexpr static auto k_checkInterval = 100ms;

		for (std::chrono::milliseconds counter{ };
			counter < timeout;
			counter += k_checkInterval)
		{
			const auto payloadFound = PayloadExistsIn(process);
			if (!payloadFound)
				return false;  // Process likely terminated
			else if (payloadFound.value())
				return true;  // Payload detected in target memory

			::Sleep(static_cast<DWORD>(k_checkInterval.count()));
		}
		return false;
	}
};


class Lauss
{
public:
	[[noreturn]] static void RunMainLoop()
	{
		std::vector<InjectedClient> activeClients;

		while (true)
		{
			constexpr auto k_sleepDuration = 1s;
			std::this_thread::sleep_for(k_sleepDuration);

			RemoveTerminatedClients(activeClients);

			for (const auto& proc : LineProcessHelper::GetTargetProcessList())
			{
				if (std::ranges::find(activeClients, proc.pid, &InjectedClient::pid) != activeClients.end())
					continue;

				if (auto injectResult = LineProcessHelper::InjectPayload(proc.pid))
				{
					activeClients.emplace_back(std::move(injectResult.value()), proc.pid);
					Printf("Payload injected into pid=%u\n", proc.pid);
				}
				else
				{
					Printf(
						"Failed to inject payload into pid=%u, error=%s\n",
						proc.pid,
						LineProcessHelper::k_ResultStrings[std::to_underlying(injectResult.error())].data()
					);
				}
			}
		}
	}

private:
	struct InjectedClient
	{
		gan::AutoWinHandle handle;
		uint32_t pid;
	};

	static void RemoveTerminatedClients(std::vector<InjectedClient>& clients)
	{
		auto filtered = clients
			| std::views::filter([](const auto& client) { return IsProcessStillAlive(*client.handle).value_or(false); })
			| std::views::as_rvalue
			| std::ranges::to<std::vector>();
		std::swap(filtered, clients);
	}
};

}  // unnamed namespace


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	if constexpr (UseDebugConsole())
	{
		::AllocConsole();
	}

	Lauss::RunMainLoop();
}
