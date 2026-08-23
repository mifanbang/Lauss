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

#include "QtHook.hpp"

#include "QtInterface.hpp"
#include "QtUtils.hpp"

#include <Debug.hpp>
#include <LaussDef.hpp>

#include <windows.h>


namespace {

using namespace lauss;
using namespace lauss::payload;

void HandleWidgetShowHook(QWidget& widget)
{
	if (const QString str = Q(QObject::objectName)(widget);
		::lstrcmpiW(str.data.Data(), BannerObjectName()) == 0)
	{
		HideBanner(widget);
	}
}

}  // namespace


namespace lauss::payload
{

decltype(&QWidget::show) HookedQWidget::s_trampoline{ nullptr };

// If for any reason s_trampoline is null while this hook function gets called, we should just let the LINE client crash.
void HookedQWidget::Show()
{
	HandleWidgetShowHook(*this);

	(this->*s_trampoline)();
}

}  // namespace lauss::payload
