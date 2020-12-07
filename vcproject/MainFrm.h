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
		CMenuHandle subMenu = menu.GetSubMenu(0);
		subMenu.CheckMenuItem(ID_FILE_DESC_ORDER, m_view.GetDescOrder() ? MF_CHECKED : MF_UNCHECKED);
		subMenu.CheckMenuItem(ID_FILE_SAVING_IN_SESSION, m_view.GetSavingInSession() ? MF_CHECKED : MF_UNCHECKED);

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
		CMenuHandle subMenu = menu.GetSubMenu(0);
		subMenu.EnableMenuItem(ID_FILE_DESC_ORDER, enabled ? MF_ENABLED : MF_DISABLED);
		subMenu.EnableMenuItem(ID_FILE_SAVING_IN_SESSION, enabled ? MF_ENABLED : MF_DISABLED);
		
		return 1;
	}
	

	LRESULT OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		PostMessage(WM_CLOSE);
		return 0;
	}

	LRESULT OnSavingInSession(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(ID_FILE_SAVING_IN_SESSION, MF_BYCOMMAND);
		BOOL curSavingInSesstion = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FILE_SAVING_IN_SESSION, (curSavingInSesstion ? MF_UNCHECKED : MF_CHECKED));
		m_view.SetSavingInSession(!curSavingInSesstion);
		
		return 0;
	}

	LRESULT OnDescOrder(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
	{
		CMenuHandle menu = GetMenu();
		CMenuHandle subMenu = menu.GetSubMenu(0);
		UINT menuState = subMenu.GetMenuState(ID_FILE_DESC_ORDER, MF_BYCOMMAND);
		BOOL curDescOrder = (menuState != 0xFFFFFFFF) && ((menuState & MF_CHECKED) == MF_CHECKED) ? TRUE : FALSE;
		subMenu.CheckMenuItem(ID_FILE_DESC_ORDER, MF_BYCOMMAND | (curDescOrder ? MF_UNCHECKED : MF_CHECKED));
		m_view.SetDescOrder(!curDescOrder);
		
		return 0;
	}

	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CAboutDlg dlg;
		dlg.DoModal();
		return 0;
	}
};
