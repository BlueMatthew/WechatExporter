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
		COMMAND_ID_HANDLER(ID_FILE_SAVING_IN_SESSION, OnSavingInSession)
		COMMAND_ID_HANDLER(ID_FILE_DESC_ORDER, OnDescOrder)
		COMMAND_RANGE_HANDLER(ID_FORMAT_HTML, ID_FORMAT_TEXT, OnOutputFormat)
		COMMAND_ID_HANDLER(ID_FORMAT_ASYNC_LOADING, OnAsyncFormat)
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
		subMenuFile.CheckMenuItem(ID_FILE_DESC_ORDER, AppConfiguration::GetDescOrder() ? MF_CHECKED : MF_UNCHECKED);
		subMenuFile.CheckMenuItem(ID_FILE_SAVING_IN_SESSION, AppConfiguration::GetSavingInSession() ? MF_CHECKED : MF_UNCHECKED);

		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		UINT outputFormat = AppConfiguration::GetOutputFormat();
		subMenuFormat.CheckMenuRadioItem(ID_FORMAT_HTML, ID_FORMAT_TEXT, ID_FORMAT_HTML + outputFormat, MF_BYCOMMAND | MFT_RADIOCHECK);
		
		subMenuFormat.CheckMenuItem (ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND | (AppConfiguration::GetAsyncLoading() ? MF_CHECKED : MF_UNCHECKED));
		subMenuFormat.EnableMenuItem(ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND | ((outputFormat != AppConfiguration::OUTPUT_FORMAT_HTML) ? MF_DISABLED : MF_ENABLED));
		
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
		BOOL enabled = m_view.IsUIEnabled();
		
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenuFile = menu.GetSubMenu(0);
		subMenuFile.EnableMenuItem(ID_FILE_DESC_ORDER, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFile.EnableMenuItem(ID_FILE_SAVING_IN_SESSION, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		
		CMenuHandle subMenuFormat = menu.GetSubMenu(1);
		subMenuFormat.EnableMenuItem(ID_FORMAT_HTML, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));
		subMenuFormat.EnableMenuItem(ID_FORMAT_TEXT, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_DISABLED));

		UINT outputFormat = AppConfiguration::GetOutputFormat();
		BOOL asyncLoadingEnabled = (outputFormat == AppConfiguration::OUTPUT_FORMAT_HTML);
		if (asyncLoadingEnabled)
		{
			asyncLoadingEnabled = enabled;
		}
		subMenuFormat.EnableMenuItem(ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND | (asyncLoadingEnabled ? MF_ENABLED : MF_DISABLED));

		return 1;
	}

	LRESULT OnSavingInSession(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(ID_FILE_SAVING_IN_SESSION, MF_BYCOMMAND);
		BOOL curSavingInSesstion = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FILE_SAVING_IN_SESSION, (curSavingInSesstion ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetSavingInSession(!curSavingInSesstion);
		
		return 0;
	}

	LRESULT OnDescOrder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(ID_FILE_DESC_ORDER, MF_BYCOMMAND);
		BOOL curDescOrder = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FILE_DESC_ORDER, MF_BYCOMMAND | (curDescOrder ? MF_UNCHECKED : MF_CHECKED));
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

		subMenuFormat.EnableMenuItem(ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND | ((outputFormat != AppConfiguration::OUTPUT_FORMAT_HTML) ? MF_DISABLED : MF_ENABLED));

		return 0;
	}

	LRESULT OnAsyncFormat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(1);
		UINT menuState = subMenu.GetMenuState(ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND);
		BOOL asyncLoading = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FORMAT_ASYNC_LOADING, MF_BYCOMMAND | (asyncLoading ? MF_UNCHECKED : MF_CHECKED));
		AppConfiguration::SetAsyncLoading(!asyncLoading);

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
