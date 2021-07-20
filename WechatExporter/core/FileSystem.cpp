//
//  FileSystem_Win.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "FileSystem.h"
#include "Utils.h"
#include "OSDef.h"
#ifndef NDEBUG
#include <cassert>
#endif
// #include <iomanip>
#include <fstream>

#ifdef _WIN32
#include <algorithm>
#include <atlstr.h>
#include <sys/utime.h>
#include <Shlwapi.h>
#include <shlobj_core.h>
#else //  _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#endif //  _WIN32

size_t getFileSize(const std::string& path)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	HANDLE hFile = CreateFile((LPCTSTR)pszT, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return -1; // error condition, could call GetLastError to find out more

	LARGE_INTEGER size;
	if (!GetFileSizeEx(hFile, &size))
	{
		CloseHandle(hFile);
		return -1; // error condition, could call GetLastError to find out more
	}

	CloseHandle(hFile);
	return (size_t)size.QuadPart;
#else
    struct stat sb;
    int rc = stat(path.c_str(), &sb);
    return rc == 0 ? sb.st_size : -1;
#endif
}

bool existsDirectory(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode));
#endif
}

#ifndef _WIN32
int makePathImpl(const std::string::value_type *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0)
    {
        // Directory does not exist. EEXIST for race condition
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        // errno = ENOTDIR;
        status = -1;
    }

    return status;
}
#endif

bool makeDirectory(const std::string& path)
{
#ifndef NDEBUG
    // The path must be full/absolute path
#ifdef _WIN32
    assert(path.find(":\\") != std::string::npos);
#else
    assert(startsWith(path, "/"));
#endif
#endif
    
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	return ::SHCreateDirectoryEx(NULL, (LPCTSTR)pszT, NULL) == ERROR_SUCCESS;
#else
    std::vector<std::string::value_type> copypath;
    copypath.reserve(path.size() + 1);
    std::copy(path.begin(), path.end(), std::back_inserter(copypath));
    copypath.push_back('\0');
    std::replace(copypath.begin(), copypath.end(), '\\', '/');
    
    std::vector<std::string::value_type>::iterator itStart = copypath.begin();
    std::vector<std::string::value_type>::iterator it;
    
    int status = 0;
    mode_t mode = 0777;
    while (status == 0 && (it = std::find(itStart, copypath.end(), '/')) != copypath.end())
    {
        if (it != copypath.begin())
        {
            // Neither root nor double slash in path
            *it = '\0';
            status = makePathImpl(&copypath[0], mode);
            *it = '/';
        }
        itStart = it + 1;
    }
    if (status == 0)
    {
        status = makePathImpl(&copypath[0], mode);
    }
    
    return status == 0;
#endif
}

bool deleteFile(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	return ::DeleteFile((LPCTSTR)pszT) == TRUE;
#else
    return 0 == std::remove(path.c_str());
#endif
}

bool deleteDirectory(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	size_t len = _tcslen(pszT);
	TCHAR* pszT2 = new TCHAR[len + 2];
	_tcscpy(pszT2, pszT);
	pszT2[len + 1] = 0;
	pszT2[len] = 0;

	SHFILEOPSTRUCT file_op = {
		NULL,
		FO_DELETE,
		pszT2,
		NULL,
		FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
		FALSE,
		NULL,
		NULL };

	bool result = (SHFileOperation(&file_op) == 0);
	delete[] pszT2;
	return result;
#else
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    // Cast needed (in C) because fts_open() takes a "char * const *", instead
    // of a "const char * const *", which is only allowed in C++. fts_open()
    // does not modify the argument.
    char *files[] = { (char *) path.c_str(), NULL };

    // FTS_NOCHDIR  - Avoid changing cwd, which could cause unexpected behavior
    //                in multithreaded programs
    // FTS_PHYSICAL - Don't follow symlinks. Prevents deletion of files outside
    //                of the specified directory
    // FTS_XDEV     - Don't cross filesystem boundaries
    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        // fprintf(stderr, "%s: fts_open failed: %s\n", dir, strerror(errno));
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp)))
    {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            // fprintf(stderr, "%s: fts_read error: %s\n", curr->fts_accpath, strerror(curr->fts_errno));
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            // Not reached unless FTS_LOGICAL, FTS_SEEDOT, or FTS_NOSTAT were
            // passed to fts_open()
            break;

        case FTS_D:
            // Do nothing. Need depth-first search, so directories are deleted
            // in FTS_DP
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                // fprintf(stderr, "%s: Failed to remove: %s\n", curr->fts_path, strerror(curr->fts_errno));
                ret = -1;
            }
            break;
        }
    }

