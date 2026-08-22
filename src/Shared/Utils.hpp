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

#pragma once

#include "Debug.hpp"

#include <Gandr/Handle.hpp>
#include <Gandr/Types.hpp>

#include <cstdio>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace lauss
{

[[nodiscard]] std::wstring AddDoubleQuotes(std::wstring_view str);


void PrintConsole(std::string_view msg);

template <class... Args>
void Printf([[maybe_unused]] Args... args)
{
	if constexpr (UseDebugConsole())
	{
		char buffer[1024];
		sprintf_s(buffer, sizeof(buffer), args...);
		PrintConsole({ buffer });
	}
}


[[nodiscard]] std::vector<std::wstring> GetCmdLineArgs();

[[nodiscard]] std::wstring GetExePath();

// Inclusive of the trailing `\\`
[[nodiscard]] std::wstring GetExeDir();

// Installation directory: C:\Users\[username]\AppData\Local\Lauss\
// Inclusive of the trailing `\\`
[[nodiscard]] std::wstring GetInstallationDir();


[[nodiscard]] bool CreateDirRecursively(std::wstring_view path);

[[nodiscard]] std::wstring CreateTempFile();

[[nodiscard]] bool IsFileReadable(std::wstring_view path);


// Per API's doc, `cmdline` must not be const.
struct CreatedProcess
{
	gan::AutoWinHandle process;
	gan::AutoWinHandle thread;
};
[[nodiscard]] std::optional<CreatedProcess> CreateProcessWithCommand(std::wstring& cmdline, gan::WinDword flag);

[[nodiscard]] std::optional<bool> IsProcessStillAlive(gan::WinHandle proc);

// Returns std::nullopt when timed out or an error occurred
std::optional<gan::WinDword> WaitOnProcess(gan::WinHandle process, std::optional<std::chrono::milliseconds> timeout);

}  // namespace lauss
