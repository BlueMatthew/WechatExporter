#pragma once

#include <cstdint>

#define ASYNC_NONE              0
#define ASYNC_ONSCROLL          1
#define ASYNC_PAGER_NORMAL      2
#define ASYNC_PAGER_ON_YEAR     3
#define ASYNC_PAGER_ON_MONTH    4

class AppConfiguration
{
public:

	enum { OUTPUT_FORMAT_HTML = 0, OUTPUT_FORMAT_TEXT, OUTPUT_FORMAT_PDF, OUTPUT_FORMAT_LAST };

	static void SetDescOrder(BOOL descOrder);
	static BOOL GetDescOrder();
	static UINT GetOutputFormat();
	static void SetOutputFormat(UINT outputFormat);
	static void SetSavingInSession(BOOL savingInSession);
	static BOOL GetSavingInSession();

	static void SetUsingRemoteEmoji(BOOL usingRemoteEmoji);
	static BOOL GetUsingRemoteEmoji();

	static void SetIncrementalExporting(BOOL incrementalExporting);
	static BOOL GetIncrementalExporting();
	
	static void SetLastOutputDir(LPCTSTR szOutputDir);
	static CString GetLastOrDefaultOutputDir();
	static CString GetDefaultOutputDir();

	static void SetLastBackupDir(LPCTSTR szBackupDir);
	static CString GetLastBackupDir();
	static CString GetDefaultBackupDir(BOOL bCheckExistence = TRUE);

	static DWORD GetLastCheckUpdateTime();
	static void SetLastCheckUpdateTime(DWORD lastCheckUpdateTime = 0);

	static void SetCheckingUpdateDisabled(BOOL disabled);
	static BOOL GetCheckingUpdateDisabled();

	static void SetLoadingDataOnScroll();
	static BOOL GetLoadingDataOnScroll();

	static void SetSyncLoading();
	static BOOL GetSyncLoading();
	static DWORD GetAsyncLoading();
	static void SetAsyncLoading(DWORD asyncLoading);

	static void SetNormalPagination();
	static BOOL GetNormalPagination();
	static void SetPaginationOnYear();
	static BOOL GetPaginationOnYear();
	static void SetPaginationOnMonth();
	static BOOL GetPaginationOnMonth();

	static void SetSupportingFilter(BOOL supportingFilter);
	static BOOL GetSupportingFilter();

	static void SetOutputDebugLogs(BOOL dbgLogs);
	static BOOL OutputDebugLogs();

	static void SetIncludingSubscriptions(BOOL includingSubscriptions);
	static BOOL IncludeSubscriptions();

	static void SetOpenningFolderAfterExp(BOOL openningFolderAfterExp);
	static BOOL GetOpenningFolderAfterExp();

	static BOOL IsPdfSupported();

	static void upgrade();
	static uint64_t BuildOptions();

protected:
	static BOOL GetStringProperty(LPCTSTR name, CString& value);
	static BOOL SetStringProperty(LPCTSTR name, LPCTSTR value);

	static BOOL GetDwordProperty(LPCTSTR name, DWORD& value);
	static BOOL SetDwordProperty(LPCTSTR name, DWORD value);
	static DWORD GetDwordValue(LPCTSTR name, DWORD defaultValue);

	static BOOL IsAppInstalled(LPCTSTR name, BOOL lmOrCU);

};
