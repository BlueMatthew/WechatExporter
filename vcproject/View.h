// View.h : interface of the CView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <future>
#include <chrono>
#include "Core.h"
#include "LoggerImpl.h"
#include "PdfConverterImpl.h"
#include "ExportNotifierImpl.h"
// #include "ColoredControls.h"
#include "LogListBox.h"
#include "ProgressListViewCtrl.h"
#include "AppConfiguration.h"
#include "VersionDetector.h"
#include "ViewHelper.h"

#define SAFE_DELETE(ptr) { delete ptr; ptr = NULL; }

class CView : public CDialogImpl<CView>, public CDialogResize<CView>
{
private:

	enum VIEW_STATE
	{
		VS_IDLE = 0,
		VS_LOADING,
		VS_EXPORTING
	};

	static const int SUBITEM_PROGRESS = 4;
	static const UINT_PTR EVENT_ID_PROGRESS = 1;

	// CColoredComboBoxCtrl	m_cbmBoxBackups;
	// CColoredComboBoxCtrl	m_cbmBoxUsers;
	CLogListBox				m_logListBox;
	CSortListViewCtrl		m_sessionsListCtrl;
	CProgressListViewCtrl	m_progressListCtrl;
	CStatic					m_progressTextCtrl;
	
	LoggerImpl*			m_logger;
	PdfConverterImpl*	m_pdfConverter;
	ExportNotifierImpl* m_notifier;
	Exporter*			m_exporter;

	std::vector<BackupManifest> m_manifests;
	std::vector<std::pair<Friend, std::vector<Session>>> m_usersAndSessions;

	int m_itemClicked;

	VIEW_STATE m_viewState;
	UINT_PTR m_eventIdProgress;

	class CLoadingHandler
	{
	protected:
		HWND m_hWnd;
		std::future<bool> m_task;
		Exporter m_exp;
		CWaitCursor m_waitCursor;

		bool runTask()
		{
			bool ret = m_exp.loadUsersAndSessions();
			::PostMessage(m_hWnd, WM_LOADDATA, 0, reinterpret_cast<LPARAM>(this));
			return ret;
		}
	public:

		CLoadingHandler(HWND hWnd, const std::string& resDir, const std::string& backupDir, Logger* logger) : m_hWnd(hWnd), m_exp(resDir, backupDir, "", logger, NULL)
		{
		}

		~CLoadingHandler()
		{
		}

		void startTask()
		{
			m_task = std::async(std::launch::async, &CLoadingHandler::runTask, this);
		}

		void waitForCompletion()
		{
			m_task.wait();
		}

		void getUsersAndSessions(std::vector<std::pair<Friend, std::vector<Session>>>& usersAndSessions)
		{
			m_exp.swapUsersAndSessions(usersAndSessions);
		}

		CString getVersions() const
		{
			CW2T pszV1(CA2W(m_exp.getITunesVersion().c_str(), CP_UTF8));
			CW2T pszV2(CA2W(m_exp.getIOSVersion().c_str(), CP_UTF8));
			CW2T pszV3(CA2W(m_exp.getWechatVersion().c_str(), CP_UTF8));

			CString versions;
			versions.Format(IDS_VERSIONS, (LPCTSTR)pszV1, (LPCTSTR)pszV2, (LPCTSTR)pszV3);

			return versions;
		}
	};

	class CUpdateHandler : public CIdleHandler
	{
	protected:
		HWND m_hWnd;
		std::future<bool> m_task;
		Updater m_updater;
	public:

		CUpdateHandler(HWND hWnd, const std::string& currentVersion, const std::string& userAgent) : m_hWnd(hWnd), m_updater(currentVersion)
		{
			m_updater.setUserAgent(userAgent);
		}

		~CUpdateHandler()
		{
		}

		void startTask()
		{
			m_task = std::async(std::launch::async, &Updater::checkUpdate, &m_updater);
		}

		virtual BOOL OnIdle()
		{
			std::future_status status = m_task.wait_for(std::chrono::seconds(0));
			if (status == std::future_status::ready)
			{
				::PostMessage(m_hWnd, WM_CHKUPDATE, 1, reinterpret_cast<LPARAM>(this));
				return FALSE;
			}

			return TRUE;
		}

