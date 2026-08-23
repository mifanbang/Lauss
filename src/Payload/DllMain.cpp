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
#include <Utils.hpp>

#include <Gandr/Hook.hpp>

#include <windows.h>

#include <thread>


using namespace lauss;
using namespace std::literals;


namespace
{

enum class PayloadResult : DWORD
{
	Success,
	CannotResolveQtDeps,
	CannotResolveQtTarget,
	CannotHook,
	CannotGetTrampoline,
};

PayloadResult InitializePayload()
{
	// 1. Manually resolve symbols imported from Qt DLLs
	if (!ResolveQtFunctions())
	{
		Printf("[ERROR] Failed to resolve Qt dependencies.\n");
		return PayloadResult::CannotResolveQtDeps;
	}
	auto qWidgetShow = QP(QWidget::show);
	Printf("[INFO] Resolved QWidget::show=%p\n", qWidgetShow);

	// 2. Hook target function
	{
		gan::Hook hook{ qWidgetShow, gan::ToMemFn<decltype(&QWidget::show)>(gan::FromMemFn(&HookedQWidget::Show))};
		const auto hookResult = hook.Install();
		if (hookResult != gan::Hook::OpResult::Hooked)
		{
			Printf("[ERROR] Failed to hook QWidget::show. Code=%u\n", static_cast<uint32_t>(hookResult));
			return PayloadResult::CannotHook;
		}
		Printf("[INFO] Successfully hooked QWidget::show.\n");
	}

	// 3. Obtain and store address of trampoline
	{
		HookedQWidget::s_trampoline = gan::Hook::GetTrampoline(qWidgetShow);
		if (!HookedQWidget::s_trampoline)
		{
			Printf("[ERROR] Failed to get trampoline.\n");
			return PayloadResult::CannotGetTrampoline;
		}
		Printf("[INFO] Trampoline generated at %p\n", HookedQWidget::s_trampoline);
	}

	return PayloadResult::Success;
}

[[maybe_unused]] void WidgetSpyThread()
{
	constexpr const uint32_t k_sleepDurationMs = 20;  // 50 fps
	constexpr const auto k_triggerKey = VK_CONTROL;

	for (auto keyPressed = false;
		;  // Run infinitely
		::Sleep(k_sleepDurationMs))
	{
		const uint16_t keyState = ::GetKeyState(k_triggerKey);
		const auto currKeyPressed = (keyState >> 8 != 0);
		if (keyPressed == currKeyPressed)
			continue;
		keyPressed = currKeyPressed;
		if (currKeyPressed)
			continue;

		::POINT cursorPos{ };
		if (!::GetCursorPos(&cursorPos))
			continue;

		if (QWidget* widgetUnderCursor = Q(QApplication::widgetAt)(cursorPos.x, cursorPos.y))
		{
			for (auto* widget : FindOwningWidgets(*widgetUnderCursor))
			{
				const auto* className = GetQtClassName(*widget);
				const auto* parent = Q(QWidget::parentWidget)(widget);
				Printf(
					"[WidgetSpyThread] widget=%p objName=%S cls=%s parentCls=%s\n",
					widget,
					Q(QObject::objectName)(widget).data.Data(),
					className,
					parent ? GetQtClassName(*parent) : "(n/a)"
				);

				PrintMethodsWithSignalConnections(*widget);
			}
		}
	}
}

bool OnProcessAttached()
{
	if constexpr (UseDebugConsole())
	{
		::AllocConsole();
		Printf("Lauss payload has started.\n");

		// Hack: We probably shouldn't allocate console from DllMain's thread, but waiting for
		//       a short while after console allocation seems to fix observable issues.
		constexpr const uint32_t k_consoleWaitDurationMs = 1000;
		::Sleep(k_consoleWaitDurationMs);
	}

	if (InitializePayload() != PayloadResult::Success)
		return false;

	// Hide existing banners
	for (auto* adWidget : FindBanners())
		HideBanner(*adWidget);

	if constexpr (UseDebugConsole())
	{
		std::thread thWidgetSpy{ WidgetSpyThread };
		thWidgetSpy.detach();
	}

	return true;
}

void OnProcessDetached()
{
	if constexpr (UseDebugConsole())
	{
		::FreeConsole();
	}
}

}  // namespace


BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
	if (::GetModuleHandleW(LineImageName()) == nullptr)
		return TRUE;

	if (reason == DLL_PROCESS_ATTACH)
	{
		// Hack: Force Windows to increment the ref count of payload DLL and avoid getting unloaded
		//       after laucher uninstalls hook and exits process.
		auto hMod = ::LoadLibraryW(PayloadName());

		const auto payloadResult = OnProcessAttached();

		// Hack: Failed to start payload. Can safely roll back ref count and unload normally.
		if (!payloadResult && hMod)
			::FreeLibrary(hMod);

		return static_cast<BOOL>(payloadResult);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		OnProcessDetached();
	}
	return TRUE;
}


extern "C" __declspec(dllexport)
LRESULT CALLBACK EXPORTED_HOOK_FUNC_NAME(int code, WPARAM wParam, LPARAM lParam)
{
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}