finish:
    if (ftsp)
    {
        fts_close(ftsp);
    }

    return ret == 0;
#endif
}

bool existsFile(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));

	DWORD dwAttrib = ::GetFileAttributes((LPCTSTR)pszT);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		(dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0);
#endif
}

bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories)
{
#ifdef _WIN32
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
#else
    struct dirent *entry;
    DIR *dir = opendir(path.c_str());
    if (dir == NULL)
    {
        return false;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        std::string subDir = entry->d_name;
        // TODO: Check directory or not
        subDirectories.push_back(subDir);
    }
    closedir(dir);
#endif
    return true;
}

bool copyFile(const std::string& src, const std::string& dest, bool overwrite)
{
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

	BOOL bRet = FALSE;
	if (::PathFileExists((LPCTSTR)pszSrc))
	{
		bRet = ::CopyFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest, (overwrite ? FALSE : TRUE));
#ifndef NDEBUG
		DWORD err = ::GetLastError();
		TCHAR buffer[256] = { 0 };
		_itot((int)err, buffer, 10);
		_tcscat(buffer, TEXT(" "));
		_tcscat(buffer, pszDest);
		assert(buffer);
#endif
	}
	return (bRet == TRUE);
#else
    if (existsFile(dest) && !overwrite)
    {
        return false;
    }
    
    std::ifstream ss(src, std::ios::in | std::ios::binary);
    if (!ss.is_open())
    {
#ifndef NDEBUG
        // assert(false);
#endif
        return false;
    }
    std::ofstream ds(dest, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ds.is_open())
    {
#ifndef NDEBUG
        assert(false);
#endif
        ss.close();
        return false;
    }
    ds << ss.rdbuf();

    ss.close();
    ds.close();
    
    return true;
#endif
}

bool moveFile(const std::string& src, const std::string& dest, bool overwrite/* = true*/)
{
#ifndef NDEBUG
	if (src.empty())
	{
		assert(!"src is empty");
	}
	if (dest.empty())
	{
		assert(!"dest is empty");
	}
#endif
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
	if (overwrite && ::PathFileExists((LPCTSTR)pszDest))
	{
		::DeleteFile((LPCTSTR)pszDest);
	}
	bool bRet = false;
	if (::PathFileExists((LPCTSTR)pszSrc))
	{
		bRet = ::MoveFile((LPCTSTR)pszSrc, (LPCTSTR)pszDest) == TRUE;
#ifndef NDEBUG
		if (!bRet)
		{
			// MessageBox(NULL, (LPCTSTR)pszSrc, (LPCTSTR)pszDest, MB_OK);
			assert(!((LPCTSTR)pszDest));
		}
#endif
	}
    return bRet;
#else
    if (overwrite)
    {
        remove(dest.c_str());
    }
    else if (existsFile(dest))
    {
        return false;
    }
    if (!existsFile(src))
    {
        return false;
    }
    int ret = rename(src.c_str(), dest.c_str());;
#ifndef NDEBUG
    assert(ret == 0);
#endif
    return ret == 0;
#endif
}

