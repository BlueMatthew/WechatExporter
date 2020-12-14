#if !defined(AFX_COLOREDCONTROLS_H__20020226_0D16_1B29_088A_0080AD509054__INCLUDED_)
#define AFX_COLOREDCONTROLS_H__20020226_0D16_1B29_088A_0080AD509054__INCLUDED_

#pragma once

/////////////////////////////////////////////////////////////////////////////
// Colored Control - Windows Controls with colours
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2002 Bjarke Viksoe.
//
// This file include the following controls:
//   CColoredDialog
//   CColoredStaticCtrl
//   CColoredEditCtrl
//   CColoredButtonCtrl
//   CColoredComboBoxCtrl
//   CColoredTabCtrl
//   CColoredListViewCtrl
//   CColoredTreeViewCtrl
//
// Add the following macro to the parent's message map:
//   REFLECT_NOTIFICATIONS()
//
// This code may be used in compiled form in any way you desire. This
// source file may be redistributed by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. 
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability if it causes any damage to you or your
// computer whatsoever. It's free, so don't hassle me about it.
//
// Beware of bugs.
//

#ifndef __cplusplus
  #error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLCTRLS_H__
  #error ColoredControls.h requires atlctrls.h to be included first
#endif



/////////////////////////////////////////////////////////////////////////////
// Dialog with new colour or bitmap background

// To use this class: Derive from CColoredDialog<Base> and then
// chain the message map CHAIN_MSG_MAP(CColoredDialog<Base>)...
template< class T >
class CColoredDialog
{
public:
   CBrush m_brBack;

   // Operations

   BOOL SetBitmap(UINT nRes)
   {
      // Load the bitmap and create a new brush
      CBitmap bmBack;
      bmBack.LoadBitmap(nRes);
      if( !m_brBack.IsNull() ) m_brBack.DeleteObject();
      m_brBack.CreatePatternBrush(bmBack);
      // Repaint
      T* pT = static_cast<T*>(this);
      pT->Invalidate();
   }
   void SetColor(COLORREF clr)
   {
      // Create a new brush from the color
      if( !m_brBack.IsNull() ) m_brBack.DeleteObject();
      m_brBack.CreateSolidBrush(clr);
      // Repaint
      T* pT = static_cast<T*>(this);
      pT->Invalidate();
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredDialog)
      MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtlColorDlg)
   END_MSG_MAP()

   LRESULT OnCtlColorDlg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      T* pT = static_cast<T*>(this);
      if( m_brBack.IsNull() ) return pT->DefWindowProc();
      return (LRESULT) (HBRUSH) m_brBack;
   }
};


/////////////////////////////////////////////////////////////////////////////
// CColoredStaticCtrl - A Static control with new colours

template< class T, class TBase = CStatic, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredStaticImpl : 
   public CWindowImpl< T, TBase, TWinTraits >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   // Color codes for Static control
   COLORREF      m_clrBackground;
   COLORREF      m_clrNormalText;
   COLORREF      m_clrNormalBk;
   COLORREF      m_clrDisabledText;
   COLORREF      m_clrDisabledBk;
   // User-defined colours needs a brush
   CBrush        m_brBackground;
   CBrush        m_brNormalBk;
   CBrush        m_brDisabledBk;
   // We use a brush-handle when no custom colours have been assigned
   // because system-color brushes must not be destroyed...
   CBrushHandle  m_hbrNormalBk;
   CBrushHandle  m_hbrDisabledBk;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[16];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetNormalColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_BTNTEXT);
      m_clrNormalText = clrText;
      // Background
      if( !m_brNormalBk.IsNull() ) m_brNormalBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrNormalBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrNormalBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrNormalBk = clrBack;
         m_brNormalBk.CreateSolidBrush(clrBack);
         m_hbrNormalBk = m_brNormalBk;
      }
      // Repaint
      Invalidate();
   }
   void SetDisabledColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_GRAYTEXT);
      m_clrDisabledText = clrText;
      // Background
      if( !m_brDisabledBk.IsNull() ) m_brDisabledBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrDisabledBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrDisabledBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrDisabledBk = clrBack;
         m_brDisabledBk.CreateSolidBrush(clrBack);
         m_hbrDisabledBk = m_brDisabledBk;
      }
      // Repaint
      Invalidate();
   }
   void SetBkColor(COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Background
      if( !m_brBackground.IsNull() ) m_brBackground.DeleteObject();
      m_clrBackground = clrBack;
      m_brBackground.CreateSolidBrush(clrBack);
      // Repaint
      Invalidate();
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      COLORREF clrNone = CLR_INVALID;
      SetNormalColors( clrNone, clrNone );
      SetDisabledColors( clrNone, clrNone );
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredStaticImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnCtlColorStatic)
      MESSAGE_HANDLER(OCM_CTLCOLORDLG, OnCtlColorDlg)
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }

   LRESULT OnCtlColorStatic(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      BOOL bEnabled = IsWindowEnabled();
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor( bEnabled ? m_clrNormalText : m_clrDisabledText );
      dc.SetBkColor( bEnabled ? m_clrNormalBk : m_clrDisabledBk );
      return (LRESULT) (HBRUSH) ( bEnabled ? m_hbrNormalBk : m_hbrDisabledBk );
   }
   LRESULT OnCtlColorDlg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      T* pT = static_cast<T*>(this);
      if( m_brBackground.IsNull() ) return pT->DefWindowProc();
      return (LRESULT) (HBRUSH) m_brBackground;
   }
};

