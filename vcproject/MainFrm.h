// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>, 
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, public CIdleHandler
{
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	BOOL m_pdfSupported;
	CView m_view;

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{
		if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
			return TRUE;

		return m_view.PreTranslateMessage(pMsg);
	}

	virtual BOOL OnIdle()
	{
		return FALSE;
	}

	BEGIN_UPDATE_UI_MAP(CMainFrame)
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP(CMainFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_INITMENU, OnInitMenu)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_HELP_HOMEPAGE, OnHelpHomePage)
		COMMAND_ID_HANDLER(ID_FILE_CHK_UPDATE, OnCheckUpdate)
		COMMAND_ID_HANDLER(ID_FILE_EXP_ITUNES, OnExportITunes)
		COMMAND_ID_HANDLER(ID_FILE_DBG_LOGS, OnOutputDbgLogs)
		COMMAND_ID_HANDLER(ID_FILE_OPEN_FOLDER, OnOpenFolder)
		COMMAND_RANGE_HANDLER(ID_FORMAT_HTML, ID_FORMAT_PDF, OnOutputFormat)
		COMMAND_ID_HANDLER(ID_OPT_DESC_ORDER, OnDescOrder)
		COMMAND_ID_HANDLER(ID_OPT_DL_EMOJI, OnDownloadingEmoji)
		COMMAND_ID_HANDLER(ID_OPT_LM_ONSCROLL, OnAsyncLoading)
		COMMAND_ID_HANDLER(ID_OPT_NORMALPAGINATION, OnAsyncLoading)
		COMMAND_ID_HANDLER(ID_OPT_PAGINATION_YEAR, OnAsyncLoading)
		COMMAND_ID_HANDLER(ID_OPT_PAGINATION_MONTH, OnAsyncLoading)
		COMMAND_ID_HANDLER(ID_OPT_FILTER, OnFilter)
		COMMAND_ID_HANDLER(ID_OPT_INCREMENTALEXP, OnIncrementalExporting)
		COMMAND_ID_HANDLER(ID_OPT_SUBSCRIPTIONS, OnIncludeSubscriptions)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	
// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		m_pdfSupported = AppConfiguration::IsPdfSupported();
		CenterWindow(m_hWnd);
		m_hWndClient = m_view.Create(m_hWnd);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		AppConfiguration::upgrade();

		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFile = menu.GetSubMenu(0);
		subMenuFile.CheckMenuItem(ID_FILE_CHK_UPDATE, MF_BYCOMMAND | (AppConfiguration::GetCheckingUpdateDisabled() ? MF_UNCHECKED : MF_CHECKED));
		subMenuFile.CheckMenuItem(ID_FILE_DBG_LOGS, MF_BYCOMMAND | (AppConfiguration::OutputDebugLogs() ? MF_CHECKED : MF_UNCHECKED));
		subMenuFile.CheckMenuItem(ID_FILE_OPEN_FOLDER, MF_BYCOMMAND | (AppConfiguration::GetOpenningFolderAfterExp() ? MF_CHECKED : MF_UNCHECKED));
		AppConfiguration::upgrade();

		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		UINT outputFormat = AppConfiguration::GetOutputFormat();
		subMenuFormat.CheckMenuRadioItem(ID_FORMAT_HTML, ID_FORMAT_PDF, ID_FORMAT_HTML + outputFormat, MF_BYCOMMAND | MFT_RADIOCHECK);

		CMenuHandle subMenuOptions = menu.GetSubMenu(2);
		subMenuOptions.CheckMenuItem(ID_OPT_DESC_ORDER, MF_BYCOMMAND | (AppConfiguration::GetDescOrder() ? MF_CHECKED : MF_UNCHECKED));
		// subMenuOptions.CheckMenuItem(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND | (AppConfiguration::GetSavingInSession() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_DL_EMOJI, MF_BYCOMMAND | (AppConfiguration::GetUsingRemoteEmoji() ? MF_UNCHECKED : MF_CHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_FILTER, MF_BYCOMMAND | (AppConfiguration::GetSupportingFilter() ? MF_CHECKED : MF_UNCHECKED));

		InitAsyncLoadingMenu(subMenuOptions, TRUE);

		subMenuOptions.CheckMenuItem(ID_OPT_INCREMENTALEXP, MF_BYCOMMAND | (AppConfiguration::GetIncrementalExporting() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_SUBSCRIPTIONS, MF_BYCOMMAND | (AppConfiguration::IncludeSubscriptions() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.RemoveMenu(ID_OPT_SUBSCRIPTIONS, MF_BYCOMMAND);

		return 0;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		// unregister message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->RemoveMessageFilter(this);
		pLoop->RemoveIdleHandler(this);

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnInitMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		BOOL enabled = m_view.IsViewIdle();

		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFile = menu.GetSubMenu(0);
		subMenuFile.EnableMenuItem(ID_FILE_EXP_ITUNES, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFile.EnableMenuItem(ID_FILE_DBG_LOGS, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFile.EnableMenuItem(ID_FILE_OPEN_FOLDER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));

		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		subMenuFormat.EnableMenuItem(ID_FORMAT_HTML, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFormat.EnableMenuItem(ID_FORMAT_TEXT, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFormat.EnableMenuItem(ID_FORMAT_PDF, MF_BYCOMMAND | ((enabled && m_pdfSupported) ? MF_ENABLED : MF_DISABLED));

		CMenuHandle subMenuOptions = menu.GetSubMenu(2);
		subMenuOptions.EnableMenuItem(ID_OPT_DESC_ORDER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		// subMenuOptions.EnableMenuItem(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.EnableMenuItem(ID_OPT_DL_EMOJI, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.EnableMenuItem(ID_OPT_FILTER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));

		InitAsyncLoadingMenu(subMenuOptions, enabled);

		subMenuOptions.CheckMenuItem(ID_OPT_INCREMENTALEXP, MF_BYCOMMAND | (AppConfiguration::GetIncrementalExporting() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_SUBSCRIPTIONS, MF_BYCOMMAND | (AppConfiguration::IncludeSubscriptions() ? MF_CHECKED : MF_UNCHECKED));

		return 1;
	}

	void InitAsyncLoadingMenu(CMenuHandle& subMenuOptions, BOOL isIdle)
	{
		UINT outputFormat = AppConfiguration::GetOutputFormat();
		BOOL asyncLoadingEnabled = (outputFormat == AppConfiguration::OUTPUT_FORMAT_HTML);
		if (asyncLoadingEnabled)
		{
			asyncLoadingEnabled = isIdle;
		}

		DWORD asyncLoading = AppConfiguration::GetAsyncLoading();
		UINT ids[] = { ID_OPT_LM_ONSCROLL, ID_OPT_NORMALPAGINATION, ID_OPT_PAGINATION_YEAR, ID_OPT_PAGINATION_MONTH };
		UINT asyncLoadingStates[] = { ASYNC_ONSCROLL, ASYNC_PAGER_NORMAL, ASYNC_PAGER_ON_YEAR, ASYNC_PAGER_ON_MONTH };

		for (int idx = 0; idx < (sizeof(ids) / sizeof(UINT)); ++idx)
		{
			subMenuOptions.EnableMenuItem(ids[idx], MF_BYCOMMAND | (asyncLoadingEnabled ? MF_ENABLED : MF_DISABLED));
			subMenuOptions.CheckMenuItem(ids[idx], MF_BYCOMMAND | ((asyncLoading == asyncLoadingStates[idx]) ? MF_CHECKED : MF_UNCHECKED));
		}
	}

	LRESULT OnFilter(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_FILTER, MF_BYCOMMAND);
		BOOL supportingFilter = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_FILTER, MF_BYCOMMAND | (supportingFilter ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetSupportingFilter(!supportingFilter);

		return 0;
	}

	LRESULT OnDescOrder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_DESC_ORDER, MF_BYCOMMAND);
		BOOL curDescOrder = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_DESC_ORDER, MF_BYCOMMAND | (curDescOrder ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetDescOrder(!curDescOrder);
		
		return 0;
	}

	LRESULT OnDownloadingEmoji(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_DL_EMOJI, MF_BYCOMMAND);
		BOOL curState = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_DL_EMOJI, MF_BYCOMMAND | (curState ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetUsingRemoteEmoji(curState);

		return 0;
	}

	LRESULT OnOutputFormat(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		subMenuFormat.CheckMenuRadioItem(ID_FORMAT_HTML, ID_FORMAT_PDF, wID, MF_BYCOMMAND | MFT_RADIOCHECK);
		UINT outputFormat = wID - ID_FORMAT_HTML;
		AppConfiguration::SetOutputFormat(outputFormat);

		// subMenuFormat.EnableMenuItem(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | ((outputFormat != AppConfiguration::OUTPUT_FORMAT_HTML) ? MF_DISABLED : MF_ENABLED));
		
		return 0;
	}

	LRESULT OnIncrementalExporting(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL incrementalExporting = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(wID, MF_BYCOMMAND | (incrementalExporting ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetIncrementalExporting(!incrementalExporting);

		return 0;
	}

	LRESULT OnIncludeSubscriptions(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL stateValue = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(wID, MF_BYCOMMAND | (stateValue ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetIncludingSubscriptions(!stateValue);

		return 0;
	}

	LRESULT OnAsyncLoading(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		UINT ids[] = { ID_OPT_LM_ONSCROLL, ID_OPT_NORMALPAGINATION, ID_OPT_PAGINATION_YEAR, ID_OPT_PAGINATION_MONTH};
		UINT asyncLoading[] = { ASYNC_ONSCROLL, ASYNC_PAGER_NORMAL, ASYNC_PAGER_ON_YEAR, ASYNC_PAGER_ON_MONTH };

		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL asyncState = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;

		int selectedIdx = 0;
		for (int idx = 0; idx < sizeof(ids) / sizeof(UINT); ++idx)
		{
			if (ids[idx] != wID)
			{
				subMenu.CheckMenuItem(ids[idx], MF_BYCOMMAND | MF_UNCHECKED);
			}
			else
			{
				selectedIdx = idx;
				subMenu.CheckMenuItem(ids[idx], MF_BYCOMMAND | (asyncState ? MF_UNCHECKED : MF_CHECKED));
			}
		}

		if (asyncState)
		{
			AppConfiguration::SetSyncLoading();
		}
		else
		{
			AppConfiguration::SetAsyncLoading(asyncLoading[selectedIdx]);
		}
		
		return 0;
	}

	LRESULT OnCheckUpdate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL stateValue = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(wID, MF_BYCOMMAND | (stateValue ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetCheckingUpdateDisabled(stateValue);

		return 0;
	}

	LRESULT OnExportITunes(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		m_view.ExportITunesBackup();

		return 0;
	}

	LRESULT OnOpenFolder(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL stateValue = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(wID, MF_BYCOMMAND | (stateValue ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetOpenningFolderAfterExp(!stateValue);

		return 0;
	}

	LRESULT OnOutputDbgLogs(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(wID, MF_BYCOMMAND);
		BOOL stateValue = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(wID, MF_BYCOMMAND | (stateValue ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetOutputDebugLogs(!stateValue);

		return 0;
	}

	LRESULT OnHelpHomePage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		DWORD_PTR dwRet = (DWORD_PTR)::ShellExecute(0, _T("open"), TEXT("https://github.com/BlueMatthew/WechatExporter"), 0, 0, SW_SHOWNORMAL);

		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}
};
