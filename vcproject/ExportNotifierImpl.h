#pragma once

#include "stdafx.h"
#include "Core.h"

class ExportNotifierImpl : public ExportNotifier
{
protected:
	HWND m_hWnd;

public:
	static const UINT WM_START = WM_USER + 10;
	static const UINT WM_COMPLETE = WM_USER + 11;
	static const UINT WM_PROGRESS = WM_USER + 12;
	static const UINT WM_USR_SESS_START = WM_USER + 13;
	static const UINT WM_USR_SESS_COMPLETE = WM_USER + 14;
	static const UINT WM_SESSION_START = WM_USER + 16;
	static const UINT WM_SESSION_COMPLETE = WM_USER + 17;
	static const UINT WM_SESSION_PROGRESS = WM_USER + 18;
	static const UINT WM_TASKS_START = WM_USER + 19;
	static const UINT WM_TASKS_COMPLETE = WM_USER + 20;
	static const UINT WM_TASKS_PROGRESS = WM_USER + 21;
	static const UINT WM_EN_END = WM_TASKS_PROGRESS;

public:
	ExportNotifierImpl(HWND hWnd) : m_hWnd(hWnd)
	{
	}

	~ExportNotifierImpl()
	{
		m_hWnd = NULL;
	}

	void onStart() const
	{
		::PostMessage(m_hWnd, WM_START, (WPARAM)0, (LPARAM)1/*cancellable*/);
	}

	void onProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const
	{
		::PostMessage(m_hWnd, WM_PROGRESS, (WPARAM)numberOfMessages, (LPARAM)numberOfTotalMessages);
	}
	
	void onComplete(bool cancelled) const
	{
		::PostMessage(m_hWnd, WM_COMPLETE, (WPARAM)0, (LPARAM)(cancelled ? 1 : 0));
	}

	void onUserSessionStart(const std::string& usrName, uint32_t numberOfSessions) const
	{

	}

	void onUserSessionComplete(const std::string& usrName) const
	{

	}

	void onSessionStart(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfTotalMessages) const
	{
		::PostMessage(m_hWnd, WM_SESSION_START, reinterpret_cast<WPARAM>(sessionData), (LPARAM)numberOfTotalMessages);
	}

	void onSessionProgress(const std::string& sessionUsrName, void * sessionData, uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const
	{
		::PostMessage(m_hWnd, WM_SESSION_PROGRESS, reinterpret_cast<WPARAM>(sessionData), (LPARAM)numberOfMessages);
	}

	void onSessionComplete(const std::string& sessionUsrName, void * sessionData, bool cancelled) const
	{
		::PostMessage(m_hWnd, WM_SESSION_COMPLETE, reinterpret_cast<WPARAM>(sessionData), (LPARAM)(cancelled ? 1 : 0));
	}

	void onTasksStart(const std::string& usrName, uint32_t numberOfTotalTasks) const
	{
		::PostMessage(m_hWnd, WM_TASKS_START, (WPARAM)numberOfTotalTasks, 0);
	}

	void onTasksProgress(const std::string& usrName, uint32_t numberOfCompletedTasks, uint32_t numberOfTotalMessages) const
	{
		::PostMessage(m_hWnd, WM_TASKS_PROGRESS, numberOfTotalMessages, (LPARAM)numberOfCompletedTasks);
	}

	void onTasksComplete(const std::string& usrName, bool cancelled) const
	{
		::PostMessage(m_hWnd, WM_TASKS_COMPLETE, 0, (LPARAM)(cancelled ? 1 : 0));
	}
};

