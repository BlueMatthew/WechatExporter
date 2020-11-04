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
	ExportNotifierImpl(HWND hWndProgress, std::vector<HWND>& ctrls) : m_hWndProgress(m_hWndProgress), m_isCancelled(false)
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
	void onProgress(double progress) const
	{
		if (::IsWindow(m_hWndProgress))
		{
			PBRANGE pbr;
			::SendMessage(m_hWndProgress, PBM_GETRANGE, TRUE, (LPARAM)&pbr);

			int pos = pbr.iLow + static_cast<int>((pbr.iHigh - pbr.iLow) * progress);

			::SendMessage(m_hWndProgress, PBM_SETPOS, pos, 0L);
		}


		// CA2T ca2t(log.c_str(), CP_UTF8);

		// ::SendMessage(m_hWndLog, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)ca2t);
	}
	
	void onComplete(bool cancelled) const
	{
		for (std::vector<HWND>::const_iterator it = m_ctrls.cbegin(); it != m_ctrls.cend(); ++it)
		{
			::EnableWindow(*it, TRUE);
		}
	}

	
};