class CColoredStaticCtrl : public CColoredStaticImpl<CColoredStaticCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredStatic"), GetWndClassName())  
};

class CColoredCheckboxCtrl : public CColoredStaticImpl<CColoredCheckboxCtrl, CButton>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredCheckbox"), GetWndClassName())  
};

class CColoredOptionCtrl : public CColoredStaticImpl<CColoredOptionCtrl, CButton>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredOption"), GetWndClassName())  
};



/////////////////////////////////////////////////////////////////////////////
// CColoredEditCtrl - An Edit control with new colours

template< class T, class TBase = CEdit, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredEditImpl : 
   public CWindowImpl< T, TBase, TWinTraits >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   // Color codes for Edit control
   COLORREF      m_clrNormalText;
   COLORREF      m_clrNormalBk;
   COLORREF      m_clrDisabledText;
   COLORREF      m_clrDisabledBk;
   COLORREF      m_clrReadOnlyText;
   COLORREF      m_clrReadOnlyBk;
   // User-defined colours needs a brush
   CBrush        m_brNormalBk;
   CBrush        m_brDisabledBk;
   CBrush        m_brReadOnlyBk;
   // We use a brush-handle when no custom colours have been assigned
   // because system-color brushes must not be destroyed...
   CBrushHandle  m_hbrNormalBk;
   CBrushHandle  m_hbrDisabledBk;
   CBrushHandle  m_hbrReadOnlyBk;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[16];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetNormalColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_WINDOWTEXT);
      m_clrNormalText = clrText;
      // Background
      if( !m_brNormalBk.IsNull() ) m_brNormalBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrNormalBk = ::GetSysColor(COLOR_WINDOW);
         m_hbrNormalBk = ::GetSysColorBrush(COLOR_WINDOW);
      }
      else {
         m_clrNormalBk = clrBack;
         m_brNormalBk.CreateSolidBrush(clrBack);
         m_hbrNormalBk = m_brNormalBk;
      }
      // Repaint
      Invalidate();
   }
   void SetDisabledColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_GRAYTEXT);
      m_clrDisabledText = clrText;
      // Background
      if( !m_brDisabledBk.IsNull() ) m_brDisabledBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrDisabledBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrDisabledBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrDisabledBk = clrBack;
         m_brDisabledBk.CreateSolidBrush(clrBack);
         m_hbrDisabledBk = m_brDisabledBk;
      }
      // Repaint
      Invalidate();
   }
   void SetReadOnlyColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_BTNTEXT);
      m_clrReadOnlyText = clrText;
      // Background
      if( !m_brReadOnlyBk.IsNull() ) m_brReadOnlyBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrReadOnlyBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrReadOnlyBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrReadOnlyBk = clrBack;
         m_brReadOnlyBk.CreateSolidBrush(clrBack);
         m_hbrReadOnlyBk = m_brReadOnlyBk;
      }
      // Repaint
      Invalidate();
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      COLORREF clrNone = CLR_INVALID;
      SetNormalColors( clrNone, clrNone );
      SetDisabledColors( clrNone, clrNone );
      SetReadOnlyColors( clrNone, clrNone );
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredEditImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(OCM_CTLCOLOREDIT, OnCtlColorEdit)
      MESSAGE_HANDLER(OCM_CTLCOLORMSGBOX, OnCtlColorEdit)
      MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnCtlColorStatic)
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }

   LRESULT OnCtlColorEdit(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor( m_clrNormalText );
      dc.SetBkColor( m_clrNormalBk );
      return (LRESULT) (HBRUSH) m_hbrNormalBk;
   }

   LRESULT OnCtlColorStatic(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      // Microsoft Q130952 explains why we also need this
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      if( IsWindowEnabled() ) {
         ATLASSERT(GetStyle() & ES_READONLY);
         dc.SetTextColor( m_clrReadOnlyText );
         dc.SetBkColor( m_clrReadOnlyBk );
         return (LRESULT) (HBRUSH) m_hbrReadOnlyBk;
      }
      else {
         dc.SetTextColor( m_clrDisabledText );
         dc.SetBkColor( m_clrDisabledBk );
         return (LRESULT) (HBRUSH) m_hbrDisabledBk;
      }
   }
};

