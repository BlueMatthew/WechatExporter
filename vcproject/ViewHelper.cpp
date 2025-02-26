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


// Return value of GetCurrentExplorerFolders()
struct ExplorerFolderInfo
{
	HWND hwnd = nullptr;  // window handle of explorer
	UniquePidlPtr pidl;   // PIDL that points to current folder
};

// Get information about all currently open explorer windows.
// Throws std::system_error exception to report errors.
std::vector< ExplorerFolderInfo > GetCurrentExplorerFolders()
{
	CComPtr< IShellWindows > pshWindows;
	ThrowIfFailed(
		pshWindows.CoCreateInstance(CLSID_ShellWindows),
		"Could not create instance of IShellWindows");

	long count = 0;
	ThrowIfFailed(
		pshWindows->get_Count(&count),
		"Could not get number of shell windows");

	std::vector< ExplorerFolderInfo > result;
	result.reserve(count);

	for (long i = 0; i < count; ++i)
	{
		ExplorerFolderInfo info;

		CComVariant vi{ i };
		CComPtr< IDispatch > pDisp;
		ThrowIfFailed(
			pshWindows->Item(vi, &pDisp),
			"Could not get item from IShellWindows");

		if (!pDisp)
			// Skip - this shell window was registered with a NULL IDispatch
			continue;

		CComQIPtr< IWebBrowserApp > pApp{ pDisp };
		if (!pApp)
			// This window doesn't implement IWebBrowserApp 
			continue;

		// Get the window handle.
		pApp->get_HWND(reinterpret_cast<SHANDLE_PTR*>(&info.hwnd));

		CComQIPtr< IServiceProvider > psp{ pApp };
		if (!psp)
			// This window doesn't implement IServiceProvider
			continue;

		CComPtr< IShellBrowser > pBrowser;
		if (FAILED(psp->QueryService(SID_STopLevelBrowser, &pBrowser)))
			// This window doesn't provide IShellBrowser
			continue;

		CComPtr< IShellView > pShellView;
		if (FAILED(pBrowser->QueryActiveShellView(&pShellView)))
			// For some reason there is no active shell view
			continue;

		CComQIPtr< IFolderView > pFolderView{ pShellView };
		if (!pFolderView)
			// The shell view doesn't implement IFolderView
			continue;

		// Get the interface from which we can finally query the PIDL of
		// the current folder.
		CComPtr< IPersistFolder2 > pFolder;
		if (FAILED(pFolderView->GetFolder(IID_IPersistFolder2, (void**)&pFolder)))
			continue;

		LPITEMIDLIST pidl = nullptr;
		if (SUCCEEDED(pFolder->GetCurFolder(&pidl)))
		{
			// Take ownership of the PIDL via std::unique_ptr.
			info.pidl = UniquePidlPtr{ pidl };
			result.push_back(std::move(info));
		}
	}

	return result;
}

BOOL OpenFolder(LPCTSTR szFolder)
{
	CT2W wszFolder(szFolder);
	try
	{
		// std::wcout << L"Currently open explorer windows:\n";
		for (const auto& info : GetCurrentExplorerFolders())
		{
			CComHeapPtr<wchar_t> pPath;
			if (SUCCEEDED(::SHGetNameFromIDList(info.pidl.get(), SIGDN_FILESYSPATH, &pPath)))
			{
				if (wcscmp(wszFolder, pPath) == 0)
				{
					SetForegroundWindow(info.hwnd);
					return TRUE;
				}
				// std::wcout << L"hwnd: 0x" << std::hex << info.hwnd
				// 	<< L", path: " << static_cast<LPWSTR>(pPath) << L"\n";
			}
		}
	}
	catch (std::system_error& e)
	{
		// std::cout << "ERROR: " << e.what() << "\nError code: " << e.code() << "\n";
	}

	HINSTANCE inst = ::ShellExecute(NULL, TEXT("open"), szFolder, NULL, NULL, SW_SHOWNORMAL);
	return (INT_PTR)inst > 32;
}
