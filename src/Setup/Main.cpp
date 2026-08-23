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

#include "Setup.hpp"

#include <LaussDef.hpp>
#include <Utils.hpp>

#include <Gandr/Types.hpp>

#include <windows.h>
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <cassert>
#include <string_view>


using namespace std::literals;


namespace
{

// Ref: Chen, R. (2013). "How do I wait until all processes in a job have exited?"
//      https://devblogs.microsoft.com/oldnewthing/20130405-00/?p=4743
class JobObject
{
public:
	JobObject()
	{
		constexpr LPSECURITY_ATTRIBUTES k_noSecAttr = nullptr;
		constexpr const wchar_t* k_nameless = nullptr;
		m_hJob = gan::AutoWinHandle{ ::CreateJobObjectW(k_noSecAttr, k_nameless) };

		constexpr HANDLE k_noExistingPort = nullptr;
		constexpr uintptr_t k_noCompletionKey = 0;
		constexpr gan::WinDword k_numThreads = 1;
		m_hPort = gan::AutoWinHandle{ ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, k_noExistingPort, k_noCompletionKey, k_numThreads) };

		if (m_hJob && m_hPort)
		{
			JOBOBJECT_ASSOCIATE_COMPLETION_PORT port{ .CompletionKey = *m_hJob, .CompletionPort = *m_hPort };
			const auto setResult = ::SetInformationJobObject(
				*m_hJob,
				JobObjectAssociateCompletionPortInformation,
				&port,
				static_cast<gan::WinDword>(sizeof(port))
			);
			m_fullyInit = (setResult != FALSE);
		}
	}
	bool SetProcess(gan::WinHandle process)
	{
		assert(operator bool());
		if (!operator bool())
			return false;

		return (::AssignProcessToJobObject(*m_hJob, process) != FALSE);
	}
	void Wait()
	{
		assert(operator bool());
		if (!operator bool())
			return;

		gan::WinDword completionCode{ };
		uintptr_t completionKey{ };
		LPOVERLAPPED overlapped{ };
		while (::GetQueuedCompletionStatus(*m_hPort, &completionCode, &completionKey, &overlapped, INFINITE) != FALSE)
		{
			if (completionKey == reinterpret_cast<uintptr_t>(*m_hJob)
				&& completionCode == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
			{
				break;
			}
		}
	}
	operator bool() const
	{
		return m_fullyInit;
	}

private:
	gan::AutoWinHandle m_hJob{ };
	gan::AutoWinHandle m_hPort{ };
	bool m_fullyInit{ false };
};


[[nodiscard]] bool LaunchExistingUninstaller(std::wstring_view uninstallerPath)
{
	auto cmdLine = lauss::MakeUninstallCmdLine(lauss::AddDoubleQuotes(uninstallerPath));
	auto uninstallerProcess = lauss::CreateProcessWithCommand(cmdLine, CREATE_SUSPENDED);
	assert(uninstallerProcess);
	if (!uninstallerProcess)
		return false;

	gan::Deferred killSuspendedProcess{ [process=*uninstallerProcess->process](){
		::TerminateProcess(process, NO_ERROR);
	} };
	{
		JobObject job;
		assert(job);
		if (!job)
			return false;

		const auto setProcessResult = job.SetProcess(*uninstallerProcess->process);
		assert(setProcessResult);
		if (!setProcessResult)
			return false;

		::ResumeThread(*uninstallerProcess->thread);
		job.Wait();
	}
	return true;
}

}  // unnamed namespace


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	const auto args = lauss::GetCmdLineArgs();

	const auto installDir = lauss::GetInstallationDir();
	assert(installDir.size() > 0);
	if (installDir.size() == 0)
	{
		// "Critical error: Failed to obtain install path.\n";
		return -1;
	}

	const auto installCtx = lauss::setup::InstallContext::Make(installDir);
	assert(installCtx);
	if (!installCtx)
	{
		// "Critical error: Failed to generate install context.\n"
		return -1;
	}

	if (args.size() > 1
		&& ::lstrcmpiW(args[1].c_str(), lauss::CmdLineOptUninstall()) == 0)
	{
		lauss::setup::Uninstall(installCtx.value());
	}
	else
	{
		if (lauss::IsFileReadable(installCtx->pathUninstaller)
			&& !LaunchExistingUninstaller(installCtx->pathUninstaller))
		{
			// "Critical error: Failed to run uninstaller of the previous installation.\n"
			return -1;
		}

		if (!lauss::IsFileReadable(installCtx->pathUninstaller))
			lauss::setup::Install(installCtx.value());
	}

	return NO_ERROR;
}