class CColoredEditCtrl : public CColoredEditImpl<CColoredEditCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredEdit"), GetWndClassName())  
};


/////////////////////////////////////////////////////////////////////////////
// CColoredButtonCtrl - The Button control with custom colours

template< class T, class TBase = CButton, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredButtonImpl : 
   public CWindowImpl< T, TBase, TWinTraits >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   // Color codes for Button control
   COLORREF      m_clrText;
   COLORREF      m_clrBackUp;
   COLORREF      m_clrBackDown;
   COLORREF      m_clrBackDisabled;
   // Image support
   CImageList    m_ImageList;
   UINT          m_nImage;
   UINT          m_nSelImage;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[20];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetTextColor(COLORREF clrText)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_BTNTEXT);
      m_clrText = clrText;
      // Repaint
      Invalidate();
   }
   void SetBkColor(COLORREF clrUp, COLORREF clrDown=-1)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      if( clrUp == CLR_INVALID ) clrUp = ::GetSysColor(COLOR_BTNFACE);
      if( clrDown == CLR_INVALID ) clrDown = clrUp;
      m_clrBackUp = clrUp;
      m_clrBackDown = clrDown;
      // Repaint
      Invalidate();
   }
   void SetDisabledColor(COLORREF clrDisabled)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      if( clrDisabled == CLR_INVALID ) clrDisabled = ::GetSysColor(COLOR_BTNFACE);
      m_clrBackDisabled = clrDisabled;
      // Repaint
      Invalidate();
   }
   void SetImageList(HIMAGELIST hImageList)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      m_ImageList = hImageList;
   }
   void SetImage(UINT nImage)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(!m_ImageList.IsNull());
      m_nImage = nImage;
   }
   void SetSelImage(UINT nImage)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(!m_ImageList.IsNull());
      m_nSelImage = nImage;
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));

      // We need this style to prevent Windows from painting the button
      ModifyStyle(0, BS_OWNERDRAW);
      
      COLORREF clrNone = CLR_INVALID;
      SetTextColor( clrNone );
      SetBkColor( clrNone, clrNone );
      SetDisabledColor( clrNone );
      m_nImage = m_nSelImage = (UINT)-1;
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredButtonImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
      MESSAGE_HANDLER(WM_LBUTTONDBLCLK, OnLButtonDblClick)
      MESSAGE_HANDLER(OCM_DRAWITEM, OnDrawItem)
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }
   LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      return 1;   // no background needed
   }
   LRESULT OnLButtonDblClick(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      return DefWindowProc(WM_LBUTTONDOWN, wParam, lParam);
   }   
   LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT) lParam;
      ATLASSERT(lpDIS->CtlType==ODT_BUTTON);
      CDCHandle dc = lpDIS->hDC;
      RECT rc = lpDIS->rcItem;
      DWORD dwStyle = GetStyle();
      bool bSelected = (lpDIS->itemState & ODS_SELECTED) != 0;
      bool bDisabled = (lpDIS->itemState & (ODS_DISABLED|ODS_GRAYED)) != 0;

      COLORREF clrBack = bSelected ? m_clrBackDown : m_clrBackUp;
      if( bDisabled ) clrBack = m_clrBackDisabled;
      dc.FillSolidRect(&rc, clrBack);

      // Draw edge
      if( dwStyle & BS_FLAT ) {
         dc.DrawEdge(&rc, bSelected ? BDR_SUNKENOUTER : BDR_RAISEDINNER, BF_RECT);
      }
      else {
         dc.DrawEdge(&rc, bSelected ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
      }
      ::InflateRect(&rc, -2 * ::GetSystemMetrics(SM_CXEDGE), -2 * ::GetSystemMetrics(SM_CYEDGE));

      // Draw focus rectangle
      if( lpDIS->itemState & ODS_FOCUS ) dc.DrawFocusRect(&rc);
      ::InflateRect(&rc, -1, -1);

      // Offset when button is selected
      if( bSelected ) ::OffsetRect(&rc, 1, 1);

      // Draw image
      UINT nImage = m_nImage;
      if( bSelected && m_nSelImage != -1 ) nImage = m_nSelImage;
      if( !m_ImageList.IsNull() && nImage != -1 ) {
         POINT pt = { rc.left, rc.top-1 };
         SIZE sizeIcon;
         m_ImageList.GetIconSize(sizeIcon);
         if( bDisabled ) {
            //m_ImageList.DrawEx(nImage, dc, pt.x, pt.y, sizeIcon.cx, sizeIcon.cy, CLR_NONE, ::GetSysColor(COLOR_GRAYTEXT), ILD_BLEND50 | ILD_TRANSPARENT);
            HICON hIcon = m_ImageList.GetIcon(nImage);
            dc.DrawState(pt, sizeIcon, hIcon, DST_ICON | DSS_DISABLED);
            ::DestroyIcon(hIcon);
         }
         else {
            m_ImageList.Draw(dc, nImage, pt, bDisabled ? ILD_BLEND25 : ILD_TRANSPARENT);
         }
         rc.left += sizeIcon.cx + 3;
      }

      // Draw text
      UINT nLen = GetWindowTextLength();
      LPTSTR pstr = (LPTSTR) _alloca( (nLen+1)*sizeof(TCHAR) );
      GetWindowText(pstr, nLen+1);
      UINT uFlags = 0;
      if( dwStyle & BS_LEFT ) uFlags |= DT_LEFT;
      else if( dwStyle & BS_RIGHT ) uFlags |= DT_RIGHT;
      else if( dwStyle & BS_CENTER ) uFlags |= DT_CENTER;
      else uFlags |= DT_CENTER;
      if( dwStyle & BS_TOP ) uFlags |= DT_TOP;
      else if( dwStyle & BS_BOTTOM ) uFlags |= DT_BOTTOM;
      else if( dwStyle & BS_VCENTER ) uFlags |= DT_VCENTER;
      else uFlags |= DT_VCENTER;
      if( (dwStyle & BS_MULTILINE) == 0 ) uFlags |= DT_SINGLELINE;
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor(bDisabled ? ::GetSysColor(COLOR_GRAYTEXT) : m_clrText);
      dc.DrawText(pstr, nLen, &rc, uFlags);

      return TRUE;
   }
};

