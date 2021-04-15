#pragma once

#include "stdafx.h"
#include "Core.h"

class PdfConverterImpl : public PdfConverter
{
public:
	PdfConverterImpl() : m_pdfSupported(false)
	{
		if (isChromeInstalled(m_assemblyPath))
		{
			m_pdfSupported = true;
			m_param = TEXT("--headless --disable-gpu --print-to-pdf=\"%%DEST%%\" --print-to-pdf-no-header \"file://%%SRC%%\"");
		}
		else if (isEdgeInstalled(m_assemblyPath))
		{
			m_pdfSupported = true;
			m_param = TEXT("--headless --disable-gpu --print-to-pdf=\"%%DEST%%\" --print-to-pdf-no-header \"file://%%SRC%%\"");
		}
	}
	
	bool isPdfSupported() const
	{
		return m_pdfSupported;
	}

	~PdfConverterImpl()
	{

	}

	bool convert(const std::string& htmlPath, const std::string& pdfPath)
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

private:
	bool m_pdfSupported;
	CString m_assemblyPath;
	CString m_param;
};
