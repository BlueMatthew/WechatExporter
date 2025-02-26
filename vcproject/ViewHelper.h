#pragma once

#include <shlobj.h>
#include <atlcomcli.h>  // for COM smart pointers
#include <atlbase.h>    // for COM smart pointers
#include <vector>
#include <system_error>
#include <memory>
#include <iostream>

#define MB_TIMEDOUT 32000

int MessageBoxTimeoutA(IN HWND hWnd, IN LPCSTR lpText,
	IN LPCSTR lpCaption, IN UINT uType,
	IN WORD wLanguageId, IN DWORD dwMilliseconds);
int MessageBoxTimeoutW(IN HWND hWnd, IN LPCWSTR lpText,
	IN LPCWSTR lpCaption, IN UINT uType,
	IN WORD wLanguageId, IN DWORD dwMilliseconds);

#ifdef UNICODE
#define MessageBoxTimeout MessageBoxTimeoutW
#else
#define MessageBoxTimeout MessageBoxTimeoutA
#endif 
int MsgBox(HWND hWnd, UINT uStrId, UINT uType = MB_OK);
int MsgBox(HWND hWnd, const CString& text, UINT uType = MB_OK);
void SetComboBoxCurSel(HWND hWnd, CComboBox &cbm, int nCurSel);
void CheckAllItems(CListViewCtrl& listViewCtrl, BOOL fChecked);
void SetHeaderCheckState(CListViewCtrl& listViewCtrl, BOOL fChecked);
BOOL SetHeaderCheckState(CListViewCtrl& listViewCtrl);

template< typename T >
void ThrowIfFailed(HRESULT hr, T&& msg)
{
	if (FAILED(hr))
		throw std::system_error{ hr, std::system_category(), std::forward<T>(msg) };
}

// Deleter for a PIDL allocated by the shell.
struct CoTaskMemDeleter
{
	void operator()(ITEMIDLIST* pidl) const { ::CoTaskMemFree(pidl); }
};
// A smart pointer for PIDLs.
using UniquePidlPtr = std::unique_ptr< ITEMIDLIST, CoTaskMemDeleter >;
BOOL OpenFolder(LPCTSTR szFolder);
