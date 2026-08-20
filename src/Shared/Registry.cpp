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

#include "Registry.h"

#include <Gandr/Types.hpp>

#include <windows.h>

#include <cassert>
#include <string_view>



RegistryKey::RegistryKey(std::wstring_view name)
	: m_hKey(nullptr)
{
	constexpr gan::WinDword k_reserved = 0;
	constexpr wchar_t* k_noUserClass = nullptr;
	constexpr LPSECURITY_ATTRIBUTES k_noSecAttr = nullptr;
	constexpr gan::WinDword* k_noDispotionInfo = nullptr;
	::RegCreateKeyExW(
		HKEY_CURRENT_USER,
		name.data(),
		k_reserved,
		k_noUserClass,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE,
		k_noSecAttr,
		&m_hKey,
		k_noDispotionInfo
	);
}

RegistryKey::~RegistryKey()
{
	if (operator bool())
		::RegCloseKey(m_hKey);
}

bool RegistryKey::SetValueUnsafe(std::wstring_view name, Type type, const void* value, size_t size)
{
	constexpr wchar_t* k_noSubKey = nullptr;
	const auto setKeyResult = ::RegSetKeyValueW(
		m_hKey,
		k_noSubKey,
		name.data(),
		std::to_underlying<Type>(type),
		value,
		static_cast<gan::WinDword>(size)
	);
	return setKeyResult == NO_ERROR;
}

bool RegistryKey::RemoveValue(std::wstring_view name)
{
	const auto deleteKeyResult = ::RegDeleteValueW(m_hKey, name.data());
	return deleteKeyResult == NO_ERROR;
}

bool RegistryKey::Remove()
{
	const auto deleteKeyResult = ::RegDeleteKeyExW(m_hKey, L"", 0, 0);
	return deleteKeyResult == NO_ERROR;
}
