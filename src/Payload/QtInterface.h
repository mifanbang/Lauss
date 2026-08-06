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

#include <DllLookup.h>
#include <Types.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>


bool ResolveQtFunctions();


// ------------------------------------------------------------------------------
// Mockups of Qt types and interface used by the payload


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/tools/qarraydata.h
struct QArrayData
{
	std::atomic<int> ref;
	// ...
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/tools/qarraydatapointer.h
template <class T>
struct QArrayDataPointer
{
	QArrayData* d;  // QTypedArrayData<T> : public QArrayData
	T* ptr;
	size_t size;

	~QArrayDataPointer();

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
};
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobjectdefs.h
struct QMetaObject
{
	const char* className() const;

	QMetaMethod method(int index) const;
	int methodCount() const;
	int methodOffset() const;
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
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject.h
// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qtmetamacros.h
struct QObject
{
	QObjectPrivate* d_ptr;  // QScopedPointer<QObjectData>

	QString objectName() const;

	virtual const QMetaObject* metaObject() const;  // vtbl idx=0
};

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/corelib/kernel/qobject.h
constexpr uint32_t k_FindChildrenRecursively = 1;
extern void qt_qFindChildren_helper(
	const QObject* parent,
	const QString& name,
	const QMetaObject& mo,
	QList<struct QWidget*>* list,
	uint32_t /* Qt::FindChildOptions */ options
);

// ref: https://github.com/qt/qtbase/blob/6.6.3/src/widgets/kernel/qwidget.h
struct QWidget : public QObject
{
	QWidget* parentWidget() const;
	void resize(int w, int h);
	void hide();

	// Hook target
	void show();

	static const QMetaObject staticMetaObject;
};


// ref: https://github.com/qt/qtbase/blob/6.6.3/src/widgets/kernel/qapplication.h
struct QApplication {
	static QList<QWidget*> topLevelWidgets();
	static QWidget* widgetAt(int x, int y);
};


// ------------------------------------------------------------------------------
// Helpers to make mocked Qt interfaces easier to use through manual dynamic
// resolution, eliminating the need to have linker bind us to Qt library files.


template <size_t N>
struct _FixedString
{
	char _s[N];
	constexpr _FixedString(const char (&s)[N]) { std::copy_n(s, N, _s); }
};

class DynamicImports;

template <_FixedString s, class T>
	requires std::is_member_function_pointer_v<T> || std::is_pointer_v<T>
class _ImportedSymbol
{
public:
	T addr;

