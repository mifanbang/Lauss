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

#include "Installer.h"
#include "InstallerRegistry.h"
#include "LaussDef.h"
#include "Registry.h"
#include "Resource.h"
#include "RestartSession.h"
#include "Utils.h"

#include <Gandr/Handle.hpp>
#include <Gandr/Types.hpp>

#include <windows.h>
#include <commctrl.h>
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>


using namespace std::literals;


namespace
{

class InstallHelper
{
public:
	struct PackedItem
	{
		uint32_t resName;
		const std::wstring InstallContext::* path;
	};
	[[nodiscard]] static bool Install(const InstallContext& ctx)
	{
		// Unpack files from resource section
		constexpr std::array<PackedItem, 2> k_items{ {
			{ .resName = LAUNCHER,	.path = &InstallContext::pathLauncher },
			{ .resName = PAYLOAD,	.path = &InstallContext::pathPayload }
		} };
		for (const auto& item : k_items)
		{
			const auto data = GetResource(MAKEINTRESOURCE(item.resName));
			assert(!data.empty());
			if (data.empty())
				return false;

			const auto writeResult = WriteToFile(data, ctx.*(item.path));
			assert(writeResult);
			if (!writeResult)
				return false;
		}

		// Copy installer
		constexpr BOOL k_alwaysOverwrite = FALSE;
		const auto copyResult = ::CopyFileW(GetExePath().c_str(), ctx.pathUninstaller.c_str(), k_alwaysOverwrite);
		assert(copyResult);
		return copyResult != FALSE;
	}

private:
	[[nodiscard]] static std::span<const std::byte> GetResource(const wchar_t* resName)
	{
		constexpr gan::WinModule k_currMod = nullptr;
		const auto hResInfo = ::FindResourceW(k_currMod, resName, RT_RCDATA);
		assert(hResInfo);
		if (hResInfo == nullptr)
			return { };

		const size_t dataSize = ::SizeofResource(k_currMod, hResInfo);
		assert(dataSize > 0);
		if (dataSize == 0)
			return { };

		const auto hResData = ::LoadResource(k_currMod, hResInfo);
		assert(hResData);
		if (hResData == nullptr)
			return { };

		const auto* data = reinterpret_cast<const std::byte*>(::LockResource(hResData));
		assert(data);
		if (data == nullptr)
			return { };

		return { data, dataSize };
	}

	[[nodiscard]] static bool WriteToFile(std::span<const std::byte> data, std::wstring_view path)
	{
		assert(!data.empty());
		assert(!path.empty());

		constexpr LPSECURITY_ATTRIBUTES k_noSecAttr = nullptr;
		constexpr gan::WinDword k_noShare = 0;
		constexpr gan::WinHandle k_noTemplateFile = nullptr;
		gan::AutoWinHandle hFile{ ::CreateFileW(
			path.data(),
			GENERIC_WRITE,
			k_noShare,
			k_noSecAttr,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			k_noTemplateFile
		) };
		assert(hFile);
		if (!hFile)
			return false;

		constexpr LPOVERLAPPED k_noOverlap = nullptr;
		gan::WinDword sizeWritten{ };
		const BOOL writeResult = ::WriteFile(
			*hFile,
			data.data(),
			static_cast<gan::WinDword>(data.size()),
			&sizeWritten,
			k_noOverlap
		);
		assert(writeResult);
		if (writeResult == FALSE)
			return false;

		return true;
	}
};


class LaussInstaller
{
public:
	static void Exec()
	{
		const auto installDir = GetInstallationDir();
		assert(installDir.size() > 0);
		if (installDir.size() == 0)
		{
			// "Critical error: Failed to obtain install path.\n";
			return;
		}

		const auto installCtx = InstallContext::Make(installDir);
		assert(installCtx);
		if (!installCtx)
		{
			// "Critical error: Failed to generate install context.\n"
			return;
		}

		[[maybe_unused]] const auto createPathResult = CreateDirRecursively(installDir);

		const auto installResult = InstallHelper::Install(installCtx.value());
		assert(installResult);
		if (!installResult)
		{
			// "Critical error: Failed to install files.\n"
			CleanUpFailedFiles(installCtx.value());
			return;
		}

		[[maybe_unused]] const auto createRegStartUp = StartUpRegistry::Create(installCtx.value());
		[[maybe_unused]] const auto createRegUninstall = UninstallRegistry::Create(installCtx.value());

		const auto launchResult = RunLauncher(installCtx.value());
		assert(launchResult);
		if (!launchResult)
		{
			// "Critical error: Failed to launch Lauss.\n"
			return;
		}
	}

private:
	[[nodiscard]] static bool RunLauncher(const InstallContext& ctx)
	{
		constexpr wchar_t* k_emptyCliArgs = nullptr;
		constexpr LPSECURITY_ATTRIBUTES k_noProcSecAttr = nullptr;
		constexpr LPSECURITY_ATTRIBUTES k_noThrdSecAttr = nullptr;
		constexpr BOOL k_noInheritHandles = FALSE;
		constexpr void* k_useCurrentEnv = nullptr;

		STARTUPINFOW k_startupInfo{ .cb = sizeof(k_startupInfo) };
		PROCESS_INFORMATION procInfo{ };
		const BOOL newProcessResult = ::CreateProcessW(
			ctx.pathLauncher.c_str(),
			k_emptyCliArgs,
			k_noProcSecAttr,
			k_noThrdSecAttr,
			k_noInheritHandles,
			NORMAL_PRIORITY_CLASS,
			k_useCurrentEnv,
			ctx.installDir.c_str(),
			&k_startupInfo,
			&procInfo
		);
		if (newProcessResult == FALSE)
			return false;

		::CloseHandle(procInfo.hThread);
		::CloseHandle(procInfo.hProcess);
		return true;
	}

	static void CleanUpFailedFiles(const InstallContext& /*ctx*/)
	{
	}
};


class LaussUninstaller
{
public:
	static void Exec()
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

private:
	static bool CreateAndRunShadowUninstaller()
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

	static bool MsgBoxCloseLine()
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

	static bool RestartSessionProcessHandler(auto&& procList)
	{
		const bool lineFound = std::ranges::any_of(
			procList,
			[](const auto& procName) { return ::lstrcmpiW(procName.c_str(), L"LINE") == 0; }
		);
		return lineFound ? MsgBoxCloseLine() : true;  // Don't care about processes other than LINE and will just shut them down
	}

	static void CleanUpShadowUninstaller()
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
};

}  // unnamed namespace


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	const auto args = GetCmdLineArgs();

	if (args.size() > 1 && ::lstrcmpiW(args[1].c_str(), CmdLineOptUninstall()) == 0)
	{
		LaussUninstaller::Exec();
	}
	else
	{
		// TODO: Detect previous installation
		LaussInstaller::Exec();
	}

	return NO_ERROR;
}
