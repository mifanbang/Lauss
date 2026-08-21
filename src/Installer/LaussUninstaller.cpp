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

#include "LaussUninstaller.h"

#include "Installer.h"
#include "InstallerRegistry.h"
#include "RestartSession.h"

#include <Utils.h>

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cassert>


namespace
{

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

	constexpr wchar_t* k_emptyAppName = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noProcSecAttr = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noThrdSecAttr = nullptr;
	constexpr BOOL k_noInheritHandles = FALSE;
	constexpr void* k_useCurrentEnv = nullptr;
	constexpr wchar_t* k_useCurrentDir = nullptr;

	std::wstring cmdLine =
		AddDoubleQuotes(tmpFilePath)
		.append(1, L' ')
		.append(CmdLineOptUninstall());
	STARTUPINFOW k_startupInfo{ .cb = sizeof(k_startupInfo) };
	PROCESS_INFORMATION procInfo{ };
	const auto newProcessResult = ::CreateProcessW(
		k_emptyAppName,
		cmdLine.data(),
		k_noProcSecAttr,
		k_noThrdSecAttr,
		k_noInheritHandles,
		NORMAL_PRIORITY_CLASS,
		k_useCurrentEnv,
		k_useCurrentDir,
		&k_startupInfo,
		&procInfo
	);
	if (newProcessResult == FALSE)
		return false;

	::CloseHandle(procInfo.hThread);
	::CloseHandle(procInfo.hProcess);
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
	return lineFound ? MsgBoxCloseLine() : true;  // Don't care about processes other than LINE and will just shut them down
}

void CleanUpShadowUninstaller()
{
	const auto exePath = GetExePath();
	assert(exePath.size() > 0);
	if (exePath.size() == 0)
		return;

	constexpr wchar_t* k_emptyAppName = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noProcSecAttr = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noThrdSecAttr = nullptr;
	constexpr BOOL k_noInheritHandles = FALSE;
	constexpr void* k_useCurrentEnv = nullptr;
	constexpr wchar_t* k_useCurrentDir = nullptr;

	auto cmdLine =
		std::wstring{ L"cmd.exe /C timeout /t 5 /nobreak >nul & del \"" }
		.append(exePath)
		.append(1, L'"');
	STARTUPINFOW k_startupInfo{ .cb = sizeof(k_startupInfo) };
	PROCESS_INFORMATION procInfo{ };
	const auto newProcessResult = ::CreateProcessW(
		k_emptyAppName,
		cmdLine.data(),
		k_noProcSecAttr,
		k_noThrdSecAttr,
		k_noInheritHandles,
		NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,
		k_useCurrentEnv,
		k_useCurrentDir,
		&k_startupInfo,
		&procInfo
	);
	assert(newProcessResult);
	if (newProcessResult == FALSE)
		return;

	::CloseHandle(procInfo.hThread);
	::CloseHandle(procInfo.hProcess);
}

}  // unnames namespace


void LaussUninstaller::Exec()
{
	const auto exeDir = GetExeDir();
	assert(exeDir.size() > 0);
	if (exeDir.size() == 0)
	{
		// "Critical error: Failed to obtain current exe's parent path.\n"
		return;
	}

	const auto installDir = GetInstallationDir();
	assert(installDir.size() > 0);
	if (installDir.size() == 0)
	{
		// "Critical error: Failed to obtain install path.\n";
		return;
	}

	if (::lstrcmpiW(exeDir.c_str(), installDir.c_str()) == 0)
	{
		const auto runShadowResult = CreateAndRunShadowUninstaller();
		assert(runShadowResult);
		if (!runShadowResult)
		{
			// "Critical error: Failed to create and run shadow uninstaller.\n"
		}
		return;
	}

	WaitOnParentProcess(UninstallerName());

	const auto installCtx = InstallContext::Make(installDir);
	assert(installCtx);
	if (!installCtx)
	{
		// "Critical error: Failed to generate install context.\n"
		return;
	}

	// File and directory removal
	{
		RestartSession rmSession;
		assert(rmSession);

		const auto procRestartResult = rmSession.RestartProcessesUsingFiles(
			installCtx.value(),
			[](auto&& arg) { return RestartSessionProcessHandler(arg); }
		);
		if (!procRestartResult)
			return;

		::DeleteFileW(installCtx->pathLauncher.c_str());
		::DeleteFileW(installCtx->pathPayload.c_str());
		::DeleteFileW(installCtx->pathUninstaller.c_str());
		::RemoveDirectoryW(installCtx->installDir.c_str());
	}

	// Registry clean-ups
	[[maybe_unused]] const auto removeRegStartUp = StartUpRegistry::Remove();
	[[maybe_unused]] const auto removeRegUninstall = UninstallRegistry::Remove();

	CleanUpShadowUninstaller();
}
