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
	static const UINT WM_SESSION_START = WM_USER + 13;
	static const UINT WM_SESSION_COMPLETE = WM_USER + 14;
	static const UINT WM_SESSION_PROGRESS = WM_USER + 15;
	static const UINT WM_EN_END = WM_SESSION_PROGRESS;

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
};

