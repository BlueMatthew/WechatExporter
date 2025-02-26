#pragma once

#include "stdafx.h"
#include "Core.h"
#include <locale>
#include <codecvt>

class LoggerImpl : public Logger
{
protected:
#if !defined(NDEBUG) || defined(DBG_PERF)
	CRITICAL_SECTION	m_cs;
	TCHAR m_szLogFile[MAX_PATH];
#endif
	HWND m_hWndLog;
public:
	LoggerImpl(HWND hWndLog) : m_hWndLog(hWndLog)
	{
#if !defined(NDEBUG) || defined(DBG_PERF)
		InitializeCriticalSection(&m_cs);
		m_szLogFile[0] = 0;
#endif
	}

	~LoggerImpl()
	{
		m_hWndLog = NULL;
#if !defined(NDEBUG) || defined(DBG_PERF)
		DeleteCriticalSection(&m_cs);
#endif
	}

	void setLogPath(LPCTSTR lpszLogPath)
	{
#if !defined(NDEBUG) || defined(DBG_PERF)
		EnterCriticalSection(&m_cs);
		_tcscpy(m_szLogFile, lpszLogPath);
		PathAppend(m_szLogFile, TEXT("log.txt"));
		HANDLE hFile = CreateFile(m_szLogFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (INVALID_HANDLE_VALUE != hFile)
		{
			CloseHandle(hFile);
		}
		LeaveCriticalSection(&m_cs);
#endif
	}

	void outputLog(LPCTSTR pszLog)
	{
		::SendMessage(m_hWndLog, LB_ADDSTRING, 0, (LPARAM)pszLog);
		LRESULT count = ::SendMessage(m_hWndLog, LB_GETCOUNT, 0, 0L);
		::SendMessage(m_hWndLog, LB_SETTOPINDEX, count - 1, 0L);
	}

	void write(const std::string& log)
	{
#if !defined(NDEBUG) || defined(DBG_PERF)
		std::string timeString = getTimestampString(false, true) + ": ";
#else
		std::string timeString = getTimestampString() + ": ";
#endif
		CA2T szTime(timeString.c_str());

#if !defined(NDEBUG) || defined(DBG_PERF)
		EnterCriticalSection(&m_cs);
		if (_tcslen(m_szLogFile) > 0)
		{
			HANDLE hFile = ::CreateFile(m_szLogFile, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (INVALID_HANDLE_VALUE != hFile)
			{
				::SetFilePointer(hFile, 0, 0, FILE_END);
				DWORD dwBytesToWrite = static_cast<DWORD>(log.size());
				DWORD dwBytesWritten = 0;
				BOOL bErrorFlag = WriteFile(hFile, timeString.c_str(), timeString.size(), &dwBytesWritten, NULL);
				bErrorFlag = WriteFile(hFile, log.c_str(), dwBytesToWrite, &dwBytesWritten, NULL);
				if (bErrorFlag)
				{
					WriteFile(hFile, "\r\n", 2, &dwBytesWritten, NULL);
				}
				CloseHandle(hFile);
			}
		}
		LeaveCriticalSection(&m_cs);
#endif

		CW2T pszT(CA2W(log.c_str(), CP_UTF8));

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

