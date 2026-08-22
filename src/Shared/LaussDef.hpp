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


namespace lauss
{

#define PRODUCT_NAME	L"Lauss"

consteval auto ProductName()
{
	return PRODUCT_NAME;
}
consteval auto VersionStr()
{
	return L"0.0";
}
consteval auto CmdLineOptUninstall()
{
	return L"--uninstall";
}

consteval auto UninstallerName()
{
	return L"Uninstaller.exe";
}
consteval auto LauncherName()
{
	return L"Lauss.exe";
}
consteval auto PayloadName()
{
	return L"Payload.dll";
}
consteval auto LineImageName()
{
	return L"LINE.exe";
}

#define EXPORTED_HOOK_FUNC_NAME	Dummy

consteval auto ExportedHookFuncName()
{
#define _STRINGIFY_INNER(x)	#x
#define _STRINGIFY(x)		_STRINGIFY_INNER(x)
	return _STRINGIFY(EXPORTED_HOOK_FUNC_NAME);
#undef _STRINGIFY
#undef _STRINGIFY_INNER
}

consteval auto BannerObjectName()
{
	return L"bannerWholeImage";
}
consteval auto BannerRootClassName()
{
	return "AdvertisementPanel";
}
consteval auto MainWindowClassName()
{
	return "AllInOneWindow";
}

}  // namespace lauss
