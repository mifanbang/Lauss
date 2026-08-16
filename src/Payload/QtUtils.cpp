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

#include "QtUtils.h"

#include "LaussDef.h"
#include "QtHook.h"
#include "QtInterface.h"
#include "Utils.h"

#include <windows.h>

#include <cstdio>
#include <optional>
#include <ranges>
#include <string>
#include <vector>
#include <utility>


const char* GetQtClassName(const QObject& object)
{
	const auto metaObj = object.metaObject();
	return Q(QMetaObject::className)(metaObj);
}

std::vector<IndexedMethod> GetMethods(const QObject& object)
{
	const QMetaObject* metaObj = (object.metaObject)();

	const auto idxRange = std::ranges::iota_view{
		0,  // Or Q(QMetaObject::methodOffset)(metaObj) for the leaf class only
		Q(QMetaObject::methodCount)(metaObj)
	};
	auto methodList = idxRange
		| std::views::transform([metaObj](auto idx) { return std::make_pair(idx, Q(QMetaObject::method)(metaObj, idx)); })
		| std::ranges::to<std::vector>();
	return methodList;
}

std::optional<std::vector<Connection*>> GetConnections(QObject& object, const char* signal)
{
	if (!object.d_ptr->connections
		|| !object.d_ptr->connections->signalVector)
	{
		return std::nullopt;
	}

	const auto signalIndex = Q(QObjectPrivate::signalIndex)(object.d_ptr, signal, nullptr);
	if (signalIndex < 0
		|| signalIndex >= object.d_ptr->connections->signalVector->allocated)
	{
		return std::nullopt;
	}

	std::vector<Connection*> connections;
	{
		const auto connList =
			object
			.d_ptr
			->connections
			->signalVector
			->GetConnectionList(signalIndex);
		for (Connection* conn = connList.front;
			conn;
			conn = conn->nextConnectionList)
		{
			connections.emplace_back(conn);
		}
	}
	return connections;
}

bool PrintConnectionsToSignal(QObject& object, const char* signal)
{
	const auto connections = GetConnections(object, signal);
	if (!connections)
		return false;

	for (const Connection* conn : connections.value())
	{
		const auto* receiverObj = conn->receiver;
		if (!receiverObj)
			continue;

		const auto* receiverClass = GetQtClassName(*receiverObj);
		if (conn->isSlotObject)
		{
			// conn->isSlotObject indicates that conn->slotObj is valid.
			// conn->slotObj->impl points to a function of the signature:
			//     using ImplFunc = void (*)(
			//	       int32_t which,
			//         QSlotObjectBase* this_,
			//         const QObject* r,
			//         void** a,
			//         bool* ret
			//     );
			//     const auto impl = reinterpret_cast<ImplFunc>(conn->slotObj->impl);
			if (conn->slotObj == nullptr)
				continue;
			Printf("    --> receiver=%p cls=%s  slotImpl=%p\n", receiverObj, receiverClass, conn->slotObj->impl);
		}
		else
		{
			const auto* metaObj = receiverObj->metaObject();
			const auto methodIndex = static_cast<int32_t>(conn->methodOffset + conn->methodRelative);
			const auto slotMethod = Q(QMetaObject::method)(metaObj, methodIndex);
			const char* methodSig =
				(methodIndex >= 0 && methodIndex < Q(QMetaObject::methodCount)(metaObj))
				? Q(QMetaMethod::methodSignature)(slotMethod).data.ptr
				: "invalid";

			Printf(
				"    --> receiver=%p cls=%s  methodIndex=%d(%d(off)+%d(rel)) method=%s\n",
				receiverObj,
				receiverClass,
				methodIndex,
				static_cast<int32_t>(conn->methodOffset),
				static_cast<int32_t>(conn->methodRelative),
				methodSig
			);
		}
	}

	return true;
}

void PrintMethodsWithSignalConnections(QObject& object)
{
	for (const auto& [idx, metaMethod] : GetMethods(object))
	{
		const auto signature = Q(QMetaMethod::methodSignature)(metaMethod);
		const auto methodType = Q(QMetaMethod::methodType)(metaMethod);
		Printf(
			"  method idx=%d type=%d ret=%s sig=%s\n",
			idx,
			methodType,
			Q(QMetaMethod::typeName)(metaMethod),
			signature.data.ptr
		);

		if (methodType == QMetaMethod::MethodType::Signal)
			PrintConnectionsToSignal(object, signature.data.ptr);
	}
}

std::vector<QWidget*> FindOwningWidgets(QWidget& bottomWidget)
{
	std::vector<QWidget*> parents;
	for (QWidget* widget = &bottomWidget; widget; widget = Q(QWidget::parentWidget)(widget))
	{
		parents.emplace_back(widget);

		const auto* className = GetQtClassName(*widget);
		Printf(
			"[FindOwningWidgets()] this=%p objName=%S class=%s\n",
			widget,
			Q(QObject::objectName)(widget).data.Data(),
			className
		);
	}
	return parents;
}

std::vector<QWidget*> FindBanners()
{
	constexpr const size_t k_expectedMaxBanner = 4;
	std::vector<QWidget*> adWidgets;
	adWidgets.reserve(k_expectedMaxBanner);

	const auto topWidgets = Q(QApplication::topLevelWidgets)();
	const std::span<QWidget*> widgetSpan{ topWidgets.data.ptr, topWidgets.data.size };
	Printf("[FindBanners()] QApplication::TopLevelWidgets() returned %zu widgets.\n", widgetSpan.size());

	static std::wstring s_bannerObjName = BannerObjectName();
	static const QString adWidgetNameStr{
		.data{
			.d = nullptr,  // Raw string like QString::fromRawData(), no heap alloc or control block
			.ptr = s_bannerObjName.data(),
			.size = s_bannerObjName.size()
		}
	};

	const QMetaObject* metaObjQWidget = Q(QWidget::staticMetaObject)();
	for (auto* widget : widgetSpan)
	{
		if (::lstrcmpA(GetQtClassName(*widget), MainWindowClassName()) != 0)
			continue;

		QList<QWidget*> list{};
		Q(qt_qFindChildren_helper)(widget, adWidgetNameStr, *metaObjQWidget, &list, k_FindChildrenRecursively);
		for (size_t i = 0; i < list.data.size; ++i)
		{
			if (auto* w = list.data.ptr[i])
				adWidgets.emplace_back(w);
		}
	}

	return adWidgets;
}

bool HideBanner(QWidget& bannerWidget)
{
	const auto owningWidgets = FindOwningWidgets(bannerWidget);
	const auto rootAdWidget =
		std::views::enumerate(owningWidgets)
		| std::views::filter([](const auto& kv) { return ::lstrcmpiA(GetQtClassName(*std::get<1>(kv)), BannerRootClassName()) == 0; })
		| std::views::elements<0>
		| std::views::take(1)
		| std::ranges::to<std::vector>();

	if (rootAdWidget.size() == 0)
	{
		// If we reached here, LINE must have changed its UI design or behavior.
		Printf("[HideBanner()] Failed to hide ad banner due to unexpected QWidget hierarchy. The current LINE version might be unsupported.\n");
		return false;
	}
	const size_t numAdWidgets = rootAdWidget.front() + 1;
	Printf("[HideBanner()] Hiding %zu widgets\n", numAdWidgets);

	for (auto* adWidget : owningWidgets | std::views::take(numAdWidgets))
	{
		Q(QWidget::resize)(adWidget, 0, 1);
		Q(QWidget::hide)(adWidget);
	}

	return true;
}