	_ImportedSymbol() = delete;
	_ImportedSymbol(const wchar_t* lib, const char* sym)
	{
		if constexpr (std::is_member_function_pointer_v<T>)
			addr = gan::ToMemFn<T>(DynamicImports::_RegisterImport(lib, sym));
		else
			addr = reinterpret_cast<T>(DynamicImports::_RegisterImport(lib, sym));
	}
	auto operator()(auto&& ...args) const
	{
		if constexpr (std::is_member_function_pointer_v<T>)
			return std::mem_fn(addr)(std::forward<decltype(args)>(args)...);
		else if constexpr (std::is_function_v<std::remove_pointer_t<T>>)
			return addr(std::forward<decltype(args)>(args)...);
		else
			return addr;
	}
};

#undef _CONCAT
#ifndef _CONCAT
	#define _CONCAT_INNER(a, b)	a##b
	#define _CONCAT(a, b)	_CONCAT_INNER(a, b)
#endif
#define _EXPAND_TYPE(sym)	<#sym, decltype(&sym)>
#define _SYMBOL(sym)		_CONCAT(_ImportedSymbol, _EXPAND_TYPE(sym))

#define DECL_SYM(sym)		public _SYMBOL(sym)
#define QT6CORE		L"Qt6Core.dll"
#define QT6WIDGETS	L"Qt6Widgets.dll"
#define UCRTBASE	L"ucrtbase.dll"

class DynamicImports
	: public gan::Singleton<DynamicImports>
	// QMetaMethod
	, DECL_SYM(QMetaMethod::methodSignature)
	, DECL_SYM(QMetaMethod::methodType)
	, DECL_SYM(QMetaMethod::typeName)
	// QMetaObject
	, DECL_SYM(QMetaObject::className)
	, DECL_SYM(QMetaObject::method)
	, DECL_SYM(QMetaObject::methodCount)
	, DECL_SYM(QMetaObject::methodOffset)
	// QObjectPrivate
	, DECL_SYM(QObjectPrivate::signalIndex)
	// QObject
	, DECL_SYM(QObject::objectName)
	// QWidget
	, DECL_SYM(QWidget::hide)
	, DECL_SYM(QWidget::parentWidget)
	, DECL_SYM(QWidget::resize)
	, DECL_SYM(QWidget::show)
	, DECL_SYM(QWidget::staticMetaObject)
	// QApplication
	, DECL_SYM(QApplication::topLevelWidgets)
	, DECL_SYM(QApplication::widgetAt)
	// Qt internals
	, DECL_SYM(qt_qFindChildren_helper)
	// C runtime
	, DECL_SYM(::free)
{
public:
	DynamicImports()
		// QMetaMethod
		: _SYMBOL(QMetaMethod::methodSignature)	{ QT6CORE, "?methodSignature@QMetaMethod@@QEBA?AVQByteArray@@XZ" }  // QByteArray QMetaMethod::methodSignature() const
		, _SYMBOL(QMetaMethod::methodType)		{ QT6CORE, "?methodType@QMetaMethod@@QEBA?AW4MethodType@1@XZ" }  // QMetaMethod::MethodType QMetaMethod::methodType() const
		, _SYMBOL(QMetaMethod::typeName)		{ QT6CORE, "?typeName@QMetaMethod@@QEBAPEBDXZ" }  // char const* QMetaMethod::typeName() const
		// QMetaObject
		, _SYMBOL(QMetaObject::className)		{ QT6CORE, "?className@QMetaObject@@QEBAPEBDXZ" }  // const char* QMetaObject::className() const
		, _SYMBOL(QMetaObject::method)			{ QT6CORE, "?method@QMetaObject@@QEBA?AVQMetaMethod@@H@Z" }  // QMetaMethod QMetaObject::method(int) const
		, _SYMBOL(QMetaObject::methodCount)		{ QT6CORE, "?methodCount@QMetaObject@@QEBAHXZ" }  // int QMetaObject::methodCount() const
		, _SYMBOL(QMetaObject::methodOffset)	{ QT6CORE, "?methodOffset@QMetaObject@@QEBAHXZ" }  // int QMetaObject::methodOffset() const
		// QObjectPrivate
		, _SYMBOL(QObjectPrivate::signalIndex)	{ QT6CORE, "?signalIndex@QObjectPrivate@@QEBAHPEBDPEAPEBUQMetaObject@@@Z" }  // int QObjectPrivate::signalIndex(const char*, const QMetaObject**) const
		// QObject
		, _SYMBOL(QObject::objectName)			{ QT6CORE, "?objectName@QObject@@QEBA?AVQString@@XZ" }  // QString QObject::objectName() const
		// QWidget
		, _SYMBOL(QWidget::hide)				{ QT6WIDGETS, "?hide@QWidget@@QEAAXXZ" }  // void QWidget::hide()
		, _SYMBOL(QWidget::parentWidget)		{ QT6WIDGETS, "?parentWidget@QWidget@@QEBAPEAV1@XZ" }  // QWidget* QWidget::parentWidget() const
		, _SYMBOL(QWidget::resize)				{ QT6WIDGETS, "?resize@QWidget@@QEAAXHH@Z" }  // void QWidget::resize(int, int)
		, _SYMBOL(QWidget::show)				{ QT6WIDGETS, "?show@QWidget@@QEAAXXZ" }  // void QWidget::show()
		, _SYMBOL(QWidget::staticMetaObject)	{ QT6WIDGETS, "?staticMetaObject@QWidget@@2UQMetaObject@@B" }  // static QMetaObject const QWidget::staticMetaObject
		// QApplication
		, _SYMBOL(QApplication::topLevelWidgets){ QT6WIDGETS, "?topLevelWidgets@QApplication@@SA?AV?$QList@PEAVQWidget@@@@XZ" }  // static QList<QWidget*> QApplication::topLevelWidgets()
		, _SYMBOL(QApplication::widgetAt)		{ QT6WIDGETS, "?widgetAt@QApplication@@SAPEAVQWidget@@HH@Z" }  // static QWidget* QApplication::widgetAt(int, int)
		// Qt internals
		, _SYMBOL(qt_qFindChildren_helper)		{ QT6CORE, "?qt_qFindChildren_helper@@YAXPEBVQObject@@AEBVQString@@AEBUQMetaObject@@PEAV?$QList@PEAX@@V?$QFlags@W4FindChildOption@Qt@@@@@Z" }  // void qt_qFindChildren_helper(const QObject*, const QString&, const QMetaObject&, QList<void*>*, QFlags<Qt::FindChildOption>)
		, _SYMBOL(::free)						{ UCRTBASE, "free" }
	{ };

	static void* _RegisterImport(const wchar_t* lib, const char* sym)
	{
		auto* ptr = gan::DllLookup::Get<void*>(lib, sym);
		if (ptr == nullptr)
			DynamicImports::s_unresolved.emplace_back(sym);
		return ptr;
	}

	static const std::vector<std::string>& GetUnresolvedSymbols()
	{
		return s_unresolved;
	}

private:
	// Must be static or otherwise _ImportedSymbol constructions would precede s_unresolved's
	static std::vector<std::string> s_unresolved;
};

#undef DECL_SYM
#undef QT6CORE
#undef QT6WIDGETS

#define _GET_SYM(sym)	(static_cast<_SYMBOL(sym)>(DynamicImports::GetInstance()))
#define QP(sym)			(_GET_SYM(sym).addr)
#define Q(sym)			(_GET_SYM(sym).operator())


// Destructor depends on macro Q()
template <class T>
QArrayDataPointer<T>::~QArrayDataPointer()
{
	if (d && d->ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
		Q(::free)(d);
}