class CColoredButtonCtrl : public CColoredButtonImpl<CColoredButtonCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredButton"), GetWndClassName())  
};


/////////////////////////////////////////////////////////////////////////////
// CColoredComboBoxCtrl - The ComboBox control with custom colours

template< class T, class TBase = CComboBox, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredComboBoxImpl : 
   public CWindowImpl< T, TBase, TWinTraits >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   // Color codes for ComboBox control
   COLORREF      m_clrListText;
   COLORREF      m_clrListBk;
   COLORREF      m_clrSelectedListText;
   COLORREF      m_clrSelectedListBk;
   COLORREF      m_clrDisabledText;
   COLORREF      m_clrDisabledBk;
   COLORREF      m_clrEditText;
   COLORREF      m_clrEditBk;
   // User-defined colours needs a brush
   CBrush        m_brListBk;
   CBrush        m_brSelectedListBk;
   CBrush        m_brDisabledBk;
   CBrush        m_brEditBk;
   // We use a brush-handle when no custom colours have been assigned
   // because system-color brushes must not be destroyed...
   CBrushHandle  m_hbrListBk;
   CBrushHandle  m_hbrSelectedListBk;
   CBrushHandle  m_hbrDisabledBk;
   CBrushHandle  m_hbrEditBk;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[16];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetListColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_WINDOWTEXT);
      m_clrListText = clrText;
      // Background
      if( !m_brListBk.IsNull() ) m_brListBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrListBk = ::GetSysColor(COLOR_WINDOW);
         m_hbrListBk = ::GetSysColorBrush(COLOR_WINDOW);
      }
      else {
         m_clrListBk = clrBack;
         m_brListBk.CreateSolidBrush(clrBack);
         m_hbrListBk = m_brListBk;
      }
      // Repaint
      Invalidate();
   }
   void SetDisabledColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_GRAYTEXT);
      m_clrDisabledText = clrText;
      // Background
      if( !m_brDisabledBk.IsNull() ) m_brDisabledBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrDisabledBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrDisabledBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrDisabledBk = clrBack;
         m_brDisabledBk.CreateSolidBrush(clrBack);
         m_hbrDisabledBk = m_brDisabledBk;
      }
      // Repaint
      Invalidate();
   }
   void SetEditColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_BTNTEXT);
      m_clrEditText = clrText;
      // Background
      if( !m_brEditBk.IsNull() ) m_brEditBk.DeleteObject();
      if( clrBack == CLR_INVALID ) {
         m_clrEditBk = ::GetSysColor(COLOR_BTNFACE);
         m_hbrEditBk = ::GetSysColorBrush(COLOR_BTNFACE);
      }
      else {
         m_clrEditBk = clrBack;
         m_brEditBk.CreateSolidBrush(clrBack);
         m_hbrEditBk = m_brEditBk;
      }
      // Repaint
      Invalidate();
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      COLORREF clrNone = CLR_INVALID;
      SetListColors( clrNone, clrNone );
      SetDisabledColors( clrNone, clrNone );
      SetEditColors( clrNone, clrNone );
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredComboBoxImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorEdit)
      MESSAGE_HANDLER(OCM_CTLCOLOREDIT, OnCtlColorEdit)
      MESSAGE_HANDLER(WM_CTLCOLORMSGBOX, OnCtlColorEdit)
      MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnCtlColorListBox)
      MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnCtlColorStatic)
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }

   LRESULT OnCtlColorEdit(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor( m_clrEditText );
      dc.SetBkColor( m_clrEditBk );
      return (LRESULT) (HBRUSH) m_hbrEditBk;
   }
   LRESULT OnCtlColorListBox(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      dc.SetTextColor( m_clrListText );
      dc.SetBkColor( m_clrListBk );
      return (LRESULT) (HBRUSH) m_hbrListBk;
   }
   LRESULT OnCtlColorStatic(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      CDCHandle dc( (HDC) wParam );
      dc.SetBkMode(TRANSPARENT);
      if( IsWindowEnabled() ) {
         dc.SetTextColor( m_clrEditText );
         dc.SetBkColor( m_clrEditBk );
         return (LRESULT) (HBRUSH) m_hbrEditBk;
      }
      else {
         dc.SetTextColor( m_clrDisabledText );
         dc.SetBkColor( m_clrDisabledBk );
         return (LRESULT) (HBRUSH) m_hbrDisabledBk;
      }
   }
};

