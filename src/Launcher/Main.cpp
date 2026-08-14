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

#include "Debug.h"
#include "LaussDef.h"
#include "Utils.h"

#include <DllInjector.h>
#include <DllLookup.h>
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
#include <filesystem>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>


using namespace std::literals;


namespace
{

class LineHelper
{
public:
	[[nodiscard]] static auto GetTargetProcessList()
	{
		return gan::ProcessEnumerator{}().value_or({ })
			| std::views::filter([](const auto& proc) { return ::StrStrIW(proc.imageName.c_str(), LineImageName()); });
	}

	[[nodiscard]] static std::optional<uint32_t> GetTargetThreadId(uint32_t pid)
	{
		if (const auto threadList = gan::ThreadEnumerator{}(pid);
			threadList && threadList->size() > 0)
		{
			return (*threadList)[0].tid;  // Any thread should do. Just return the first one.
		}
		return std::nullopt;
	}

	[[nodiscard]] static gan::AutoWinHandle OpenTargetProcess(uint32_t pid)
	{
		constexpr BOOL k_notInheritable = FALSE;
		constexpr DWORD k_procAccessFlags =
			PROCESS_VM_OPERATION
			| PROCESS_VM_WRITE
			| PROCESS_SUSPEND_RESUME
			| SYNCHRONIZE;
		return gan::AutoWinHandle{ ::OpenProcess(k_procAccessFlags, k_notInheritable, pid) };
	}

	[[nodiscard]] static gan::AutoWinHandle OpenTargetThread(uint32_t tid)
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
	struct LoadedPayload
	{
		gan::AutoWinModule mod;
		const HOOKPROC hookFunc;
	};

	[[nodiscard]] static std::optional<LoadedPayload> LoadPayload()
	{
		const auto currDir = GetExeDirectory();
		assert(currDir.size() > 0);
		if (currDir.size() == 0)
			return std::nullopt;

		const auto pathPayload = currDir + L"\\" + PayloadName();
		gan::AutoWinModule hMod{ ::LoadLibraryW(pathPayload.c_str()) };
		assert(hMod);
		if (!hMod)
			return std::nullopt;

		constexpr const char* k_hookFuncName = "Dummy";
		const auto hookFunc = gan::DllLookup::Get<HOOKPROC>(pathPayload, k_hookFuncName);
		assert(hookFunc);
		if (hookFunc == nullptr)
			return std::nullopt;

		return std::make_optional<LoadedPayload>(std::move(hMod), hookFunc);
	}

	enum class InjectionError
	{
		InvalidArgument,
		ProcessInaccessible,
		ThreadInaccessible,
		HookError,
		_Count
	};
	constexpr static std::array<std::string_view, std::to_underlying(InjectionError::_Count)> k_ResultStrings
	{
		"InvalidArgument"sv,
		"ProcessInaccessible"sv,
		"ThreadInaccessible"sv,
		"HookError"sv
	};
	[[nodiscard]] static std::expected<gan::AutoWinHandle, InjectionError> InjectPayload(uint32_t pid, const LoadedPayload& payload)
	{
		assert(payload.mod);
		assert(payload.hookFunc);
		if (payload.mod == nullptr || payload.hookFunc == nullptr)
			return std::unexpected{ InjectionError::InvalidArgument };

		auto hProc = LineHelper::OpenTargetProcess(pid);
		assert(hProc);
		if (!hProc)
		{
			// Either process not accessible or terminated
			return std::unexpected{ InjectionError::ProcessInaccessible };
		}
		else
		{
			const auto payloadFound = PayloadExistsIn(*hProc);
			if (!payloadFound)
				return std::unexpected{ InjectionError::ProcessInaccessible };  // Error during module enumeration
			else if (payloadFound.value())
				return hProc;
		}

		const auto tid = LineHelper::GetTargetThreadId(pid);
		if (!tid)
			return std::unexpected{ InjectionError::ThreadInaccessible };  // Likely process terminated
		auto hThread = LineHelper::OpenTargetThread(*tid);
		assert(hThread);
		if (!hThread)
			return std::unexpected{ InjectionError::ThreadInaccessible };  // Note: could also be process terminated

		const auto hHook = ::SetWindowsHookExW(WH_GETMESSAGE, payload.hookFunc, *payload.mod, ::GetThreadId(*hThread));
		if (hHook == nullptr)
			return std::unexpected{ InjectionError::HookError };

		WaitForPayload(*hProc, 5s);
		::UnhookWindowsHookEx(hHook);

		return hProc;
	}

private:
	[[nodiscard]] static std::optional<bool> PayloadExistsIn(gan::WinHandle process)
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

		auto payload = PayloadHelper::LoadPayload();
		if (!payload)
		{
			Printf("Exiting due to errors when loading %S.\n", PayloadName());
			return;
		}

		while (m_crashCounter < k_maxTolerableCrashes)
		{
			SleepAdaptively(m_activeClients);

			RemoveTerminatedClients(m_activeClients);
			const auto injectionSummary = PatchCandidateClients(payload.value(), m_activeClients);
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
	[[nodiscard]] static InjectionSummary PatchCandidateClients(const PayloadHelper::LoadedPayload& payload, std::vector<InjectedClient>& activeClients)
	{
		auto injectionTarget =
			LineHelper::GetTargetProcessList()
			| std::views::filter([&activeClients](const auto& proc) {
				return std::ranges::find(activeClients, proc.pid, &InjectedClient::pid) == activeClients.end();
			});

		auto newlyInjected = InjectClients(payload, injectionTarget);
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

	[[nodiscard]] static std::vector<InjectedClient> InjectClients(const PayloadHelper::LoadedPayload& payload, auto&& candidates)
	{
		std::vector<InjectedClient> injected;

		for (const auto& proc : candidates)
		{
			if (auto injectResult = PayloadHelper::InjectPayload(proc.pid, payload))
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

	[[nodiscard]] static size_t UpdatedCrashCounter(size_t crashCounter, InjectionSummary injection)
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
