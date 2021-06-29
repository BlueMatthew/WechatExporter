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

	void outputLog(LPCTSTR pszLog)
	{
		::SendMessage(m_hWndLog, LB_ADDSTRING, 0, (LPARAM)pszLog);
		LRESULT count = ::SendMessage(m_hWndLog, LB_GETCOUNT, 0, 0L);
		::SendMessage(m_hWndLog, LB_SETTOPINDEX, count - 1, 0L);
	}

	void write(const std::string& log)
	{
		CW2T pszT(CA2W(log.c_str(), CP_UTF8));
		
#if !defined(NDEBUG) || defined(DBG_PERF)
		std::string timeString = getTimestampString(false, true) + ": ";
#else
		std::string timeString = getTimestampString() + ": ";
#endif
		CA2T szTime(timeString.c_str());

		std::vector<TCHAR> szLog;
		szLog.resize(_tcslen(pszT) + _tcslen(szTime) + 1, 0);

		_tcscpy(&szLog[0], (LPCTSTR)szTime);
		_tcscat(&szLog[0], (LPCTSTR)pszT);
		
		outputLog(&szLog[0]);
	}

	void debug(const std::string& log)
	{
// #if !defined(NDEBUG) || defined(DBG_PERF)
		write("[DBG] " + log);
// #endif
	}
};