class CColoredComboBoxCtrl : public CColoredComboBoxImpl<CColoredComboBoxCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredComboBox"), GetWndClassName())  
};



/////////////////////////////////////////////////////////////////////////////
// CColoredTabCtrl - A Tab control with tab colours

template< class T, class TBase = CTabCtrl, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredTabImpl : 
   public CWindowImpl< T, TBase, TWinTraits >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   // Color codes for Tab control
   COLORREF m_clrBackground;
   COLORREF m_clrNormalText;
   COLORREF m_clrNormalBk;
   COLORREF m_clrDisabledText;
   COLORREF m_clrDisabledBk;
   COLORREF m_clrInactiveText;
   COLORREF m_clrInactiveBk;
   COLORREF m_clrHighlightText;
   COLORREF m_clrHighlightBk;
   HFONT    m_hInactiveFont;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[20];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetBackgroundColor(COLORREF clrBack)
   {
      if( clrBack == CLR_INVALID ) clrBack = ::GetSysColor(COLOR_BTNFACE);
      m_clrBackground = clrBack;
      // Repaint
      Invalidate();
   }
   void SetNormalColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_WINDOWTEXT);
      m_clrNormalText = clrText;
      // Background
      if( clrBack == CLR_INVALID ) clrBack = ::GetSysColor(COLOR_BTNFACE);
      m_clrNormalBk = clrBack;
      // Repaint
      Invalidate();
   }
   void SetDisabledColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_GRAYTEXT);
      m_clrDisabledText = clrText;
      // Background
      if( clrBack == CLR_INVALID ) clrBack = ::GetSysColor(COLOR_BTNFACE);
      m_clrDisabledBk = clrBack;
      // Repaint
      Invalidate();
   }
   void SetInactiveColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_BTNTEXT);
      m_clrInactiveText = clrText;
      // Background
      if( clrBack == CLR_INVALID ) clrBack = ::GetSysColor(COLOR_BTNFACE);
      m_clrInactiveBk = clrBack;
      // Repaint
      Invalidate();
   }
   void SetHighlightColors(COLORREF clrText, COLORREF clrBack)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // Text 
#if(WINVER >= 0x0500)
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_HOTLIGHT);
#else
      if( clrText == CLR_INVALID ) clrText = ::GetSysColor(COLOR_HIGHLIGHT);
