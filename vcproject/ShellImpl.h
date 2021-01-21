#pragma once

#include "stdafx.h"
#include <string>
#include <shlobj_core.h>
#include "Core.h"

class ShellImpl : public Shell
{
public:
	~ShellImpl() {}

	bool existsDirectory(const std::string& path) const
	{
		CW2T pszT(CA2W(path.c_str(), CP_UTF8));

		DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
	}

	bool makeDirectory(const std::string& path) const
	{
		CW2T pszT(CA2W(path.c_str(), CP_UTF8));

		return ::SHCreateDirectoryEx(NULL, (LPCTSTR)pszT, NULL) == ERROR_SUCCESS;
	}

	bool deleteFile(const std::string& path) const
	{
		CW2T pszT(CA2W(path.c_str(), CP_UTF8));
		return ::DeleteFile((LPCTSTR)pszT) == TRUE;
	}
	
	bool deleteDirectory(const std::string& path) const
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

	bool existsFile(const std::string& path) const
	{
		CW2T pszT(CA2W(path.c_str(), CP_UTF8));

		DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories) const
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
	
	bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const
	{
		CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
		CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

		BOOL bRet = ::CopyFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest, (overwrite ? FALSE : TRUE));
		return (bRet == TRUE);
	}

	bool moveFile(const std::string& src, const std::string& dest) const
	{
		CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
		CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

		return ::MoveFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest) == TRUE;
	}

	bool openOutputFile(std::ofstream& ofs, const std::string& fileName, std::ios_base::openmode mode/* = std::ios::out*/) const
	{
		if (ofs.is_open())
		{
			return false;
		}

		CW2A psz(CA2W(fileName.c_str(), CP_UTF8));
		ofs.open((LPCSTR)psz, mode);

		return ofs.is_open();
	}

	std::string removeInvalidCharsForFileName(const std::string& fileName) const
    {
		CW2T pszT(CA2W(fileName.c_str(), CP_UTF8));

		CString validFileName = removeInvalidCharsForFileName(CString(pszT));
        
		CW2A pszU8(CT2W(validFileName), CP_UTF8);
        return std::string(pszU8);
    }

	CString removeInvalidCharsForFileName(const CString& fileName) const
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



protected:
	
};

