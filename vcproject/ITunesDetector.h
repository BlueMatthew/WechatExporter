#pragma once

#include "stdafx.h"
#include "VersionDetector.h"

class ITunesDetector
{
public:
	ITunesDetector() : m_installed(false)
	{
		detectStandaloneITunes();

		// detectAppWithWmi();

		if (!m_installed)
		{
			// Check if there is iTunes installed in MS Store
			// detectITunesApp();
		}

		if (!m_installed)
		{
			// Check if there is iTunes installed in MS Store from registry
			detectITunesAppFromRegistry2();
		}

		if (!m_installed)
		{
			// Check if there is iTunes installed in MS Store from registry
			CString rootKeyPath = TEXT("Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages");
			detectITunesAppFromRegistry(HKEY_CURRENT_USER, rootKeyPath);
			if (!m_installed)
			{
				rootKeyPath = TEXT("SOFTWARE\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\PackageRepository\\Packages");
				detectITunesAppFromRegistry(HKEY_LOCAL_MACHINE, rootKeyPath);
			}
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

	void detectITunesAppFromRegistry(HKEY rootKey, const CString& rootKeyPath)
	{
		// Check if there is iTunes installed in MS Store
		CRegKey rkITunes;
		
		LRESULT res = rkITunes.Open(rootKey, rootKeyPath, KEY_READ);
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
#ifndef NDEBUG
								MessageBox(NULL, value, TEXT("Debug"), MB_OK);
#endif
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
				chars = MAX_PATH;
				dwIndex++;
			}

			rkITunes.Close();

#ifndef NDEBUG
			
#endif
		}
#ifndef NDEBUG
		else
		{
			MessageBox(NULL, TEXT("Failed to Open Registry Key: Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages"), TEXT("Debug"), MB_OK);
		}
#endif
	}

	void detectITunesAppFromRegistry2()
	{
		// Check if there is iTunes installed in MS Store
		CRegKey rkITunes;

		CString rootKeyPath = TEXT("Software\\Classes");
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
					if (regValueEqualsTo(rkITunes.m_hKey, subkeyName, NULL, TEXT("iTunes")))
					{
						TCHAR shellPath[MAX_PATH] = { 0 };
						_tcscpy(shellPath, subkeyName);
						_tcscat(shellPath, TEXT("\\"));
						_tcscat(shellPath, TEXT("Shell\\open"));
						if (regValueEqualsTo(rkITunes.m_hKey, shellPath, TEXT("AppUserModelID"), TEXT("AppleInc.iTunes_nzyj5cx40ttqa!iTunes")))
						{
							chars = MAX_PATH;
							TCHAR value[MAX_PATH] = { 0 };
							if (queryRegValue(rkITunes.m_hKey, shellPath, TEXT("PackageId"), value, chars))
							{
								m_installed = true;
								m_version = parseVersionFromPackageId(value);
								m_path = TEXT("");
							}	
						}	
					}	
				}

				if (m_installed)
				{
					break;
				}
				chars = MAX_PATH;
				dwIndex++;
			}

			rkITunes.Close();

#ifndef NDEBUG

#endif
		}
#ifndef NDEBUG
		else
		{
			MessageBox(NULL, TEXT("Failed to Open Registry Key: Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages"), TEXT("Debug"), MB_OK);
		}
#endif
	}

	bool regValueEqualsTo(HKEY rootKey, const CString& keyPath, LPCTSTR szValueName, LPCTSTR szExpectedValue)
	{
		bool result = false;

		ULONG chars = MAX_PATH;
		TCHAR value[MAX_PATH] = { 0 };
		
		if (queryRegValue(rootKey, keyPath, szValueName, value, chars))
		{
			if (_tcsstr(value, szExpectedValue) != NULL)
			{
				result = true;
			}
		}

		return result;
	}

	bool queryRegValue(HKEY rootKey, const CString& keyPath, LPCTSTR szValueName, LPTSTR szValue, ULONG nChars)
	{
		bool result = false;
		CRegKey rk;
		LRESULT res = rk.Open(rootKey, keyPath, KEY_READ);
		if (res == ERROR_SUCCESS)
		{

			if (rk.QueryStringValue(szValueName, szValue, &nChars) == ERROR_SUCCESS)
			{
				result = true;
			}
			rk.Close();
		}

		return result;
	}

	void detectITunesApp()
	{
		// Check if there is iTunes installed in MS Store
		HRESULT result = S_OK;
		TCHAR szPath[MAX_PATH] = { 0 };
		result = SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, szPath);
		if (FAILED(result))
		{
			return;
		}
		_tcscat(szPath, TEXT("\\WindowsApps\\"));

		if (!PathFileExists(szPath))
		{
			return;
		}

		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		TCHAR szPath2[MAX_PATH] = { 0 };
		_tcscpy(szPath2, szPath);
		_tcscat(szPath2, TEXT("*.*"));

		hFind = FindFirstFile(szPath2, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			return;
		}
		do
		{
			if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
			{
				continue;
			}
			if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				continue;
			}
			
			// AppleInc.iTunes_12110.26.53016.0_x64__nzyj5cx40ttqa
			if (_tcsncmp(FindFileData.cFileName, TEXT("AppleInc.iTunes_"), 16) == 0)
			{
				TCHAR szITunes[MAX_PATH] = { 0 };
				_tcscpy(szITunes, szPath);
				_tcscat(szITunes, FindFileData.cFileName);

				TCHAR szITunesExe[MAX_PATH] = { 0 };
				_tcscpy(szITunesExe, szITunes);
				_tcscat(szITunes, TEXT("\\iTunes.exe"));

				if (!PathFileExists(szITunes))
				{
					continue;
				}

				m_path = szITunes;
				VersionDetector versionDetector;
				m_version = versionDetector.GetProductVersion(szITunesExe);
				if (m_version.IsEmpty())
				{
					m_version = parseVersionFromPackageId(FindFileData.cFileName);
				}

				m_installed = true;
				break;
				
			}
			// CW2A pszU8(CT2W(FindFileData.cFileName), CP_UTF8);
			// subDirectories.push_back((LPCSTR)pszU8);
			
		} while (::FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
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
