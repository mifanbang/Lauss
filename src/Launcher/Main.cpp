/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2020-2023 Mifan Bang <https://debug.tw>.
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
		Succeeded,
		AlreadyActive,
		ProcessNotFound,
		SystemCallError,

		Count
	};
	constexpr static std::array<std::string_view, std::to_underlying(InjectionResult::Count)> k_ResultStrings
	{
		"Success"sv,
		"AlreadyActive"sv,
		"ProcessNotFound"sv,
		"SystemCallError"sv
	};

	// .first = process is found
	// .second = payload is found
	static std::pair<bool, bool> FindPayloadInProcess(uint32_t pid)
	{
		gan::ModuleList modList;
		if (const auto enumResult = gan::ModuleEnumerator::Enumerate(pid, modList);
			enumResult == gan::ModuleEnumerator::Result::Success)
		{
			const auto itr = std::find_if(
				modList.begin(),
				modList.end(),
				[](const auto& mod) -> bool { return StrStrIW(mod.imageName.c_str(), L"Payload.dll"); }
			);
			return std::make_pair(true, itr != modList.end());
		}
		return std::make_pair(false, false);
	}

	static InjectionResult InjectPayload()
	{
		if (const auto pair = GetProcessThreadIds())
		{
			auto [pid, tid] = *pair;
			if (FindPayloadInProcess(pid).second)
				return InjectionResult::AlreadyActive;  // Payload had already been injected

			constexpr BOOL k_notInheritable = FALSE;
			constexpr DWORD k_procAccessFlags =
				PROCESS_VM_OPERATION
				| PROCESS_VM_WRITE
				| PROCESS_SUSPEND_RESUME
				| SYNCHRONIZE;
			gan::AutoWinHandle hProc{ OpenProcess(k_procAccessFlags, k_notInheritable, pid) };
			assert(hProc);

			constexpr DWORD k_threadAccessFlags = 
				THREAD_QUERY_INFORMATION
				| THREAD_SET_INFORMATION
				| THREAD_SUSPEND_RESUME
				| THREAD_GET_CONTEXT
				| THREAD_SET_CONTEXT
				| SYNCHRONIZE;
			gan::AutoWinHandle hThread{ OpenThread(k_threadAccessFlags, k_notInheritable, tid) };
			assert(hThread);

			gan::DllInjectorByContext injector(*hProc, *hThread);
			const auto injectResult = injector.Inject(GetCurrentDirectoryW() + L"\\Payload.dll"s);
			return (injectResult == gan::DllInjectorByContext::Result::Succeeded) && WaitForPayloadBeingLoaded(pid, 5s) ?
				InjectionResult::Succeeded :
				InjectionResult::SystemCallError;
		}
		return InjectionResult::ProcessNotFound;
	}

private:
	static std::optional<uint32_t> GetProcessId()
	{
		gan::ProcessList procList;
		[[maybe_unused]] const auto enumResult = gan::ProcessEnumerator::Enumerate(procList);
		assert(enumResult == gan::ProcessEnumerator::Result::Success);

		for (const auto& proc : procList)
		{
			if (StrStrIW(proc.imageName.c_str(), L"line.exe"))
				return proc.pid;
		}
		return std::nullopt;
	}

	static std::optional<uint32_t> GetThreadId(uint32_t pid)
	{
		// Any thread should do. Just return the first one.
		gan::ThreadList threadList;
		const auto enumThreadResult = gan::ThreadEnumerator::Enumerate(pid, threadList);
		if (enumThreadResult == gan::ThreadEnumerator::Result::Success
			&& threadList.size() > 0)
		{
			return threadList[0].tid;
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

	static bool WaitForPayloadBeingLoaded(uint32_t pid, std::chrono::seconds timeout)
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
	printf("Result: %s\n", LineProcessHelper::k_ResultStrings[std::to_underlying(launchResult)].data());
	return NO_ERROR;
}
