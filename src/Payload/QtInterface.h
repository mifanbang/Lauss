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

#include <cstdint>


bool ResolveQtFunctions();


// Mockups of Qt types and interface used by the payload


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/tools/qarraydatapointer.h
template <class T>
struct QArrayDataPointer
{
	void* d;
	T* ptr;
	size_t size;

	const T* Data() const { return ptr; }
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/text/qstring.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/text/qstringliteral.h
struct QString
{
	QArrayDataPointer<wchar_t> data;
};


struct QMetaObject
{
	const char* className() const;

	// Dynamically resolved function pointer
	static decltype(&QMetaObject::className) ClassName;
};


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qtmetamacros.h
struct QObject
{
	QString objectName() const;

	virtual const QMetaObject* metaObject() const;

	// Dynamically resolved function pointer
	static decltype(&QObject::objectName) ObjectName;
};


struct QWidget : public QObject
{
	QWidget* parentWidget();
	void resize(int w, int h);

	// Hook target
	void show();

	// Dynamically resolved function pointers
	static decltype(&QWidget::parentWidget) ParentWidget;
	static decltype(&QWidget::resize) Resize;
};
