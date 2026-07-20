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

#include "Debug.h"

#include <DynamicCall.h>

#include <cstdio>


decltype(QMetaObject::ClassName) QMetaObject::ClassName{ nullptr };
decltype(QObject::ObjectName) QObject::ObjectName{ nullptr };
decltype(QWidget::ParentWidget) QWidget::ParentWidget{ nullptr };
decltype(QWidget::Resize) QWidget::Resize{ nullptr };

using namespace std::literals;


bool ResolveQtFunctions()
{
	// const char* QMetaObject::className() const __ptr64
	QMetaObject::ClassName = gan::DynamicCall::Get<decltype(QMetaObject::ClassName)>(
		L"Qt6Core.dll"sv,
		"?className@QMetaObject@@QEBAPEBDXZ"sv
	);
	Printf("[INFO] Resolved QMetaObject::className=%p\n", gan::FromMemFn(QMetaObject::ClassName));


	// class QString QObject::objectName() const __ptr64
	QObject::ObjectName = gan::DynamicCall::Get<decltype(QObject::ObjectName)>(
		L"Qt6Core.dll"sv,
		"?objectName@QObject@@QEBA?AVQString@@XZ"sv
	);
	Printf("[INFO] Resolved QObject::objectName=%p\n", gan::FromMemFn(QObject::ObjectName));


	// class QWidget* __ptr64 QWidget::parentWidget() const __ptr64
	QWidget::ParentWidget = gan::DynamicCall::Get<decltype(QWidget::ParentWidget)>(
		L"Qt6Widgets.dll"sv,
		"?parentWidget@QWidget@@QEBAPEAV1@XZ"sv
	);
	Printf("[INFO] Resolved QWidget::parentWidget=%p\n", gan::FromMemFn(QWidget::ParentWidget));

	// void QWidget::resize(int,int) __ptr64
	QWidget::Resize = gan::DynamicCall::Get<decltype(QWidget::Resize)>(
		L"Qt6Widgets.dll"sv,
		"?resize@QWidget@@QEAAXHH@Z"sv
	);
	Printf("[INFO] Resolved QWidget::resize=%p\n", gan::FromMemFn(QWidget::Resize));


	return QMetaObject::ClassName
		&& QObject::ObjectName
		&& QWidget::ParentWidget
		&& QWidget::Resize;
}
