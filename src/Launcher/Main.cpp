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


class LineHelper
{
public:
	static auto GetTargetProcessList()
	{
		return gan::ProcessEnumerator{}().value_or({ })
			| std::views::filter([](const auto& proc) { return ::StrStrIW(proc.imageName.c_str(), LineImageName()); });
	}

	static std::optional<uint32_t> GetTargetThreadId(uint32_t pid)
	{
		if (const auto threadList = gan::ThreadEnumerator{}(pid);
			threadList && threadList->size() > 0)
		{
			return (*threadList)[0].tid;  // Any thread should do. Just return the first one.
		}
		return std::nullopt;
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
};


class PayloadHelper
{
public:
	enum class InjectionError
	{
		ProcessNotFound,
		HookError,
		SystemCallError,
		_Count
	};
	constexpr static std::array<std::string_view, std::to_underlying(InjectionError::_Count)> k_ResultStrings
	{
		"ProcessNotFound"sv,
		"HookError"sv,
		"SystemCallError"sv
	};

	static std::expected<gan::AutoWinHandle, InjectionError> InjectPayload(uint32_t pid)
	{
		auto hProc = LineHelper::OpenTargetProcess(pid);
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
				return hProc;
		}

		const auto tid = LineHelper::GetTargetThreadId(pid);
		if (!tid)
			return std::unexpected{ InjectionError::ProcessNotFound };  // Likely process terminated
		auto hThread = LineHelper::OpenTargetThread(*tid);
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

		WaitForPayload(*hProc, 5s);
		::UnhookWindowsHookEx(hHook);

		return hProc;
	}

private:
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
	void RunMainLoop()
	{
		constexpr size_t k_maxTolerableCrashes = 2;

		while (m_crashCounter < k_maxTolerableCrashes)
		{
			SleepAdaptively(m_activeClients);

			RemoveTerminatedClients(m_activeClients);
			const auto injectionSummary = PatchCandidateClients(m_activeClients);
			m_crashCounter = UpdatedCrashCounter(m_crashCounter, injectionSummary);
		}
		Printf("Exiting due to consecutive %zu crashes detected.\n", k_maxTolerableCrashes);
	}

private:
	struct InjectedClient
	{
		gan::AutoWinHandle handle;
		uint32_t pid;
	};

	static void SleepAdaptively(const std::vector<InjectedClient>& activeClients)
	{
		constexpr auto k_sleepDurationHiFreq = 1s;
		constexpr auto k_sleepDurationLoFreq = 10s;
		std::this_thread::sleep_for(
			activeClients.empty() ? k_sleepDurationHiFreq : k_sleepDurationLoFreq
		);
	}

	static void RemoveTerminatedClients(std::vector<InjectedClient>& activeClients)
	{
		auto filtered = activeClients
			| std::views::filter([](const auto& client) { return IsProcessStillAlive(*client.handle).value_or(false); })
			| std::views::as_rvalue
			| std::ranges::to<std::vector>();
		std::swap(filtered, activeClients);
	}

	struct InjectionSummary
	{
		size_t total;
		size_t success;
	};
	static InjectionSummary PatchCandidateClients(std::vector<InjectedClient>& activeClients)
	{
		auto injectionTarget =
			LineHelper::GetTargetProcessList()
			| std::views::filter([&activeClients](const auto& proc) {
				return std::ranges::find(activeClients, proc.pid, &InjectedClient::pid) == activeClients.end();
			});

		auto newlyInjected = InjectClients(injectionTarget);
		const auto numInjected = newlyInjected.size();
		if (numInjected == 0)
			return { };

		// Detect potential crash due to payload injection
		{
			// Give newly injected clients a little bit of time to potentially crash
			constexpr auto k_sleepDurationAfterInject = 3s;
			std::this_thread::sleep_for(k_sleepDurationAfterInject);

			RemoveTerminatedClients(newlyInjected);
		}
		const auto numStillAlive = newlyInjected.size();

		activeClients.append_range(newlyInjected | std::views::as_rvalue);
		return { .total=numInjected, .success=numStillAlive };
	}

	static std::vector<InjectedClient> InjectClients(auto&& candidates)
	{
		std::vector<InjectedClient> injected;

		for (const auto& proc : candidates)
		{
			if (auto injectResult = PayloadHelper::InjectPayload(proc.pid))
			{
				injected.emplace_back(std::move(injectResult.value()), proc.pid);
				Printf("Payload has been injected into pid=%u\n", proc.pid);
			}
			else
			{
				Printf(
					"Failed to inject payload into pid=%u, error=%s\n",
					proc.pid,
					PayloadHelper::k_ResultStrings[std::to_underlying(injectResult.error())].data()
				);
			}
		}
		return injected;
	}

	static size_t UpdatedCrashCounter(size_t crashCounter, InjectionSummary injection)
	{
		if (injection.success > 0)  // Any successful injection resets counter
		{
			crashCounter = 0;
			Printf("Crash counter reset.\n");
		}
		else if (injection.total > 0)  // `total` must now be #crashes
		{
			crashCounter += injection.total;
			Printf("%zu crash(es) detected. Now counter=%zu\n", injection.total, crashCounter);
		}
		return crashCounter;
	}

	std::vector<InjectedClient> m_activeClients{ };
	size_t m_crashCounter{ };
};

}  // unnamed namespace


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	if constexpr (UseDebugConsole())
	{
		::AllocConsole();
	}

	Lauss{ }.RunMainLoop();
	return NO_ERROR;
}
