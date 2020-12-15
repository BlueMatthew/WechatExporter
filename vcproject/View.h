// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include "Core.h"
#include "LoggerImpl.h"
#include "ShellImpl.h"
#include "ExportNotifierImpl.h"
#include "ColoredControls.h"
#include "LogListBox.h"

class CView : public CDialogImpl<CView>, public CDialogResize<CView>
{
private:

	CColoredStaticCtrl	m_iTunesLabel;
	CLogListBox			m_logListBox;

	ShellImpl			m_shell;
	LoggerImpl*			m_logger;
	ExportNotifierImpl* m_notifier;
	Exporter*			m_exporter;

	std::vector<BackupManifest> m_manifests;
	std::vector<std::pair<Friend, std::vector<Session>>> m_usersAndSessions;

	int m_itemClicked;

public:
	enum { IDD = IDD_WECHATEXPORTER_FORM };

	static const DWORD WM_START = ExportNotifierImpl::WM_START;
	static const DWORD WM_COMPLETE = ExportNotifierImpl::WM_COMPLETE;
	static const DWORD WM_PROGRESS = ExportNotifierImpl::WM_PROGRESS;

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_iTunesLabel.SubclassWindow(GetDlgItem(IDC_ITUNES));
		m_logListBox.SubclassWindow(GetDlgItem(IDC_LOGS));

		m_logger = NULL;
		m_notifier = NULL;
		m_exporter = NULL;

		m_itemClicked = -2;

		// Init the CDialogResize code
		DlgResize_Init();

		InitializeSessionList();

		m_notifier = new ExportNotifierImpl(m_hWnd);
		m_logger = new LoggerImpl(GetDlgItem(IDC_LOGS));

		BOOL descOrder = FALSE;
		BOOL savingInSession = TRUE;
		TCHAR szOutput[MAX_PATH] = { 0 };
		BOOL outputDirFound = FALSE;
#ifndef NDEBUG
		TCHAR szPrevBackup[MAX_PATH] = { 0 };
#endif

		CRegKey rk;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), KEY_READ) == ERROR_SUCCESS)
		{
			descOrder = GetDescOrder(rk);
			savingInSession = GetSavingInSession(rk);
			ULONG chars = MAX_PATH;
			if (rk.QueryStringValue(TEXT("OutputDir"), szOutput, &chars) == ERROR_SUCCESS)
			{
				outputDirFound = TRUE;
			}
#ifndef NDEBUG
			chars = MAX_PATH;
			rk.QueryStringValue(TEXT("BackupDir"), szPrevBackup, &chars);
#endif
			rk.Close();
		}

		BOOL iTunesInstalled = FALSE;
		TCHAR iTunesVersion[MAX_PATH] = { 0 };
		CRegKey rkITunes;
		if (rkITunes.Open(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Apple Computer, Inc.\\iTunes"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rkITunes.QueryStringValue(TEXT("Version"), iTunesVersion, &chars) == ERROR_SUCCESS)
			{
				iTunesInstalled = TRUE;
			}
			rkITunes.Close();
		}

		CString iTunesStatus;
		iTunesInstalled ? iTunesStatus.Format(IDS_ITUNES_VERSION, iTunesVersion) : iTunesStatus.LoadString(IDS_ITUNES_NOT_INSTALLED);
		m_iTunesLabel.SetWindowText(iTunesStatus);
		// iTunesInstalled ? m_iTunesLabel.SetNormalColors(RGB(0, 0xFF, 0), RGB(0xFF, 0xFF, 0xFF)) : m_iTunesLabel.SetNormalColors(RGB(0xFF, 0, 0), CLR_INVALID);
		iTunesInstalled ? m_iTunesLabel.SetNormalColors(RGB(0x32, 0xCD, 0x32), CLR_INVALID) : m_iTunesLabel.SetNormalColors(RGB(0xFF, 0, 0), CLR_INVALID);
		// m_iTunesLabel.

		HRESULT result = S_OK;
		if (!outputDirFound)
		{
			result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, szOutput);
		}
		SetDlgItemText(IDC_OUTPUT, szOutput);

		TCHAR szPath[MAX_PATH] = { 0 };
		// Check iTunes Folder
		result = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath);
		_tcscat(szPath, TEXT("\\Apple Computer\\MobileSync\\Backup"));

		CStatic label = GetDlgItem(IDC_STATIC_BACKUP);
		CString text;
		text.Format(IDS_STATIC_BACKUP, szPath);
		label.SetWindowText(text);

		DWORD dwAttrib = ::GetFileAttributes(szPath);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		{
			CW2A backupDir(CT2W(szPath), CP_UTF8);

			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			std::vector<BackupManifest> manifests;
			if (parser.parse(manifests))
			{
				UpdateBackups(manifests);
			}
			
		}