#ifdef _WIN32
CString removeInvalidCharsForFileName(const CString& fileName)
{
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
#endif

std::string removeInvalidCharsForFileName(const std::string& fileName)
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

#ifdef _WIN32
	CW2T pszT(CA2W(fileName.c_str(), CP_UTF8));

	CString validFileName = removeInvalidCharsForFileName(CString(pszT));

	CW2A pszU8(CT2W(validFileName), CP_UTF8);
	return std::string(pszU8);
#else
    std::string validFileName = fileName;
    
    static std::string invalidChars = "[\\/:*?\"<>|]";

    for (unsigned int i = 0; i < invalidChars.size(); ++i)
    {
        validFileName.erase(remove(validFileName.begin(), validFileName.end(), invalidChars[i]), validFileName.end());
    }

    size_t pos = validFileName.find_first_not_of(".");
    if (pos == std::string::npos)
    {
        return "";
    }
    else if (pos != 0)
    {
        validFileName.erase(0, pos);
    }

    pos = validFileName.find_last_not_of(" .");
    if (pos == std::string::npos)
    {
        return "";
    }
    else
    {
        validFileName.erase(pos + 1);
    }

    if (validFileName.size() > 255)
    {
        validFileName = validFileName.substr(0, 255);
    }
    return validFileName;
#endif
}

bool isValidFileName(const std::string& fileName)
{
#ifdef _WIN32
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
#else
    char const *tmpdir = getenv("TMPDIR");
    
    std::string tempDir;

    if (tmpdir == NULL)
    {
        tempDir = "/tmp";
    }
    else
    {
        tempDir = tmpdir;
    }
    
    std::string path = combinePath(tempDir, fileName);
    int status = mkdir(path.c_str(), 0);
    int lastErrorNo = errno;
    if (status == 0)
    {
        remove(path.c_str());
    }
    
    return status == 0 || lastErrorNo == EEXIST;
#endif
}


std::string readFile(const std::string& path)
{
    std::string contents;
    std::vector<unsigned char> data;
    if (readFile(path, data))
    {
        contents.append(reinterpret_cast<const char*>(&data[0]), data.size());
    }
    
    return contents;
}

bool readFile(const std::string& path, std::vector<unsigned char>& data)
{
#ifdef _WIN32
    CA2W pszW(path.c_str(), CP_UTF8);
    std::ifstream ifs(pszW, std::ios::in | std::ios::binary | std::ios::ate);
#else
    std::ifstream ifs(path, std::ios::in | std::ios::binary | std::ios::ate);
#endif
    
    if (ifs.is_open())
    {
        std::streampos size = ifs.tellg();
        std::vector<std::string::value_type> buffer;
        data.resize(size);
        
		ifs.seekg (0, std::ios::beg);
		ifs.read((char *)(&data[0]), size);
		ifs.close();
        
        return true;
    }
    
    return false;
}

bool writeFile(const std::string& path, const std::vector<unsigned char>& data)
{
    return writeFile(path, &(data[0]), data.size());
}

bool writeFile(const std::string& path, const std::string& data)
{
    return writeFile(path, reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
}

bool writeFile(const std::string& path, const unsigned char* data, size_t dataLength)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));

    HANDLE hFile = CreateFile(pszT, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD dwBytesToWrite = static_cast<DWORD>(dataLength);
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = WriteFile(hFile, data, dwBytesToWrite, &dwBytesWritten, NULL);

    ::CloseHandle(hFile);
    return (TRUE == bErrorFlag);
#else

    std::ofstream ofs;
    ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (ofs.is_open())
    {
        ofs.write(reinterpret_cast<const char *>(data), dataLength);
        ofs.close();
        return true;
    }

    return false;
#endif
}

bool appendFile(const std::string& path, const std::string& data)
{
    return appendFile(path, reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
}

bool appendFile(const std::string& path, const unsigned char* data, size_t dataLength)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));

    HANDLE hFile = ::CreateFile(pszT, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    ::SetFilePointer(hFile, 0, 0, FILE_END);
    DWORD dwBytesToWrite = static_cast<DWORD>(dataLength);
    DWORD dwBytesWritten = 0;
    BOOL bErrorFlag = WriteFile(hFile, data, dwBytesToWrite, &dwBytesWritten, NULL);
    ::CloseHandle(hFile);
    return (TRUE == bErrorFlag);
#else
    std::ofstream ofs;
    ofs.open(path, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
    if (ofs.is_open())
    {
        ofs.write(reinterpret_cast<const char *>(data), dataLength);
        ofs.close();
        return true;
    }

    return false;
#endif
}
