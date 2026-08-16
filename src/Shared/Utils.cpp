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

#include <Types.h>

#include <windows.h>
#include <shlobj.h>

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

std::optional<bool> IsProcessStillAlive(gan::WinHandle proc)
{
	gan::WinDword exitCode;
	const auto getExitCodeResult = ::GetExitCodeProcess(proc, &exitCode);

	return getExitCodeResult ?
		std::make_optional(exitCode == STILL_ACTIVE)
		: std::nullopt;
}
