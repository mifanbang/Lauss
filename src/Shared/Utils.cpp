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

#include "Utils.h"

#include "LaussDef.h"

#include <Gandr/Handle.hpp>
#include <Gandr/ProcessList.hpp>
#include <Gandr/Types.hpp>

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>


std::wstring AddDoubleQuotes(std::wstring_view str)
{
	return std::wstring{ L"\"" }.append(str).append(1, L'"');
}

void PrintConsole(std::string_view msg)
{
	DWORD written;
	constexpr void* k_unused = nullptr;
	::WriteConsoleA(
		::GetStdHandle(STD_OUTPUT_HANDLE),
		msg.data(),
		static_cast<gan::WinDword>(msg.size()),
		&written,
		k_unused
	);
}

std::vector<std::wstring> GetCmdLineArgs()
{
	static std::vector<std::wstring> resultVector;
	if (resultVector.size() > 0)
		return resultVector;

	if (int argc;
		wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc))
	{
		resultVector.reserve(argc);
		for (auto idx : std::views::iota(0, argc))
			resultVector.emplace_back(argv[idx]);
		::LocalFree(argv);
	}
	return resultVector;
}

std::wstring GetExePath()
{
	const auto args = GetCmdLineArgs();
	assert(args.size() > 0);
	if (args.empty())
		return { };

	return std::filesystem::path{ args[0] };
}

std::wstring GetExeDir()
{
	const auto args = GetCmdLineArgs();
	assert(args.size() > 0);
	if (args.empty())
		return { };

	return std::filesystem::path{ args[0] }
		.parent_path()
		.wstring()
		.append(1, L'\\');
}

std::wstring GetInstallationDir()
{
	constexpr gan::WinDword k_defaultFlag = 0;
	constexpr gan::WinHandle k_noAccessToken = nullptr;

	std::wstring installDir;
	{
		wchar_t* pPath = nullptr;
		const auto dirGetResult = ::SHGetKnownFolderPath(FOLDERID_LocalAppData, k_defaultFlag, k_noAccessToken, &pPath);
		if (dirGetResult == S_OK)
		{
			installDir =
				std::wstring{ pPath }
				.append(1, L'\\')
				.append(ProductName())
				.append(1, L'\\');
		}
		::CoTaskMemFree(pPath);
	}
	return installDir;
}

bool CreateDirRecursively(std::wstring_view path)
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

std::wstring CreateTempFile()
{
	std::array<wchar_t, MAX_PATH+1> dirPath{ };
	const auto getPathResult = ::GetTempPathW(static_cast<gan::WinDword>(dirPath.size()), dirPath.data());
	assert(getPathResult);
	if (getPathResult == 0)
		return { };

	constexpr unsigned int k_defaultUniqueNum = 0;
	std::array<wchar_t, MAX_PATH+1> filePath{ };
	const auto genFileNameResult = ::GetTempFileNameW(dirPath.data(), ProductName(), k_defaultUniqueNum, filePath.data());
	assert(genFileNameResult);
	if (genFileNameResult == 0)
		return { };

	return { filePath.data() };
}

std::optional<bool> IsProcessStillAlive(gan::WinHandle proc)
{
	gan::WinDword exitCode;
	const auto getExitCodeResult = ::GetExitCodeProcess(proc, &exitCode);

	return getExitCodeResult ?
		std::make_optional(exitCode == STILL_ACTIVE)
		: std::nullopt;
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

	constexpr gan::WinDword k_waitPeriodMs = 5'000;  // 5 sec
	::WaitForSingleObject(*hProc, k_waitPeriodMs);
}
