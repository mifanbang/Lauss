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

#include "InstallerRegistry.h"

#include <LaussDef.h>
#include <Registry.h>
#include <Utils.h>

#include <array>
#include <string>
#include <string_view>


using namespace std::literals;


bool StartUpRegistry::Create(const InstallContext& ctx)
{
	RegistryKey regKey{ k_keyPath };
	assert(regKey);
	if (!regKey)
		return false;

	const auto quotedPath = AddDoubleQuotes(ctx.pathLauncher);
	return regKey.SetValue<RegistryKey::Type::String>(
		k_subkeyLauss,
		quotedPath.c_str(),
		(quotedPath.size() + 1) << 1  // Per API doc, terminating '\0' must be included.
	);
}

bool StartUpRegistry::Remove()
{
	RegistryKey regKey{ k_keyPath };
	assert(regKey);
	if (!regKey)
		return false;

	return regKey.RemoveValue(k_subkeyLauss);
}

bool UninstallRegistry::Create(const InstallContext& ctx)
{
	RegistryKey regKey{ k_keyPath };
	assert(regKey);
	if (!regKey)
		return false;

	const auto unintallCmd =
		AddDoubleQuotes(ctx.pathUninstaller)
		.append(1, L' ')
		.append(CmdLineOptUninstall());

	using RegKeyValue = std::pair<std::wstring_view, std::wstring_view>;
	const std::array<RegKeyValue, 6> keyValues{ {
		{ L"DisplayName"sv, ProductName() },
		{ L"Publisher"sv, L"Mifan Bang" },
		{ L"URLInfoAbout"sv, L"https://debug.tw"sv },
		{ L"UninstallString"sv, L"cmd.exe"sv },
		{ L"DisplayVersion"sv, VersionStr() },
		{ L"UninstallString"sv, unintallCmd }
	} };

	size_t succeeded = 0;
	for (const auto [key, value] : keyValues)
		// Per API doc, terminating '\0' must be included.
		succeeded += regKey.SetValue<RegistryKey::Type::String>(key, value.data(), (value.size() + 1) << 1);
	return succeeded == keyValues.size();
}

bool UninstallRegistry::Remove()
{
	RegistryKey regKey{ k_keyPath };
	assert(regKey);
	if (!regKey)
		return false;

	return regKey.Remove();
}
