#if !defined(AFX_WAITINGANIM_H__20080601_4222_AC05_C5F6_0080AD509054__INCLUDED_)
#define AFX_WAITINGANIM_H__20080601_4222_AC05_C5F6_0080AD509054__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// Spinning Wheel Animation
//
// Copyright (c) 2008 Bjarke Viksoe.
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
  #error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
  #error WaitingAnim.h requires atlapp.h to be included first
#endif

#ifndef __ATLCTRLS_H__
  #error WaitingAnim.h requires atlctrls.h to be included first
#endif

#if (_WIN32_IE < 0x0600)
  #error WaitingAnim.h requires _WIN32_IE >= 0x0600
#endif



template< class T, class TBase = CWindow, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CWaitingAnimImpl : public CWindowImpl< T, TBase, TWinTraits >
{
public:
   enum { ANIM_TIMERID = 810 };

   CWaitingAnimImpl() : m_dwStartTick(0), m_dwDelay(50)
   {
   }

   CBitmap m_bmpAnim;
   BITMAP m_BmpInfo;
   DWORD m_dwStartTick;
   DWORD m_dwDelay;

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd == NULL);
      ATLASSERT(::IsWindow(hWnd));
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void Start()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      SetTimer(ANIM_TIMERID, m_dwDelay);
   }
   void Stop()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      KillTimer(ANIM_TIMERID);
   }
   void SetAnimDelay(DWORD dwDelayMS)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      m_dwDelay = dwDelayMS;
      SetTimer(ANIM_TIMERID, dwDelayMS);
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));

      _ExtractBitmap();

      int cxy = abs(m_BmpInfo.bmHeight);
      ResizeClient(cxy, cxy);
   }

   void _ExtractBitmap()
   {
      if( !m_bmpAnim.IsNull() ) m_bmpAnim.DeleteObject();

      // First attempt to load the resource bitmap from the ieframe.dll.
      // The animation might be located in the Internet Explorer resource.
      HINSTANCE hInst = ::LoadLibraryEx(_T("IEFrame.dll"), NULL, LOAD_LIBRARY_AS_DATAFILE);
      if( hInst != NULL ) {
         m_bmpAnim = (HBITMAP) ::LoadImage(hInst, MAKEINTRESOURCE(625), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION);
         ::FreeLibrary(hInst);
      }

      // If not found, we'll fall back to a saved version
      if( m_bmpAnim.IsNull() ) {
		m_bmpAnim = (HBITMAP) ::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDB_WAITING), IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_CREATEDIBSECTION);
      }

      m_bmpAnim.GetBitmap(&m_BmpInfo);
   }

   // Message map and handlers

   BEGIN_MSG_MAP(CWaitingAnimImpl)
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_TIMER, OnTimer)
      MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
      MESSAGE_HANDLER(WM_PAINT, OnPaint)
      MESSAGE_HANDLER(WM_PRINTCLIENT, OnPaint)
   END_MSG_MAP()

   LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      _Init();
      return 0;
   }

   LRESULT OnTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
   {
      if( wParam == ANIM_TIMERID )
      {
         CClientDC dc = m_hWnd;
         DoPaint(dc.m_hDC);
      }
      bHandled = FALSE;
      return 0;
   }

   LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      return 1;
   }

   LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
   {
      T* pT = static_cast<T*>(this);
      if( wParam != NULL )
      {
         pT->DoPaint((HDC)wParam);
      }
      else
      {
         CPaintDC dc(m_hWnd);
         pT->DoPaint(dc.m_hDC);
      }
      return 0;
   }

   void DoPaint(CDCHandle dc)
   {
      RECT rc = { 0 };
      GetClientRect(&rc); 

      HBRUSH hBrush = (HBRUSH)::SendMessage(GetParent(), WM_CTLCOLORSTATIC, (WPARAM)dc.m_hDC, (LPARAM)m_hWnd);
      if( hBrush != NULL ) dc.FillRect(&rc, hBrush);

      if( m_bmpAnim.IsNull() ) return;

      DWORD dwTick = ::GetTickCount();
      if( m_dwStartTick == 0 ) m_dwStartTick = dwTick;
      if( dwTick < m_dwStartTick ) m_dwStartTick = dwTick;

      int cxy = abs(m_BmpInfo.bmHeight);
      int nImages = m_BmpInfo.bmWidth / cxy;
      int iFrame = ((dwTick - m_dwStartTick) / m_dwDelay) % nImages;

      CDC dcCompat;
      dcCompat.CreateCompatibleDC(dc);
      HBITMAP hOldBitmap = dcCompat.SelectBitmap(m_bmpAnim);
      BLENDFUNCTION bf;
      bf.BlendOp = AC_SRC_OVER; 
      bf.BlendFlags = 0; 
      bf.AlphaFormat = AC_SRC_ALPHA;
      bf.SourceConstantAlpha = 255;
      BOOL bRes = ::AlphaBlend(dc, rc.left, rc.top, cxy, cxy, dcCompat, iFrame * cxy, 0, cxy, cxy, bf);
      dcCompat.SelectBitmap(hOldBitmap);
   }
};


class CWaitingAnimCtrl : public CWaitingAnimImpl<CWaitingAnimCtrl>
{
public:
   DECLARE_WND_CLASS(_T("WTL_WaitingAnim"))
};


#endif // !defined(AFX_WAITINGANIM_H__20080601_4222_AC05_C5F6_0080AD509054__INCLUDED_)

