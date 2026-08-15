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
#include "Resource.h"

#include <Handle.h>
#include <Types.h>

#include <windows.h>
#include <shlobj.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <span>
#include <string>


namespace
{

struct InstallContext
{
	const std::wstring installDir;
	const std::wstring pathLauncher;
	const std::wstring pathPayload;

	// The convention is that all instantiated InstallContext's must have non-empty contents.
	static std::optional<InstallContext> Make(std::wstring_view installDir)
	{
		assert(installDir.size() > 0);
		if (installDir.size() == 0)
			return std::nullopt;

		return std::make_optional<InstallContext>(
			std::wstring{ installDir },
			std::wstring{ installDir }.append(1, L'\\').append(LauncherName()),
			std::wstring{ installDir }.append(1, L'\\').append(PayloadName())
		);
	}
};


[[nodiscard]] bool CreateFullPath(std::wstring_view path)
{
	assert(!path.empty());
	if (path.empty())
		return false;

	constexpr HWND k_noParent = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noSecAttr = nullptr;
	const auto sysResult = ::SHCreateDirectoryExW(k_noParent, path.data(), k_noSecAttr);
	return sysResult == NO_ERROR
		|| sysResult == ERROR_FILE_EXISTS
		|| sysResult == ERROR_ALREADY_EXISTS;
}


class InstallHelper
{
public:
	// Installation directory: C:\Users\[username]\AppData\Local\Lauss\
	// Inclusive of the trailing `\\`
	[[nodiscard]] static std::wstring GetTargetDir()
	{
		constexpr gan::WinDword k_defaultFlag = 0;
		constexpr gan::WinHandle k_noAccessToken = nullptr;

		std::wstring installDir;
		{
			wchar_t* pPath = nullptr;
			const auto dirGetResult = ::SHGetKnownFolderPath(FOLDERID_LocalAppData, k_defaultFlag, k_noAccessToken, &pPath);
			if (dirGetResult == S_OK)
				installDir = std::wstring{ pPath }.append(L"\\Lauss\\");
			::CoTaskMemFree(pPath);
		}
		return installDir;
	}

	struct InstallItem
	{
		uint32_t resName;
		const std::wstring InstallContext::* path;
	};
	[[nodiscard]] static bool Install(const InstallContext& ctx)
	{
		constexpr std::array<InstallItem, 2> k_items{
			InstallItem{ .resName = LAUNCHER,	.path = &InstallContext::pathLauncher },
			InstallItem{ .resName = PAYLOAD,	.path = &InstallContext::pathPayload }
		};

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
		return true;
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


class InstallManager
{
public:
	static void Exec()
	{
		const auto installDir = InstallHelper::GetTargetDir();
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

		[[maybe_unused]] const auto createPathResult = CreateFullPath(installDir);

		const auto installResult = InstallHelper::Install(installCtx.value());
		assert(installResult);
		if (!installResult)
		{
			// "Critical error: Failed to install files.\n"
			CleanUpFailedFiles(installCtx.value());
			return;
		}

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
		constexpr void* k_useInstallerEnv = nullptr;

		STARTUPINFOW k_startupInfo{ .cb = sizeof(k_startupInfo) };
		PROCESS_INFORMATION procInfo{ };
		const BOOL sysResult = ::CreateProcessW(
			ctx.pathLauncher.c_str(),
			k_emptyCliArgs,
			k_noProcSecAttr,
			k_noThrdSecAttr,
			k_noInheritHandles,
			NORMAL_PRIORITY_CLASS,
			k_useInstallerEnv,
			ctx.installDir.c_str(),
			&k_startupInfo,
			&procInfo
		);
		if (sysResult == FALSE)
			return false;

		::CloseHandle(procInfo.hThread);
		::CloseHandle(procInfo.hProcess);
		return true;
	}

	static void CleanUpFailedFiles(const InstallContext& /*ctx*/)
	{
	}
};

}  // unnamed namespace


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	InstallManager::Exec();

	return NO_ERROR;
}
