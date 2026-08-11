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

#include <array>
#include <cassert>
#include <chrono>
#include <optional>
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


class LineProcessHelper
{
public:
	enum class InjectionResult
	{
		Success,
		AlreadyActive,
		ProcessNotFound,
		HookError,
		SystemCallError,

		Count
	};
	constexpr static std::array<std::string_view, std::to_underlying(InjectionResult::Count)> k_ResultStrings
	{
		"Success"sv,
		"AlreadyActive"sv,
		"ProcessNotFound"sv,
		"HookError"sv,
		"SystemCallError"sv
	};

	// .first = process is found
	// .second = payload is found
	static std::pair<bool, bool> FindPayloadInProcess(uint32_t pid)
	{
		if (const auto modList = gan::ModuleEnumerator{}(pid))
		{
			const auto itr = std::find_if(
				modList->begin(),
				modList->end(),
				[](const auto& mod) -> bool { return StrStrIW(mod.imageName.c_str(), PayloadName()); }
			);
			return std::make_pair(true, itr != modList->end());
		}
		return std::make_pair(false, false);
	}

	static InjectionResult InjectPayload()
	{
		const auto targetPidTid = GetProcessThreadIds();
		if (!targetPidTid)
			return InjectionResult::ProcessNotFound;

		const auto [pid, tid] = *targetPidTid;
		const auto [procFound, payloadFound] = FindPayloadInProcess(pid);
		if (!procFound)
			return InjectionResult::ProcessNotFound;
		else if (payloadFound)
			return InjectionResult::AlreadyActive;  // Payload already injected

		constexpr BOOL k_notInheritable = FALSE;
		constexpr DWORD k_procAccessFlags =
			PROCESS_VM_OPERATION
			| PROCESS_VM_WRITE
			| PROCESS_SUSPEND_RESUME
			| SYNCHRONIZE;
		gan::AutoWinHandle hProc{ OpenProcess(k_procAccessFlags, k_notInheritable, pid) };
		assert(hProc);
		if (!hProc)
			return InjectionResult::SystemCallError;  // Note: could also be process terminated

		constexpr DWORD k_threadAccessFlags =
			THREAD_QUERY_INFORMATION
			| THREAD_SET_INFORMATION
			| THREAD_SUSPEND_RESUME
			| THREAD_GET_CONTEXT
			| THREAD_SET_CONTEXT
			| SYNCHRONIZE;
		gan::AutoWinHandle hThread{ OpenThread(k_threadAccessFlags, k_notInheritable, tid) };
		assert(hThread);
		if (!hThread)
			return InjectionResult::SystemCallError;  // Note: could also be process terminated

		const auto path = GetCurrentDirectoryW() + L"\\" + PayloadName();
		const auto hMod = ::LoadLibraryW(path.c_str());
		assert(hMod);
		if (hMod == nullptr)
			return InjectionResult::HookError;
		const auto hHookProc = reinterpret_cast<HOOKPROC>(::GetProcAddress(hMod, "Dummy"));
		assert(hHookProc);
		const auto hHook = ::SetWindowsHookExW(WH_GETMESSAGE, hHookProc, hMod, ::GetThreadId(*hThread));
		if (hHook == nullptr)
			return InjectionResult::HookError;

		if (!WaitForPayload(pid, 5s))
			return InjectionResult::SystemCallError;

		::UnhookWindowsHookEx(hHook);
		return InjectionResult::Success;
	}

private:
	static std::optional<uint32_t> GetProcessId()
	{
		if (const auto procList = gan::ProcessEnumerator{}())
		{
			for (const auto& proc : *procList)
			{
				if (StrStrIW(proc.imageName.c_str(), LineImageName()))
					return proc.pid;
			}
		}
		return std::nullopt;
	}

	static std::optional<uint32_t> GetThreadId(uint32_t pid)
	{
		if (const auto threadList = gan::ThreadEnumerator{}(pid);
			threadList && threadList->size() > 0)
		{
			return (*threadList)[0].tid;  // Any thread should do. Just return the first one.
		}
		return std::nullopt;
	}

	static std::optional<std::pair<uint32_t, uint32_t>> GetProcessThreadIds()
	{
		if (const auto procId = GetProcessId())
		{
			if (const auto threadId = GetThreadId(*procId))
				return std::make_optional(std::make_pair(*procId, *threadId));
		}
		return std::nullopt;
	}

	static bool WaitForPayload(uint32_t pid, std::chrono::seconds timeout)
	{
		constexpr static std::chrono::milliseconds k_checkInterval{ 100ms };

		for (std::chrono::milliseconds counter{ };
			counter < timeout;
			counter += k_checkInterval)
		{
			if (FindPayloadInProcess(pid).second)
				return true;
			Sleep(static_cast<DWORD>(k_checkInterval.count()));
		}
		return false;
	}
};


}  // unnamed namespace


int main()
{
	const auto launchResult = LineProcessHelper::InjectPayload();
	Printf("Result: %s\n", LineProcessHelper::k_ResultStrings[std::to_underlying(launchResult)].data());
	return NO_ERROR;
}
