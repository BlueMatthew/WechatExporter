#include "stdafx.h"
#include "AppConfiguration.h"
#include "PdfConverterImpl.h"
#include <Shlobj.h>
#include "Utils.h"

#define APP_ROOT_PATH TEXT("Software\\WechatExporter")

void AppConfiguration::SetDescOrder(BOOL descOrder)
{
	SetDwordProperty(TEXT("DescOrder"), descOrder);
}

BOOL AppConfiguration::GetDescOrder()
{
	BOOL descOrder = FALSE;
	DWORD value = 0;
	if (GetDwordProperty(TEXT("DescOrder"), value))
	{
		descOrder = (value != 0) ? TRUE : FALSE;
	}

	return descOrder;
}

void AppConfiguration::SetAsyncLoading(BOOL asyncLoading)
{
	SetDwordProperty(TEXT("AsyncLoading"), asyncLoading);
}

BOOL AppConfiguration::GetAsyncLoading()
{
	DWORD value = 1;
	GetDwordProperty(TEXT("AsyncLoading"), value);
	return (value != 0) ? TRUE : FALSE;
}

void AppConfiguration::SetUsingRemoteEmoji(BOOL usingRemoteEmoji)
{
	SetDwordProperty(TEXT("UsingRemoteEmoji"), usingRemoteEmoji);
}

BOOL AppConfiguration::GetUsingRemoteEmoji()
{
	DWORD value = 0;
	GetDwordProperty(TEXT("UsingRemoteEmoji"), value);
	return (value != 0) ? TRUE : FALSE;
}

void AppConfiguration::SetIncrementalExporting(BOOL incrementalExporting)
{
	SetDwordProperty(TEXT("IncrementalExp"), incrementalExporting);
}

BOOL AppConfiguration::GetIncrementalExporting()
{
	DWORD value = 0;	// default is 0
	GetDwordProperty(TEXT("IncrementalExp"), value);
	return (value != 0) ? TRUE : FALSE;
}

UINT AppConfiguration::GetOutputFormat()
{
	UINT outputFormat = OUTPUT_FORMAT_HTML;
	DWORD dwValue = 0;
	if (GetDwordProperty(TEXT("OutputFormat"), dwValue))
	{
		if (dwValue >= OUTPUT_FORMAT_HTML && dwValue < OUTPUT_FORMAT_LAST)
		{
			outputFormat = dwValue;
		}
	}

	return outputFormat;
}

void AppConfiguration::SetOutputFormat(UINT outputFormat)
{
	if (outputFormat < OUTPUT_FORMAT_HTML || outputFormat >= OUTPUT_FORMAT_LAST)
	{
		outputFormat = OUTPUT_FORMAT_HTML;
	}
	SetDwordProperty(TEXT("OutputFormat"), outputFormat);
}

void AppConfiguration::SetSavingInSession(BOOL savingInSession)
{
	SetDwordProperty(TEXT("SaveFilesInSF"), savingInSession);
}

BOOL AppConfiguration::GetSavingInSession()
{
	DWORD value = 1;
	if (GetDwordProperty(TEXT("SaveFilesInSF"), value))
	{
		return value != 0;
	}
	return TRUE;
}

void AppConfiguration::SetLastOutputDir(LPCTSTR szOutputDir)
{
	SetStringProperty(TEXT("OutputDir"), szOutputDir);
}

CString AppConfiguration::GetLastOrDefaultOutputDir()
{
	CString outputDir;
	if (GetStringProperty(TEXT("OutputDir"), outputDir))
	{
		return outputDir;
	}

	return GetDefaultOutputDir();
}

CString AppConfiguration::GetDefaultOutputDir()
{
	TCHAR szOutput[MAX_PATH] = { 0 };
	HRESULT hr = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, szOutput);
	return CString(SUCCEEDED(hr) ? szOutput : TEXT(""));
}

void AppConfiguration::SetLastBackupDir(LPCTSTR szBackupDir)
{
	SetStringProperty(TEXT("BackupDir"), szBackupDir);
}

CString AppConfiguration::GetLastBackupDir()
{
	CString backupDir;
	GetStringProperty(TEXT("BackupDir"), backupDir);
	return backupDir;
}