		bool hasNewVersion()
		{
			return m_task.get();
		}

		CString getNewVersion() const
		{
			CW2T pszT(CA2W(m_updater.getNewVersion().c_str(), CP_UTF8));
			return CString(pszT);
		}

		CString getUpdateUrl() const
		{
			CW2T pszT(CA2W(m_updater.getUpdateUrl().c_str(), CP_UTF8));
			return CString(pszT);
		}

	};

public:
	enum { IDD = IDD_WECHATEXPORTER_FORM };

	static const UINT WM_START = ExportNotifierImpl::WM_START;
	static const UINT WM_COMPLETE = ExportNotifierImpl::WM_COMPLETE;
	static const UINT WM_PROGRESS = ExportNotifierImpl::WM_PROGRESS;
	static const UINT WM_USR_SESS_START = ExportNotifierImpl::WM_USR_SESS_START;
	static const UINT WM_USR_SESS_COMPLETE = ExportNotifierImpl::WM_USR_SESS_COMPLETE;
	static const UINT WM_SESSION_START = ExportNotifierImpl::WM_SESSION_START;
	static const UINT WM_SESSION_COMPLETE = ExportNotifierImpl::WM_SESSION_COMPLETE;
	static const UINT WM_SESSION_PROGRESS = ExportNotifierImpl::WM_SESSION_PROGRESS;
	static const UINT WM_TASKS_START = ExportNotifierImpl::WM_TASKS_START;
	static const UINT WM_TASKS_COMPLETE = ExportNotifierImpl::WM_TASKS_COMPLETE;
	static const UINT WM_TASKS_PROGRESS = ExportNotifierImpl::WM_TASKS_PROGRESS;
	static const UINT WM_MSG_START = ExportNotifierImpl::WM_EN_END;
	static const UINT WM_UPD_VIEWSTATE = WM_MSG_START + 1;
	static const UINT WM_LOADDATA = WM_MSG_START + 2;
	static const UINT WM_CHKUPDATE = WM_MSG_START + 3;

	CView() : CDialogImpl<CView>(), CDialogResize<CView>(), m_viewState(VS_IDLE), m_eventIdProgress(0)
	{
	}

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_progressTextCtrl = GetDlgItem(IDC_PROGRESS_TEXT);
		m_logListBox.SubclassWindow(GetDlgItem(IDC_LOGS));
		// m_cbmBoxBackups.SubclassWindow(GetDlgItem(m_exporter));
		// m_cbmBoxUsers.SubclassWindow(GetDlgItem(IDC_USERS));

		// m_cbmBoxBackups.SetEditColors(CLR_INVALID, ::GetSysColor(COLOR_WINDOWTEXT));
		// m_cbmBoxUsers.SetEditColors(CLR_INVALID, ::GetSysColor(COLOR_WINDOWTEXT));
		
		m_logger = NULL;
		m_pdfConverter = NULL;
		m_notifier = NULL;
		m_exporter = NULL;

		m_itemClicked = -2;

		// Init the CDialogResize code
		DlgResize_Init();

		InitializeSessionList();
		InitializeSessionProgressList();

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
		if (!backupDir.IsEmpty())
		{
			CW2A backupDir(CT2W(backupDir), CP_UTF8);
			ManifestParser parser((LPCSTR)backupDir);
			parser.parse(manifests);
		}
#ifndef NDEBUG
		if (!lastBackupDir.IsEmpty() && lastBackupDir != backupDir)
		{
			CW2A backupDir(CT2W(lastBackupDir), CP_UTF8);
			ManifestParser parser((LPCSTR)backupDir);
			parser.parse(manifests);
		}
#endif
		if (!manifests.empty())
		{
			UpdateBackups(manifests);
		}

		if (!AppConfiguration::GetCheckingUpdateDisabled() && (getUnixTimeStamp() - AppConfiguration::GetLastCheckUpdateTime()) > 86400u)
		{
			::PostMessage(m_hWnd, WM_CHKUPDATE, 0, 0);
		}

