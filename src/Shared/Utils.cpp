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
#include <string>


std::wstring GetExeDirectory()
{
	std::wstring cwd;
	if (int argc;
		wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc))
	{
		assert(argc >= 1);
		if (argc >= 1)
			cwd = std::filesystem::path{ argv[0] }.parent_path();
		::LocalFree(argv);
	}
	return cwd;
}

std::optional<bool> IsProcessStillAlive(gan::WinHandle proc)
{
	gan::WinDword exitCode;
	const auto getExitCodeResult = ::GetExitCodeProcess(proc, &exitCode);

	return getExitCodeResult ?
		std::make_optional(exitCode == STILL_ACTIVE)
		: std::nullopt;
}
