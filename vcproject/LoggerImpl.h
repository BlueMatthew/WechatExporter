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
		// std::string localString = utf8ToString(log, std::locale());
		// CA2T ca2t(localString.c_str());

		static char newline[] = { 13, 10 };
		CA2T ca2t(log.c_str(), CP_UTF8);
#ifndef NDEBUG
		std::string fileName = "D:\\debug1.log";
		std::ofstream logfile;

		// std::locale utf8_locale(std::locale(), new std::utf8cvt<false>);
		// logfile.imbue(utf8_locale);

		logfile.open(fileName, std::ios::out | std::ios::binary | std::ios::app);

		{
			logfile.write(log.c_str(), log.size());
			logfile.write(newline, 2);
			logfile.close();
		}

		ATLTRACE(TEXT("%s\r\n"), (LPCTSTR)ca2t);
#endif

		::SendMessage(m_hWndLog, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)ca2t);
	}

	void debug(const std::string& log)
	{
		write(log);
	}
};

