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

#include "Debug.h"
#include "QtHook.h"

#include <DynamicCall.h>
#include <Hook.h>

#include <windows.h>

#include <utility>


using namespace std::literals;


enum class PayloadResult : DWORD
{
	Success,
	CannotResolveQtDeps,
	CannotResolveQtTarget,
	CannotHook,
	CannotGetTrampoline,
};


std::underlying_type_t<PayloadResult> WINAPI PayloadMain([[maybe_unused]] void* param)
{
	using QWidget_Show = decltype(&QWidget::show);

	// 1. Resolve required Qt functions
	if (!ResolveQtFunctions())
	{
		Printf("[ERROR] Failed to resolve Qt dependencies.\n");
		return std::to_underlying(PayloadResult::CannotResolveQtDeps);
	}

	// 2. Resolve target function, i.e., QWidget::show
	auto targetFunc = gan::DynamicCall::Get<QWidget_Show>(L"Qt5Widgets.dll"sv, "?show@QWidget@@QAEXXZ"sv);  // QWidget::show()
	{
		if (!targetFunc)
		{
			Printf("[ERROR] Failed to resolve target function.\n");
			return std::to_underlying(PayloadResult::CannotResolveQtTarget);
		}
		Printf("[INFO] Resolved QWidget::show=%p\n", targetFunc);
	}

	// 3. Hook target function
	{
		gan::Hook hook{ targetFunc, gan::ToMemFn<QWidget_Show>(gan::FromMemFn(&Hook_QWidget::Show)) };
		const auto hookResult = hook.Install();
		if (hookResult == gan::Hook::OpResult::Hooked)
		{
			Printf("[INFO] QWidget::show hooked.\n");
		}
		else
		{
			printf("[ERROR] Failed to hook QWidget::show. Code=%u\n", static_cast<uint32_t>(hookResult));
			return std::to_underlying(PayloadResult::CannotHook);
		}
	}

	// 4. Obtain and store address of trampoline to global states
	{
		Hook_QWidget::s_trampoline = gan::Hook::GetTrampoline(targetFunc);
		if (!Hook_QWidget::s_trampoline)
		{
			Printf("[ERROR] Failed to get trampoline.\n");
			return std::to_underlying(PayloadResult::CannotGetTrampoline);
		}

		const auto trampoline = gan::FromMemFn(Hook_QWidget::s_trampoline);
		Printf("[INFO] trampoline=%p\n", trampoline);
		Printf(
			"[INFO] copied prolog in trampoline: [op_mov]=%02x [addr]=%p\n",
			gan::ConstMemAddr{ trampoline }.ConstRef<unsigned char>(),
			gan::ConstMemAddr{ trampoline }.Offset(1).ConstRef<void*>()
		);
	}

	return std::to_underlying(PayloadResult::Success);
}


BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    switch (reason)
    {
		case DLL_PROCESS_ATTACH:
		{
			if constexpr (UseDebugConsole())
			{
				FILE* fp;
				AllocConsole();
				freopen_s(&fp, "CONIN$", "r+t", stdin);
				freopen_s(&fp, "CONOUT$", "w+t", stdout);
				freopen_s(&fp, "CONOUT$", "w+t", stderr);
			}

			const auto hThread = CreateThread(nullptr, 0, PayloadMain, nullptr, 0, nullptr);
			if (!hThread)
				return FALSE;
			break;
		}

		case DLL_PROCESS_DETACH:
		{
			if constexpr (UseDebugConsole())
				FreeConsole();
			break;
		}

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		default:
			break;
	}
    return TRUE;
}
