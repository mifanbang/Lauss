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
#include "LaussDef.h"
#include "QtHook.h"
#include "QtInterface.h"
#include "QtUtils.h"

#include <DllLookup.h>
#include <Hook.h>

#include <windows.h>

#include <thread>


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
	using QWidget_Show = decltype(&QWidget::show);

	// 1. Resolve required Qt functions
	if (!ResolveQtFunctions())
	{
		Printf("[ERROR] Failed to resolve Qt dependencies.\n");
		return PayloadResult::CannotResolveQtDeps;
	}

	// 2. Resolve target function, i.e., QWidget::show
	auto targetFunc = gan::DllLookup::Get<QWidget_Show>(
		L"Qt6Widgets.dll"sv,
		"?show@QWidget@@QEAAXXZ"sv  // void QWidget::show() __ptr64
	);
	{
		if (!targetFunc)
		{
			Printf("[ERROR] Failed to resolve target function.\n");
			return PayloadResult::CannotResolveQtTarget;
		}
		Printf("[INFO] Resolved QWidget::show=%p\n", targetFunc);
	}

	// 3. Hook target function
	{
		gan::Hook hook{ targetFunc, gan::ToMemFn<QWidget_Show>(gan::FromMemFn(&HookedQWidget::Show)) };
		const auto hookResult = hook.Install();
		if (hookResult != gan::Hook::OpResult::Hooked)
		{
			Printf("[ERROR] Failed to hook QWidget::show. Code=%u\n", static_cast<uint32_t>(hookResult));
			return PayloadResult::CannotHook;
		}
		Printf("[INFO] QWidget::show hooked.\n");
	}

	// 4. Obtain and store address of trampoline to global states
	{
		HookedQWidget::s_trampoline = gan::Hook::GetTrampoline(targetFunc);
		if (!HookedQWidget::s_trampoline)
		{
			Printf("[ERROR] Failed to get trampoline.\n");
			return PayloadResult::CannotGetTrampoline;
		}
		Printf("[INFO] Trampoline generated at %p\n", HookedQWidget::s_trampoline);
	}

	return PayloadResult::Success;
}


bool OnProcessAttached()
{
	// Increment refcount to keep DLL alive after hooking process terminates
	static auto hMod = ::LoadLibraryW(PayloadName());

	if constexpr (UseDebugConsole())
	{
		FILE* fp;
		::AllocConsole();
		::freopen_s(&fp, "CONIN$", "r+t", stdin);
		::freopen_s(&fp, "CONOUT$", "w+t", stdout);
		::freopen_s(&fp, "CONOUT$", "w+t", stderr);
	}

	if (InitializePayload() == PayloadResult::Success)
	{
		// Hide existing banners
		for (auto* adWidget : FindActiveBanners())
			ResizeAdWidget(*adWidget);
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
	if (reason == DLL_PROCESS_ATTACH)
	{
		return static_cast<BOOL>(OnProcessAttached());
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		OnProcessDetached();
	}
    return TRUE;
}


extern "C" __declspec(dllexport)
LRESULT CALLBACK Dummy(int code, WPARAM wParam, LPARAM lParam)
{
	return ::CallNextHookEx(nullptr, code, wParam, lParam);
}
