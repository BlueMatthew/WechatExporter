#if !defined(AFX_LOGLISTBOX_H_INCLUDED_)
#define AFX_LOGLISTBOX_H_INCLUDED_

#include <vector>
#include <atlctrls.h>
#pragma once

#ifndef __cplusplus
#error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
#error LogListBox.h requires atlapp.h to be included first
#endif

#ifndef __ATLCTRLS_H__
#error LogListBox.h requires atlctrls.h to be included first
#endif

template <class T, class TBase = CListBox, class TWinTraits = ATL::CControlWinTraits>
class ATL_NO_VTABLE CLogListBoxImpl : public ATL::CWindowImpl< T, TBase, TWinTraits >
{
public:
	// DECLARE_WND_SUPERCLASS(_T("WTL_LogListBox"), GetWndClassName())
	DECLARE_WND_SUPERCLASS2(NULL, T, TBase::GetWndClassName())
	// DECLARE_WND_SUPERCLASS2(NULL, CLogListBox, CListBox::GetWndClassName())
	// DECLARE_WND_SUPERCLASS(_T("WTL_LogListBox"), GetWndClassName())

	// Message map
	BEGIN_MSG_MAP(CLogListBoxImpl)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
	END_MSG_MAP()

	// Message handlers
	LRESULT OnKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0)
		{
			if (wParam == 'A' || wParam == 'a')
			{
				SetSel(-1);
			}
			else if (wParam == 'C' || wParam == 'c')
			{
				int selCount = GetSelCount();
				if (selCount > 0)
				{
					std::vector<int> rgIndex(selCount, 0);
					GetSelItems(selCount, &rgIndex[0]);
					CString contents;
					CString line;
					for (int idx = 0; idx < selCount; ++idx)
					{
						GetText(rgIndex[idx], line);
						contents.Append(line);
						contents.Append(TEXT("\r\n"));
					}
					
					int cch = contents.GetLength();
					if (cch > 0 && ::OpenClipboard(NULL))
					{
						::EmptyClipboard();

						HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, (cch + 1) * sizeof(TCHAR));
						if (NULL != hglbCopy)
						{
							// Lock the handle and copy the text to the buffer. 
							LPTSTR lptstrCopy = (LPTSTR)::GlobalLock(hglbCopy);
							LPTSTR lptstrBuffer = contents.LockBuffer();
							memcpy(lptstrCopy, lptstrBuffer, cch * sizeof(TCHAR));
							contents.UnlockBuffer();
							lptstrCopy[cch] = (TCHAR)0;    // null character 
							::GlobalUnlock(hglbCopy);
							// Place the handle on the clipboard. 

#ifdef _UNICODE
							::SetClipboardData(CF_UNICODETEXT, hglbCopy);
#else
							::SetClipboardData(CF_TEXT, hglbCopy);
#endif
							
						}

						::CloseClipboard();
						if (NULL != hglbCopy) ::GlobalFree(hglbCopy);
					}
				}
			}
		}

		return 0;
	}
   
};



class CLogListBox : public CLogListBoxImpl<CLogListBox>
{
public:
	DECLARE_WND_SUPERCLASS(_T("WTL_LogListBox"), GetWndClassName())

};


#endif // !defined(AFX_LOGLISTBOX_H_INCLUDED_)
