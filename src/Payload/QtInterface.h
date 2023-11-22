/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2020-2023 Mifan Bang <https://debug.tw>.
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


template <class T>
struct QArray
{
	int32_t ref;
	int32_t size;
	uint32_t alloc : 31;
	uint32_t capacityReserved : 1;
	ptrdiff_t offset;

	T* Data() { return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + offset); }
};
using QStringData = QArray<wchar_t>;

struct QString
{
	QStringData* data;
};


struct QObject
{
	QString objectName() const;

	// Dynamically resolved function pointer
	static decltype(&QObject::objectName) ObjectName;
};


struct QWidget : public QObject
{
	QWidget* parentWidget();
	void resize(int w, int h);
	void show();

	// Dynamically resolved function pointers
	static decltype(&QWidget::parentWidget) ParentWidget;
	static decltype(&QWidget::resize) Resize;
};
