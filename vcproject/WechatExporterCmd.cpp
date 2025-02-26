// WechatExporterCmd.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <vector>


#include <windows.h>
#include <shlwapi.h>
#include <string.h>
#include <tchar.h>
#include <atlstr.h>
#include <fcntl.h>
#include "Core.h"
#include <WechatExporterCmd.h>

std::string getCurrentLanguageCode();

static const WCHAR UNICODE_BOM = 0xFEFF;

void UPrint(LPCWSTR String) {

#ifndef NDEBUG
	// std::wcout << String;
	// return;
#endif

	DWORD ConsoleMode;
	BOOL ConsoleOutput;
	DWORD FileType;
	BOOL Result;
	HANDLE StdOut;
	DWORD StringCharCount;
	DWORD Written;

	//
	// StdOut describes the standard output device.  This can be the console
	// or (if output has been redirected) a file or some other device type.
	//
	StdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (StdOut == INVALID_HANDLE_VALUE) {
		goto PrintExit;
	}

	//
	// Check whether the handle describes a character device.  If it does, then
	// it may be a console device.  A call to GetConsoleMode will fail with
	// ERROR_INVALID_HANDLE if it is not a console device.
	//
	FileType = GetFileType(StdOut);

	if ((FileType == FILE_TYPE_UNKNOWN) && (GetLastError() != ERROR_SUCCESS))
	{
		goto PrintExit;
	}

	FileType &= ~(FILE_TYPE_REMOTE);

	if (FileType == FILE_TYPE_CHAR)
	{
		Result = GetConsoleMode(StdOut, &ConsoleMode);

		if ((Result == FALSE) && (GetLastError() == ERROR_INVALID_HANDLE))
		{
			ConsoleOutput = FALSE;
		}
		else
		{
			ConsoleOutput = TRUE;
		}
	}
	else
	{
		ConsoleOutput = FALSE;
	}

	//
	// If StdOut is a console device then just use the UNICODE console write
	// API.  This API doesn't work if StdOut has been redirected to a file or
	// some other device.  In this case, write to StdOut using WriteFile.
	//

	StringCharCount = (DWORD)wcslen(String);

	if (ConsoleOutput != FALSE)
	{
		WriteConsoleW(StdOut, (PVOID)String, StringCharCount, &Written, NULL);
	}
	else
	{
		//
		// Write out a Unicode BOM to ensure proper processing by text readers
		//
		WriteFile(StdOut, (PVOID)&UNICODE_BOM, sizeof(UNICODE_BOM), &Written, NULL);

		//
		// The number of bytes to write to standard output must exclude the null
		// terminating character.
		//
		WriteFile(StdOut, (PVOID)String, (StringCharCount * sizeof(WCHAR)), &Written, NULL);
	}

PrintExit:
	return;
}

class LoggerImpl : public Logger
{
protected:
	UINT m_cp;

public:

	LoggerImpl()
	{
		m_cp = GetConsoleOutputCP();
		if (m_cp != CP_UTF8)
		{
#ifdef NDEBUG
			// _setmode(_fileno(stdout), _O_U16TEXT);
#endif
		}
		std::wcout << L"CodePage:" << m_cp << std::endl;
	}

	void write(const std::string& log)
	{
		if (m_cp == CP_UTF8)
		{
			std::cout << log << std::endl;
		}
		else
		{
			CA2W str(log.c_str(), CP_UTF8);
			// CW2W targetStr(str, m_cp);
			UPrint((LPCWSTR)str);
			UPrint(L"\r\n");
		}
	}

	void debug(const std::string& log)
	{
		if (m_cp == CP_UTF8)
		{
			std::cout << "DBG:: " << log << std::endl;
		}
		else
		{
			CA2W str(log.c_str(), CP_UTF8);
			// CW2W targetStr(str, m_cp);
			UPrint(L"DBG::");
			UPrint((LPCWSTR)str);
			UPrint(L"\r\n");
		}
	}
};

