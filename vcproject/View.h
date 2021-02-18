// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <future>
#include <chrono>
#include "Core.h"
#include "LoggerImpl.h"
#include "ShellImpl.h"
#include "ExportNotifierImpl.h"
// #include "ColoredControls.h"
#include "LogListBox.h"
// #include "ITunesDetector.h"
#include "AppConfiguration.h"

class CView : public CDialogImpl<CView>, public CDialogResize<CView>
{
private:
	// CColoredComboBoxCtrl	m_cbmBoxBackups;
	// CColoredComboBoxCtrl	m_cbmBoxUsers;
	CLogListBox				m_logListBox;
	CSortListViewCtrl		m_sessionsListCtrl;

	ShellImpl			m_shell;
	LoggerImpl*			m_logger;
	ExportNotifierImpl* m_notifier;
	Exporter*			m_exporter;

	std::vector<BackupManifest> m_manifests;
	std::vector<std::pair<Friend, std::vector<Session>>> m_usersAndSessions;

	int m_itemClicked;

	class CLoadingHandler : public CIdleHandler
	{
	protected:
		HWND m_hWnd;
		std::future<bool> m_task;
		Exporter m_exp;
		CWaitCursor m_waitCursor;
	public:

		CLoadingHandler(HWND hWnd, const std::string& resDir, const std::string& backupDir, Shell* shell, Logger* logger) : m_hWnd(hWnd), m_exp(resDir, backupDir, "", shell, logger)
		{
			m_task = std::async(std::launch::async, &Exporter::loadUsersAndSessions, &m_exp);
		}

		~CLoadingHandler()
		{
		}

		virtual BOOL OnIdle()
		{
			std::future_status status = m_task.wait_for(std::chrono::seconds(0));
			if (status == std::future_status::ready)
			{
				::PostMessage(m_hWnd, WM_LOADDATA, 0, reinterpret_cast<LPARAM>(this));
				return TRUE;
			}

			return FALSE;
		}

		void getUsersAndSessions(std::vector<std::pair<Friend, std::vector<Session>>>& usersAndSessions)
		{
			m_exp.swapUsersAndSessions(usersAndSessions);
		}

	};

public:
	enum { IDD = IDD_WECHATEXPORTER_FORM };

	static const DWORD WM_START = ExportNotifierImpl::WM_START;
	static const DWORD WM_COMPLETE = ExportNotifierImpl::WM_COMPLETE;
	static const DWORD WM_PROGRESS = ExportNotifierImpl::WM_PROGRESS;
	static const DWORD WM_LOADDATA = WM_PROGRESS + 1;

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_logListBox.SubclassWindow(GetDlgItem(IDC_LOGS));
		// m_cbmBoxBackups.SubclassWindow(GetDlgItem(IDC_BACKUP));
		// m_cbmBoxUsers.SubclassWindow(GetDlgItem(IDC_USERS));

		// m_cbmBoxBackups.SetEditColors(CLR_INVALID, ::GetSysColor(COLOR_WINDOWTEXT));
		// m_cbmBoxUsers.SetEditColors(CLR_INVALID, ::GetSysColor(COLOR_WINDOWTEXT));

		m_logger = NULL;
		m_notifier = NULL;
		m_exporter = NULL;

		m_itemClicked = -2;

		// Init the CDialogResize code
		DlgResize_Init();

		InitializeSessionList();

		m_notifier = new ExportNotifierImpl(m_hWnd);
		m_logger = new LoggerImpl(GetDlgItem(IDC_LOGS));

		CString backupDir = AppConfiguration::GetDefaultBackupDir(FALSE);
		CString text;
		text.Format(IDS_STATIC_BACKUP, (LPCTSTR)backupDir);
		CStatic label = GetDlgItem(IDC_STATIC_BACKUP);
		label.SetWindowText(text);

		SetDlgItemText(IDC_OUTPUT, AppConfiguration::GetLastOrDefaultOutputDir());

		backupDir = AppConfiguration::GetDefaultBackupDir();
#ifndef NDEBUG
		CString lastBackupDir = AppConfiguration::GetLastBackupDir();
#endif

		std::vector<BackupManifest> manifests;
#ifndef NDEBUG
		if (!lastBackupDir.IsEmpty() && lastBackupDir != backupDir)
		{
			CW2A backupDir(CT2W(lastBackupDir), CP_UTF8);
			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			parser.parse(manifests);
		}
#endif
		if (!backupDir.IsEmpty())
		{
			CW2A backupDir(CT2W(backupDir), CP_UTF8);
			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			parser.parse(manifests);
		}

