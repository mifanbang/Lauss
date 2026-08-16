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

#include <Types.h>

#include <cassert>
#include <string_view>


// Forward declarations
struct HKEY__;
using HKEY = HKEY__*;


class RegistryKey
{
public:
	enum class Type : gan::WinDword
	{
		String = 1
	};

	RegistryKey(std::wstring_view name);
	~RegistryKey();

	RegistryKey(const RegistryKey&) = delete;
	RegistryKey(RegistryKey&&) = delete;
	RegistryKey& operator=(const RegistryKey&) = delete;
	RegistryKey& operator=(RegistryKey&&) = delete;

	template <Type type>
		requires (type == Type::String)
	bool SetValue(std::wstring_view name, const wchar_t* value, size_t size)
	{
		assert(operator bool());
		if (!operator bool())
			return false;
		return SetValueUnsafe(name, type, value, size);
	}

	bool RemoveValue(std::wstring_view name);
	bool Remove();

	operator bool() const
	{
		return m_hKey != nullptr;
	}

private:
	bool SetValueUnsafe(std::wstring_view name, Type type, const void* value, size_t size);

	HKEY m_hKey;
};