#endif
      m_clrHighlightText = clrText;
      // Background
      if( clrBack == CLR_INVALID ) clrBack = ::GetSysColor(COLOR_BTNFACE);
      m_clrHighlightBk = clrBack;
      // Repaint
      Invalidate();
   }
   void SetInactiveFont(HFONT hFont)
   {
      m_hInactiveFont = hFont;
      // Repaint
      Invalidate();
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT((GetStyle() & (TCS_BUTTONS|TCS_VERTICAL))==0);

      // We're drawing the text...
      ModifyStyle(0, TCS_OWNERDRAWFIXED);

      m_hInactiveFont = NULL;
      COLORREF clrNone = CLR_INVALID;
      SetBackgroundColor( clrNone );
      SetNormalColors( clrNone, clrNone );
      SetDisabledColors( clrNone, clrNone );
      SetInactiveColors( clrNone, clrNone );
      SetHighlightColors( clrNone, clrNone );
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredTabImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_PAINT, OnPaint)
      MESSAGE_HANDLER(WM_PRINTCLIENT, OnPaint)
      MESSAGE_HANDLER(OCM_DRAWITEM, OnDrawItem)
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }

   LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT) lParam;
      ATLASSERT(lpDIS->CtlType==ODT_TAB);
      ATLASSERT(lpDIS->itemAction & ODA_DRAWENTIRE);
      CDCHandle dc = lpDIS->hDC;
      RECT rc = lpDIS->rcItem;

      DWORD dwStyle = GetStyle();
      HFONT hFont = GetFont();

      TCHAR szText[128];
      TCITEM itm = { 0 };
      itm.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
      itm.pszText = szText;
      itm.cchTextMax = sizeof(szText)/sizeof(TCHAR);
      itm.dwStateMask = (DWORD) -1;
      GetItem(lpDIS->itemID, &itm);

      COLORREF clrText;
      COLORREF clrBack;
      int cyOffset = 0;
      if( lpDIS->itemState & ODS_DISABLED ) {
         clrText = m_clrDisabledText;
         clrBack = m_clrDisabledBk;
      }
      else if( itm.dwState & TCIS_HIGHLIGHTED ) {
         clrText = m_clrHighlightText;
         clrBack = m_clrHighlightBk;
      }
      else if( lpDIS->itemState & ODS_SELECTED ) {
         clrText = m_clrNormalText;
         clrBack = m_clrNormalBk;
         cyOffset = 1;
      }
      else {
         clrText = m_clrInactiveText;
         clrBack = m_clrInactiveBk;
         if( m_hInactiveFont != NULL ) hFont = m_hInactiveFont;
      }

      dc.FillSolidRect(&rc, clrBack);

      if( itm.iImage != -1 ) {
         CImageList iml = GetImageList();
         int cxImage, cyImage;
         iml.GetIconSize(cxImage, cyImage);
         POINT pt = { rc.left + 5, rc.top + 1 + ((rc.bottom-rc.top)/2-(cyImage)/2) };
         iml.Draw(dc, itm.iImage, pt, ILD_TRANSPARENT );
         rc.left += cxImage;
      }

      HFONT hOldFont = dc.SelectFont(hFont);
      dc.SetBkColor(clrBack),
      dc.SetTextColor(clrText);
      dc.SetBkMode(TRANSPARENT);
      UINT dwFlags = DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
      if( dwStyle & TCS_FORCELABELLEFT ) {
         dwFlags |= DT_LEFT; 
         rc.left += 3;
      }
      else {
         dwFlags |= DT_CENTER;
      }
      dc.DrawText(itm.pszText, -1, &rc, dwFlags);
      dc.SelectFont(hOldFont);

      return TRUE;
   }

   LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
   {
      T* pT = static_cast<T*>(this);
      if( wParam != NULL ) {
         RECT rc;
         GetClientRect(&rc);
         pT->DoPaint( (HDC) wParam );
         // Let the tab-control paint itself
         bHandled = FALSE;
      }
      else {
         PAINTSTRUCT m_ps;
         HDC hDC = ::BeginPaint(m_hWnd, &m_ps);
         pT->DoPaint(hDC);
         // Let the tab-control paint itself
         DefWindowProc(WM_PRINTCLIENT, (WPARAM) hDC, PRF_CLIENT);
         ::EndPaint(m_hWnd, &m_ps);
      }
      return 0;
   }

   void DoPaint(CDCHandle dc)
   {
      // Calculate the upper box, so we can paint a
      // different background colour. We need to do the calculation
      // because of a possible multi-line tab-control...
      RECT rcWin;
      GetWindowRect(&rcWin);
      RECT rcClient = rcWin;
      AdjustRect(FALSE, &rcClient);
      RECT rcTop = { 0, 0, rcWin.right-rcWin.left, rcClient.top-rcWin.top };
      dc.FillSolidRect(&rcTop, m_clrBackground);
      // TODO: If you want to paint the tab client-area a different
      //       colour, this is the place to do it...
   }
};

class CColoredTabCtrl : public CColoredTabImpl<CColoredTabCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredTab"), GetWndClassName())  
};


/////////////////////////////////////////////////////////////////////////////
// CColoredListViewCtrl - A ListView control with individual item colours

