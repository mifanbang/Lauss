/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2023 Mifan Bang <https://debug.tw>.
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

#include <DynamicCall.h>

#include <cstdio>


decltype(QObject::ObjectName) QObject::ObjectName{ nullptr };
decltype(QWidget::ParentWidget) QWidget::ParentWidget{ nullptr };
decltype(QWidget::Resize) QWidget::Resize{ nullptr };

using namespace std::literals;


bool ResolveQtFunctions()
{
	QObject::ObjectName = gan::DynamicCall::Get<decltype(QObject::ObjectName)>(L"Qt5Core.dll"sv, "?objectName@QObject@@QBE?AVQString@@XZ"sv);  // QObject::objectName()
	printf("[INFO] Resolved QObject::objectName=%p\n", gan::FromMemFn(QObject::ObjectName));

	QWidget::ParentWidget = gan::DynamicCall::Get<decltype(QWidget::ParentWidget)>(L"Qt5Widgets.dll"sv, "?parentWidget@QWidget@@QBEPAV1@XZ"sv);  // QWidget::parentWidget()
	printf("[INFO] Resolved QWidget::parentWidget=%p\n", gan::FromMemFn(QWidget::ParentWidget));

	QWidget::Resize = gan::DynamicCall::Get<decltype(QWidget::Resize)>(L"Qt5Widgets.dll"sv, "?resize@QWidget@@QAEXHH@Z"sv);  // QWidget::resize(int, int)
	printf("[INFO] Resolved QWidget::resize=%p\n", gan::FromMemFn(QWidget::Resize));

	return QObject::ObjectName
		&& QWidget::ParentWidget
		&& QWidget::Resize;
}
