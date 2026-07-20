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

#include "Debug.h"
#include "QtHook.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <vector>


namespace {

// Inclusive of the passed in widget
std::optional<std::vector<QWidget*>> FindOwningWidgetsUntil(QWidget& bottom, const char* parentCls)
{
	std::vector<QWidget*> parents;
	for (QWidget* widget = &bottom; widget; widget = (widget->*QWidget::ParentWidget)())
	{
		parents.emplace_back(widget);

		const auto metaObj = widget->metaObject();
		const char* className = (metaObj->*QMetaObject::ClassName)();
		Printf(
			"  this=%p name=%S type=%s\n",
			widget,
			(widget->*QWidget::ObjectName)().data.Data(),
			className
		);

		if (::lstrcmpiA(className, parentCls) == 0)
			return parents;
	}
	return std::nullopt;
}

void HandleWidgetShowHook(QWidget& widget)
{
	if (const QString str = (widget.*QWidget::ObjectName)();
		::lstrcmpiW(str.data.Data(), L"bannerWholeImage") == 0)
	{
		Printf("[QWidget::Show()] QObject with name \"bannerWholeImage\" found.\n");

		const auto adWidgets = FindOwningWidgetsUntil(widget, "AdvertisementPanel");
		if (!adWidgets)
		{
			// If we reached here, LINE must have changed its UI design or behavior.
			Printf("[QWidget::Show()] Failed to hide ad banner due to unexpected QWidget hierarchy. The current LINE version is unsupported.\n");
			return;
		}

		for (auto* adWidget : *adWidgets)
			(adWidget->*QWidget::Resize)(0, 0);
	}
}

}  // namespace


decltype(&QWidget::show) Hook_QWidget::s_trampoline{ nullptr };


// If for any reason s_trampoline is null while this hook function gets called,
// we should just let LINE crash.
void Hook_QWidget::Show()
{
	HandleWidgetShowHook(*this);

	(this->*s_trampoline)();
}
