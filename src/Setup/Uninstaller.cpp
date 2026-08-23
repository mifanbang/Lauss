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

#include "SetupRegistries.hpp"
#include "RestartSession.hpp"

#include <Utils.hpp>

#include <Gandr/ProcessList.hpp>

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <string_view>


namespace
{

using namespace lauss;
using namespace lauss::setup;
using namespace std::literals;


[[nodiscard]] bool CreateAndRunShadowUninstaller()
{
	const auto exePath = GetExePath();
	assert(exePath.size() > 0);
	if (exePath.size() == 0)
		return false;

	const auto tmpFilePath = CreateTempFile();
	assert(tmpFilePath.size() > 0);
	if (tmpFilePath.size() == 0)
		return false;

	constexpr BOOL k_overwrite = FALSE;
	::CopyFileW(exePath.c_str(), tmpFilePath.c_str(), k_overwrite);

	auto cmdLine = MakeUninstallCmdLine(AddDoubleQuotes(tmpFilePath));
	constexpr gan::WinDword k_noFlags = 0;
	const auto newProcess = CreateProcessWithCommand(cmdLine, k_noFlags);
	assert(newProcess);
	if (!newProcess)
		return false;

	return true;
}

[[nodiscard]] bool MsgBoxCloseLine()
{
	TASKDIALOGCONFIG config{
		.cbSize = sizeof(config),
		.hwndParent = nullptr,
		.dwFlags = TDF_SIZE_TO_CONTENT,
		.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_CLOSE_BUTTON,
		.pszWindowTitle = L"Lauss Installer",
		.pszMainIcon = TD_WARNING_ICON,
		.pszMainInstruction = L"Lauss is running",
		.pszContent =
			L"Lauss is currently running inside your LINE app.\n\n"
			L"To continue with the current operation, LINE is required to shut down.\n\n"
			L"[YES] to let Lauss shut down LINE for you, or\n"
			L"[CLOSE] to abort the current operation",
	};

	int buttonResult{ };
	::TaskDialogIndirect(&config, &buttonResult, nullptr, nullptr);
	return buttonResult == IDYES;
}

[[nodiscard]] bool RestartSessionProcessHandler(const RestartSession::ProcessList& procList)
{
	const bool lineFound = std::ranges::any_of(
		procList,
		[](const auto& procName) { return ::lstrcmpiW(procName.c_str(), L"LINE") == 0; }
	);

	// Don't care about processes other than LINE and will just shut them down
	return lineFound ? MsgBoxCloseLine() : true;
}

void WaitOnParentProcess(std::wstring_view parentImage)
{
	const auto processList = gan::ProcessEnumerator{}();
	assert(processList);
	if (!processList)
		return;

	const uint32_t currPid = ::GetCurrentProcessId();
	const auto currProcessInfo = std::ranges::find(processList.value(), currPid, &gan::ProcessInfo::pid);
	assert(currProcessInfo != processList->end());
	if (currProcessInfo == processList->end())
		return;

	if (!parentImage.empty())
	{
		const auto parentProcessInfo = std::ranges::find(processList.value(), currProcessInfo->pidParent, &gan::ProcessInfo::pid);
		if (parentProcessInfo == processList->end()
			|| ::lstrcmpiW(parentProcessInfo->imageName.c_str(), parentImage.data()) != 0)
		{
			return;
		}
	}

	constexpr BOOL k_nonInheritable = FALSE;
	gan::AutoWinHandle hProc{ ::OpenProcess(SYNCHRONIZE, k_nonInheritable, currProcessInfo->pidParent) };
	assert(hProc);
	if (!hProc)
		return;

	constexpr auto k_waitDuration = 5s;
	WaitOnProcess(*hProc, k_waitDuration);
}

[[nodiscard]] bool RemoveFilesAndDir(const InstallContext& ctx)
{
	RestartSession rmSession;
	assert(rmSession);
	if (!rmSession)
		return false;

	const auto procRestartResult = rmSession.RestartProcessesUsingFiles(
		ctx,
		[](auto&& arg) { return RestartSessionProcessHandler(arg); }
	);
	if (!procRestartResult)
		return false;

	::DeleteFileW(ctx.pathDaemon.c_str());
	::DeleteFileW(ctx.pathPayload.c_str());
	::DeleteFileW(ctx.pathUninstaller.c_str());
	::RemoveDirectoryW(ctx.installDir.c_str());

	return true;  // Report a success even if file/dir removal fails
}

void CleanUpShadowUninstaller()
{
	const auto exePath = GetExePath();
	assert(exePath.size() > 0);
	if (exePath.size() == 0)
		return;

	auto cmdLine = std::move(
		std::wstring{ L"cmd.exe /C timeout /t 5 /nobreak >nul & del \"" }
		.append(exePath)
		.append(1, L'"')
	);
	const auto newProcess = CreateProcessWithCommand(cmdLine, CREATE_NO_WINDOW);
	assert(newProcess);
	if (!newProcess)
		return;
}

}  // unnames namespace


namespace lauss::setup
{

void Uninstall(const InstallContext& ctx)
{
	const auto exeDir = GetExeDir();
	assert(exeDir.size() > 0);
	if (exeDir.size() == 0)
	{
		// "Critical error: Failed to obtain current exe's parent path.\n"
		return;
	}

	if (::lstrcmpiW(exeDir.c_str(), ctx.installDir.c_str()) == 0)
	{
		const auto runShadowResult = CreateAndRunShadowUninstaller();
		assert(runShadowResult);
		if (!runShadowResult)
		{
			// "Critical error: Failed to create and run shadow uninstaller.\n"
		}
		return;
	}

	// To get here, the current process must be a "shadow uninstaller" run from the image
	// copied into the user's tmp dir. Therefore we must wait for the parent process, which
	// is loaded from the image inside installation dir, to terminate first before deleting
	// its file.
	WaitOnParentProcess(UninstallerName());

	// File and directory removal
	const auto filesRemoved = RemoveFilesAndDir(ctx);
	if (!filesRemoved)
		return;

	// Registry clean-ups
	[[maybe_unused]] const auto regStartUpRemoved = StartUpRegistry::Remove();
	[[maybe_unused]] const auto regUninstallRemoved = UninstallRegistry::Remove();

	CleanUpShadowUninstaller();
}

}  // namespace lauss::setup
