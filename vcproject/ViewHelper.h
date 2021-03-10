#pragma once

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