#pragma once

template <class T, class TBase = CButton, class TWinTraits = ATL::CControlWinTraits>
class ATL_NO_VTABLE CToolTipButtonImpl : public ATL::CWindowImpl< T, TBase, TWinTraits >
{
public:
	DECLARE_WND_SUPERCLASS2(NULL, T, TBase::GetWndClassName())

	enum
	{
		_nImageNormal = 0,
		_nImagePushed,
		_nImageFocusOrHover,
		_nImageDisabled,

		_nImageCount = 4,
	};

	enum
	{
		ID_TIMER_FIRST = 1000,
		ID_TIMER_REPEAT = 1001
	};

	CToolTipCtrl m_tip;
	LPTSTR m_lpstrToolTipText;

	// Constructor/Destructor
	CToolTipButtonImpl() :
		m_lpstrToolTipText(NULL)
	{

	}

	~CToolTipButtonImpl()
	{
		delete[] m_lpstrToolTipText;
	}

	// overridden to provide proper initialization
	BOOL SubclassWindow(HWND hWnd)
	{
		BOOL bRet = ATL::CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
		if (bRet != FALSE)
		{
			T* pT = static_cast<T*>(this);
			pT->Init();
		}

		return bRet;
	}

	// Attributes

	int GetToolTipTextLength() const
	{
		return (m_lpstrToolTipText == NULL) ? -1 : lstrlen(m_lpstrToolTipText);
	}

	bool GetToolTipText(LPTSTR lpstrText, int nLength) const
	{
		ATLASSERT(lpstrText != NULL);
		if (m_lpstrToolTipText == NULL)
			return false;

		errno_t nRet = ATL::Checked::tcsncpy_s(lpstrText, nLength, m_lpstrToolTipText, _TRUNCATE);

		return ((nRet == 0) || (nRet == STRUNCATE));
	}

	bool SetToolTipText(LPCTSTR lpstrText)
	{
		if (m_lpstrToolTipText != NULL)
		{
			delete[] m_lpstrToolTipText;
			m_lpstrToolTipText = NULL;
		}

		if (lpstrText == NULL)
		{
			if (m_tip.IsWindow())
				m_tip.Activate(FALSE);
			return true;
		}

		int cchLen = lstrlen(lpstrText) + 1;
		ATLTRY(m_lpstrToolTipText = new TCHAR[cchLen]);
		if (m_lpstrToolTipText == NULL)
			return false;

		ATL::Checked::tcscpy_s(m_lpstrToolTipText, cchLen, lpstrText);
		if (m_tip.IsWindow())
		{
			m_tip.Activate(TRUE);
			m_tip.AddTool(this->m_hWnd, m_lpstrToolTipText);
		}

		return true;
	}

	// Overrideables

	// Message map and handlers
	BEGIN_MSG_MAP(CBitmapButtonImpl)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
		// MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		// MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
	END_MSG_MAP()

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		T* pT = static_cast<T*>(this);
		pT->Init();

		bHandled = FALSE;
		return 1;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if (m_tip.IsWindow())
		{
			m_tip.DestroyWindow();
			m_tip.m_hWnd = NULL;
		}
		bHandled = FALSE;
		return 1;
	}

	LRESULT OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		MSG msg = { this->m_hWnd, uMsg, wParam, lParam };
		if (m_tip.IsWindow())
			m_tip.RelayEvent(&msg);
		bHandled = FALSE;
		return 1;
	}

	// Implementation
	void Init()
	{

		// create a tool tip
		m_tip.Create(this->m_hWnd);
		ATLASSERT(m_tip.IsWindow());
		if (m_tip.IsWindow() && (m_lpstrToolTipText != NULL))
		{
			m_tip.Activate(TRUE);
			m_tip.AddTool(this->m_hWnd, m_lpstrToolTipText);
		}

	}

	BOOL StartTrackMouseLeave()
	{
		TRACKMOUSEEVENT tme = {};
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.hwndTrack = this->m_hWnd;
		return ::TrackMouseEvent(&tme);
	}
};

class CToolTipButton : public CToolTipButtonImpl<CToolTipButton>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_ToolTipButton"), GetWndClassName())

	CToolTipButton() :
		CToolTipButtonImpl<CToolTipButton>()
	{ }
};
