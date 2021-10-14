#pragma once

#include <atlctrls.h>
#include <atlmisc.h>
#include <atltheme.h>

template<typename T, typename TBase = CProgressBarCtrl, typename TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CTextProgressBarCtrlT : public CWindowImpl<T, TBase, TWinTraits>, public CThemeImpl<T>
{
public:
	CTextProgressBarCtrlT()
	{
		SetThemeClassList(L"PROGRESS");
	}

	BOOL SubclassWindow(HWND hWnd)
	{
		BOOL result = CWindowImpl<T, TBase, TWinTraits>::SubclassWindow(hWnd);
		if (result)
		{
			if (GetThemeClassList() != NULL)
				OpenThemeData();
		}

		return result;
	}

	BEGIN_MSG_MAP(CTextProgressBarCtrl)
		CHAIN_MSG_MAP(CThemeImpl<CTextProgressBarCtrl>)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	// message handlers

	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		RECT rc;
		GetClientRect(&rc);

		CPaintDC dc(m_hWnd);

		if (rc.right > rc.left && rc.bottom > rc.top)
		{
			CMemoryDC dcMem(dc.m_hDC, rc);

			if (IsAppThemed())
				DrawThemedProgressBar(dcMem, rc);
			else
				DrawClassicProgressBar(dcMem, rc);
			
			DrawText(dcMem, rc);
		}

		bHandled = TRUE;
		return 1;
	}

	LRESULT OnEraseBkgnd(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		return 1;    // we painted the background
	}


protected:
	
	void CalcFillRect(CRect& rc)
	{
		int nLower = 0, nUpper = 0;
		GetRange(nLower, nUpper);
		if (nLower >= nUpper) nLower = nUpper - 1;
		int nPos = GetPos();

		if (nPos < nLower) nPos = nLower;
		if (nPos > nUpper) nPos = nUpper;

		DWORD dwStyle = GetStyle();
		BOOL bVertical = (dwStyle & PBS_VERTICAL) == PBS_VERTICAL;

		if (bVertical)
			rc.top = rc.bottom - (rc.bottom - rc.top) * (nPos - nLower) / (nUpper - nLower);
		else
			rc.right = rc.left + (rc.right - rc.left) * (nPos - nLower) / (nUpper - nLower);
	}

	void DrawThemedProgressBar(CMemoryDC& dc, const CRect& rc)
	{
		DrawThemeBackground(dc, PP_BAR, 0, &rc, NULL);

		CRect rcFill = rc;
		CalcFillRect(rcFill);
		DrawThemeBackground(dc, PP_FILL, PBFS_NORMAL, &rcFill, NULL);
	}

	void DrawClassicProgressBar(CMemoryDC& dc, const CRect& rc)
	{
		CRect rcEdge = rc;
		dc.DrawEdge(&rcEdge, EDGE_ETCHED, BF_RECT);

		CRect rc2 = rc;
		rc2.DeflateRect(1, 1, 1, 1);
		dc.FillSolidRect(&rc2, ::GetSysColor(COLOR_BTNFACE));

		CRect rcFill = rc2;
		if (rcFill.right > rcFill.left && rcFill.bottom > rcFill.top)
		{
			CalcFillRect(rcFill);
			dc.FillSolidRect(&rcFill, ::GetSysColor(COLOR_HIGHLIGHT));
		}
	}

	// draw text on the bar
	void DrawText(CMemoryDC& dc, const CRect& rc)
	{
		CString text;
		if (GetWindowText(text) > 0)
		{
			int oldBkMode = dc.SetBkMode(TRANSPARENT);

			HFONT hFont = GetFont();
			if (NULL == hFont)
			{
				hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			}
			HFONT oldFont = dc.SelectFont(hFont);
			CRect rcText = rc;
			dc.DrawText((LPCTSTR)text, -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			dc.SelectFont(oldFont);

			dc.SetBkMode(oldBkMode);
		}
	}
};

class CTextProgressBarCtrl : public CTextProgressBarCtrlT<CTextProgressBarCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_progressbar"), GetWndClassName())
};
