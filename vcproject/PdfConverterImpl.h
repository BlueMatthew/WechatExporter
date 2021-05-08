#pragma once

#include "stdafx.h"
#include "Core.h"
#include <atlstr.h>

class PdfConverterImpl : public PdfConverter
{
public:
	PdfConverterImpl(LPCTSTR outputDir) : m_pdfSupported(false)
	{
		if (isChromeInstalled(m_assemblyPath))
		{
			m_pdfSupported = true;
			m_param = TEXT("--headless --disable-extensions --disable-gpu --print-to-pdf=\"%%DEST%%\" --print-to-pdf-no-header \"file://%%SRC%%\"");
		}
		else if (isEdgeInstalled(m_assemblyPath))
		{
			m_pdfSupported = true;
			m_param = TEXT("--headless --disable-extensions --disable-gpu --print-to-pdf=\"%%DEST%%\" --print-to-pdf-no-header \"file://%%SRC%%\"");
		}

		if (NULL != outputDir)
		{
			initShellFile(outputDir);
		}
	}
	
	bool isPdfSupported() const
	{
		return m_pdfSupported;
	}

	~PdfConverterImpl()
	{
	}

	void executeCommand()
	{
		if (!::PathFileExists(m_shellPath))
		{
			return;
		}

		const char *pauseCmd = "pause\r\n";
		appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(pauseCmd), strlen(pauseCmd));

		ShellExecute(NULL, TEXT("open"), m_shellPath, NULL, NULL, SW_SHOW);
	}

	bool makeUserDirectory(const std::string& dirName)
	{
		CW2T pszDir(CA2W(dirName.c_str(), CP_UTF8));

		TCHAR pdfOutputDir[MAX_PATH] = { 0 };
		PathCombine(pdfOutputDir, m_output, TEXT("pdf"));

		TCHAR userOutputDir[MAX_PATH] = { 0 };
		PathCombine(userOutputDir, pdfOutputDir, pszDir);
		CString command = TEXT("\r\nIF NOT EXIST \"");
		command += userOutputDir;
		command += TEXT("\" MKDIR \"");
		command += userOutputDir;
		command += TEXT("\"\r\n");

		CW2A pszU(CT2W(command), TARGET_CODE_PAGE);

		appendFile(m_shellPath, reinterpret_cast<const unsigned char *>((LPCSTR)pszU), strlen(pszU));

		return true;
	}

	bool convert(const std::string& htmlPath, const std::string& pdfPath)
	{
		if (!m_pdfSupported)
		{
			return false;
		}

		CW2T pszHtmlPath(CA2W(htmlPath.c_str(), CP_UTF8));
		TCHAR url[MAX_PATH * 4] = { 0 };
		DWORD cchUrl = MAX_PATH * 4; // max posible buffer size
		
		HRESULT res = UrlCreateFromPath(pszHtmlPath, url, &cchUrl, NULL);
		if (FAILED(res))
		{
			return false;
		}

		CW2T pszPdfPath(CA2W(pdfPath.c_str(), CP_UTF8));

		CString command(TEXT("\""));
		command += m_assemblyPath;
		command += TEXT("\" ");
		command += TEXT("--headless --disable-extensions --disable-gpu --print-to-pdf-no-header --print-to-pdf=\"");
		command += (LPCTSTR)pszPdfPath;
		command += TEXT("\" ");
		command += url;
		command += TEXT("\r\n");

		CT2A content(command, TARGET_CODE_PAGE);
		
		appendFile((LPCTSTR)m_shellPath, reinterpret_cast<const unsigned char *>((LPCSTR)content), strlen(content));
		// appendFile((LPCTSTR)m_shellPath, reinterpret_cast<const unsigned char *>((LPCTSTR)command), _tcslen(command) * sizeof(TCHAR));

		return true;
	}

	
	bool convert2(const std::string& htmlPath, const std::string& pdfPath)
	{
		CString param = m_param;
		CW2T pszSrc(CA2W(htmlPath.c_str(), CP_UTF8));
		CW2T pszDest(CA2W(pdfPath.c_str(), CP_UTF8));

		param.Replace(TEXT("%%SRC%%"), pszSrc);
		param.Replace(TEXT("%%DEST%%"), pszDest);

		SHELLEXECUTEINFO ShExecInfo = { 0 };
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.hwnd = NULL;
		ShExecInfo.lpVerb = TEXT("open");
		ShExecInfo.lpFile = (LPCTSTR)m_assemblyPath;
		ShExecInfo.lpParameters = (LPCTSTR)param;
		ShExecInfo.lpDirectory = NULL;
		ShExecInfo.nShow = SW_HIDE;
		ShExecInfo.hInstApp = NULL;
		ShellExecuteEx(&ShExecInfo);
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		CloseHandle(ShExecInfo.hProcess);

		// ShellExecute(NULL, TEXT("open"), m_assemblyPath, param, NULL, SW_HIDE);

		return true;
	}

protected:
	bool isChromeInstalled(CString& assemblyPath)
	{
		return isAppInstalled(TEXT("chrome.exe"), true, assemblyPath) || isAppInstalled(TEXT("chrome.exe"), false, assemblyPath);
	}

	bool isEdgeInstalled(CString& assemblyPath)
	{
		return isAppInstalled(TEXT("msedge.exe"), true, assemblyPath) || isAppInstalled(TEXT("msedge.exe"), false, assemblyPath);
	}

	bool isAppInstalled(LPCTSTR name, bool lmOrCU, CString& assemblyPath)
	{
		BOOL installed = false;
		CRegKey rk;
		CString path = lmOrCU ? TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\") : TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\");
		path += name;
		if (rk.Open(lmOrCU ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, (LPCTSTR)path, KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = 0;
			HRESULT hr = rk.QueryStringValue(NULL, NULL, &chars);
			if (SUCCEEDED(hr))
			{
				CString appPath;
				hr = rk.QueryStringValue(NULL, appPath.GetBufferSetLength(chars), &chars);
				appPath.ReleaseBuffer();

				if (PathFileExists(appPath))
				{
					assemblyPath = appPath;
					installed = true;
				}
			}
			rk.Close();
		}

		return installed;
	}

	void initShellFile(LPCTSTR outputDir)
	{
		m_output = outputDir;

		TCHAR shellFile[MAX_PATH] = { 0 };
		PathCombine(shellFile, outputDir, TEXT("pdf.bat"));
		m_shellPath = shellFile;
		::DeleteFile(shellFile);

		CString command;
		if (TARGET_CODE_PAGE == CP_UTF8)
		{
			command += TEXT("CHCP 65001\r\n");
		}
		TCHAR pdfPath[MAX_PATH] = { 0 };
		PathCombine(pdfPath, outputDir, TEXT("pdf"));

		command += TEXT("IF NOT EXIST \"");
		command += pdfPath;
		command += TEXT("\" MKDIR \"");
		command += pdfPath;
		command += TEXT("\"\r\n");

		CW2A pszU(CT2W(command), TARGET_CODE_PAGE);

		appendFile((LPCTSTR)m_shellPath, reinterpret_cast<const unsigned char *>((LPCSTR)pszU), strlen(pszU));
	}

	bool appendFile(LPCTSTR path, const unsigned char* data, size_t dataLength)
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

private:
	bool m_pdfSupported;
	CString m_assemblyPath;
	CString m_output;
	CString m_shellPath;
	CString m_param;

	const UINT TARGET_CODE_PAGE = CP_ACP;
	// const UINT TARGET_CODE_PAGE = CP_UTF8;
};
