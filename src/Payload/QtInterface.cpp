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

#include <DllLookup.h>


decltype(QMetaMethod::MethodSignature) QMetaMethod::MethodSignature{ nullptr };
decltype(QMetaMethod::MethodType) QMetaMethod::MethodType{ nullptr };
decltype(QMetaMethod::TypeName) QMetaMethod::TypeName{ nullptr };

decltype(QMetaObject::ClassName) QMetaObject::ClassName{ nullptr };
decltype(QMetaObject::Method) QMetaObject::Method{ nullptr };
decltype(QMetaObject::MethodCount) QMetaObject::MethodCount{ nullptr };
decltype(QMetaObject::MethodOffset) QMetaObject::MethodOffset{ nullptr };

decltype(QObjectPrivate::SignalIndex) QObjectPrivate::SignalIndex{ nullptr };
decltype(QObject::ObjectName) QObject::ObjectName{ nullptr };

decltype(QWidget::ParentWidget) QWidget::ParentWidget{ nullptr };
decltype(QWidget::Resize) QWidget::Resize{ nullptr };
decltype(QWidget::Hide) QWidget::Hide{ nullptr };

decltype(QApplication::TopLevelWidgets) QApplication::TopLevelWidgets{ nullptr };
decltype(QApplication::WidgetAt) QApplication::WidgetAt{ nullptr };

void (*qt_qFindChildren_helper)(const QObject*, const QString&, const QMetaObject&, QList<struct QWidget*>*, uint32_t) { nullptr };

const QMetaObject* QWidget::staticMetaObjectPtr{ nullptr };


using namespace std::literals;


