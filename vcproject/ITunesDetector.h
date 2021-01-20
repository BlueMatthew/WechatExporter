#pragma once

#include "stdafx.h"

class ITunesDetector
{
public:
	ITunesDetector() : m_installed(false)
	{
		detectStandaloneITunes();

		if (!m_installed)
		{
			// Check if there is iTunes installed in MS Store
			detectApp();
		}
	}

	~ITunesDetector()
	{

	}


	bool isInstalled() const
	{
		return m_installed;
	}
	CString getVersion() const
	{
		return m_version;
	}
	CString getInstallPath() const
	{
		return m_path;
	}

protected:
	void detectStandaloneITunes()
	{
		TCHAR value[MAX_PATH] = { 0 };
		CRegKey rkITunes;
		if (rkITunes.Open(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Apple Computer, Inc.\\iTunes"), KEY_READ) == ERROR_SUCCESS)
		{
			ULONG chars = MAX_PATH;
			if (rkITunes.QueryStringValue(TEXT("Version"), value, &chars) == ERROR_SUCCESS)
			{
				m_installed = true;
				m_version = value;
			}
			if (rkITunes.QueryStringValue(TEXT("InstallDir"), value, &chars) == ERROR_SUCCESS)
			{
				m_path = value;
			}
			
			rkITunes.Close();
		}	
	}

	void detectApp()
	{
		// Check if there is iTunes installed in MS Store
		CRegKey rkITunes;
		
		CString rootKeyPath = TEXT("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages");
		LRESULT res = rkITunes.Open(HKEY_CURRENT_USER, rootKeyPath, KEY_READ);
		if (res == ERROR_SUCCESS)
		{
			DWORD dwIndex = 0;
			ULONG chars = MAX_PATH;
			TCHAR subkeyName[MAX_PATH] = { 0 };
			while ((res = rkITunes.EnumKey(dwIndex, subkeyName, &chars)) != ERROR_NO_MORE_ITEMS)
			{
				if (res == ERROR_SUCCESS)
				{
					CRegKey rk;
					res = rk.Open(rkITunes.m_hKey, subkeyName, KEY_READ);
					if (res == ERROR_SUCCESS)
					{
						chars = MAX_PATH;
						TCHAR value[MAX_PATH] = { 0 };
						if (rk.QueryStringValue(TEXT("PackageID"), value, &chars) == ERROR_SUCCESS)
						{
							if (_tcsstr(value, TEXT("AppleInc.iTunes_")) != NULL)
							{
								m_installed = true;
								m_version = parseVersionFromPackageId(value);
								chars = MAX_PATH;
								if (rk.QueryStringValue(TEXT("PackageRootFolder"), value, &chars) == ERROR_SUCCESS)
								{
									m_path = value;
								}
							}
						}

						rk.Close();
						if (m_installed)
						{
							break;
						}
					}
				}
				dwIndex++;
			}

			rkITunes.Close();
		}
	}

	CString parseVersionFromPackageId(const CString packageId)
	{
		CString version = packageId;
		// AppleInc.iTunes_12110.26.53016.0_x64__nzyj5cx40ttqa
		int pos = version.Find('_');
		if (pos != -1)
		{
			version = version.Mid(pos + 1);
			pos = version.Find('_');
			if (pos != -1)
			{
				version = version.Left(pos);
				return version;
			}
		}

		return TEXT("");
	}

private:
	bool m_installed;
	CString m_path;
	CString m_version;

};
