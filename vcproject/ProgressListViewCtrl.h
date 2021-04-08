#ifndef _PROGRESSLISTVIEWCTRL_H
#define _PROGRESSLISTVIEWCTRL_H

class CProgressListViewCtrl : public CWindowImpl<CProgressListViewCtrl, CListViewCtrl>,
	public CCustomDraw<CProgressListViewCtrl>
{
public:

	BEGIN_MSG_MAP(CProgressListViewCtrl)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		// REFLECTED_NOTIFY_CODE_HANDLER(LVN_DELETEITEM, OnDeleteItem)
		CHAIN_MSG_MAP_ALT(CCustomDraw<CProgressListViewCtrl>, 1)
		// Because WTL 3.1 does not support subitem custom drawing.
		// Should be forward-compatible with new versions...
		// REFLECTED_NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnNotifyCustomDraw)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	CProgressListViewCtrl() : CWindowImpl()
	{
		m_itemForProgressBar = -1;
		m_subItemForProgressBar = -1;
		m_progressUpper = 0;
		m_progressPos = 0;
	}

	BOOL SubclassWindow(HWND hWnd)
	{
		return CWindowImpl<CProgressListViewCtrl, CListViewCtrl>::SubclassWindow(hWnd) ? _Init() : FALSE;
	}

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		bHandled = FALSE;
		return _Init() ? 0 : -1;
	}

	LRESULT OnNotifyCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
	{
		SetMsgHandled(FALSE);
		LPNMCUSTOMDRAW lpNMCustomDraw = (LPNMCUSTOMDRAW)pnmh;
		DWORD dwRet = CDRF_DODEFAULT;
		switch (lpNMCustomDraw->dwDrawStage) {
		case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
			dwRet = OnSubItemPrePaint(idCtrl, lpNMCustomDraw);
			SetMsgHandled(TRUE);
			return dwRet;
		}
		bHandled = FALSE;
		return dwRet;
	}

	DWORD OnPrePaint(int idCtrl, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
	{
		return CDRF_NOTIFYITEMDRAW;
	}

	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
	{
		return CDRF_NOTIFYSUBITEMDRAW; // We need per-subitem notifications
	}

	DWORD OnSubItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
	{
		NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(lpNMCustomDraw);
		if (pLVCD->nmcd.dwItemSpec == m_itemForProgressBar && pLVCD->iSubItem == m_subItemForProgressBar)
		{
			CDCHandle dcPaint(pLVCD->nmcd.hdc);
			int nContextState = dcPaint.SaveDC();

			int State = ListView_GetItemState(m_hWnd, pLVCD->nmcd.dwItemSpec, LVIS_CUT | LVIS_SELECTED | LVIS_FOCUSED);
			CString text;
			GetItemText((int)pLVCD->nmcd.dwItemSpec, pLVCD->iSubItem, text);

			int colorIndex = (State & LVIS_SELECTED) ? 1 : 0;

			//3D border spacer (not exactly what we need either)
			int nSpacerW = GetSystemMetrics(SM_CXEDGE);
			//Get horizontal DPI setting
			int dpiX = dcPaint.GetDeviceCaps(LOGPIXELSX);
			int nSpacePx = (int)(nSpacerW * 2.5 * dpiX / 96.0);

			RECT rc = pLVCD->nmcd.rc;
			// ATLTRACE(TEXT("SubItem=%d Bounds: left=%d right=%d\r\n"), pLVCD->iSubItem, rc.left, rc.right);
			rc.left += 2;
			rc.right -= 2;
			rc.top += 1;
			rc.bottom -= 2;

			RECT rcText = rc;
			rcText.left += nSpacePx;
			rcText.right -= nSpacePx;

			dcPaint.FillSolidRect(&pLVCD->nmcd.rc, m_clrBkgnd[colorIndex]);
			dcPaint.SetTextColor(m_clrText[colorIndex]);
			dcPaint.DrawText(text, -1, &rcText, DT_VCENTER | DT_SINGLELINE);

			rc.right = rc.left + (LONG)(m_percent * (rc.right - rc.left));
			if (rcText.right > rc.right)
			{
				rcText.right = rc.right;
			}

			dcPaint.FillSolidRect(&rc, m_clrPercent[colorIndex]);
			dcPaint.SetTextColor(m_clrPercentText[colorIndex]);
			dcPaint.DrawText(text, -1, &rcText, DT_VCENTER | DT_SINGLELINE);

			dcPaint.RestoreDC(nContextState);

			return CDRF_SKIPDEFAULT;
		}
		else if (pLVCD->nmcd.dwItemSpec != m_itemForProgressBar && pLVCD->iSubItem == m_subItemForProgressBar)
		{
			pLVCD->clrText = RGB(0x0, 0xFF, 0x0);
		}

		return CDRF_DODEFAULT;
	}

	void SetProgressBarInfo(int item, int subItem, int progressUpper, int progressPos = 0)
	{
		int nOrgItem = m_itemForProgressBar;
		int nOrgSubItem = m_subItemForProgressBar;
		m_itemForProgressBar = item;
		m_subItemForProgressBar = subItem;
		m_progressUpper = progressUpper;

		if (nOrgItem != -1 && nOrgSubItem != -1)
		{
			RECT rc;
			GetSubItemRect(nOrgItem, nOrgSubItem, LVIR_BOUNDS, &rc);
			InvalidateRect(&rc);
		}
		
		SetProgressPos(progressPos);
	}

	void SetProgressPos(int progressPos)
	{
		m_progressPos = progressPos;
		m_percent = (m_progressUpper > 0) ? (((float)progressPos) / ((float)m_progressUpper)) : 0.0f;

		RECT rc;
		GetSubItemRect(m_itemForProgressBar, m_subItemForProgressBar, LVIR_BOUNDS, &rc);
		InvalidateRect(&rc);
	}

	void ClearProgressBar()
	{
		int nOrgItem = m_itemForProgressBar;
		int nOrgSubItem = m_subItemForProgressBar;
		m_itemForProgressBar = -1;
		// m_subItemForProgressBar = -1;
		m_progressUpper = 0;

		if (nOrgItem != -1 && nOrgSubItem != -1)
		{
			RECT rc;
			GetSubItemRect(nOrgItem, nOrgSubItem, LVIR_BOUNDS, &rc);
			InvalidateRect(&rc);
		}
	}

protected:

	BOOL _Init()
	{
		m_clrText[0] = ::GetSysColor(COLOR_WINDOWTEXT);
		m_clrBkgnd[0] = ::GetSysColor(COLOR_WINDOW);
		m_clrPercentText[0] = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		m_clrPercent[0] = ::GetSysColor(COLOR_HIGHLIGHT);

		m_clrText[1] = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
		m_clrBkgnd[1] = ::GetSysColor(COLOR_HIGHLIGHT);
		m_clrPercentText[1] = ::GetSysColor(COLOR_HOTLIGHT);
		m_clrPercent[1] = ::GetSysColor(COLOR_WINDOW);

		return TRUE;
	}

	int m_itemForProgressBar;
	int m_subItemForProgressBar;
	int m_progressUpper;
	int m_progressPos;
	float m_percent;

	COLORREF m_clrText[2];
	COLORREF m_clrBkgnd[2];
	COLORREF m_clrPercent[2];
	COLORREF m_clrPercentText[2];

};

#endif // _PROGRESSLISTVIEWCTRL_H