// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include "Core.h"
#include "LoggerImpl.h"
#include "ShellImpl.h"
#include "ExportNotifierImpl.h"

class CView : public CDialogImpl<CView>, public CDialogResize<CView>
{
private:
	ShellImpl			m_shell;
	LoggerImpl*			m_logger;
	ExportNotifierImpl* m_notifier;
	Exporter* m_exporter;

	std::vector<BackupManifest> m_manifests;

public:
	enum { IDD = IDD_WECHATEXPORTER_FORM };

	static const DWORD WM_START = ExportNotifierImpl::WM_START;
	static const DWORD WM_COMPLETE = ExportNotifierImpl::WM_COMPLETE;
	static const DWORD WM_PROGRESS = ExportNotifierImpl::WM_PROGRESS;

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.ModifyStyle(0, PBS_MARQUEE);

		m_logger = NULL;
		m_notifier = NULL;
		m_exporter = NULL;

		// Init the CDialogResize code
		DlgResize_Init();

		m_notifier = new ExportNotifierImpl(m_hWnd);
		m_logger = new LoggerImpl(GetDlgItem(IDC_LOG));

		// ::PostMessage(GetDlgItem(IDC_EXPORT), BM_CLICK, 0, 0L);

		TCHAR szPath[MAX_PATH] = { 0 };
		CRegKey rk;
		HRESULT result = S_OK;
		BOOL found = FALSE;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rk.QueryStringValue(TEXT("OutputDir"), szPath, &chars) == ERROR_SUCCESS)
			{
				found = TRUE;
			}
		}
		if (!found)
		{
			result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, szPath);
		}
		SetDlgItemText(IDC_OUTPUT, szPath);

		// Check iTunes Folder
		result = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath);
		_tcscat(szPath, TEXT("\\Apple Computer\\MobileSync\\Backup"));
		
		DWORD dwAttrib = ::GetFileAttributes(szPath);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		{
			CT2A backupDir(szPath, CP_UTF8);

			ManifestParser parser((LPCSTR)backupDir, "Info.plist", &m_shell);
			std::vector<BackupManifest> manifests = parser.parse();
			UpdateBackups(manifests);
		}
		
		return TRUE;
	}

	void OnFinalMessage(HWND hWnd)
	{
		if (NULL != m_exporter)
		{
			m_exporter->cancel();
			m_exporter->waitForComplition();
			delete m_exporter;
			m_exporter = NULL;
		}
		if (NULL != m_notifier)
		{
			delete m_notifier;
			m_notifier = NULL;
		}
		if (NULL != m_logger)
		{
			delete m_logger;
			m_logger = NULL;
		}
		// override to do something, if needed
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDialogResize<CView>)
		COMMAND_HANDLER(IDC_CHOOSE_BKP, BN_CLICKED, OnBnClickedChooseBkp)
		COMMAND_HANDLER(IDC_CHOOSE_OUTPUT, BN_CLICKED, OnBnClickedChooseOutput)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, OnBnClickedExport)
		COMMAND_HANDLER(IDC_CANCEL, BN_CLICKED, OnBnClickedCancel)
		MESSAGE_HANDLER(WM_START, OnStart)
		MESSAGE_HANDLER(WM_COMPLETE, OnComplete)
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CView)
		DLGRESIZE_CONTROL(IDC_CHOOSE_BKP, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_BACKUP, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_CHOOSE_OUTPUT, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_OUTPUT, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_LOG, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_PROGRESS, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_EXPORT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()


// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnBnClickedChooseBkp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString text;
		text.LoadString(IDS_SEL_BACKUP_DIR);

		CFolderDialog folder(NULL, text, BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON);
		if (IDOK == folder.DoModal()) {
			// std::wstring dir = folder.m_szFolderPath;
			// if ([backupUrl.absoluteString hasSuffix : @"/Backup"] || [backupUrl.absoluteString hasSuffix:@" / Backup / "])
			{
				CT2A backupDir(folder.m_szFolderPath, CP_UTF8);

				ManifestParser parser((LPCSTR)backupDir, "Info.plist", &m_shell);
				std::vector<BackupManifest> manifests = parser.parse();
				UpdateBackups(manifests);
			}
		}

		return 0;
	}

	LRESULT OnBnClickedChooseOutput(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString text;
		text.LoadString(IDS_SEL_OUTPUT_DIR);
		CFolderDialog folder(NULL, text, BIF_RETURNONLYFSDIRS | BIF_USENEWUI);
		
		TCHAR outputDir[MAX_PATH] = { 0 };
		GetDlgItemText(IDC_OUTPUT, outputDir, MAX_PATH);
		if (_tcscmp(outputDir, TEXT("")) == 0)
		{
			HRESULT result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, outputDir);
		}
		if (_tcscmp(outputDir, TEXT("")) != 0)
		{
			folder.SetInitialFolder(outputDir);
		}
		
		if (IDOK == folder.DoModal()) {
			CRegKey rk;
			if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
			{
				rk.SetStringValue(TEXT("OutputDir"), folder.m_szFolderPath);
			}

			SetDlgItemText(IDC_OUTPUT, folder.m_szFolderPath);
		}

		return 0;
	}

	LRESULT OnBnClickedCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString text;
		CString caption;
		caption.LoadString(IDR_MAINFRAME);
		text.LoadString(IDS_CANCEL_PROMPT);
		if (MessageBox(text, caption, MB_OKCANCEL) == IDCANCEL)
		{
			return 0;
		}

		if (NULL == m_exporter)
		{
			return 0;
		}

		m_exporter->cancel();

		return 0;
	}

	LRESULT OnBnClickedExport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		if (NULL != m_exporter)
		{
			return 0;
		}

		CComboBox cbmBox = GetDlgItem(IDC_BACKUP);
		if (cbmBox.GetCurSel() == -1)
		{
			CString text;
			text.LoadString(IDS_SEL_BACKUP_DIR);
			MessageBox(text);
			return 0;
		}

		std::string backup = m_manifests[cbmBox.GetCurSel()].getPath();

		TCHAR buffer[MAX_PATH] = { 0 };
		GetDlgItemText(IDC_OUTPUT, buffer, MAX_PATH);
		CW2A output(CT2W(buffer), CP_UTF8);

		DWORD dwRet = GetCurrentDirectory(MAX_PATH, buffer);
		if (dwRet == 0)
		{
			// printf("GetCurrentDirectory failed (%d)\n", GetLastError());
			return 0;
		}

		CListBox lstboxLogs = GetDlgItem(IDC_LOG);
		lstboxLogs.ResetContent();

		CW2A resDir(CT2W(buffer), CP_UTF8);
		Run((LPCSTR)resDir, backup, (LPCSTR)output);

		return 0;
	}

	LRESULT OnStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		EnableInteractiveCtrls(FALSE);
		return 0;
	}

	LRESULT OnComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (m_exporter)
		{
			m_exporter->waitForComplition();
			delete m_exporter;
			m_exporter = NULL;
		}
		EnableInteractiveCtrls(TRUE);
		return 0;
	}

	void Run(const std::string& resDir, const std::string& backup, const std::string& output)
	{
		m_exporter = new Exporter(resDir, backup, output, &m_shell, m_logger);
		m_exporter->setNotifier(m_notifier);
		if (m_exporter->run())
		{
			EnableInteractiveCtrls(FALSE);
		}
	}

	void EnableInteractiveCtrls(BOOL enabled)
	{
		::EnableWindow(GetDlgItem(IDC_BACKUP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_BKP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_OUTPUT), enabled);
		::EnableWindow(GetDlgItem(IDC_EXPORT), enabled);
		::EnableWindow(GetDlgItem(IDC_CANCEL), !enabled);
	}

	void UpdateBackups(const std::vector<BackupManifest>& manifests)
	{
		int selectedIndex = -1;
		if (!manifests.empty())
		{
			// Add default backup folder
			for (std::vector<BackupManifest>::const_iterator it = manifests.cbegin(); it != manifests.cend(); ++it)
			{
				std::vector<BackupManifest>::const_iterator it2 = std::find(m_manifests.cbegin(), m_manifests.cend(), *it);
				if (it2 != m_manifests.cend())
				{
					if (selectedIndex == -1)
					{
						selectedIndex = static_cast<int>(std::distance(it2, m_manifests.cbegin()));
					}
				}
				else
				{
					m_manifests.push_back(*it);
					if (selectedIndex == -1)
					{
						selectedIndex = static_cast<int>(m_manifests.size() - 1);
					}
				}
			}

			// update
			CComboBox cmb = GetDlgItem(IDC_BACKUP);
			cmb.LockWindowUpdate();
			cmb.Clear();
			for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
			{
				std::string itemTitle = it->toString();
				// String* item = [NSString stringWithUTF8String : itemTitle.c_str()];
				CA2T item(it->toString().c_str(), CP_UTF8);
				cmb.AddString((LPCTSTR)item);
			}
			if (selectedIndex != -1 && selectedIndex < cmb.GetCount())
			{
				cmb.SetCurSel(selectedIndex);
			}
			cmb.LockWindowUpdate(FALSE);
		}
	}
	

};