		return TRUE;
	}

	void OnFinalMessage(HWND hWnd)
	{
		if (NULL != m_exporter)
		{
			m_exporter->cancel();
			m_exporter->waitForComplition();
			SAFE_DELETE(m_exporter);
			m_exporter = NULL;
		}
		SAFE_DELETE(m_notifier);
		SAFE_DELETE(m_pdfConverter);
		SAFE_DELETE(m_logger);

		// override to do something, if needed
	}

	BOOL PreTranslateMessage(MSG* pMsg)
	{
		return CWindow::IsDialogMessage(pMsg);
	}

	BEGIN_MSG_MAP(CView)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDialogResize<CView>)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		COMMAND_HANDLER(IDC_BACKUP, CBN_SELCHANGE, OnBackupSelChange)
		COMMAND_HANDLER(IDC_CHOOSE_BKP, BN_CLICKED, OnBnClickedChooseBkp)
		COMMAND_HANDLER(IDC_CHOOSE_OUTPUT, BN_CLICKED, OnBnClickedChooseOutput)
		COMMAND_HANDLER(IDC_USERS, CBN_SELCHANGE, OnUserSelChange)
		COMMAND_HANDLER(IDC_SHOW_LOGS, BN_CLICKED, OnBnClickedShowLogs)
		COMMAND_HANDLER(IDC_EXPORT, BN_CLICKED, OnBnClickedExport)
		COMMAND_HANDLER(IDC_CANCEL, BN_CLICKED, OnBnClickedCancel)
		COMMAND_HANDLER(IDC_CLOSE, BN_CLICKED, OnBnClickedClose)
		MESSAGE_HANDLER(WM_START, OnStart)
		MESSAGE_HANDLER(WM_COMPLETE, OnComplete)
		MESSAGE_HANDLER(WM_PROGRESS, OnProgress)
		MESSAGE_HANDLER(WM_USR_SESS_START, OnUserSessionStart)
		MESSAGE_HANDLER(WM_USR_SESS_COMPLETE, OnUserSessionComplete)
		MESSAGE_HANDLER(WM_SESSION_START, OnSessionStart)
		MESSAGE_HANDLER(WM_SESSION_COMPLETE, OnSessionComplete)
		MESSAGE_HANDLER(WM_SESSION_PROGRESS, OnSessionProgress)
		MESSAGE_HANDLER(WM_TASKS_START, OnTasksStart)
		MESSAGE_HANDLER(WM_TASKS_COMPLETE, OnTasksComplete)
		MESSAGE_HANDLER(WM_TASKS_PROGRESS, OnTasksProgress)
		MESSAGE_HANDLER(WM_UPD_VIEWSTATE, OnUpdateViewState)
		MESSAGE_HANDLER(WM_LOADDATA, OnLoadData)
		MESSAGE_HANDLER(WM_CHKUPDATE, OnCheckUpdate)
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
		DLGRESIZE_CONTROL(IDC_GRP_USR_CHAT, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_VERSIONS, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_PROGRESS, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_PROGRESS_TEXT, DLSZ_MOVE_X)
		DLGRESIZE_CONTROL(IDC_SESSIONS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_SESS_PROGRESS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_GRP_LOGS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_LOGS, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		DLGRESIZE_CONTROL(IDC_SHOW_LOGS, DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_CLOSE, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDC_EXPORT, DLSZ_MOVE_X | DLSZ_MOVE_Y)
	END_DLGRESIZE_MAP()


// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		UINT_PTR nIDEvent = static_cast<UINT_PTR>(wParam);
		if (nIDEvent == m_eventIdProgress)
		{
			UpdateProgressBar(TRUE);
		}

		return 0;
	}

	LRESULT OnUpdateViewState(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_viewState = static_cast<VIEW_STATE>(wParam);
		if (lParam != 0)
		{
			UpdateUI();
		}

		return 0;
	}

	LRESULT OnLoadData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		CLoadingHandler *handler = reinterpret_cast<CLoadingHandler *>(lParam);
		if (NULL != handler)
		{
			handler->waitForCompletion();
			handler->getUsersAndSessions(m_usersAndSessions);

			LoadUsers(handler->getVersions());
			PostMessage(WM_UPD_VIEWSTATE, VS_IDLE, 1);
			
			delete handler;
		}

		return 0;
	}
	
	LRESULT OnCheckUpdate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (0 == wParam)
		{
			VersionDetector vd;
			CString newVersion = vd.GetProductVersion();
			CW2A pszNV(CT2W((LPCTSTR)newVersion), CP_UTF8);

			char userAgent[512] = { 0 };
			DWORD size = sizeof(userAgent);
			ObtainUserAgentString(0, userAgent, &size);
			CW2A pszUA(CA2W(userAgent), CP_UTF8);

			CUpdateHandler *handler = new CUpdateHandler(m_hWnd, (LPCSTR)pszNV, (LPCSTR)pszUA);
			_Module.GetMessageLoop()->AddIdleHandler(handler);
			handler->startTask();
		}
		else if (1 == wParam)
		{
			CUpdateHandler *handler = reinterpret_cast<CUpdateHandler *>(lParam);
			if (NULL != handler)
			{
				if (_Module.GetMessageLoop()->RemoveIdleHandler(handler))
				{
					AppConfiguration::SetLastCheckUpdateTime();

					bool hasNewVersion = handler->hasNewVersion();
					CString newVersion = handler->getNewVersion();
					CString updateUrl = handler->getUpdateUrl();
					delete handler;

					if (hasNewVersion)
					{
						CString text;
						text.Format(IDS_NEW_VERSION, (LPCTSTR)newVersion);
						CString caption;
						caption.LoadStringW(IDR_MAINFRAME);
						UINT ret = MessageBoxTimeout(NULL, text, caption, MB_OKCANCEL, 0, 6000);
						if (ret == IDOK)
						{
							CT2A url(updateUrl);
							::ShellExecute(NULL, TEXT("open"), (LPCTSTR)updateUrl, NULL, NULL, SW_SHOWNORMAL);
						}
					}
				}
				// If the handler is not in array, throw it away
			}
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

			ManifestParser parser((LPCSTR)backupDir);
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
				m_logger->debug(parser.getLastError());
				MsgBox(m_hWnd, IDS_FAILED_TO_LOAD_BKP);
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
			MsgBox(m_hWnd, IDS_ENC_BKP_NOT_SUPPORTED);
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
		
		SendMessage(WM_NEXTDLGCTL, 0, 0);
		CW2A resDir(CT2W(buffer), CP_UTF8);

		PostMessage(WM_UPD_VIEWSTATE, VS_LOADING, 1);
		
		std::string backup = manifest.getPath();
		CLoadingHandler *handler = new CLoadingHandler(m_hWnd, (LPCSTR)resDir, backup, m_logger);
		// _Module.GetMessageLoop()->AddIdleHandler(handler);
		handler->startTask();

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
		if (MsgBox(m_hWnd, IDS_CANCEL_PROMPT, MB_YESNO) == IDNO)
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

	LRESULT OnBnClickedShowLogs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
	{	
		CButton btn = hWndCtl;
		BOOL showLogs = btn.GetCheck() == BST_CHECKED;
		CString text;
		text.LoadString(showLogs ? IDS_HIDE_LOGS : IDS_SHOW_LOGS);
		btn.SetWindowText(text);

		UpdateUI();

		return 0;
	}

	LRESULT OnBnClickedClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		::PostMessage(GetParent(), WM_CLOSE, 0, 0);
		return 0;
	}

	LRESULT OnHeaderItemStateIconClick(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
	{
		CHeaderCtrl header = m_sessionsListCtrl.GetHeader();
		if (pnmh->hwndFrom == header.m_hWnd)
		{
			LPNMHEADER pnmHeader = (LPNMHEADER)pnmh;
			if (pnmHeader->pitem->mask & HDI_FORMAT && pnmHeader->pitem->fmt & HDF_CHECKBOX)
			{
				CheckAllItems(m_sessionsListCtrl, !(pnmHeader->pitem->fmt & HDF_CHECKED));
				SyncHeaderCheckbox();
				return 1;
			}
		}

		return 0;
	}

	LRESULT OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/)
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
			MsgBox(m_hWnd, IDS_SEL_BACKUP_DIR);
			return 0;
		}

		const BackupManifest& manifest = m_manifests[cbmBox.GetCurSel()];
		if (manifest.isEncrypted())
		{
			MsgBox(m_hWnd, IDS_ENC_BKP_NOT_SUPPORTED);
			return 0;
		}

		std::string backup = manifest.getPath();

		TCHAR outputDir[MAX_PATH] = { 0 };
		GetDlgItemText(IDC_OUTPUT, outputDir, MAX_PATH);
		if (!::PathFileExists(outputDir))
		{
			MsgBox(m_hWnd, IDS_INVALID_OUTPUT_DIR);
			return 0;
		}
		CW2A output(CT2W(outputDir), CP_UTF8);

		TCHAR curDir[MAX_PATH] = { 0 };
		DWORD dwRet = GetCurrentDirectory(MAX_PATH, curDir);
		if (dwRet == 0)
		{
			// printf("GetCurrentDirectory failed (%d)\n", GetLastError());
			return 0;
		}

		// CButton btn = GetDlgItem(IDC_DESC_ORDER);
		bool descOrder = AppConfiguration::GetDescOrder();
		bool saveFilesInSessionFolder = AppConfiguration::GetSavingInSession();
		bool asyncLoading = AppConfiguration::GetAsyncLoading();
		bool loadingDataOnScroll = AppConfiguration::GetLoadingDataOnScroll();
		bool supportingFilter = AppConfiguration::GetSupportingFilter();
		UINT outputFormat = AppConfiguration::GetOutputFormat();

		if (outputFormat == AppConfiguration::OUTPUT_FORMAT_PDF)
		{
			asyncLoading = false;
			loadingDataOnScroll = false;
			supportingFilter = false;
		}

		CListBox lstboxLogs = GetDlgItem(IDC_LOGS);
		lstboxLogs.ResetContent();

		CW2A resDir(CT2W(curDir), CP_UTF8);

		std::map<std::string, std::map<std::string, void *>> usersAndSessions;
		int numberOfSessions = 0;
		int numberOfRecords = 0;
		GetCheckedSessionsAndCopyItems(usersAndSessions, numberOfSessions, numberOfRecords);
		
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		progressCtrl.SetWindowText(TEXT(""));
		progressCtrl.ModifyStyle(PBS_MARQUEE, 0);
		progressCtrl.SetRange32(1, ((numberOfRecords > 0) ? numberOfRecords : 1));
		progressCtrl.SetStep(1);
		progressCtrl.SetPos(0);

		UpdateProgressBarText(0, true);

