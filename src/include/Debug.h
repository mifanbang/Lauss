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

#include <cstdio>

#include <windows.h>


consteval bool IsDebugBuild()
{
#if defined(_DEBUG)
	return true;
#else
	return false;
#endif  // _DEBUG
}


consteval bool UseDebugConsole()
{
	return IsDebugBuild();
}


template <class... Args>
void Printf([[maybe_unused]] Args... args)
{
	if constexpr (UseDebugConsole())
	{
		char buffer[1024];
		sprintf_s(buffer, sizeof(buffer), args...);

		DWORD written;
		constexpr void* k_unused = nullptr;
		::WriteConsoleA(::GetStdHandle(STD_OUTPUT_HANDLE), buffer, ::lstrlenA(buffer), &written, k_unused);
	}
}

