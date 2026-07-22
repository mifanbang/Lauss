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

#include <atomic>
#include <cstdint>


bool ResolveQtFunctions();


// Mockups of Qt types and interface used by the payload


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/tools/qarraydatapointer.h
template <class T>
struct QArrayDataPointer
{
	void* d;  // QTypedArrayData<T>
	T* ptr;
	size_t size;

	// TODO: destructor

	const T* Data() const { return ptr; }
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/text/qstring.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/text/qstringliteral.h
struct QString
{
	QArrayDataPointer<wchar_t> data;
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/tools/qlist.h
template <class T>
struct QList
{
	QArrayDataPointer<T> data;
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/text/qbytearray.h
struct QByteArray
{
	QArrayDataPointer<char> data;
};


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qmetaobject.h
struct QMetaObject;
struct QObject;
struct QMetaMethod
{
	// Qt's convention is using QMetaMethod as a value, so it's full size has to be mocked.
	// See also QMetaObject::method(int).
	const QMetaObject* mobj;
	const uint32_t* data;

	enum class MethodType { Method, Signal, Slot, Constructor };

	QByteArray methodSignature() const;
	MethodType methodType() const;
	const char* typeName() const;

	// Dynamically resolved function pointers
	static decltype(&methodSignature) MethodSignature;
	static decltype(&methodType) MethodType;
	static decltype(&typeName) TypeName;
};
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobjectdefs.h
struct QMetaObject
{
	const char* className() const;

	QMetaMethod method(int index) const;
	int methodCount() const;
	int methodOffset() const;

	// Dynamically resolved function pointers
	static decltype(&className) ClassName;
	static decltype(&method) Method;
	static decltype(&methodCount) MethodCount;
	static decltype(&methodOffset) MethodOffset;
};


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject_p.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject_p_p.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobjectdefs_impl.h
struct QSlotObjectBase
{
	std::atomic<int32_t> ref;
	void* impl;
};
struct QCallableObject : public QSlotObjectBase  // Memory layout compatible with QPrivateSlotObject
{
	void* func;
};
struct Connection;
struct ConnectionOrSignalVector
{
	union {
		struct {  // TaggedSignalVector
			uintptr_t c;
		} nextInOrphanList;
		Connection* next;
	};
};
struct Connection : public ConnectionOrSignalVector
{
	Connection** prev;
	Connection* nextConnectionList;  // <-- We ned this
	Connection* prevConnectionList;
	QObject* sender;
	QObject* receiver;  // <-- We need this
	void* threadData;
	union {
		void* callFunction;
		QCallableObject* slotObj;  // For functors or function pointers. Type is actually QSlotObjectBase*.
	};
	const int* argumentTypes;
	std::atomic<int32_t> ref;
	uint32_t id;
	uint16_t methodOffset;  // <-- We need this
	uint16_t methodRelative;  // <-- We need this
	int32_t signalIndex : 27;
	uint8_t connectionType : 2;
	bool isSlotObject : 1;  // Whether slotObj is valid. <-- We need this.
	bool ownArgumentTypes : 1;
	bool isSingleShot : 1;
};
struct ConnectionList
{
	Connection* front;
	Connection* back;
};
struct ConnectionData {
	uint32_t id;
	uint32_t ref;
	struct SignalVector : public ConnectionOrSignalVector {
		intptr_t allocated;
		ConnectionList& GetConnectionList(int idx)
		{
			return reinterpret_cast<ConnectionList*>(this + 1)[idx + 1];
		}
	}* signalVector;  // <-- We need this
};
struct QObjectData
{
	void* vtbl;
	QObject* q_ptr;
	QObject* parent;
	QList<QObject*> children;
	uint32_t flags;
	uint32_t postedEvents;
	struct {  // QDynamicMetaObjectData
		const void* vtbl;
	} metaObj;
	struct {  // QBindingStorage
		void* ptrs[2];
	} binding;
};
static_assert(sizeof(QObjectData) == 80);
struct QObjectPrivate : public QObjectData
{
	void* extraData;
	void* threadData;
	ConnectionData* connections;  // <-- We need this

	int signalIndex(const char* signalName, const QMetaObject** meta) const;

	// Dynamically resolved function pointer
	static decltype(&signalIndex) SignalIndex;
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qtmetamacros.h
struct QObject
{
	QObjectPrivate* d_ptr;  // QScopedPointer<QObjectData>

	QString objectName() const;

	virtual const QMetaObject* metaObject() const;  // vtbl idx=0

	// Dynamically resolved function pointer
	static decltype(&objectName) ObjectName;
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject.h
constexpr uint32_t k_FindChildrenRecursively = 1;
extern void (*qt_qFindChildren_helper)(
	const QObject* parent,
	const QString& name,
	const QMetaObject& mo,
	QList<struct QWidget*>* list,
	uint32_t /* Qt::FindChildOptions */ options
);

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/widgets/kernel/qwidget.h
struct QWidget : public QObject
{
	QWidget* parentWidget();
	void resize(int w, int h);
	void hide();

	// Hook target
	void show();

	// Dynamically resolved function pointers
	static decltype(&parentWidget) ParentWidget;
	static decltype(&resize) Resize;
	static decltype(&hide) Hide;

	// Dynamically resolved data pointer
	static const QMetaObject* staticMetaObjectPtr;
};


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/widgets/kernel/qapplication.h
struct QApplication {
	static QList<QWidget*> topLevelWidgets();
	static QWidget* widgetAt(int x, int y);

	// Dynamically resolved function pointers
	static decltype(&topLevelWidgets) TopLevelWidgets;
	static decltype(&widgetAt) WidgetAt;
};