#if !defined(NDEBUG) || defined(DBG_PERF)
		std::string log = "Record Count:" + std::to_string(numberOfRecords);
		m_logger->debug(log);
#endif
		if (NULL != m_pdfConverter)
		{
			delete m_pdfConverter;
			m_pdfConverter = NULL;
		}
		if (outputFormat == AppConfiguration::OUTPUT_FORMAT_PDF)
		{	
			m_pdfConverter = new PdfConverterImpl(outputDir);
		}

		m_exporter = new Exporter((LPCSTR)resDir, backup, (LPCSTR)output, m_logger, m_pdfConverter);
		m_exporter->setNotifier(m_notifier);
		m_exporter->setOrder(!descOrder);
		m_exporter->setSyncLoading(!asyncLoading);
		m_exporter->setLoadingDataOnScroll(loadingDataOnScroll);
		m_exporter->supportsFilter(supportingFilter);
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
		else if (outputFormat == AppConfiguration::OUTPUT_FORMAT_PDF)
		{
			m_exporter->setPdfMode();
		}
		
		m_exporter->filterUsersAndSessions(usersAndSessions);
		if (m_exporter->run())
		{
			PostMessage(WM_UPD_VIEWSTATE, VS_EXPORTING, 1);
		}

		return 0;
	}
	
	LRESULT OnStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (0 == m_eventIdProgress)
		{
			m_eventIdProgress = SetTimer(EVENT_ID_PROGRESS, 100, NULL);
		}
		
		return 0;
	}

	LRESULT OnComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		BOOL cancelled = (lParam != 0) ? TRUE : FALSE;
		if (NULL != m_pdfConverter)
		{
			if (!cancelled)
			{
				m_pdfConverter->executeCommand();
			}
			delete m_pdfConverter;
			m_pdfConverter = NULL;
		}

		if (m_exporter)
		{
			m_exporter->waitForComplition();
			delete m_exporter;
			m_exporter = NULL;
		}

		if (0 != m_eventIdProgress)
		{
			KillTimer(m_eventIdProgress);
			m_eventIdProgress = 0;
		}

		PostMessage(WM_UPD_VIEWSTATE, VS_IDLE, 1);
		return 0;
	}

	LRESULT OnProgress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnUserSessionStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (m_eventIdProgress != 0)
		{
			KillTimer(m_eventIdProgress);
			m_eventIdProgress = 0;
		}
		return 0;
	}

	LRESULT OnUserSessionComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (0 == m_eventIdProgress)
		{
			m_eventIdProgress = SetTimer(EVENT_ID_PROGRESS, 100, NULL);
		}
		return 0;
	}

	LRESULT OnSessionStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		// wParam: nItem
		// lParam: NumberOfMessages
		int nItem = wParam;
		uint32_t numberOfMessages = static_cast<uint32_t>(lParam);
		
		m_progressListCtrl.EnsureVisible(nItem, FALSE);
		m_progressListCtrl.SetProgressBarInfo(nItem, SUBITEM_PROGRESS, numberOfMessages, 0);
		
		CString text;
		text.Format(IDS_SESSION_PROGRESS, 0u, numberOfMessages);
		m_progressListCtrl.SetItemText(nItem, SUBITEM_PROGRESS, text);

		return 0;
	}

	LRESULT OnSessionComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		m_progressListCtrl.ClearProgressBar();

		int nItem = wParam;
		CString text;
		text.LoadString((lParam != 0) ? IDS_SESSION_CANCELLED : IDS_SESSION_DONE);
		m_progressListCtrl.SetItemText(nItem, SUBITEM_PROGRESS, text);

		return 0;
	}

	LRESULT OnSessionProgress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		int nItem = wParam;
		uint32_t numberOfMessages = static_cast<uint32_t>(lParam);

		const Session *session = reinterpret_cast<const Session *>(m_progressListCtrl.GetItemData(nItem));
		
		m_progressListCtrl.SetProgressPos(static_cast<int>(lParam));
		CString text;
		text.Format(IDS_SESSION_PROGRESS, numberOfMessages, static_cast<uint32_t>(session->getRecordCount()));
		m_progressListCtrl.SetItemText(nItem, SUBITEM_PROGRESS, text);

		UpdateProgressBar();

		return 0;
	}

	LRESULT OnTasksStart(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		UpdateProgressBar(0, static_cast<int>(lParam));

		return 0;
	}

	LRESULT OnTasksComplete(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 0;
	}

	LRESULT OnTasksProgress(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		UpdateProgressBar(static_cast<int>(lParam), 0);

		return 0;
	}

	BOOL IsViewIdle() const
	{
		return m_viewState == VS_IDLE;
	}