bool ResolveQtFunctions()
{
	// QMetaMethod
	{
		// QByteArray QMetaMethod::methodSignature() const
		QMetaMethod::MethodSignature = gan::DllLookup::Get<decltype(QMetaMethod::MethodSignature)>(
			L"Qt6Core.dll"sv,
			"?methodSignature@QMetaMethod@@QEBA?AVQByteArray@@XZ"sv
		);
		Printf("[INFO] Resolved QMetaMethod::methodSignature=%p\n", gan::FromMemFn(QMetaMethod::MethodSignature));

		// QMetaMethod::MethodType QMetaMethod::methodType() const
		QMetaMethod::MethodType = gan::DllLookup::Get<decltype(QMetaMethod::MethodType)>(
			L"Qt6Core.dll"sv,
			"?methodType@QMetaMethod@@QEBA?AW4MethodType@1@XZ"sv
		);
		Printf("[INFO] Resolved QMetaMethod::methodType=%p\n", gan::FromMemFn(QMetaMethod::MethodType));

		// char const* QMetaMethod::typeName() const
		QMetaMethod::TypeName = gan::DllLookup::Get<decltype(QMetaMethod::TypeName)>(
			L"Qt6Core.dll"sv,
			"?typeName@QMetaMethod@@QEBAPEBDXZ"sv
		);
		Printf("[INFO] Resolved QMetaMethod::typeName=%p\n", gan::FromMemFn(QMetaMethod::TypeName));
	}

	// QMetaObject
	{
		// const char* QMetaObject::className() const
		QMetaObject::ClassName = gan::DllLookup::Get<decltype(QMetaObject::ClassName)>(
			L"Qt6Core.dll"sv,
			"?className@QMetaObject@@QEBAPEBDXZ"sv
		);
		Printf("[INFO] Resolved QMetaObject::className=%p\n", gan::FromMemFn(QMetaObject::ClassName));

		// QMetaMethod QMetaObject::method(int) const
		QMetaObject::Method = gan::DllLookup::Get<decltype(QMetaObject::Method)>(
			L"Qt6Core.dll"sv,
			"?method@QMetaObject@@QEBA?AVQMetaMethod@@H@Z"sv
		);
		Printf("[INFO] Resolved QMetaObject::method=%p\n", gan::FromMemFn(QMetaObject::Method));

		// int QMetaObject::methodCount() const
		QMetaObject::MethodCount = gan::DllLookup::Get<decltype(QMetaObject::MethodCount)>(
			L"Qt6Core.dll"sv,
			"?methodCount@QMetaObject@@QEBAHXZ"sv
		);
		Printf("[INFO] Resolved QMetaObject::methodCount=%p\n", gan::FromMemFn(QMetaObject::MethodCount));

		// int QMetaObject::methodOffset() const
		QMetaObject::MethodOffset = gan::DllLookup::Get<decltype(QMetaObject::MethodOffset)>(
			L"Qt6Core.dll"sv,
			"?methodOffset@QMetaObject@@QEBAHXZ"sv
		);
		Printf("[INFO] Resolved QMetaObject::methodOffset=%p\n", gan::FromMemFn(QMetaObject::MethodOffset));
	}

	// QObjectPrivate
	{
		// int QObjectPrivate::signalIndex(const char*, const QMetaObject**) const
		QObjectPrivate::SignalIndex = gan::DllLookup::Get<decltype(QObjectPrivate::SignalIndex)>(
			L"Qt6Core.dll"sv,
			"?signalIndex@QObjectPrivate@@QEBAHPEBDPEAPEBUQMetaObject@@@Z"sv
		);
		Printf("[INFO] Resolved QObjectPrivate::signalIndex=%p\n", gan::FromMemFn(QObjectPrivate::SignalIndex));
	}

	// QObject
	{
		// QString QObject::objectName() const
		QObject::ObjectName = gan::DllLookup::Get<decltype(QObject::ObjectName)>(
			L"Qt6Core.dll"sv,
			"?objectName@QObject@@QEBA?AVQString@@XZ"sv
		);
		Printf("[INFO] Resolved QObject::objectName=%p\n", gan::FromMemFn(QObject::ObjectName));
	}

	// QWidget
	{
		// QWidget* QWidget::parentWidget() const
		QWidget::ParentWidget = gan::DllLookup::Get<decltype(QWidget::ParentWidget)>(
			L"Qt6Widgets.dll"sv,
			"?parentWidget@QWidget@@QEBAPEAV1@XZ"sv
		);
		Printf("[INFO] Resolved QWidget::parentWidget=%p\n", gan::FromMemFn(QWidget::ParentWidget));

		// void QWidget::resize(int, int)
		QWidget::Resize = gan::DllLookup::Get<decltype(QWidget::Resize)>(
			L"Qt6Widgets.dll"sv,
			"?resize@QWidget@@QEAAXHH@Z"sv
		);
		Printf("[INFO] Resolved QWidget::resize=%p\n", gan::FromMemFn(QWidget::Resize));

		// void QWidget::hide()
		QWidget::Hide = gan::DllLookup::Get<decltype(QWidget::Hide)>(
			L"Qt6Widgets.dll"sv,
			"?hide@QWidget@@QEAAXXZ"sv
		);
		Printf("[INFO] Resolved QWidget::hide=%p\n", gan::FromMemFn(QWidget::Hide));

		// static QMetaObject const QWidget::staticMetaObject
		QWidget::staticMetaObjectPtr = gan::DllLookup::Get<decltype(QWidget::staticMetaObjectPtr)>(
			L"Qt6Widgets.dll"sv,
			"?staticMetaObject@QWidget@@2UQMetaObject@@B"sv
		);
		Printf("[INFO] Resolved QWidget::staticMetaObjectPtr=%p\n", QWidget::staticMetaObjectPtr);
	}

	// QApplication
	{
		// static QList<QWidget*> QApplication::topLevelWidgets()
		QApplication::TopLevelWidgets = gan::DllLookup::Get<decltype(QApplication::TopLevelWidgets)>(
			L"Qt6Widgets.dll"sv,
			"?topLevelWidgets@QApplication@@SA?AV?$QList@PEAVQWidget@@@@XZ"sv
		);
		Printf("[INFO] Resolved QApplication::topLevelWidgets=%p\n", QApplication::TopLevelWidgets);

		// static QWidget* QApplication::widgetAt(int, int)
		QApplication::WidgetAt = gan::DllLookup::Get<decltype(QApplication::WidgetAt)>(
			L"Qt6Widgets.dll"sv,
			"?widgetAt@QApplication@@SAPEAVQWidget@@HH@Z"sv
		);
		Printf("[INFO] Resolved QApplication::widgetAt=%p\n", QApplication::WidgetAt);
	}

	{
		// void qt_qFindChildren_helper(const QObject*, const QString&, const QMetaObject&, QList<void*>*, QFlags<Qt::FindChildOption>)
		qt_qFindChildren_helper = gan::DllLookup::Get<decltype(qt_qFindChildren_helper)>(
			L"Qt6Core.dll"sv,
			"?qt_qFindChildren_helper@@YAXPEBVQObject@@AEBVQString@@AEBUQMetaObject@@PEAV?$QList@PEAX@@V?$QFlags@W4FindChildOption@Qt@@@@@Z"sv
		);
		Printf("[INFO] Resolved qt_qFindChildren_helper=%p\n", qt_qFindChildren_helper);
	}

	return QMetaMethod::MethodSignature
		&& QMetaMethod::MethodType
		&& QMetaMethod::TypeName
		&& QMetaObject::ClassName
		&& QMetaObject::Method
		&& QMetaObject::MethodCount
		&& QMetaObject::MethodOffset
		&& QObjectPrivate::SignalIndex
		&& QObject::ObjectName
		&& QWidget::ParentWidget
		&& QWidget::Resize
		&& QWidget::Hide
		&& QWidget::staticMetaObjectPtr
		&& QApplication::TopLevelWidgets
		&& QApplication::WidgetAt
		&& qt_qFindChildren_helper;
}
