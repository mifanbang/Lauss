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

#include "QtInterface.h"

#include <Utils.hpp>

#include <Gandr/DllLookup.hpp>


using namespace lauss;


bool ResolveQtFunctions()
{
	const auto& unresolved = DynamicImports::GetUnresolvedSymbols();
	for (const auto& symbol : unresolved)
		Printf("[ResolveQtFunctions()] Failed to resolve symbol: %s\n", symbol.c_str());
	return unresolved.size() == 0;
}