std::string parseArgumentwithQuatoW(LPCWSTR path)
{
	std::string parsedPath;
	if (NULL == path)
	{
		return parsedPath;
	}

	size_t start = 0;
	size_t length = lstrlenW(path);
	if (length > 1 && path[length - 1] == '"')
	{
		length--;
	}
	if (path[0] == '"')
	{
		start = 1;
		length--;
	}

	std::wstring value(path, start, length);

	return std::string((LPCSTR)CW2A(value.c_str(), CP_UTF8));
}

int main()
{
	if (GetUserDefaultUILanguage() == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
	{
		::SetThreadUILanguage(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
	}
	else
	{
		::SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	}

	TCHAR curDir[MAX_PATH] = { 0 };
	DWORD dwRet = GetModuleFileName(NULL, curDir, MAX_PATH);
	if (dwRet == 0)
	{
		return 1;
	}
	
	if (!PathRemoveFileSpec(curDir))
	{
		return 1;
	}

	TCHAR buffer[MAX_PATH] = { 0 };
	_tcscpy(buffer, curDir);
	PathAppend(buffer, TEXT("Dlls"));
	SetDllDirectory(buffer);

    int outputFormat = OUTPUT_FORMAT_HTML;
    int asyncLoading = HTML_OPTION_ONSCROLL;
    bool outputFilter = false;
    std::string backupDir;
    std::string outputDir;
    std::string account;
    std::vector<std::string> sessions;

	LPWSTR *szArglist = NULL;
	int nArgs;

	std::unique_ptr<LPWSTR, HLOCAL(__stdcall *)(HLOCAL)> szArglistPtr(szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs), ::LocalFree);
	if (NULL == szArglist)
	{
		return 1;
	}

	for (int idx = 1; idx < nArgs; idx++)
    {
        if (lstrcmpW(L"--help", szArglist[idx]) == 0)
        {
			CW2A fileName(PathFindFileNameW(szArglist[0]));
            printHelp((LPCSTR)fileName);
            return 0;
        }
        
		LPWSTR equals_pos = wcschr(szArglist[idx], '=');
        if (equals_pos == NULL)
        {
            continue;
        }
        
        std::wstring name = std::wstring(szArglist[idx], equals_pos - szArglist[idx]);
        if (name == L"--backup")
        {
            backupDir = parseArgumentwithQuatoW(equals_pos + 1);
        }
        else if (name == L"--output")
        {
            outputDir = parseArgumentwithQuatoW(equals_pos + 1);
        }
        else if (name == L"--account")
        {
            account = parseArgumentwithQuatoW(equals_pos + 1);
        }
        else if (name == L"--session")
        {
            sessions.push_back(parseArgumentwithQuatoW(equals_pos + 1));
        }
        else if (name == L"--asyncloading")
        {
            if (lstrcmpW(L"sync", equals_pos + 1) == 0)
            {
                asyncLoading = HTML_OPTION_SYNC;
            }
            else if (lstrcmpW(L"oninit", equals_pos + 1) == 0)
            {
                asyncLoading = HTML_OPTION_ONINIT;
            }
        }
        else if (name == L"--filter")
        {
            if (lstrcmpW(L"yes", equals_pos + 1) == 0)
            {
                outputFilter = true;
            }
        }
    }
    
    if (backupDir.empty() || !existsDirectory(backupDir))
    {
        std::cout << "Please input valid iTunes backup directory." << std::endl;
        return 1;
    }
    if (outputDir.empty() || !existsDirectory(outputDir))
    {
        std::cout << "Please input valid output directory." << std::endl;
        return 1;
    }
    if (account.empty())
    {
        std::cout << "Please input account name." << std::endl;
        return 1;
    }
   
    std::string languageCode = getCurrentLanguageCode();
	CW2A workDir(CT2W(curDir), CP_UTF8);

	LoggerImpl logger;

    return exportSessions(languageCode, &logger, (LPCSTR)workDir, backupDir, outputDir, account, sessions, outputFormat, asyncLoading, outputFilter);
}

std::string getCurrentLanguageCode()
{
	std::string languageCode;
	if (GetUserDefaultUILanguage() == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
	{
		languageCode = "zh-Hans";
	}

	return languageCode;
}

