#pragma once

#include "stdafx.h"
#include "Core.h"

class ExportNotifierImpl : public ExportNotifier
{
protected:
	HWND m_hWnd;

public:
	static const DWORD WM_START = WM_USER + 10;
	static const DWORD WM_COMPLETE = WM_USER + 11;
	static const DWORD WM_PROGRESS = WM_USER + 12;

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
		// ::PostMessage(m_hWnd, WM_PROGRESS, (WPARAM)0, (LPARAM)&progress);
	}
	
	void onComplete(bool cancelled) const
	{
		::PostMessage(m_hWnd, WM_COMPLETE, (WPARAM)0, (LPARAM)cancelled);
	}

	void onSessionStart(const std::string& sessionUsrName) const
	{
		::PostMessage(m_hWnd, WM_START, (WPARAM)0, (LPARAM)1/*cancellable*/);
	}

	void onSessionProgress(const std::string& sessionUsrName, uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const
	{

	}

	void onSessionComplete(const std::string& sessionUsrName, bool cancelled) const
	{

	}
};

