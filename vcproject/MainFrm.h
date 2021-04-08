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
		COMMAND_ID_HANDLER(ID_FILE_CHK_UPDATE, OnCheckUpdate)
		COMMAND_RANGE_HANDLER(ID_FORMAT_HTML, ID_FORMAT_TEXT, OnOutputFormat)
		COMMAND_ID_HANDLER(ID_OPT_SAVING_IN_SESSION, OnSavingInSession)
		COMMAND_ID_HANDLER(ID_OPT_DESC_ORDER, OnDescOrder)
		COMMAND_ID_HANDLER(ID_OPT_FILTER, OnFilter)
		COMMAND_ID_HANDLER(ID_OPT_ASYNC_LOADING, OnAsyncFormat)
		COMMAND_ID_HANDLER(ID_OPT_LM_ONSCROLL, OnLoadingDataOnScroll)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	
// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		CenterWindow(m_hWnd);
		m_hWndClient = m_view.Create(m_hWnd);

		// register object for message filtering and idle updates
		CMessageLoop* pLoop = _Module.GetMessageLoop();
		ATLASSERT(pLoop != NULL);
		pLoop->AddMessageFilter(this);
		pLoop->AddIdleHandler(this);

		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFile = menu.GetSubMenu(0);
		subMenuFile.CheckMenuItem(ID_FILE_CHK_UPDATE, MF_BYCOMMAND | (AppConfiguration::GetCheckingUpdateDisabled() ? MF_UNCHECKED : MF_CHECKED));

		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		UINT outputFormat = AppConfiguration::GetOutputFormat();
		subMenuFormat.CheckMenuRadioItem(ID_FORMAT_HTML, ID_FORMAT_TEXT, ID_FORMAT_HTML + outputFormat, MF_BYCOMMAND | MFT_RADIOCHECK);
		
		CMenuHandle subMenuOptions = menu.GetSubMenu(2);
		subMenuOptions.CheckMenuItem(ID_OPT_DESC_ORDER, MF_BYCOMMAND | (AppConfiguration::GetDescOrder() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND | (AppConfiguration::GetSavingInSession() ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.CheckMenuItem(ID_OPT_FILTER, MF_BYCOMMAND | (AppConfiguration::GetSupportingFilter() ? MF_CHECKED : MF_UNCHECKED));

		BOOL asyncLoading = AppConfiguration::GetAsyncLoading();
		subMenuOptions.CheckMenuItem (ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | (asyncLoading ? MF_CHECKED : MF_UNCHECKED));
		subMenuOptions.EnableMenuItem(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | ((outputFormat != AppConfiguration::OUTPUT_FORMAT_HTML) ? MF_DISABLED : MF_ENABLED));
		
		subMenuOptions.EnableMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | ((outputFormat == AppConfiguration::OUTPUT_FORMAT_HTML) && asyncLoading ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.CheckMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | (AppConfiguration::GetLoadingDataOnScroll() ? MF_CHECKED : MF_UNCHECKED));

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
		BOOL enabled = !(m_view.IsIdle());
		
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFile = menu.GetSubMenu(0);
		
		
		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		subMenuFormat.EnableMenuItem(ID_FORMAT_HTML, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFormat.EnableMenuItem(ID_FORMAT_TEXT, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));

		CMenuHandle subMenuOptions = menu.GetSubMenu(2);
		subMenuOptions.EnableMenuItem(ID_OPT_DESC_ORDER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.EnableMenuItem(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.EnableMenuItem(ID_OPT_FILTER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));

		UINT outputFormat = AppConfiguration::GetOutputFormat();
		BOOL asyncLoadingEnabled = (outputFormat == AppConfiguration::OUTPUT_FORMAT_HTML);
		if (asyncLoadingEnabled)
		{
			asyncLoadingEnabled = enabled;
		}
		subMenuOptions.EnableMenuItem(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | (asyncLoadingEnabled ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.EnableMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | (asyncLoadingEnabled && AppConfiguration::GetAsyncLoading() ? MF_ENABLED : MF_DISABLED));
		subMenuOptions.CheckMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | (AppConfiguration::GetLoadingDataOnScroll() ? MF_CHECKED : MF_UNCHECKED));

		return 1;
	}

	LRESULT OnSavingInSession(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND);
		BOOL curSavingInSesstion = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_SAVING_IN_SESSION, MF_BYCOMMAND | (curSavingInSesstion ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetSavingInSession(!curSavingInSesstion);
		
		return 0;
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

	LRESULT OnOutputFormat(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		subMenuFormat.CheckMenuRadioItem(ID_FORMAT_HTML, ID_FORMAT_TEXT, wID, MF_BYCOMMAND | MFT_RADIOCHECK);
		UINT outputFormat = wID - ID_FORMAT_HTML;
		AppConfiguration::SetOutputFormat(outputFormat);

		subMenuFormat.EnableMenuItem(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | ((outputFormat != AppConfiguration::OUTPUT_FORMAT_HTML) ? MF_DISABLED : MF_ENABLED));
		
		return 0;
	}

	LRESULT OnAsyncFormat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND);
		BOOL asyncLoading = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_ASYNC_LOADING, MF_BYCOMMAND | (asyncLoading ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetAsyncLoading(!asyncLoading);

		subMenu.EnableMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | ((!asyncLoading) ? MF_ENABLED : MF_DISABLED));

		return 0;
	}

	LRESULT OnLoadingDataOnScroll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(2);
		UINT menuState = subMenu.GetMenuState(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND);
		BOOL loadingDataOnScroll = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_OPT_LM_ONSCROLL, MF_BYCOMMAND | (loadingDataOnScroll ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetLoadingDataOnScroll(!loadingDataOnScroll);

		return 0;
	}

	LRESULT OnCheckUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(ID_FILE_CHK_UPDATE, MF_BYCOMMAND);
		BOOL autoChkUpdate = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FILE_CHK_UPDATE, MF_BYCOMMAND | (autoChkUpdate ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetCheckingUpdateDisabled(autoChkUpdate);

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
