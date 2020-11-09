#pragma once

#include "stdafx.h"
#include "Core.h"

class ExportNotifierImpl : public ExportNotifier
{
protected:
	HWND m_hWndProgress;
	std::vector<HWND> m_ctrls;
	bool m_isCancelled;

public:
	ExportNotifierImpl(HWND hWndProgress, std::vector<HWND>& ctrls) : m_hWndProgress(hWndProgress), m_isCancelled(false)
	{
		m_ctrls = ctrls;
	}

	~ExportNotifierImpl()
	{
		m_hWndProgress = NULL;
	}

	void cancel()
	{
		m_isCancelled = true;
	}

	bool isCancelled() const
	{
		return m_isCancelled;
	}

	void onStart() const
	{
		::PostMessage(m_hWndProgress, PBM_SETMARQUEE, (WPARAM)TRUE, (LPARAM)0);
	}

	void onProgress(double progress) const
	{
	}
	
	void onComplete(bool cancelled) const
	{
		for (std::vector<HWND>::const_iterator it = m_ctrls.cbegin(); it != m_ctrls.cend(); ++it)
		{
			::EnableWindow(*it, TRUE);
		}
		::PostMessage(m_hWndProgress, PBM_SETMARQUEE, (WPARAM)FALSE, (LPARAM)0);
	}
};