		if (!manifests.empty())
		{
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
		COMMAND_HANDLER(IDC_BACKUP, CBN_SELCHANGE, OnBackupSelChange)
		COMMAND_HANDLER(IDC_CHOOSE_BKP, BN_CLICKED, OnBnClickedChooseBkp)
		COMMAND_HANDLER(IDC_CHOOSE_OUTPUT, BN_CLICKED, OnBnClickedChooseOutput)
		COMMAND_HANDLER(IDC_USERS, CBN_SELCHANGE, OnUserSelChange)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, OnBnClickedExport)
		COMMAND_HANDLER(IDC_CANCEL, BN_CLICKED, OnBnClickedCancel)
		COMMAND_HANDLER(IDC_CLOSE, BN_CLICKED, OnBnClickedClose)
		MESSAGE_HANDLER(WM_START, OnStart)
		MESSAGE_HANDLER(WM_COMPLETE, OnComplete)
		MESSAGE_HANDLER(WM_PROGRESS, OnProgress)
		MESSAGE_HANDLER(WM_LOADDATA, OnLoadData)
		NOTIFY_HANDLER(IDC_SESSIONS, LVN_ITEMCHANGING, OnListItemChanging)
		NOTIFY_HANDLER(IDC_SESSIONS, LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_CODE_HANDLER(HDN_ITEMSTATEICONCLICK, OnHeaderItemStateIconClick)
		NOTIFY_HANDLER(IDC_SESSIONS, NM_CLICK, OnListClick)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	BEGIN_DLGRESIZE_MAP(CView)
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

	LRESULT OnLoadData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CLoadingHandler *handler = reinterpret_cast<CLoadingHandler *>(lParam);
		if (NULL != handler)
		{
			if (_Module.GetMessageLoop()->RemoveIdleHandler(handler))
			{
				// exp.loadUsersAndSessions();
				handler->getUsersAndSessions(m_usersAndSessions);

				LoadUsers();
				EnableInteractiveCtrls(TRUE, FALSE);

				delete handler;
			}
			// If the handler is not in array, throw it away
		}

		return 0;
	}

	LRESULT OnBnClickedChooseBkp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CString text;
		text.LoadString(IDS_SEL_BACKUP_DIR);

		CFolderDialog folder(NULL, text, BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NONEWFOLDERBUTTON);
		if (IDOK == folder.DoModal())
		{
			CW2A backupDir(CT2W(folder.m_szFolderPath), CP_UTF8);

			ManifestParser parser((LPCSTR)backupDir, &m_shell);
			std::vector<BackupManifest> manifests;
			if (parser.parse(manifests) && !manifests.empty())
			{
				UpdateBackups(manifests);
#ifndef NDEBUG
				AppConfiguration::SetLastBackupDir(folder.m_szFolderPath);
#endif
			}
			else
			{
#ifndef NDEBUG
				m_logger->debug(parser.getLastError());
#endif
				MsgBox(IDS_FAILED_TO_LOAD_BKP);
			}
		}

		return 0;
	}

	LRESULT OnBackupSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CListBox lstboxLogs = GetDlgItem(IDC_LOGS);
		lstboxLogs.ResetContent();

		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		cbmBox.ResetContent();

		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
		// listViewCtrl.SetRedraw(FALSE);
		listViewCtrl.DeleteAllItems();
		// listViewCtrl.SetRedraw(TRUE);

		m_usersAndSessions.clear();
		cbmBox = GetDlgItem(IDC_BACKUP);
		if (cbmBox.GetCurSel() == -1)
		{
			return 0;
		}

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			MsgBox(IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		
#ifndef NDEBUG
		m_logger->write("Start loading users and sessions.");
#endif
		TCHAR buffer[MAX_PATH] = { 0 };
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, buffer);
		if (dwRet == 0)
		{
			return 0;
		}
		CW2A resDir(CT2W(buffer), CP_UTF8);

		EnableInteractiveCtrls(FALSE, FALSE);
		
		std::string backup = manifest.getPath();
		CLoadingHandler *handler = new CLoadingHandler(m_hWnd, (LPCSTR)resDir, backup, &m_shell, m_logger);
		_Module.GetMessageLoop()->AddIdleHandler(handler);
		// Exporter exp((LPCSTR)resDir, backup, "", &m_shell, m_logger);

		return 0;
	}

	LRESULT OnUserSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		int curSel = cbmBox.GetCurSel();
		if (curSel == -1)
		{
			listViewCtrl.DeleteAllItems();
			return 0;
		}

#ifndef NDEBUG
		m_logger->debug("Display Sessions Start");
