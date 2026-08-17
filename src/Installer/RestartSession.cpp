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

#include "RestartSession.h"

#include <windows.h>
#include <restartmanager.h>

#include <algorithm>
#include <ranges>


namespace
{

bool RegisterResources(gan::WinDword handle, const InstallContext& ctx)
{
	constexpr unsigned int k_zeroUniqueProcess = 0;
	constexpr RM_UNIQUE_PROCESS* k_noUniqueProcess = nullptr;
	constexpr unsigned int k_zeroService = 0;
	constexpr const wchar_t** k_noServiceName = nullptr;
	std::array<const wchar_t*, 3> fileList{ {
		ctx.pathLauncher.c_str(),
		ctx.pathPayload.c_str(),
		ctx.pathUninstaller.c_str()
	} };
	const auto registerResult = ::RmRegisterResources(
		handle,
		static_cast<unsigned int>(fileList.size()),
		fileList.data(),
		k_zeroUniqueProcess,
		k_noUniqueProcess,
		k_zeroService,
		k_noServiceName
	);
	return (registerResult == NO_ERROR);
}

std::optional<RestartSession::ProcessList> GetProcessList(gan::WinDword handle)
{
	constexpr auto k_procInfoCount = static_cast<unsigned int>(std::tuple_size_v<RestartSession::ProcessList>);

	std::array<RM_PROCESS_INFO, k_procInfoCount> procInfoList{ };
	unsigned int rmProcInfoNeeded{ };
	unsigned int rmProcInfoCount = k_procInfoCount;
	gan::WinDword rebootReason{ };
	const auto getListResult = ::RmGetList(handle, &rmProcInfoNeeded, &rmProcInfoCount, procInfoList.data(), &rebootReason);

	if (getListResult == NO_ERROR || getListResult == ERROR_MORE_DATA)
	{
		auto range = procInfoList
			| std::views::transform([](auto& procInfo) -> std::wstring { return procInfo.strAppName; });

		RestartSession::ProcessList procList{ };
		std::ranges::copy_n(range.begin(), k_procInfoCount, procList.begin());
		return procList;
	}
	return std::nullopt;
}

}  // unnamed namespace


RestartSession::RestartSession()
{
	constexpr gan::WinDword k_noFlags = 0;
	const auto startSessionResult = ::RmStartSession(&m_handle, k_noFlags, m_sessionKey.data());
	m_valid = (startSessionResult == NO_ERROR);
}

RestartSession::~RestartSession()
{
	if (operator bool())
		::RmEndSession(m_handle);
}

std::optional<RestartSession::ProcessList> RestartSession::GetRestartableProcesses(const InstallContext& ctx)
{
	const auto regResResult = RegisterResources(m_handle, ctx);
	if (!regResResult)
		return std::nullopt;

	return GetProcessList(m_handle);
}

bool RestartSession::RestartProcesses()
{
	constexpr RM_WRITE_STATUS_CALLBACK k_noCallback = nullptr;
	[[maybe_unused]] const auto shutdownResult = ::RmShutdown(m_handle, RmForceShutdown, k_noCallback);
	assert(shutdownResult == NO_ERROR);

	constexpr gan::WinDword k_noFlag = 0;
	[[maybe_unused]] const auto restartResult = ::RmRestart(m_handle, k_noFlag, k_noCallback);
	assert(restartResult == NO_ERROR);

	return restartResult == NO_ERROR;
}
