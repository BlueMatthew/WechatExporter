// wxexpatch.cpp : main source file for wxexpatch.exe
//

#include "stdafx.h"

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlstr.h>

#include "resource.h"

#include <queue>
#include <vector>

#include <shlwapi.h>

CAppModule _Module;

#define BUFFERSIZE (32 * 1024)

bool findInvalidHtmlContent(const char *buffer, size_t& startPos, size_t& endPos)
{
	const char* str = strstr(buffer, "loadMsgsForNextPage");
	if (str == NULL)
	{
		return false;
	}

	const TCHAR* errorContents = TEXT("for (var idx = 0; idx < window.moreWechatMsgs.length; idx++)");

	CW2A utf8(CT2W(errorContents), CP_UTF8);

	str = strstr(buffer, (LPCSTR)utf8);
	if (str == NULL)
	{
		return false;
	}
	startPos = str - buffer;
	endPos = strlen((LPCSTR)utf8) + startPos;

	return true;
}

bool writeFile(LPCTSTR path, const char* data, size_t dataLength)
{
	HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD dwBytesToWrite = static_cast<DWORD>(dataLength);
	DWORD dwBytesWritten = 0;
	BOOL bErrorFlag = WriteFile(hFile, data, dwBytesToWrite, &dwBytesWritten, NULL);

	::CloseHandle(hFile);
	return (TRUE == bErrorFlag);
}

bool appendFile(LPCTSTR path, const char* data, size_t dataLength)
{
	HANDLE hFile = ::CreateFile(path, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	::SetFilePointer(hFile, 0, 0, FILE_END);
	DWORD dwBytesToWrite = static_cast<DWORD>(dataLength);
	DWORD dwBytesWritten = 0;
	BOOL bErrorFlag = WriteFile(hFile, data, dwBytesToWrite, &dwBytesWritten, NULL);
	::CloseHandle(hFile);
	return (TRUE == bErrorFlag);
}

bool doPatch()
{
	if (MessageBox(NULL, TEXT("Please backup first. Continue?"), TEXT("Patch"), MB_YESNO) == IDNO)
	{
		return false;
	}

	TCHAR szRoot[MAX_PATH] = { 0 };
	DWORD dwRet = ::GetCurrentDirectory(MAX_PATH, szRoot);
	if (dwRet == 0)
	{
		return false;
	}

	PathAddBackslash(szRoot);

	std::vector<char> buffer;
	std::queue<CString> directories;
	directories.push(TEXT(""));

	TCHAR szFindPath[MAX_PATH] = { 0 };
	TCHAR szRelativePath[MAX_PATH] = { 0 };
	TCHAR szFullPath[MAX_PATH] = { 0 };
	TCHAR szNewFullPath[MAX_PATH] = { 0 };
	DWORD dwNumberOfBytesRead = 0;
	DWORD dwNumberOfBytesWritten = 0;
	size_t startPos = 0;
	size_t endPos = 0;
	const char* fixedContents = "while (window.moreWechatMsgs.length > 0)";
	const char* logTitle = "Fixed FileList:\r\n";

	TCHAR szLogPath[MAX_PATH] = { 0 };
	TCHAR szLog[MAX_PATH] = { 0 };
	_tcscpy(szLogPath, szRoot);
	PathAppend(szLogPath, TEXT("patch.log"));

	writeFile(szLogPath, logTitle, strlen(logTitle));

	while (!directories.empty())
	{
		const CString& dirName = directories.front();

		PathCombine(szFindPath, szRoot, dirName);
		PathAddBackslash(szFindPath);
		PathAppend(szFindPath, TEXT("*.*"));

		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		hFind = FindFirstFile((LPTSTR)szFindPath, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		do
		{
			if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
			{
				continue;
			}

			bool isDir = ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
			PathCombine(szRelativePath, dirName, FindFileData.cFileName);

			size_t len = 0;
			if (isDir)
			{
				PathAddBackslash(szRelativePath);
				directories.emplace(szRelativePath);
			}
			else if (((len = _tcslen(FindFileData.cFileName)) > 5) && _tcscmp(&(FindFileData.cFileName[len - 5]), TEXT(".html")) == 0)
			{
				// .html
				PathCombine(szFullPath, szRoot, dirName);
				PathAddBackslash(szFullPath);
				PathAppend(szFullPath, FindFileData.cFileName);

				_tcscpy(szNewFullPath, szFullPath);
				_tcscat(szNewFullPath, TEXT(".html"));

				HANDLE hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hFile != INVALID_HANDLE_VALUE)
				{
					DWORD dwFileLen = GetFileSize(hFile, NULL);
					buffer.resize(dwFileLen + 1);
					buffer[dwFileLen] = 0;
					if (ReadFile(hFile, &buffer[0], dwFileLen, &dwNumberOfBytesRead, NULL))
					{
						buffer[dwNumberOfBytesRead] = 0;
						if (findInvalidHtmlContent(&buffer[0], startPos, endPos))
						{
							SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
							SetEndOfFile(hFile);

							WriteFile(hFile, &buffer[0], startPos, &dwNumberOfBytesWritten, NULL);
							WriteFile(hFile, fixedContents, strlen(fixedContents), &dwNumberOfBytesWritten, NULL);
							WriteFile(hFile, &buffer[endPos], buffer.size() - endPos, &dwNumberOfBytesWritten, NULL);

							_tcscpy(szLog, szRelativePath);
							_tcscat(szLog, TEXT("\r\n"));
							CW2A utf8Log(CT2W(szLog), CP_UTF8);

							appendFile(szLogPath, (LPCSTR)utf8Log, strlen(utf8Log));
						}
					}

					CloseHandle(hFile);
				}
			}

		} while (::FindNextFile(hFind, &FindFileData));
		FindClose(hFind);

		directories.pop();
	}

	MessageBox(NULL, TEXT("All files have been fixed. May check patch.log for more details."), TEXT("Patch"), MB_OK);

	return true;
}


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	doPatch();

	_Module.Term();
	::CoUninitialize();

	return 0;
}
