#pragma once

#include "stdafx.h"
#include "Core.h"
#include <locale>
#include <codecvt>

class LoggerImpl : public Logger
{
protected:
	HWND m_hWndLog;
public:
	LoggerImpl(HWND hWndLog) : m_hWndLog(hWndLog)
	{
	}

	~LoggerImpl()
	{
		m_hWndLog = NULL;
	}

	void write(const std::string& log)
	{
		CW2T pszT(CA2W(log.c_str(), CP_UTF8));

		::SendMessage(m_hWndLog, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)pszT);
		LRESULT count = ::SendMessage(m_hWndLog, LB_GETCOUNT, 0, 0L);
		::SendMessage(m_hWndLog, LB_SETTOPINDEX, count - 1, 0L);
	}

	void debug(const std::string& log)
	{
		write(log);
	}
};