template< class T, class TBase = CListViewCtrl, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredListViewImpl : 
   public CWindowImpl< T, TBase, TWinTraits >,
   public CCustomDraw< T >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   typedef struct {
      COLORREF clrText;
      COLORREF clrBackground;
      HFONT    hFont;
      LPARAM   lParam;
   } LVCOLORPARAM;
   int m_nColumns;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[16];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   int InsertItem(UINT nMask, int nItem, LPCTSTR lpszItem, UINT nState, UINT nStateMask, int nImage=-1, LPARAM lParam=0)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      // You must have initialized columns before calling this!
      // And you are not allowed to add columns once the list is populated!
      CHeaderCtrl ctrlHeader = GetHeader();
      if( m_nColumns == 0 ) m_nColumns = ctrlHeader.GetItemCount();
      ATLASSERT(m_nColumns > 0);
      ATLASSERT(ctrlHeader.GetItemCount()==m_nColumns);
      // Create a place-holder of property controls for each subitem...
      LVCOLORPARAM* pParams;
      ATLTRY( pParams = new LVCOLORPARAM[m_nColumns] );
      ATLASSERT(pParams);
      if( pParams == NULL ) return -1;
      ::FillMemory(pParams, sizeof(LVCOLORPARAM) * m_nColumns, -1);
      // Finally create the listview item itself...
      if( nItem == -1 ) nItem = GetItemCount();
      nMask |= LVIF_PARAM;
      pParams[0].lParam = lParam;
      return TBase::InsertItem(nMask, nItem, lpszItem, nState, nStateMask, nImage, (LPARAM) pParams);
   }
   int InsertItem(int nItem, LPCTSTR lpszItem, int nImage)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      return InsertItem(LVIF_TEXT|LVIF_IMAGE, nItem, lpszItem, 0, 0, nImage, 0);
   }
   DWORD_PTR GetItemData(int nItem) const
   {
      ATLASSERT(::IsWindow(m_hWnd));
      LVITEM lvi = { 0 };
      lvi.iItem = nItem;
      lvi.mask = LVIF_PARAM;
      if( GetItem(&lvi) == -1 ) return 0;
      LVCOLORPARAM* pParams = (LVCOLORPARAM*) lvi.lParam;
      return (DWORD_PTR) pParams[0].lParam;
   }
   BOOL SetItemColors(int nItem, int nSubItem, COLORREF clrText, COLORREF clrBk)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      LVITEM lvi = { 0 };
      lvi.iItem = nItem;
      lvi.mask = LVIF_PARAM;
      if( GetItem(&lvi) == -1 ) return FALSE;
      LVCOLORPARAM* pParams = (LVCOLORPARAM*) lvi.lParam;
      ATLASSERT(nSubItem>=0 && nSubItem<GetHeader().GetItemCount());
      pParams[ nSubItem ].clrText = clrText;
      pParams[ nSubItem ].clrBackground = clrBk;
      // Repaint
      RedrawItems(nItem, nItem);
      return TRUE;
   }

   // Unsupported ListView methods

   BOOL SetItemData(int /*nItem*/, DWORD_PTR /*dwData*/)
   {
      ATLASSERT(false); // not supported
      return FALSE;
   }
   int AddItem(int /*nItem*/, int /*nSubItem*/, LPCTSTR /*strItem*/, int /*nImageIndex = -1*/)
   {
      ATLASSERT(false); // not supported
      return -1;
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      m_nColumns = 0;
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredListViewImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      REFLECTED_NOTIFY_CODE_HANDLER(LVN_DELETEITEM, OnDeleteItem)
      CHAIN_MSG_MAP_ALT( CCustomDraw< T >, 1 )
      // Because WTL 3.1 does not support subitem custom drawing.
      // Should be forward-compatible with new versions...
      REFLECTED_NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnNotifyCustomDraw)
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }
   LRESULT OnDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
   {
      ATLASSERT(m_nColumns>0);
      LPNMLISTVIEW pnmlv = (LPNMLISTVIEW) pnmh;
      ATLASSERT(pnmlv->lParam);
      LVCOLORPARAM* pParams = reinterpret_cast<LVCOLORPARAM*>(pnmlv->lParam);
      delete [] pParams;
      return 0;
   }
   LRESULT OnNotifyCustomDraw(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
   {
      T* pT = static_cast<T*>(this);
      pT->SetMsgHandled(FALSE);
      LPNMCUSTOMDRAW lpNMCustomDraw = (LPNMCUSTOMDRAW) pnmh;
      DWORD dwRet = 0;
      switch( lpNMCustomDraw->dwDrawStage ) {
      case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
         dwRet = pT->OnSubItemPrePaint(idCtrl, lpNMCustomDraw);
         pT->SetMsgHandled(TRUE);
         return dwRet;
      }
      bHandled = FALSE;
      return dwRet;
   }

   // Custom painting

   DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
   {
      return CDRF_NOTIFYITEMDRAW;   // We need per-item notifications
   }
   DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
   {
      return CDRF_NOTIFYSUBITEMDRAW; // We need per-subitem notifications
   }
   DWORD OnSubItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
   {
      LPNMLVCUSTOMDRAW lpNMLVCD = (LPNMLVCUSTOMDRAW) lpNMCustomDraw;
      ATLASSERT(lpNMLVCD->nmcd.lItemlParam);
      LVCOLORPARAM* pParams = reinterpret_cast<LVCOLORPARAM*>(lpNMLVCD->nmcd.lItemlParam);     
      const LVCOLORPARAM& param = pParams[ lpNMLVCD->iSubItem ];
      if( param.clrText != CLR_INVALID ) lpNMLVCD->clrText = param.clrText;
      if( param.clrBackground != CLR_INVALID ) lpNMLVCD->clrTextBk = param.clrBackground;
      return CDRF_NEWFONT; // We changed the colors
   }
};

