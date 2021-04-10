// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <winver.h>
#include "VersionDetector.h"

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
		m_homePageLinkCtrl.SubclassWindow(GetDlgItem(IDC_VERSION));
		// Replace to current version
		// If the version is set in aboud dlg resource directly,
		// The code below won't bring error.
		// CStatic lblVersion = GetDlgItem(IDC_VERSION);
		CString version;
		m_homePageLinkCtrl.GetWindowText(version);
		VersionDetector vd;
		CString newVersion = vd.GetProductVersion();
		newVersion = TEXT("v") + newVersion;
		version.Replace(TEXT("v1.0"), newVersion);
		m_homePageLinkCtrl.SetWindowTextW(version);

		m_homePageLinkCtrl.SetHyperLink(TEXT("https://github.com/BlueMatthew/WechatExporter"));

		CenterWindow(GetParent());
		return TRUE;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

protected:
	CHyperLink m_homePageLinkCtrl;
};
