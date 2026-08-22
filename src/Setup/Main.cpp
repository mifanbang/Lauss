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

#include "Setup.hpp"

#include <LaussDef.hpp>
#include <Utils.hpp>

#include <windows.h>
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <cassert>


int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ wchar_t*, _In_ int)
{
	const auto args = lauss::GetCmdLineArgs();

	const auto installDir = lauss::GetInstallationDir();
	assert(installDir.size() > 0);
	if (installDir.size() == 0)
	{
		// "Critical error: Failed to obtain install path.\n";
		return -1;
	}

	const auto installCtx = lauss::setup::InstallContext::Make(installDir);
	assert(installCtx);
	if (!installCtx)
	{
		// "Critical error: Failed to generate install context.\n"
		return -1;
	}

	if (args.size() > 1
		&& ::lstrcmpiW(args[1].c_str(), lauss::CmdLineOptUninstall()) == 0)
	{
		lauss::setup::Uninstall(installCtx.value());
	}
	else
	{
		// TODO: Detect previous installation
		lauss::setup::Install(installCtx.value());
	}

	return NO_ERROR;
}
