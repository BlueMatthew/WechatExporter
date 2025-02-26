// WechatExporter.cpp : main source file for WechatExporter.exe
//

#include "stdafx.h"

#include <atlframe.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atldlgs.h>
#include "ToolTipButton.h"

#include "resource.h"

#include "Core.h"

#include "BackupDlg.h"
#include "View.h"
#include "aboutdlg.h"
#include "MainFrm.h"

CAppModule _Module;

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	if (GetUserDefaultUILanguage() == MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
	{
		::SetThreadUILanguage(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED));
	}
	else
	{
		::SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	}

	AtlInitCommonControls(ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_USEREX_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	TCHAR buffer[MAX_PATH] = { 0 };
	DWORD dwRet = GetModuleFileName(NULL, buffer, MAX_PATH);
	if (dwRet > 0)
	{
		if (PathRemoveFileSpec(buffer))
		{
			PathAppend(buffer, TEXT("Dlls"));
			SetDllDirectory(buffer);
		}
	}
#ifndef NDEBUG
	else
	{
		assert(false);
	}
#endif

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
