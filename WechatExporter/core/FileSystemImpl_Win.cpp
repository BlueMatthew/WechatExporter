//
//  FileSystem_Win.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifdef _WIN32

#include "FileSystem.h"
// #include <direct.h>
#include <algorithm>
#ifndef NDEBUG
#include <cassert>
#endif
#include <atlstr.h>
#include <sys/utime.h>
#include <Shlwapi.h>
#include <shlobj_core.h>
#include "OSDef.h"
#include "Utils.h"

bool existsDirectory(const std::string& path)
{
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
}

bool makeDirectory(const std::string& path)
{
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	return ::SHCreateDirectoryEx(NULL, (LPCTSTR)pszT, NULL) == ERROR_SUCCESS;
}

bool deleteFile(const std::string& path)
{
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	return ::DeleteFile((LPCTSTR)pszT) == TRUE;
}

bool deleteDirectory(const std::string& path)
{
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	SHFILEOPSTRUCT file_op = {
        NULL,
        FO_DELETE,
        (LPCTSTR)pszT,
        TEXT(""),
        FOF_NOCONFIRMATION |
        FOF_NOERRORUI |
        FOF_SILENT,
        FALSE,
        0,
        TEXT("") };
	return SHFileOperation(&file_op) == 0;
}

bool existsFile(const std::string& path)
{
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	std::string formatedPath = combinePath(path, "*.*");
	std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);

	CW2T localPath(CA2W(formatedPath.c_str(), CP_UTF8));

	hFind = FindFirstFile((LPTSTR)localPath, &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	do
	{
		if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
		{
			continue;
		}
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			CW2A pszU8(CT2W(FindFileData.cFileName), CP_UTF8);
			subDirectories.push_back((LPCSTR)pszU8);
		}
	} while (::FindNextFile(hFind, &FindFileData));
	FindClose(hFind);

	return true;
}

bool copyFile(const std::string& src, const std::string& dest, bool overwrite)
{
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

	BOOL bRet = ::CopyFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest, (overwrite ? FALSE : TRUE));
#ifndef NDEBUG
    assert(bRet);
#endif
	return (bRet == TRUE);
}

bool moveFile(const std::string& src, const std::string& dest, bool overwrite/* = true*/)
{
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
	if (overwrite)
	{
		::DeleteFile((LPCTSTR)pszDest);
	}
	BOOL bRet = ::MoveFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest) == TRUE;
#ifndef NDEBUG
    assert(bRet);
#endif
    return bRet;
}

CString removeInvalidCharsForFileName(const CString& fileName)
{
	/*
	Windows NT:
	Do not use any of the following characters when naming your files or folders:
		/ ? < > \ : * | ” and any character you can type with the Ctrl key
	File and folder names may be up to 256 characters long
	The maximum length of a full path is 260 characters
	Placing a space at the end of the name
	Placing a period at the end of the name

	 MAC:
	 The only illegal character for file and folder names in Mac OS X is the colon “:”
	 File and folder names are not permitted to begin with a dot “.”
	 File and folder names may be up to 255 characters in length
	 */

	LPCTSTR invalidChars = TEXT("[\\/:*?\"<>|]");
	CString validFileName = fileName;
	for (LPCTSTR ptr = invalidChars; *ptr != '\0'; ptr++)
	{
		validFileName.Remove(*ptr);
	}
	validFileName.TrimLeft(TEXT("."));
	validFileName.TrimRight(TEXT(" ."));

	if (validFileName.GetLength() > 255)
	{
		validFileName = validFileName.Left(255);
	}
	return validFileName;
}

std::string removeInvalidCharsForFileName(const std::string& fileName)
{
	CW2T pszT(CA2W(fileName.c_str(), CP_UTF8));

	CString validFileName = removeInvalidCharsForFileName(CString(pszT));

	CW2A pszU8(CT2W(validFileName), CP_UTF8);
	return std::string(pszU8);
}

bool isValidFileName(const std::string& fileName)
{
    TCHAR tmpPath[MAX_PATH] = { 0 };
    if (GetTempPath(MAX_PATH, tmpPath))
    {
        // CT2A t2a(charPath);
        // tempDir = (LPCSTR)t2a;
    }

    TCHAR path[MAX_PATH] = { 0 };
    CW2T pszT(CA2W(fileName.c_str(), CP_UTF8));
    ::PathCombine(path, tmpPath, (LPCTSTR)pszT);

    bool valid = false;
    if (::CreateDirectory(path, NULL))
    {
        valid = true;
        RemoveDirectory(path);
    }
    else
    {
        valid = (::GetLastError() == ERROR_ALREADY_EXISTS);
    }
    return valid;
}


#endif // #ifdef _WIN32