#ifndef NDEBUG
		else if (_tcslen(szPrevBackup) != 0)
		{
			CW2A backupDir(CT2W(szPrevBackup), CP_UTF8);
			
			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			std::vector<BackupManifest> manifests;
			if (parser.parse(manifests))
			{
				UpdateBackups(manifests);
			}
		}
#endif
		
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
		COMMAND_HANDLER(IDC_BACKUP, CBN_SELCHANGE, OnBackupSelChange)
		COMMAND_HANDLER(IDC_CHOOSE_BKP, BN_CLICKED, OnBnClickedChooseBkp)
		COMMAND_HANDLER(IDC_CHOOSE_OUTPUT, BN_CLICKED, OnBnClickedChooseOutput)
		COMMAND_HANDLER(IDC_USERS, CBN_SELCHANGE, OnUserSelChange)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, OnBnClickedExport)
		COMMAND_HANDLER(IDC_CANCEL, BN_CLICKED, OnBnClickedCancel)
		COMMAND_HANDLER(IDC_CLOSE, BN_CLICKED, OnBnClickedClose)
		MESSAGE_HANDLER(WM_KEYUP, OnKeyUp)
		MESSAGE_HANDLER(WM_START, OnStart)
		MESSAGE_HANDLER(WM_COMPLETE, OnComplete)
		NOTIFY_HANDLER(IDC_SESSIONS, LVN_ITEMCHANGING, OnListItemChanging)
		NOTIFY_HANDLER(IDC_SESSIONS, LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_CODE_HANDLER(HDN_ITEMSTATEICONCLICK, OnHeaderItemStateIconClick)
		NOTIFY_HANDLER(IDC_SESSIONS, NM_CLICK, OnListClick)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CView)
		DLGRESIZE_CONTROL(IDC_ITUNES, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_CHOOSE_BKP, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_BACKUP, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_CHOOSE_OUTPUT, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_OUTPUT, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_GRP_USR_CHAT, DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_SESSIONS, DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_GRP_LOGS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_LOGS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_PROGRESS, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CLOSE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
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
		if (IDOK == folder.DoModal())
		{
			CT2A backupDir(folder.m_szFolderPath, CP_UTF8);

			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			std::vector<BackupManifest> manifests;
			if (parser.parse(manifests))
			{
				UpdateBackups(manifests);
#ifndef NDEBUG
				CRegKey rk;
				if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
				{
					rk.SetStringValue(TEXT("BackupDir"), folder.m_szFolderPath);
				}
#endif
			}
			else
			{
				MsgBox(IDS_FAILED_TO_LOAD_BKP);
			}
		}

		return 0;
	}

	LRESULT OnBackupSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		cbmBox.ResetContent();
		
		m_usersAndSessions.clear();
		cbmBox = GetDlgItem(IDC_BACKUP);
		if (cbmBox.GetCurSel() == -1)
		{
			CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
			listViewCtrl.SetRedraw(FALSE);
			listViewCtrl.DeleteAllItems();
			listViewCtrl.SetRedraw(TRUE);

			return 0;
		}

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
			listViewCtrl.SetRedraw(FALSE);
			listViewCtrl.DeleteAllItems();
			listViewCtrl.SetRedraw(TRUE);

			MsgBox(IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		CWaitCursor waitCursor;
#ifndef NDEBUG
		m_logger->write("Start loading users and sessions.");
#endif

		std::string backup = m_manifests[cbmBox.GetCurSel()].getPath();
		Exporter exp("", backup, "", &m_shell, m_logger);
		exp.loadUsersAndSessions(m_usersAndSessions);

#ifndef NDEBUG
		m_logger->write("Data Loaded.");
#endif

		LoadUsers();

#ifndef NDEBUG
		m_logger->write("Display Completed.");
#endif
		return 0;
	}

	LRESULT OnUserSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		if (cbmBox.GetCurSel() == -1)
		{
			listViewCtrl.DeleteAllItems();
			return 0;
		}

