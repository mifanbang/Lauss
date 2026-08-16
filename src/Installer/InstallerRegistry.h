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


class StartUpRegistry
{
public:
	[[nodiscard]] static bool Create(const InstallContext& ctx);
	[[nodiscard]] static bool Remove();

private:
	constexpr static auto k_keyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	constexpr static auto k_subkeyLauss = L"Lauss";
};


class UninstallRegistry
{
public:
	[[nodiscard]] static bool Create(const InstallContext& ctx);
	[[nodiscard]] static bool Remove();

private:
	constexpr static auto k_keyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Lauss";
};
