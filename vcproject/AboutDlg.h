// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <winver.h>

class CAboutDlg : public CDialogImpl<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUTBOX };

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// Replace to current version
		// If the version is set in aboud dlg resource directly,
		// The code below won't bring error.
		CStatic lblVersion = GetDlgItem(IDC_VERSION);
		CString version;
		lblVersion.GetWindowText(version);
		CString newVersion = GetProductVersion();
		newVersion = TEXT("v") + newVersion;
		version.Replace(TEXT("v1.0"), newVersion);
		lblVersion.SetWindowTextW(version);

		CenterWindow(GetParent());
		return TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	CString GetProductVersion()
	{
		CString strResult;

		TCHAR szPath[MAX_PATH] = { 0 };

		GetModuleFileName(NULL, szPath, MAX_PATH);
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