#endif

		BOOL allUsers = (curSel == 0);
		std::string usrName;
		if (curSel > 0)
		{
			std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin() + curSel - 1;
			usrName = it->first.getUsrName();
		}
		
		listViewCtrl.SetRedraw(FALSE);
		if (listViewCtrl.GetItemCount() > 0)
		{
			listViewCtrl.DeleteAllItems();
		}
		LoadSessions(allUsers, usrName);
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
			AppConfiguration::SetLastOutputDir(folder.m_szFolderPath);
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
		bool descOrder = AppConfiguration::GetDescOrder();
		bool saveFilesInSessionFolder = AppConfiguration::GetSavingInSession();
		bool asyncLoading = AppConfiguration::GetAsyncLoading();
		UINT outputFormat = AppConfiguration::GetOutputFormat();

		CListBox lstboxLogs = GetDlgItem(IDC_LOGS);
		lstboxLogs.ResetContent();

		CW2A resDir(CT2W(buffer), CP_UTF8);

		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);
		std::map<std::string, std::set<std::string>> usersAndSessions;
		for (int nItem = 0; nItem < listViewCtrl.GetItemCount(); nItem++)
		{
			if (!listViewCtrl.GetCheckState(nItem))
			{
				continue;
			}

			const Session* session = reinterpret_cast<const Session*>(listViewCtrl.GetItemData(nItem));
			if (NULL != session)
			{
				std::string usrName = session->getOwner()->getUsrName();
				std::map<std::string, std::set<std::string>>::iterator it = usersAndSessions.find(usrName);
				if (it == usersAndSessions.end())
				{
					it = usersAndSessions.insert(usersAndSessions.end(), std::pair<std::string, std::set<std::string>>(usrName, std::set<std::string>()));
				}

				it->second.insert(session->getUsrName());
			}
		}

		m_exporter = new Exporter((LPCSTR)resDir, backup, (LPCSTR)output, &m_shell, m_logger);
		m_exporter->setNotifier(m_notifier);
		m_exporter->setOrder(!descOrder);
		m_exporter->setSyncLoading(!asyncLoading);
		if (saveFilesInSessionFolder)
		{
			m_exporter->saveFilesInSessionFolder();
		}
		if (outputFormat == AppConfiguration::OUTPUT_FORMAT_TEXT)
		{
			m_exporter->setTextMode();
			m_exporter->setExtName("txt");
			m_exporter->setTemplatesName("templates_txt");
		}
		m_exporter->filterUsersAndSessions(usersAndSessions);
		if (m_exporter->run())
		{
			EnableInteractiveCtrls(FALSE, TRUE);
		}

		return 0;
	}
	
	LRESULT OnStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		BOOL cancellable = static_cast<BOOL>(lParam);
		EnableInteractiveCtrls(FALSE, cancellable);
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.ModifyStyle(0, PBS_MARQUEE);
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
		progressCtrl.ModifyStyle(PBS_MARQUEE, 0);
		progressCtrl.SetMarquee(FALSE, 0);
		progressCtrl.SetPos(0);
		EnableInteractiveCtrls(TRUE);
		return 0;
	}

	LRESULT OnProgress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	BOOL IsUIEnabled() const
	{
		return ::IsWindowEnabled(GetDlgItem(IDC_EXPORT));
	}

