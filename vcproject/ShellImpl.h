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

	bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const
	{
		CA2T s(src.c_str(), CP_UTF8);
		CA2T d(dest.c_str(), CP_UTF8);

		return ::CopyFile((LPCTSTR)s, (LPCTSTR)d, overwrite ? FALSE : TRUE) == TRUE;
		return true;
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
	
	bool convertPlist(const std::string& plist, const std::string& xml) const
	{
		return true;
	}

	bool convertSilk(const std::string& silk, const std::string& mp3) const
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