protected:

	void UpdateUI()
	{
		CButton btn = GetDlgItem(IDC_SHOW_LOGS);
		BOOL showLogs = btn.GetCheck() == BST_CHECKED;

		::ShowWindow(GetDlgItem(IDC_GRP_LOGS), showLogs ? SW_SHOW : SW_HIDE);
		::ShowWindow(GetDlgItem(IDC_LOGS), showLogs ? SW_SHOW : SW_HIDE);

		::ShowWindow(GetDlgItem(IDC_GRP_USR_CHAT), showLogs ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_USERS), showLogs || m_viewState == VS_EXPORTING ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_VERSIONS), showLogs || m_viewState == VS_EXPORTING ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_SESSIONS), showLogs || m_viewState == VS_EXPORTING ? SW_HIDE : SW_SHOW);

		::ShowWindow(GetDlgItem(IDC_PROGRESS), showLogs || m_viewState != VS_EXPORTING ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_PROGRESS_TEXT), showLogs || m_viewState != VS_EXPORTING ? SW_HIDE : SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_SESS_PROGRESS), showLogs || m_viewState != VS_EXPORTING ? SW_HIDE : SW_SHOW);

		::ShowWindow(GetDlgItem(IDC_CANCEL), m_viewState == VS_EXPORTING ? SW_SHOW : SW_HIDE);
		::ShowWindow(GetDlgItem(IDC_CLOSE), m_viewState != VS_EXPORTING ? SW_SHOW : SW_HIDE);

		UINT ids[] = { IDC_BACKUP, IDC_CHOOSE_BKP, IDC_CHOOSE_OUTPUT, IDC_USERS, IDC_CLOSE, IDC_EXPORT };
		for (int idx = 0; idx < sizeof(ids) / sizeof(UINT); ++idx)
		{
			::EnableWindow(GetDlgItem(ids[idx]), m_viewState == VS_IDLE);
		}

		UINT state = (m_viewState == VS_IDLE) ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
		::EnableMenuItem(::GetSystemMenu(::GetParent(m_hWnd), FALSE), SC_CLOSE, MF_BYCOMMAND | state);
	}

	void UpdateProgressBar(BOOL increaseUpper = FALSE)
	{
		UpdateProgressBar(1, increaseUpper ? 1 : 0);
	}

	void UpdateProgressBar(int increasedPos, int increasedUpper)
	{
		CProgressBarCtrl progressCtrl = GetDlgItem(IDC_PROGRESS);
		PBRANGE range;
		int pos = progressCtrl.GetPos();
		progressCtrl.GetRange(&range);
		if ((increasedUpper > 0) || pos == range.iHigh)
		{
			range.iHigh += increasedUpper;
			progressCtrl.SetRange32(range.iLow, range.iHigh);
		}
		if (increasedPos > 0)
		{
			progressCtrl.OffsetPos(increasedPos);
			pos = progressCtrl.GetPos();
		}

		int percent = (range.iHigh > range.iLow) ? ((pos - range.iLow) * 100 / (range.iHigh - range.iLow)) : 0;
		if (percent >= 100)
		{
			percent = (pos < range.iHigh) ? 99 : 100;
		}

		UpdateProgressBarText(percent);
	}

	void UpdateProgressBarText(int percent, bool forceUpdate = false)
	{
		if (forceUpdate || percent != m_progressTextCtrl.GetWindowLongPtr(GWLP_USERDATA))
		{
			// Avoid flashing
			CString text;
			text.Format(TEXT("%d%%"), percent);
			m_progressTextCtrl.SetWindowLongPtr(GWLP_USERDATA, percent);
			m_progressTextCtrl.SetWindowText(text);
		}
	}

	void UpdateBackups(const std::vector<BackupManifest>& manifests, BOOL onLaunch = FALSE)
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

		// Update
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
			SetComboBoxCurSel(m_hWnd, cmb, selectedIndex);
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
		dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER;
		m_sessionsListCtrl.SetExtendedListViewStyle(dwStyle, dwStyle);

		LVCOLUMN lvc = { 0 };
		ListView_InsertColumn(m_sessionsListCtrl, 0, &lvc);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn1;
		lvc.cx = 256;
		ListView_InsertColumn(m_sessionsListCtrl, 1, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn2;
		lvc.cx = 96;
		ListView_InsertColumn(m_sessionsListCtrl, 2, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn3;
		lvc.cx = 192;
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

	void InitializeSessionProgressList()
	{
		m_progressListCtrl.SubclassWindow(GetDlgItem(IDC_SESS_PROGRESS));

		CString strColumn0;
		CString strColumn1;
		CString strColumn2;
		CString strColumn3;
		CString strColumn4;

		strColumn0.LoadString(IDS_SESSION_NUMBER);
		strColumn1.LoadString(IDS_SESSION_NAME);
		strColumn2.LoadString(IDS_SESSION_COUNT);
		strColumn3.LoadString(IDS_SESSION_USER);
		strColumn4.LoadString(IDS_SESSION_STATUS);

		DWORD dwStyle = m_progressListCtrl.GetExStyle();
		dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
		dwStyle &= ~LVS_EX_CHECKBOXES;
		
		m_progressListCtrl.SetExtendedListViewStyle(dwStyle, dwStyle);

		LVCOLUMN lvc = { 0 };
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn0;
		lvc.cx = 32;
		ListView_InsertColumn(m_progressListCtrl, 0, &lvc);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn1;
		lvc.cx = 256;
		ListView_InsertColumn(m_progressListCtrl, 1, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn2;
		lvc.cx = 96;
		ListView_InsertColumn(m_progressListCtrl, 2, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn3;
		lvc.cx = 192;
		ListView_InsertColumn(m_progressListCtrl, 3, &lvc);
		lvc.iSubItem++;
		lvc.pszText = (LPTSTR)(LPCTSTR)strColumn4;
		lvc.cx = 192;
		ListView_InsertColumn(m_progressListCtrl, 4, &lvc);

		// Set column widths
		ListView_SetColumnWidth(m_progressListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(m_progressListCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(m_progressListCtrl, 1, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(m_progressListCtrl, 2, LVSCW_AUTOSIZE_USEHEADER);
		// ListView_SetColumnWidth(m_progressListCtrl, 3, LVSCW_AUTOSIZE_USEHEADER);
		
	}

	void GetCheckedSessionsAndCopyItems(std::map<std::string, std::map<std::string, void *>>& usersAndSessions, int& numberOfSessions, int& numberOfRecords)
	{
		numberOfSessions = 0;
		numberOfRecords = 0;
		m_progressListCtrl.SetRedraw(FALSE);
		if (m_progressListCtrl.GetItemCount() > 0)
		{
			m_progressListCtrl.DeleteAllItems();
		}

		TCHAR numberString[32] = { 0 };
		CListViewCtrl listViewCtrl = GetDlgItem(IDC_SESSIONS);

		for (int column = 0; column < listViewCtrl.GetHeader().GetItemCount(); column++)
		{
			m_progressListCtrl.SetColumnWidth(column, listViewCtrl.GetColumnWidth(column));
		}

		// std::map<std::string, std::set<std::string>> usersAndSessions;
		for (int nItem = 0; nItem < listViewCtrl.GetItemCount(); nItem++)
		{
			if (!listViewCtrl.GetCheckState(nItem))
			{
				continue;
			}

			++numberOfSessions;
			_itot(m_progressListCtrl.GetItemCount() + 1, numberString, 10);
			LVITEM lvItem = {};
			lvItem.mask = LVIF_TEXT | LVIF_PARAM;
			lvItem.iItem = m_progressListCtrl.GetItemCount();
			lvItem.iSubItem = 0;
			lvItem.pszText = numberString;
			lvItem.lParam = listViewCtrl.GetItemData(nItem);
			int newItem = m_progressListCtrl.InsertItem(&lvItem);

			CString text;
			for (int nSubItem = 1; nSubItem < 4; nSubItem++)
			{
				listViewCtrl.GetItemText(nItem, nSubItem, text);
				m_progressListCtrl.AddItem(newItem, nSubItem, text);
			}
			
			Session* session = reinterpret_cast<Session*>(listViewCtrl.GetItemData(nItem));
			if (NULL != session)
			{
				numberOfRecords += session->getRecordCount();
				
				session->setData(reinterpret_cast<void *>(newItem));
				std::string usrName = session->getOwner()->getUsrName();
				std::map<std::string, std::map<std::string, void *>>::iterator it = usersAndSessions.find(usrName);
				if (it == usersAndSessions.end())
				{
					it = usersAndSessions.insert(usersAndSessions.end(), std::pair<std::string, std::map<std::string, void *>>(usrName, std::map<std::string, void *>()));
				}

				it->second.insert(std::pair<std::string, void *>(session->getUsrName(), session->getData()));
			}
		}

		ListView_SetColumnWidth(m_progressListCtrl, 0, LVSCW_AUTOSIZE_USEHEADER);
		m_progressListCtrl.SetRedraw(TRUE);
	}
	
	void LoadUsers(const CString& versions)
	{
		CEdit edtVersions = GetDlgItem(IDC_VERSIONS);
		edtVersions.SetWindowText(versions);

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
			SetComboBoxCurSel(m_hWnd, cbmBox, 0);
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

		SetHeaderCheckState(listViewCtrl, TRUE);
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

		SetHeaderCheckState(listViewCtrl, fChecked);
	}

};