protected:

	void EnableInteractiveCtrls(BOOL enabled, BOOL cancellable = TRUE)
	{
		::EnableWindow(GetDlgItem(IDC_BACKUP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_BKP), enabled);
		::EnableWindow(GetDlgItem(IDC_CHOOSE_OUTPUT), enabled);
		::EnableWindow(GetDlgItem(IDC_DESC_ORDER), enabled);
		::EnableWindow(GetDlgItem(IDC_FILES_IN_SESSION), enabled);
		::EnableWindow(GetDlgItem(IDC_EXPORT), enabled);
		::EnableWindow(GetDlgItem(IDC_USERS), enabled);
		// ::EnableWindow(GetDlgItem(IDC_SESSIONS), enabled);
		// if (!enabled && cancellable)
		::EnableWindow(GetDlgItem(IDC_CANCEL), !enabled && cancellable);
		::ShowWindow(GetDlgItem(IDC_CANCEL), enabled ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_CLOSE), enabled ? SW_SHOW : SW_HIDE);
	}

	void UpdateBackups(const std::vector<BackupManifest>& manifests)
	{
		if (manifests.empty())
		{
			return;
		}

		int selectedIndex = -1;
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
		cmb.ResetContent();
		for (std::vector<BackupManifest>::const_iterator it = m_manifests.cbegin(); it != m_manifests.cend(); ++it)
		{
			std::string itemTitle = it->toString();
			CW2T item(CA2W(it->toString().c_str(), CP_UTF8));
			cmb.AddString((LPCTSTR)item);
		}
		cmb.SetRedraw(TRUE);
		if (selectedIndex != -1 && selectedIndex < cmb.GetCount())
		{
			SetComboBoxCurSel(cmb, selectedIndex);
		}
	}

	void InitializeSessionList()
	{
		m_sessionsListCtrl.SubclassWindow(GetDlgItem(IDC_SESSIONS));

		CString strColumn1;
		CString strColumn2;
		CString strColumn3;

		strColumn1.LoadString(IDS_SESSION_NAME);
		strColumn2.LoadString(IDS_SESSION_COUNT);
		strColumn3.LoadString(IDS_SESSION_USER);

		DWORD dwStyle = m_sessionsListCtrl.GetExStyle();
		dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES;
		m_sessionsListCtrl.SetExtendedListViewStyle(dwStyle);

		LVCOLUMN lvc = { 0 };
		ListView_InsertColumn(m_sessionsListCtrl, 0, &lvc);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn1;
		lvc.cx = 192;
		ListView_InsertColumn(m_sessionsListCtrl, 1, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn2;
		lvc.cx = 76;
		ListView_InsertColumn(m_sessionsListCtrl, 2, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn3;
		lvc.cx = 128;
		ListView_InsertColumn(m_sessionsListCtrl, 3, &lvc);

		// Set column widths
		ListView_SetColumnWidth(m_sessionsListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(m_sessionsListCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(listViewCtrl, 3, LVSCW_AUTOSIZE_USEHEADER);
		m_sessionsListCtrl.SetColumnSortType(0, LVCOLSORT_NONE);
		m_sessionsListCtrl.SetColumnSortType(2, LVCOLSORT_LONG);
		m_sessionsListCtrl.SetColumnSortType(3, LVCOLSORT_NONE);

		HWND header = ListView_GetHeader(m_sessionsListCtrl);
		DWORD dwHeaderStyle = ::GetWindowLong(header, GWL_STYLE);
		dwHeaderStyle |= HDS_CHECKBOXES;
		::SetWindowLong(header, GWL_STYLE, dwHeaderStyle);

		HDITEM hdi = { 0 };
		hdi.mask = HDI_FORMAT;
		Header_GetItem(header, 0, &hdi);
		hdi.fmt |= HDF_CHECKBOX | HDF_FIXEDWIDTH;
		Header_SetItem(header, 0, &hdi);
	}
	
	void LoadUsers()
	{
		CComboBox cbmBox = GetDlgItem(IDC_USERS);
		
		if (!m_usersAndSessions.empty())
		{
			CString text;
			text.LoadString(IDS_ALL_USERS);
			cbmBox.AddString(text);
#ifndef NDEBUG
			std::string log = std::to_string(m_usersAndSessions.size()) + " users";
			m_logger->debug(log);
#endif
		}
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

	void LoadSessions(BOOL allUsers, const std::string& usrName)
	{
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		TCHAR recordCount[16] = { 0 };
		for (std::vector<std::pair<Friend, std::vector<Session>>>::const_iterator it = m_usersAndSessions.cbegin(); it != m_usersAndSessions.cend(); ++it)
		{
			if (!allUsers)
			{
				if (it->first.getUsrName() != usrName)
				{
					continue;
				}
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
				int idx = std::distance(it->second.cbegin(), it2);
				LPARAM lParam = reinterpret_cast<LPARAM>(&(*it2));
				lvItem.lParam = lParam;
				int nItem = listViewCtrl.InsertItem(&lvItem);

				_itot(it2->getRecordCount(), recordCount, 10);
				listViewCtrl.AddItem(nItem, 1, pszDisplayName);
				listViewCtrl.AddItem(nItem, 2, recordCount);
				listViewCtrl.AddItem(nItem, 3, pszUserDisplayName);
				// BOOL bRet = listViewCtrl.SetItem(&lvSubItem);
				listViewCtrl.SetCheckState(nItem, TRUE);
			}
		}

		SetHeaderCheckState(TRUE);
	}

	void SetComboBoxCurSel(CComboBox &cbm, int nCurSel)
	{
		cbm.SetCurSel(nCurSel);
		int nID = cbm.GetDlgCtrlID();
		PostMessage(WM_COMMAND, MAKEWPARAM(nID, CBN_SELCHANGE), LPARAM(cbm.m_hWnd));
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

	int MsgBox(UINT uStrId, UINT uType = MB_OK)
	{
		CString text;
		text.LoadString(uStrId);
		return MsgBox(text, uType);
	}

	int MsgBox(const CString& text, UINT uType = MB_OK)
	{
		CString caption;
		caption.LoadString(IDR_MAINFRAME);
		return MessageBox(text, caption, uType);
	}

};
