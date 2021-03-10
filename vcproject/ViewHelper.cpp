#include "stdafx.h"
#include <atlctrls.h>
#include "resource.h"
#include "ViewHelper.h"

#include <memory>

struct LibraryDeleter
{
	typedef HMODULE pointer;
	void operator()(HMODULE h) { if (NULL != h) ::FreeLibrary(h); }
};

//Functions & other definitions required-->
typedef int(__stdcall *MSGBOXAAPI)(IN HWND hWnd,
	IN LPCSTR lpText, IN LPCSTR lpCaption,
	IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);
typedef int(__stdcall *MSGBOXWAPI)(IN HWND hWnd,
	IN LPCWSTR lpText, IN LPCWSTR lpCaption,
	IN UINT uType, IN WORD wLanguageId, IN DWORD dwMilliseconds);

int MessageBoxTimeoutA(HWND hWnd, LPCSTR lpText,
	LPCSTR lpCaption, UINT uType, WORD wLanguageId,
	DWORD dwMilliseconds)
{
	std::unique_ptr<HMODULE, LibraryDeleter> hUser32(::LoadLibraryA("user32.dll"));
	if (NULL != hUser32.get())
	{
		MSGBOXAAPI MsgBoxTOA = (MSGBOXAAPI)GetProcAddress(hUser32.get(), "MessageBoxTimeoutA");
		if (NULL != MsgBoxTOA)
		{

			return MsgBoxTOA(hWnd, lpText, lpCaption, uType, wLanguageId, dwMilliseconds);
		}
	}

	return MessageBoxA(hWnd, lpText, lpCaption, uType);
}

int MessageBoxTimeoutW(HWND hWnd, LPCWSTR lpText,
	LPCWSTR lpCaption, UINT uType, WORD wLanguageId, DWORD dwMilliseconds)
{
	std::unique_ptr<HMODULE, LibraryDeleter> hUser32(::LoadLibraryW(L"user32.dll"));
	if (NULL != hUser32.get())
	{
		MSGBOXWAPI MsgBoxTOW = (MSGBOXWAPI)GetProcAddress(hUser32.get(), "MessageBoxTimeoutW");
		if (NULL != MsgBoxTOW)
		{
			
			return MsgBoxTOW(hWnd, lpText, lpCaption, uType, wLanguageId, dwMilliseconds);
		}
	}

	return MessageBoxW(hWnd, lpText, lpCaption, uType);
}

int MsgBox(HWND hWnd, UINT uStrId, UINT uType/* = MB_OK*/)
{
	CString text;
	text.LoadString(uStrId);
	return MsgBox(hWnd, text, uType);
}

int MsgBox(HWND hWnd, const CString& text, UINT uType/* = MB_OK*/)
{
	CString caption;
	caption.LoadString(IDR_MAINFRAME);
	return ::MessageBox(hWnd, (LPCTSTR)text, (LPCTSTR)caption, uType);
}

void SetComboBoxCurSel(HWND hWnd, CComboBox &cbm, int nCurSel)
{
	cbm.SetCurSel(nCurSel);
	int nID = cbm.GetDlgCtrlID();
	::PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(nID, CBN_SELCHANGE), LPARAM(cbm.m_hWnd));
}

void CheckAllItems(CListViewCtrl& listViewCtrl, BOOL fChecked)
{
	for (int nItem = 0; nItem < ListView_GetItemCount(listViewCtrl); nItem++)
	{
		ListView_SetCheckState(listViewCtrl, nItem, fChecked);
	}
}

void SetHeaderCheckState(CListViewCtrl& listViewCtrl, BOOL fChecked)
{
	// We need to get the current format of the header
	// and set or remove the HDF_CHECKED flag
	HWND header = ListView_GetHeader(listViewCtrl);
	HDITEM hdi = { 0 };
	hdi.mask = HDI_FORMAT;
	Header_GetItem(header, 0, &hdi);
	if (fChecked) {
		hdi.fmt |= HDF_CHECKED;
	}
	else {
		hdi.fmt &= ~HDF_CHECKED;
	}
	Header_SetItem(header, 0, &hdi);
}

BOOL SetHeaderCheckState(CListViewCtrl& listViewCtrl)
{
	HWND header = ListView_GetHeader(listViewCtrl);
	HDITEM hdi = { 0 };
	hdi.mask = HDI_FORMAT;
	Header_GetItem(header, 0, &hdi);
	return (hdi.fmt & HDF_CHECKED) == HDF_CHECKED ? TRUE : FALSE;
}