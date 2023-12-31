/*
 *  Gandr - another minimalism library for hacking x86-based Windows
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

#include <Debugger.h>

#include <windows.h>

#include <cassert>


namespace gan
{


// ---------------------------------------------------------------------------
// class Debugger
// ---------------------------------------------------------------------------

Debugger::Debugger()
	: m_flagEventLoopExit(false)
{
}

Debugger::~Debugger()
{
	RemoveAllSessions(DebugSession::EndOption::Kill);
}

Debugger::EventLoopResult Debugger::EnterEventLoop()
{
	DEBUG_EVENT dbgEvent;

	m_flagEventLoopExit = false;
	while (!m_flagEventLoopExit)
	{
		if (m_sessions.size() == 0)
			return EventLoopResult::AllDetached;

		auto contStatus = DebugSession::ContinueStatus::ContinueThread;  // continue by default

		if (::WaitForDebugEvent(&dbgEvent, INFINITE) == 0)
			return EventLoopResult::ErrorOccurred;

		auto itr = m_sessions.find(dbgEvent.dwProcessId);
		if (itr == m_sessions.end())
			continue;  // this shouldn't happen though
		auto pSession = itr->second;

		DebugSession::PreEvent preEvent {
			.eventCode = dbgEvent.dwDebugEventCode,
			.threadId = dbgEvent.dwThreadId
		};
		pSession->OnPreEvent(preEvent);

		switch (dbgEvent.dwDebugEventCode)
		{
			case EXCEPTION_DEBUG_EVENT:
			{
				contStatus = pSession->OnExceptionTriggered(dbgEvent.u.Exception);
				break;
			}
			case CREATE_THREAD_DEBUG_EVENT:
			{
				contStatus = pSession->OnThreadCreated(dbgEvent.u.CreateThread);
				break;
			}
			case CREATE_PROCESS_DEBUG_EVENT:
			{
				contStatus = pSession->OnProcessCreated(dbgEvent.u.CreateProcessInfo);
				::CloseHandle(dbgEvent.u.CreateProcessInfo.hFile);
				break;
			}
			case EXIT_THREAD_DEBUG_EVENT:
			{
				contStatus = pSession->OnThreadExited(dbgEvent.u.ExitThread);
				break;
			}
			case EXIT_PROCESS_DEBUG_EVENT:
			{
				contStatus = pSession->OnProcessExited(dbgEvent.u.ExitProcess);
				break;
			}
			case LOAD_DLL_DEBUG_EVENT:
			{
				contStatus = pSession->OnDllLoaded(dbgEvent.u.LoadDll);
				::CloseHandle(dbgEvent.u.LoadDll.hFile);
				break;
			}
			case UNLOAD_DLL_DEBUG_EVENT:
			{
				contStatus = pSession->OnDllUnloaded(dbgEvent.u.UnloadDll);
				break;
			}
			case OUTPUT_DEBUG_STRING_EVENT:
			{
				contStatus = pSession->OnStringOutput(dbgEvent.u.DebugString);
				break;
			}
			case RIP_EVENT:
			{
				contStatus = pSession->OnRipEvent(dbgEvent.u.RipInfo);
				break;
			}
			default:
			{
				break;
			}
		}

		::ContinueDebugEvent(
			dbgEvent.dwProcessId,
			dbgEvent.dwThreadId,
			contStatus == DebugSession::ContinueStatus::NotHandled ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE
		);

		if (contStatus == DebugSession::ContinueStatus::CloseSession)
			RemoveSession(dbgEvent.dwProcessId, DebugSession::EndOption::Detach);
	}

	return EventLoopResult::ExitRequested;
}

bool Debugger::AddSessionInstance(const std::shared_ptr<DebugSession>& pSession)
{
	assert(pSession);
	if (!pSession || !pSession->IsValid())
		return false;

	return m_sessions
		.try_emplace(pSession->GetId(), pSession)
		.second;
}

bool Debugger::RemoveSession(DebugSession::Identifier sessionId, DebugSession::EndOption option)
{
	auto itr = m_sessions.find(sessionId);
	if (itr == m_sessions.end())
		return false;

	itr->second->End(option);
	m_sessions.erase(itr);
	return true;
}

void Debugger::RemoveAllSessions(DebugSession::EndOption option)
{
	for (auto& itr : m_sessions)
		itr.second->End(option);
	m_sessions.clear();
}

void Debugger::GetSessionList(IdList& output) const
{
	output.clear();
	for (auto& itr : m_sessions)
		output.push_back(itr.first);
}


}  // namespace gan