#ifndef NDEBUG
		m_logger->debug("Display Sessions Start");
#endif

		std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin() + cbmBox.GetCurSel();
		listViewCtrl.SetRedraw(FALSE);
		listViewCtrl.DeleteAllItems();
		LoadSessions(it->first.getUsrName());
		listViewCtrl.SetRedraw(TRUE);
#ifndef NDEBUG
		m_logger->debug("Display Sessions End");
#endif
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
		
		if (IDOK == folder.DoModal())
		{
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
		if (MsgBox(IDS_CANCEL_PROMPT, MB_OKCANCEL) == IDCANCEL)
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

	LRESULT OnBnClickedClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		::PostMessage(GetParent(), WM_CLOSE, 0, 0);
		return 0;
	}

	LRESULT OnHeaderItemStateIconClick(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
		HWND header = ListView_GetHeader(listViewCtrl);
		
		// Store the ID of the header control so we can handle its notification by ID
		if (idCtrl == ::GetDlgCtrlID(header))
		{
			LPNMHEADER pnmHeader = (LPNMHEADER)pnmh;

			if (pnmHeader->pitem->mask & HDI_FORMAT && pnmHeader->pitem->fmt & HDF_CHECKBOX)
			{
				CheckAllItems(!(pnmHeader->pitem->fmt & HDF_CHECKED));
				SyncHeaderCheckbox();
				return 1;
			}
		}

		return 0;
	}

	LRESULT OnListItemChanging(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)pnmh;

		if (pnmlv->uChanged & LVIF_STATE)
		{
			return IsUIEnabled() ? FALSE : TRUE;
		}

		return 0; // FALSE
	}

	LRESULT OnListItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		LPNMLISTVIEW pnmlv = (LPNMLISTVIEW)pnmh;

		if (pnmlv->uChanged & LVIF_STATE)
		{
			if (pnmlv->iItem == m_itemClicked)
			{
				SyncHeaderCheckbox();
				m_itemClicked = -2;
			}
			
		}
		return 0;
	}

	LRESULT OnListClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
	{
		LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)pnmh;
		DWORD pos = GetMessagePos();
		POINT pt = { LOWORD(pos), HIWORD(pos) };
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		listViewCtrl.ScreenToClient(&pt);
		UINT flags = 0;
		int nItem = listViewCtrl.HitTest(pt, &flags);
		if (flags == LVHT_ONITEMSTATEICON)
		{
			m_itemClicked = nItem;
			// listViewCtrl.SetCheckState(nItem, !listViewCtrl.GetCheckState(nItem));
			// SetHeaderCheckbox();
			// bHandled = TRUE;
		}
		
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
			MsgBox(IDS_SEL_BACKUP_DIR);
			return 0;
		}

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			MsgBox(IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		std::string backup = manifest.getPath();

		TCHAR buffer[MAX_PATH] = { 0 };
		GetDlgItemText(IDC_OUTPUT, buffer, MAX_PATH);
		if (!::PathFileExists(buffer))
		{
			MsgBox(IDS_INVALID_OUTPUT_DIR);
			return 0;
		}
		CW2A output(CT2W(buffer), CP_UTF8);

		DWORD dwRet = GetCurrentDirectory(MAX_PATH, buffer);
		if (dwRet == 0)
		{
			// printf("GetCurrentDirectory failed (%d)\n", GetLastError());
			return 0;
		}

		// CButton btn = GetDlgItem(IDC_DESC_ORDER);
		bool descOrder = GetDescOrder();
		bool saveFilesInSessionFolder = GetSavingInSession();

		CListBox lstboxLogs = GetDlgItem(IDC_LOGS);
		lstboxLogs.ResetContent();

		CW2A resDir(CT2W(buffer), CP_UTF8);
	
		// Run((LPCSTR)resDir, backup, (LPCSTR)output, descOrder, saveFilesInSessionFolder);

		// void filterUsersAndSessions(const std::map<std::string, std::set<std::string>>& usersAndSessions);
		std::map<std::string, std::set<std::string>> usersAndSessions;
		cbmBox = GetDlgItem(IDC_USERS);
		int nCurSel = cbmBox.GetCurSel();
		if (nCurSel != -1 && nCurSel < m_usersAndSessions.size())
		{
			std::pair<Friend, std::vector<Session>>& user = m_usersAndSessions[nCurSel];
			std::string usrName = user.first.getUsrName();
			usersAndSessions[usrName] = std::set<std::string>();
			std::map<std::string, std::set<std::string>>::iterator it = usersAndSessions.find(usrName);
			ATLASSERT(it != usersAndSessions.end());

			CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
			for (int nItem = 0; nItem < listViewCtrl.GetItemCount(); nItem++)
			{
				if (listViewCtrl.GetCheckState(nItem) && nItem < user.second.size())
				{
					it->second.insert(user.second[nItem].getUsrName());
				}
			}
		}

		m_exporter = new Exporter((LPCSTR)resDir, backup, (LPCSTR)output, &m_shell, m_logger);
		m_exporter->setNotifier(m_notifier);
		m_exporter->setOrder(!descOrder);
		if (saveFilesInSessionFolder)
		{
			m_exporter->saveFilesInSessionFolder();
		}
		m_exporter->filterUsersAndSessions(usersAndSessions);
		if (m_exporter->run())
		{
			EnableInteractiveCtrls(FALSE);
		}

		return 0;
	}
	
	LRESULT OnKeyUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		EnableInteractiveCtrls(FALSE);
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.SetMarquee(TRUE, 0);
		return 0;
	}

	LRESULT OnStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		EnableInteractiveCtrls(FALSE);
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.SetMarquee(TRUE, 0);
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
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.SetMarquee(FALSE, 0);
		EnableInteractiveCtrls(TRUE);
		return 0;
	}

	void EnableInteractiveCtrls(BOOL enabled)
	{
		::EnableWindow(GetDlgItem(IDC_BACKUP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_BKP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_OUTPUT), enabled);
		::EnableWindow(GetDlgItem(IDC_DESC_ORDER), enabled);
		::EnableWindow(GetDlgItem(IDC_FILES_IN_SESSION), enabled);
		::EnableWindow(GetDlgItem(IDC_EXPORT), enabled);
		::EnableWindow(GetDlgItem(IDC_USERS), enabled);
		// ::EnableWindow(GetDlgItem(IDC_SESSIONS), enabled);
		::EnableWindow(GetDlgItem(IDC_CANCEL), !enabled);
		::ShowWindow(GetDlgItem(IDC_CANCEL), enabled ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_CLOSE), enabled ? SW_SHOW : SW_HIDE);
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
			cmb.SetRedraw(FALSE);
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
				SetComboBoxCurSel(cmb, selectedIndex);
			}
			cmb.SetRedraw(TRUE);
		}
	}

	void InitializeSessionList()
	{
		CString strColumn1;
		CString strColumn2;
		CString strColumn3;

		strColumn1.LoadString(IDS_SESSION_NAME);
		strColumn2.LoadString(IDS_SESSION_COUNT);
		strColumn3.LoadString(IDS_SESSION_USER);

		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		DWORD dwStyle = listViewCtrl.GetExStyle();
		dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES;
		listViewCtrl.SetExtendedListViewStyle(dwStyle);

		LVCOLUMN lvc = { 0 };
		ListView_InsertColumn(listViewCtrl, 0, &lvc);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn1;
		lvc.cx = 192;
		ListView_InsertColumn(listViewCtrl, 1, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn2;
		lvc.cx = 96;
		ListView_InsertColumn(listViewCtrl, 2, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn3;
		lvc.cx = 128;
		ListView_InsertColumn(listViewCtrl, 3, &lvc);

		// Set column widths
		ListView_SetColumnWidth(listViewCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 3, LVSCW_AUTOSIZE_USEHEADER);


		HWND header = ListView_GetHeader(listViewCtrl);
		DWORD dwHeaderStyle = ::GetWindowLong(header, GWL_STYLE);
		dwHeaderStyle |= HDS_CHECKBOXES;
		::SetWindowLong(header, GWL_STYLE, dwHeaderStyle);

		// Store the ID of the header control so we can handle its notification by ID
		// m_HeaderId = ::GetDlgCtrlID(header);
	
		HDITEM hdi = { 0 };
		hdi.mask = HDI_FORMAT;
		Header_GetItem(header, 0, &hdi);
		hdi.fmt |= HDF_CHECKBOX | HDF_FIXEDWIDTH;
		Header_SetItem(header, 0, &hdi);
	}
	
	void LoadUsers()
	{
		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		for (std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin(); it != m_usersAndSessions.cend(); ++it)
		{
			std::string displayName = it->first.getDisplayName();
			CW2T pszDisplayName(CA2W(displayName.c_str(), CP_UTF8));
			cbmBox.AddString(pszDisplayName);
		}
		if (cbmBox.GetCount() > 0)
		{
			SetComboBoxCurSel(cbmBox, 0);
		}
	}

	void LoadSessions(const std::string& usrName)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		int index = 0;
		TCHAR recordCount[16] = { 0 };
		for (std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin(); it != m_usersAndSessions.cend(); ++it)
		{
			if (!usrName.empty() && it->first.getUsrName() != usrName)
			{
				index += it->second.size();
				continue;
			}

			std::string userDisplayName = it->first.getDisplayName();
			CW2T pszUserDisplayName(CA2W(userDisplayName.c_str(), CP_UTF8));

			for (std::vector<Session>::const_iterator it2 = it->second.cbegin(); it2 != it->second.cend(); ++it2)
			{
				std::string displayName = it2->getDisplayName();
				if (displayName.empty())
				{
					displayName = it2->getUsrName();
				}

				CW2T pszDisplayName(CA2W(displayName.c_str(), CP_UTF8));
				LVITEM lvItem = {};
				lvItem.mask = LVIF_TEXT | LVIF_PARAM;
				lvItem.iItem = listViewCtrl.GetItemCount();
				lvItem.iSubItem = 0;
				lvItem.pszText = TEXT("");
				// lvItem.state = INDEXTOSTATEIMAGEMASK(2);
				// lvItem.stateMask = LVIS_STATEIMAGEMASK;
				lvItem.lParam = reinterpret_cast<LPARAM>(&(*it2));
				int nItem = listViewCtrl.InsertItem(&lvItem);

				_itot(it2->getRecordCount(), recordCount, 10);
				listViewCtrl.AddItem(nItem, 1, pszDisplayName);
				listViewCtrl.AddItem(nItem, 2, recordCount);
				listViewCtrl.AddItem(nItem, 3, pszUserDisplayName);
				// BOOL bRet = listViewCtrl.SetItem(&lvSubItem);
				listViewCtrl.SetCheckState(nItem, TRUE);
				
				// listViewCtrl.SetItemData(nItem, static_cast<DWORD_PTR>(index));

				++index;
			}
		}

		SetHeaderCheckState(TRUE);
	}

	void SetComboBoxCurSel(CComboBox &cbm, int nCurSel)
	{
		cbm.SetCurSel(nCurSel);
		PostMessage(WM_COMMAND, MAKEWPARAM(cbm.GetDlgCtrlID(), CBN_SELCHANGE), LPARAM(cbm.m_hWnd));
	}

	void CheckAllItems(BOOL fChecked)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
		for (int nItem = 0; nItem < ListView_GetItemCount(listViewCtrl); nItem++)
		{
			ListView_SetCheckState(listViewCtrl, nItem, fChecked);
		}
	}

	void SyncHeaderCheckbox()
	{
		// Loop through all of our items.  If any of them are
		// unchecked, we'll want to uncheck the header checkbox.
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
		BOOL fChecked = TRUE;
		for (int nItem = 0; nItem < ListView_GetItemCount(listViewCtrl); nItem++)
		{
			if (!ListView_GetCheckState(listViewCtrl, nItem))
			{
				fChecked = FALSE;
				break;
			}
		}

		SetHeaderCheckState(fChecked);
	}

	void SetHeaderCheckState(BOOL fChecked)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		// We need to get the current format of the header
		// and set or remove the HDF_CHECKED flag
		HWND header = ListView_GetHeader(listViewCtrl);
		HDITEM hdi = { 0 };
		hdi.mask = HDI_FORMAT;
		Header_GetItem(header, 0, &hdi);
		if (fChecked) {
			hdi.fmt |= HDF_CHECKED;
		}
		else {
			hdi.fmt &= ~HDF_CHECKED;
		}
		Header_SetItem(header, 0, &hdi);
	}

	BOOL SetHeaderCheckState()
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		HWND header = ListView_GetHeader(listViewCtrl);
		HDITEM hdi = { 0 };
		hdi.mask = HDI_FORMAT;
		Header_GetItem(header, 0, &hdi);
		return (hdi.fmt & HDF_CHECKED) == HDF_CHECKED ? TRUE : FALSE;
	}

	void SetDescOrder(BOOL descOrder)
	{
		CRegKey rk;
		if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
		{
			rk.SetDWORDValue(TEXT("DescOrder"), descOrder);
		}
	}

	BOOL GetDescOrder() const
	{
		BOOL descOrder = FALSE;
		CRegKey rk;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), KEY_READ) == ERROR_SUCCESS)
		{
			descOrder = GetDescOrder(rk);
			rk.Close();
		}

		return descOrder;
	}

	void SetSavingInSession(BOOL savingInSession)
	{
		CRegKey rk;
		if (rk.Create(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
		{

			rk.SetDWORDValue(TEXT("SaveFilesInSF"), savingInSession);
		}
	}

	BOOL GetSavingInSession() const
	{
		CRegKey rk;
		if (rk.Open(HKEY_CURRENT_USER, TEXT("Software\\WechatExporter"), KEY_READ) == ERROR_SUCCESS)
		{
			BOOL savingInSession = GetSavingInSession(rk);
			rk.Close();

			return savingInSession;
		}

		return TRUE;
	}

	BOOL IsUIEnabled() const
	{
		return ::IsWindowEnabled(GetDlgItem(IDC_EXPORT));
	}

private:
	BOOL GetDescOrder(CRegKey& rk) const
	{
		BOOL descOrder = FALSE;
		DWORD dwValue = 0;
		if (rk.QueryDWORDValue(TEXT("DescOrder"), dwValue) == ERROR_SUCCESS)
		{
			descOrder = dwValue != 0 ? TRUE : FALSE;
		}

		return descOrder;
	}

	BOOL GetSavingInSession(CRegKey& rk) const
	{
		DWORD dwValue = 1;
		if (rk.QueryDWORDValue(TEXT("SaveFilesInSF"), dwValue) == ERROR_SUCCESS)
		{
			return (dwValue != 0) ? TRUE : FALSE;
		}

		return TRUE;
	}

	int MsgBox(UINT uStdId, UINT uType = MB_OK)
	{
		CString text;
		text.LoadString(uStdId);
		return MsgBox(text, uType);
	}

	int MsgBox(const CString& text, UINT uType = MB_OK)
	{
		CString caption;
		caption.LoadString(IDR_MAINFRAME);
		return MessageBox(text, caption, uType);
	}

};