CString AppConfiguration::GetDefaultBackupDir(BOOL bCheckExistence/* = TRUE*/)
{
	CString backupDir;
	TCHAR szPath[2][MAX_PATH] = { { 0 }, { 0 } };

	// Check iTunes Folder
	HRESULT hr = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, szPath[0]);
	_tcscat(szPath[0], TEXT("\\Apple Computer\\MobileSync\\Backup"));

	// iTunes App from MS Store
	hr = SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, szPath[1]);
	_tcscat(szPath[1], TEXT("\\Apple\\MobileSync\\Backup"));

	for (int idx = 0; idx < 2; ++idx)
	{
		DWORD dwAttrib = ::GetFileAttributes(szPath[idx]);
		if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
		{
			backupDir = szPath[idx];
			break;
		}
	}

	if (!bCheckExistence && backupDir.IsEmpty())
	{
		backupDir = szPath[0];
	}

	return backupDir;
}

DWORD AppConfiguration::GetLastCheckUpdateTime()
{
	DWORD dwValue = 0;
	GetDwordProperty(TEXT("LastChkUpdateTime"), dwValue);
	return dwValue;
}

void AppConfiguration::SetLastCheckUpdateTime(DWORD lastCheckUpdateTime/* = 0*/)
{
	if (0 == lastCheckUpdateTime)
	{
		lastCheckUpdateTime = (DWORD)getUnixTimeStamp();
	}
	SetDwordProperty(TEXT("LastChkUpdateTime"), lastCheckUpdateTime);
}

void AppConfiguration::SetCheckingUpdateDisabled(BOOL disabled)
{
	SetDwordProperty(TEXT("ChkUpdateDisabled"), disabled);
}

BOOL AppConfiguration::GetCheckingUpdateDisabled()
{
	DWORD dwValue = 0;
	GetDwordProperty(TEXT("ChkUpdateDisabled"), dwValue);
	return dwValue != 0;
}

void AppConfiguration::SetLoadingDataOnScroll(BOOL loadingDataOnScroll)
{
	SetDwordProperty(TEXT("LoadingDataOnScroll"), loadingDataOnScroll);
}

BOOL AppConfiguration::GetLoadingDataOnScroll()
{
	DWORD dwValue = 0;	// FALSE
	GetDwordProperty(TEXT("LoadingDataOnScroll"), dwValue);
	return dwValue != 0;
}

void AppConfiguration::SetSupportingFilter(BOOL supportingFilter)
{
	SetDwordProperty(TEXT("Filter"), supportingFilter);
}

BOOL AppConfiguration::GetSupportingFilter()
{
	DWORD dwValue = 0;	// FALSE
	GetDwordProperty(TEXT("Filter"), dwValue);
	return dwValue != 0;
}

BOOL AppConfiguration::IsPdfSupported()
{
	PdfConverterImpl converter(NULL);

	return converter.isPdfSupported() ? TRUE : FALSE;
}

BOOL AppConfiguration::GetStringProperty(LPCTSTR name, CString& value)
{
	CRegKey rk;
	if (rk.Open(HKEY_CURRENT_USER, APP_ROOT_PATH, KEY_READ) == ERROR_SUCCESS)
	{
		ULONG chars = 0;
		HRESULT hr = rk.QueryStringValue(name, NULL, &chars);
		if (SUCCEEDED(hr))
		{
			hr = rk.QueryStringValue(name, value.GetBufferSetLength(chars), &chars);
			value.ReleaseBuffer();
			return SUCCEEDED(hr);
		}
		
		rk.Close();
	}

	return false;
}

BOOL AppConfiguration::SetStringProperty(LPCTSTR name, LPCTSTR value)
{
	CRegKey rk;
	if (rk.Create(HKEY_CURRENT_USER, APP_ROOT_PATH, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
	{
		HRESULT hr = rk.SetStringValue(name, value);
		rk.Close();
		return SUCCEEDED(hr);
	}

	return FALSE;
}

BOOL AppConfiguration::GetDwordProperty(LPCTSTR name, DWORD& value)
{
	CRegKey rk;
	if (rk.Open(HKEY_CURRENT_USER, APP_ROOT_PATH, KEY_READ) == ERROR_SUCCESS)
	{
		HRESULT hr = rk.QueryDWORDValue(name, value);
		rk.Close();
		return SUCCEEDED(hr);
	}

	return FALSE;
}

BOOL AppConfiguration::SetDwordProperty(LPCTSTR name, DWORD value)
{
	CRegKey rk;
	if (rk.Create(HKEY_CURRENT_USER, APP_ROOT_PATH, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE) == ERROR_SUCCESS)
	{
		HRESULT hr = rk.SetDWORDValue(name, value);
		rk.Close();
		return SUCCEEDED(hr);
	}

	return FALSE;
}