class CColoredListViewCtrl : public CColoredListViewImpl<CColoredListViewCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredListView"), GetWndClassName())  
};


/////////////////////////////////////////////////////////////////////////////
// CColoredTreeViewCtrl - A TreeView with item colours

template< class T, class TBase = CTreeViewCtrl, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CColoredTreeViewImpl : 
   public CWindowImpl< T, TBase, TWinTraits >,
   public CCustomDraw< T >
{
public:
   DECLARE_WND_SUPERCLASS(NULL, TBase::GetWndClassName())

   typedef struct tagTVCOLOR {
      COLORREF clrText;
      COLORREF clrBackground;
      COLORREF clrSelText;
      COLORREF clrSelBackground;
      HFONT    hFont;
   } TVCOLOR;
   CSimpleMap< HTREEITEM, TVCOLOR > m_mapColors;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
#ifdef _DEBUG
      // Check class
      TCHAR szBuffer[20];
      if( ::GetClassName(hWnd, szBuffer, (sizeof(szBuffer)/sizeof(TCHAR))-1) ) {
         ATLASSERT(::lstrcmpi(szBuffer, TBase::GetWndClassName())==0);
      }
#endif
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   // Use syntax:
   //   CColoredTreeViewCtrl::TVCOLOR col;
   //   col.clrText = RGB(120,120,120);
   //   col.clrBackground = -1;
   //   col.clrSelText = -1;
   //   col.clrSelBackground = -1;
   //   col.hFont = NULL;
   //   ctrl.SetItemColors(hItem, &col);
   //
   void SetItemColors(HTREEITEM hItem, TVCOLOR* color)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      m_mapColors.Add(hItem, *color);
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CColoredTreeViewImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      REFLECTED_NOTIFY_CODE_HANDLER(TVN_DELETEITEM, OnDeleteItem)
      CHAIN_MSG_MAP_ALT( CCustomDraw< T >, 1 )
      DEFAULT_REFLECTION_HANDLER()
   END_MSG_MAP()

   LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
      _Init();
      return lRes;
   }
   LRESULT OnDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
   {
      LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) pnmh;
      m_mapColors.Remove( pnmtv->itemOld.hItem );
      bHandled = FALSE;
      return 0;
   }

   // Custom painting

   DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/)
   {
      return CDRF_NOTIFYITEMDRAW;   // We need per-item notifications
   }
   DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW lpNMCustomDraw)
   {
      LPNMTVCUSTOMDRAW lpNMTVCD = (LPNMTVCUSTOMDRAW) lpNMCustomDraw;
      HTREEITEM hItem = (HTREEITEM) lpNMTVCD->nmcd.dwItemSpec;
      int iIndex = m_mapColors.FindKey( hItem );
      if( iIndex == -1 ) return CDRF_DODEFAULT;
      // The TreeView allows us to change colours for
      // the selected item also (text only).
      const TVCOLOR& col = m_mapColors.GetValueAt(iIndex);
      if( lpNMTVCD->nmcd.uItemState & CDIS_SELECTED ) {
         if( col.clrSelText != CLR_INVALID ) lpNMTVCD->clrText = col.clrSelText;
         // Selection background colour cannot be set with current
         // versions of the TreeView control.
         if( col.clrSelBackground != CLR_INVALID ) lpNMTVCD->clrTextBk = col.clrSelBackground;
      }
      else {
         if( col.clrText != CLR_INVALID ) lpNMTVCD->clrText = col.clrText;
         if( col.clrBackground != CLR_INVALID ) lpNMTVCD->clrTextBk = col.clrBackground;
      }
      if( col.hFont != NULL ) ::SelectObject(lpNMTVCD->nmcd.hdc, col.hFont);
      return CDRF_NEWFONT; // We changed the colors/font
   }
};

class CColoredTreeViewCtrl : public CColoredTreeViewImpl<CColoredTreeViewCtrl>
{
public:
   DECLARE_WND_SUPERCLASS(_T("WTL_ColoredTreeView"), GetWndClassName())  
};


#endif // !defined(AFX_COLOREDCONTROLS_H__20020226_0D16_1B29_088A_0080AD509054__INCLUDED_)

