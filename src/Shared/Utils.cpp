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

#include <Types.h>

#include <windows.h>

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

std::wstring GetExeDirectory()
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

std::optional<bool> IsProcessStillAlive(gan::WinHandle proc)
{
	gan::WinDword exitCode;
	const auto getExitCodeResult = ::GetExitCodeProcess(proc, &exitCode);

	return getExitCodeResult ?
		std::make_optional(exitCode == STILL_ACTIVE)
		: std::nullopt;
}
