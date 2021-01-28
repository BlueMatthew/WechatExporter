#pragma once

#include "stdafx.h"

class VersionDetector
{
public:
	CString GetProductVersion()
	{
		TCHAR szPath[MAX_PATH] = { 0 };

		GetModuleFileName(NULL, szPath, MAX_PATH);

		return GetProductVersion(szPath);
	}

	CString GetProductVersion(LPCTSTR szPath)
	{
		CString strResult;

		DWORD dwHandle;
		DWORD dwSize = GetFileVersionInfoSize(szPath, &dwHandle);

		if (dwSize > 0)
		{
			BYTE* pbBuf = new BYTE[dwSize];
			if (GetFileVersionInfo(szPath, dwHandle, dwSize, pbBuf))
			{
				UINT uiSize;
				BYTE* lpb;
				if (VerQueryValue(pbBuf,
					TEXT("\\VarFileInfo\\Translation"),
					(void**)&lpb,
					&uiSize))
				{
					WORD* lpw = (WORD*)lpb;
					CString strQuery;
					strQuery.Format(TEXT("\\StringFileInfo\\%04x%04x\\ProductVersion"), lpw[0], lpw[1]);
					if (VerQueryValue(pbBuf,
						const_cast<LPTSTR>((LPCTSTR)strQuery),
						(void**)&lpb,
						&uiSize) && uiSize > 0)
					{
						strResult = (LPCTSTR)lpb;
					}
				}
			}
			delete[] pbBuf;
		}

		return strResult;
	}
};