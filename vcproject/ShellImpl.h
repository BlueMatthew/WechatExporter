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
		CA2T p(path.c_str(), CP_UTF8);

		DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)p);

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
	}

	bool makeDirectory(const std::string& path) const
	{
		CA2T p(path.c_str(), CP_UTF8);

		return ::SHCreateDirectoryEx(NULL, (LPCTSTR)p, NULL) == ERROR_SUCCESS;
	}

	bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories) const
	{
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		std::string formatedPath = combinePath(path, "*.*");
		std::replace(formatedPath.begin(), formatedPath.end(), DIR_SEP_R, DIR_SEP);

		CA2T localPath(formatedPath.c_str(), CP_UTF8);

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
				CT2A utf8Path(FindFileData.cFileName, CP_UTF8);
				subDirectories.push_back((LPCSTR)utf8Path);
			}
		} while (::FindNextFile(hFind, &FindFileData));
		FindClose(hFind);

		return true;
	}
	
	bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const
	{
		CA2T s(src.c_str(), CP_UTF8);
		CA2T d(dest.c_str(), CP_UTF8);

		BOOL bRet = ::CopyFile((LPCTSTR)s, (LPCTSTR)d, (overwrite ? FALSE : TRUE));
		return (bRet == TRUE);
	}

	bool openOutputFile(std::ofstream& ofs, const std::string& fileName, std::ios_base::openmode mode/* = std::ios::out*/) const
	{
		if (ofs.is_open())
		{
			return false;
		}

		std::string ansi = utf8ToString(fileName, ofs.getloc());

		CA2T localFileName(fileName.c_str(), CP_UTF8);
		ofs.open(ansi, mode);

		return ofs.is_open();
	}

	bool convertPlist(const std::vector<unsigned char>& bplist, std::string& xml) const
	{
		return true;
	}

	int exec(const std::string& cmd) const
	{
		CA2A c(cmd.c_str(), CP_UTF8);

		return ::WinExec((LPCSTR)c, SW_HIDE) > 31;
	}

protected:
	
};

