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

#include "Installer.h"

#include <Gandr/Types.hpp>

#include <array>
#include <optional>
#include <vector>


namespace lauss::setup
{

class RestartSession
{
public:
	constexpr static size_t k_processMaxCnt = 4;
	using ProcessList = std::array<std::wstring, k_processMaxCnt>;

	RestartSession();
	~RestartSession();

	bool RestartProcessesUsingFiles(const InstallContext& ctx, auto&& func)
	{
		assert(operator bool());
		if (!operator bool())
			return false;

		const auto procList = GetRestartableProcesses(ctx);
		assert(procList);
		if (!procList)
			return false;

		if (!func(procList.value()))
			return false;

		return RestartProcesses();
	}

	operator bool() const
	{
		return m_valid;
	}

private:
	std::optional<ProcessList> GetRestartableProcesses(const InstallContext& ctx);
	bool RestartProcesses();

	constexpr static size_t k_sessionKeyMaxLen = 33;
	using SessionKey = std::array<wchar_t, k_sessionKeyMaxLen>;

	bool m_valid{ };
	gan::WinDword m_handle{ };
	SessionKey m_sessionKey{ };
};

}  // namespace lauss::setup
