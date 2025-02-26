// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <future>

class CBackupDlg : public CDialogImpl<CBackupDlg>
{
public:
	enum { IDD = IDD_BACKUP_DLG };

	BEGIN_MSG_MAP(CBackupDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		// COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()


	CBackupDlg(const DeviceInfo& deviceInfo, const CString& outputDir) : m_deviceInfo(deviceInfo), m_outputDir(outputDir), m_backup(NULL), m_eventId(0), m_result(false)
	{

	}

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(GetParent());

		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		// progressCtrl.SetWindowText(TEXT(""));
		progressCtrl.ModifyStyle(PBS_MARQUEE, 0);
		progressCtrl.SetRange32(1, 100);
		progressCtrl.SetStep(1);
		progressCtrl.SetPos(0);

		CW2A outputDir(CT2W((LPCTSTR)m_outputDir), CP_UTF8);
		m_backup = new IDeviceBackup(m_deviceInfo, (LPCSTR)outputDir);

		m_task = std::async(std::launch::async, &IDeviceBackup::backup, m_backup);

		m_eventId = SetTimer(1, 200);
		return TRUE;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if (0 != m_eventId)
		{
			KillTimer(m_eventId);
		}
		CenterWindow(GetParent());

		return 0;
	}

	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		if (NULL != m_backup)
		{
			double progress = m_backup->getOverallProgress();
			int pos = (int)(progress * 100);
			if (pos > 100)
			{
				pos = 100;
			}
			else if (pos < 0)
			{
				pos = 0;
			}
			
			CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
			if (pos != progressCtrl.GetPos())
			{
				progressCtrl.SetPos(pos);
			}
		}
		std::future_status status = m_task.wait_for(std::chrono::seconds(0));
		if (status == std::future_status::ready)
		{
			KillTimer(m_eventId);
			m_eventId = 0;
			
			m_result = m_task.get();

			EndDialog(IDOK);
		}

		
		return 0;
	}

	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	bool getBackupResult() const
	{
		return m_result;
	}

protected:
	// CHyperLink m_homePageLinkCtrl;
	const DeviceInfo&	m_deviceInfo;
	CString m_outputDir;
	IDeviceBackup* m_backup;
	std::future<bool> m_task;

	UINT m_eventId;

	bool m_result;
};
