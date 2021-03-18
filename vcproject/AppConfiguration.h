#pragma once

class AppConfiguration
{
public:

	enum { OUTPUT_FORMAT_HTML = 0, OUTPUT_FORMAT_TEXT, OUTPUT_FORMAT_LAST };

	static void SetDescOrder(BOOL descOrder);
	static BOOL GetDescOrder();
	static UINT GetOutputFormat();
	static void SetOutputFormat(UINT outputFormat);
	static void SetSavingInSession(BOOL savingInSession);
	static BOOL GetSavingInSession();

	static void SetAsyncLoading(BOOL asyncLoading);
	static BOOL GetAsyncLoading();
	
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

	static void SetLoadingDataOnScroll(BOOL loadingDataOnScroll = TRUE);
	static BOOL GetLoadingDataOnScroll();

	static void SetSupportingFilter(BOOL supportingFilter);
	static BOOL GetSupportingFilter();

protected:
	static BOOL GetStringProperty(LPCTSTR name, CString& value);
	static BOOL SetStringProperty(LPCTSTR name, LPCTSTR value);

	static BOOL GetDwordProperty(LPCTSTR name, DWORD& value);
	static BOOL SetDwordProperty(LPCTSTR name, DWORD value);
};
