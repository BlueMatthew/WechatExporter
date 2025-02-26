//
//  FileSystem_Win.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "FileSystem.h"
#include "Utils.h"
#ifndef NDEBUG
#include <cassert>
#endif
// #include <iomanip>
#include <fstream>
#include <queue>

#ifdef _WIN32
#include <algorithm>
#include <atlstr.h>
#include <sys/utime.h>
#include <Shlwapi.h>
#include <shlobj_core.h>
#else //  _WIN32

#ifdef __APPLE__
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/copyfile.3.html
#include <copyfile.h>
#endif
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>
#endif //  _WIN32

#ifdef _WIN32
inline size_t getFileSizeImpl(LPCTSTR path)
#else
inline size_t getFileSizeImpl(const std::string& path)
#endif
{
#ifdef _WIN32
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

size_t getFileSize(const std::string& path)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));
    return getFileSizeImpl((LPCTSTR)pszT);
#else
    return getFileSizeImpl(path);
#endif
}

#ifdef _WIN32
inline bool existsDirectoryImpl(LPCTSTR lpszPath)
{
    DWORD dwAttrib = ::GetFileAttributes(lpszPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
}

inline bool isDirectoryImpl(LPCTSTR lpszPath)
{
    DWORD dwAttrib = ::GetFileAttributes(lpszPath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
}

time_t FileTimeToTime(FILETIME ft)
{
    // takes the last modified date
    LARGE_INTEGER date, adjust;
    date.HighPart = ft.dwHighDateTime;
    date.LowPart = ft.dwLowDateTime;
    
    const uint64_t EpochShift = 116444736000000000;

    if (date.QuadPart < EpochShift)
        return -1;
    // 100-nanoseconds = milliseconds * 10000
    
    // removes the diff between 1970 and 1601
    date.QuadPart -= EpochShift;

    // converts back from 100-nanoseconds to seconds
    return date.QuadPart / 10000000;
}

#else

inline bool isDirectoryImpl(const std::string& path)
{
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode));
}

#endif


bool existsDirectory(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	return existsDirectoryImpl(pszT);
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
    if (path == "/Users/matthew/Documents/WxExp/WechatHistory.Test/Matthew/朱磊, 石绮莹, Matthew 施立波/Portrait" || path == "/Users/matthew/Documents/WxExp/WechatHistory.Test/Matthew/朱磊, 石绮莹, Matthew 施立波/Emoji" || path == "/Users/matthew/Documents/WxExp/WechatHistory.Test/Matthew/朱磊, 石绮莹, Matthew 施立波/Portrait/" || path == "/Users/matthew/Documents/WxExp/WechatHistory.Test/Matthew/朱磊, 石绮莹, Matthew 施立波/Emoji/")
    {
        int aa = 0;
    }
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
    mode_t mode = 0755;
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

#ifdef _WIN32
inline bool existsFileImpl(LPCTSTR path)
#else
inline bool existsFileImpl(const std::string& path)
#endif
{
#ifdef _WIN32
    DWORD dwAttrib = ::GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0);
#endif
}

bool existsFile(const std::string& path)
{
#ifdef _WIN32
	CW2T pszT(CA2W(path.c_str(), CP_UTF8));
	return existsFileImpl((LPCTSTR)pszT);
#else
    return existsFileImpl(path);
#endif
}

bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories)
{
#ifdef _WIN32
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	std::string formatedPath = combinePath(path, "*.*");
	std::replace(formatedPath.begin(), formatedPath.end(), ALT_DIR_SEP, DIR_SEP);

	CW2T localPath(CA2W(formatedPath.c_str(), CP_UTF8));

	hFind = ::FindFirstFile((LPTSTR)localPath, &FindFileData);
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
	::FindClose(hFind);
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
        if (isDirectoryImpl(combinePath(path, subDir)))
        {
            subDirectories.push_back(subDir);
        }
    }
    closedir(dir);
#endif
    return true;
}

bool listDirectory(const std::string& path, std::vector<std::string>& subDirectories, std::vector<std::string>& subFiles)
{
#ifdef _WIN32
    WIN32_FIND_DATA FindFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    std::string formatedPath = combinePath(path, "*.*");
    std::replace(formatedPath.begin(), formatedPath.end(), ALT_DIR_SEP, DIR_SEP);

    CW2T localPath(CA2W(formatedPath.c_str(), CP_UTF8));

    hFind = ::FindFirstFile((LPTSTR)localPath, &FindFileData);
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
        
        CW2A pszU8(CT2W(FindFileData.cFileName), CP_UTF8);
        if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            subDirectories.push_back((LPCSTR)pszU8);
        }
        else
        {
            subFiles.push_back((LPCSTR)pszU8);
        }
    } while (::FindNextFile(hFind, &FindFileData));
    ::FindClose(hFind);
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
        if (isDirectoryImpl(combinePath(path, subDir)))
        {
            subDirectories.push_back(subDir);
        }
        else
        {
            subFiles.push_back(subDir);
        }
    }
    closedir(dir);
#endif
    return true;
}

#ifdef _WIN32
inline bool copyFileImpl(LPCTSTR src, LPCTSTR dest)
#else
inline bool copyFileImpl(const std::string& src, const std::string& dest)
#endif
{
#ifdef _WIN32
    BOOL bRet = ::CopyFile(src, dest, TRUE);
#ifndef NDEBUG
    DWORD err = ::GetLastError();
    TCHAR buffer[256] = { 0 };
    _itot((int)err, buffer, 10);
    _tcscat(buffer, TEXT(" "));
    _tcscat(buffer, dest);
    assert(buffer);
#endif
    return (bRet == TRUE);
#elif defined(__APPLE__)
    /* Initialize a state variable */
    copyfile_state_t s;
    s = copyfile_state_alloc();
    /* Copy the data and extended attributes of one file to another */
    int ret = copyfile(src.c_str(), dest.c_str(), s, COPYFILE_ALL);
    /* Release the state variable */
    copyfile_state_free(s);

    return (ret == 0);
#else
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

bool copyFile(const std::string& src, const std::string& dest, bool overwrite)
{
#ifdef _WIN32
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
	if (::PathFileExists((LPCTSTR)pszDest) && !overwrite)
	{
        return true;
    }
    
    CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	return copyFileImpl(pszSrc, pszDest);
#else
    if (existsFile(dest) && !overwrite)
    {
        return false;
    }
    
    return copyFileImpl(src, dest);
#endif
}


#ifdef _WIN32
inline bool checkFileNewer(LPCTSTR src, LPCTSTR dest)
#else
inline bool checkFileNewer(const std::string& src, const std::string& dest)
#endif
{
#ifdef _WIN32
    if (!::PathFileExists(dest))
    {
        return true;
    }
    // Check file size
    if (getFileSizeImpl(src) != getFileSizeImpl(dest))
    {
        return true;
    }
    
    // Check last modified time
    HANDLE hFileSrc = CreateFile(src, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFileSrc == INVALID_HANDLE_VALUE)
        return true;
    
    HANDLE hFileDest = CreateFile(dest, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFileDest == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFileSrc);
        return true;
    }

    LARGE_INTEGER sizeSrc;
    LARGE_INTEGER sizeDest;
    if (!GetFileSizeEx(hFileSrc, &sizeSrc) || !GetFileSizeEx(hFileDest, &sizeDest) || sizeSrc.QuadPart != sizeDest.QuadPart)
    {
        CloseHandle(hFileSrc);
        CloseHandle(hFileDest);
        return true;
    }
    
    FILETIME ftSrc, ftDest;
    if (!GetFileTime(hFileSrc, NULL, NULL, &ftSrc) || !GetFileTime(hFileDest, NULL, NULL, &ftDest))
    {
        CloseHandle(hFileSrc);
        CloseHandle(hFileDest);
        return true;
    }
    
    CloseHandle(hFileSrc);
    CloseHandle(hFileDest);

    return CompareFileTime(&ftSrc, &ftDest) != 0;
#else
    if (!existsFile(dest))
    {
        return true;
    }
    
    struct stat sbSrc, sbDest;
    int rc = stat(src.c_str(), &sbSrc);
    if (rc != 0)
    {
        return true;
    }
    rc = stat(dest.c_str(), &sbDest);
    if (rc != 0)
    {
        return true;
    }

    return (sbSrc.st_size != sbDest.st_size) || (sbSrc.st_mtime != sbDest.st_mtime);
#endif
    return true;
}

bool copyFileIfNewer(const std::string& src, const std::string& dest)
{
#ifdef _WIN32
    CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
    CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
    if (!checkFileNewer((LPCTSTR)pszSrc, (LPCTSTR)pszDest))
    {
        return true;
    }
    return copyFileImpl(pszSrc, pszDest);
    
#else
    if (!checkFileNewer(src, dest))
    {
        return true;
    }
    return copyFileImpl(src, dest);
#endif
}

bool copyDirectory(const std::string& src, const std::string& dest)
{
#ifdef _WIN32
    TCHAR pszSrc[MAX_PATH] = { 0 };
    _tcscpy(pszSrc, (LPCTSTR)CW2T(CA2W(src.c_str(), CP_UTF8)));
    if (!existsDirectoryImpl((LPCTSTR)pszSrc))
    {
        return false;
    }
    
    PathAppend(pszSrc, TEXT("*"));
    TCHAR pszDest[MAX_PATH] = { 0 };
    _tcscpy(pszDest, (LPCTSTR)CW2T(CA2W(dest.c_str(), CP_UTF8)));
    
    pszSrc[_tcslen(pszSrc) + 1] = 0;
    pszDest[_tcslen(pszDest) + 1] = 0;

    SHFILEOPSTRUCTW s = { 0 };
    s.wFunc = FO_COPY;
    s.pTo = pszDest;
    s.pFrom = pszSrc;
    s.fFlags = FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NO_UI;
    int res = SHFileOperation( &s );

    return res == 0;
// #elif defined(__APPLE__)
    
#else
    if (src.empty() || dest.empty())
    {
        return false;
    }

    struct stat st;

    /* if src does not exist */
    if ((stat(src.c_str(), &st) < 0) || !S_ISDIR(st.st_mode))
    {
        // Source directory does not exist;
        return false;
    }

    /* if dst directory does not exist */
    if ((stat(dest.c_str(), &st) < 0) || !S_ISDIR(st.st_mode))
    {
        /* create it */
        if (!makeDirectory(dest/*, 0755*/))
        {
            // Unable to create destination directory;
            return false;
        }
    }
    
    std::queue<std::pair<std::string, std::string>> pairs;
    pairs.push(std::pair<std::string, std::string>(src, dest));
    std::vector<std::pair<std::string, std::string>>::reference ref = pairs.front();
    if (ref.first[ref.first.size() - 1] != DIR_SEP)
    {
        ref.first += DIR_SEP;
    }
    if (ref.second[ref.second.size() - 1] != DIR_SEP)
    {
        ref.second += DIR_SEP;
    }
    
    bool res = true;
    while (!pairs.empty())
    {
        ref = pairs.front();
        
        /* loop over src directory contents */
        DIR *cur_dir = opendir(ref.first.c_str());
        if (cur_dir)
        {
            struct dirent* ep;
            while ((ep = readdir(cur_dir)))
            {
                if ((strcmp(ep->d_name, ".") == 0) || (strcmp(ep->d_name, "..") == 0))
                {
                    continue;
                }
                std::string srcpath = ref.first + ep->d_name;
                std::string dstpath = ref.second + ep->d_name;
                if (isDirectoryImpl(srcpath))
                {
                    pairs.push(std::pair<std::string, std::string>(srcpath + DIR_SEP, dstpath + DIR_SEP));
                }
                else
                {
                    if (!copyFile(srcpath, dstpath))
                    {
                        res = false;
                        break;
                    }
                }
            }
            closedir(cur_dir);
        }
        
        pairs.pop();
    }

    return res;
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
    if (readFile(path, data) && !data.empty())
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
        if ((long long)size > 0)
        {
            std::vector<std::string::value_type> buffer;
            data.resize(size);
            
            ifs.seekg (0, std::ios::beg);
            ifs.read((char *)(&data[0]), size);
        }
        else
        {
			data.clear();
        }
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

std::string combinePath(const std::string& p1, const std::string& p2)
{
    if (p1.empty() && p2.empty())
    {
        return "";
    }

    std::string path;
#ifdef _WIN32
    TCHAR buffer[MAX_PATH] = { 0 };
    CW2T pszT1(CA2W(p1.c_str(), CP_UTF8));
    CW2T pszT2(CA2W(p2.c_str(), CP_UTF8));

    LPTSTR psz = ::PathCombine(buffer, pszT1, pszT2);
#ifndef NDEBUG
    assert(psz != NULL);
#endif
    CW2A pszU8(CT2W(buffer), CP_UTF8);
    path = pszU8;
#else
    
    if (!p1.empty())
    {
        path = p1;
        if (path[path.size() - 1] != DIR_SEP && path[path.size() - 1] != ALT_DIR_SEP)
        {
            path += DIR_SEP;
        }
        
        path += p2;
    }
    else
    {
        path = p2;
    }
#endif
#ifndef NDEBUG
    if (path.empty())
    {
        assert(false);
    }
#endif
    return path;
}

std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3)
{
    return combinePath(combinePath(p1, p2), p3);
}

std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3, const std::string& p4)
{
    return combinePath(combinePath(p1, p2, p3), p4);
}

std::string normalizePath(const std::string& path)
{
    std::string p = path;
    normalizePath(p);
    return p;
}

void normalizePath(std::string& path)
{
    std::replace(path.begin(), path.end(), ALT_DIR_SEP, DIR_SEP);
}

int calcFreeSpace(const std::string& path, uint64_t& freeSpace)
{
    int res = -1;
#ifdef _WIN32
    ULARGE_INTEGER value;
    value.QuadPart = 0;
    if (GetDiskFreeSpaceExA(path.c_str(), &value, NULL, NULL))
    {
        res = 0;
        freeSpace = value.QuadPart;
    }
#else
    struct statvfs fs;
    memset(&fs, '\0', sizeof(fs));
    res = statvfs(path.c_str(), &fs);
    if (res == 0)
    {
        freeSpace = (uint64_t)fs.f_bavail * (uint64_t)fs.f_bsize;
    }
#endif
    return res;
}

FileEnumerator::File::File() : m_isDir(false), m_isNormalFile(false), m_fileSize(0)
{
}

const std::string& FileEnumerator::File::getFileName() const
{
    return m_fileName;
}

const std::string& FileEnumerator::File::getFullPath() const
{
    return m_fullPath;
}

bool FileEnumerator::File::isDirectory() const
{
    return m_isDir;
}

bool FileEnumerator::File::isNormalFile() const
{
    return m_isNormalFile;
}

time_t FileEnumerator::File::getModifiedTime() const
{
    return m_modifiedTime;
}

uint64_t FileEnumerator::File::getFileSize() const
{
    return m_fileSize;
}

FileEnumerator::FileEnumerator(const std::string& path) : m_path(path), m_handle(NULL), m_first(true)
{
#ifdef _WIN32
    HANDLE hFind = INVALID_HANDLE_VALUE;
    m_handle = (void *)hFind;
#else
    
#endif
}

FileEnumerator::~FileEnumerator()
{
#ifdef _WIN32
    HANDLE hFind = (HANDLE)m_handle;
    if (hFind != INVALID_HANDLE_VALUE)
    {
        ::FindClose(hFind);
    }
#else
    DIR *dir = (DIR *)m_handle;
    if (dir != NULL)
    {
        closedir(dir);
    }
#endif
}

bool FileEnumerator::nextFile(File& file)
{
#ifdef _WIN32
    bool res = false;
    HANDLE hFind = (HANDLE)m_handle;
    WIN32_FIND_DATA FindFileData;
    
    if (m_first)
    {
        m_first = false;
        std::string formatedPath = combinePath(m_path, "*.*");
        std::replace(formatedPath.begin(), formatedPath.end(), ALT_DIR_SEP, DIR_SEP);

        CW2T pszPath(CA2W(formatedPath.c_str(), CP_UTF8));
        
        hFind = ::FindFirstFile((LPTSTR)pszPath, &FindFileData);
        m_handle = (void *)hFind;
        if (hFind == INVALID_HANDLE_VALUE)
        {
            return res;
        }
        do
        {
            if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
            {
                continue;
            }
            
            res = true;
            break;
        } while (::FindNextFile(hFind, &FindFileData));
    }
    else
    {
        if (hFind == INVALID_HANDLE_VALUE)
        {
            return res;
        }

        while (::FindNextFile(hFind, &FindFileData))
        {
            if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
            {
                continue;
            }

            res = true;
            break;
        }
    }
    
    if (res)
    {
        file.m_fileName = (LPCSTR)CW2A(CT2W(FindFileData.cFileName), CP_UTF8);
        file.m_fullPath = combinePath(m_path, file.m_fileName);
        file.m_isDir = ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
        file.m_isNormalFile = !file.m_isDir && ((FindFileData.dwFileAttributes & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE)) != 0);
		if (file.m_isDir)
		{
			file.m_fileSize = (FindFileData.nFileSizeHigh * (MAXDWORD + 1)) + FindFileData.nFileSizeLow;
		}
		else
		{
			struct stat st;
			stat(file.m_fullPath.c_str(), &st);
		}
		
        file.m_modifiedTime = FileTimeToTime(FindFileData.ftLastWriteTime);
        
#ifndef NDEBUG
        struct __stat64 st;
        _tstat64(CW2T(CA2W(file.m_fullPath.c_str(), CP_UTF8)), &st);
        assert(file.m_isNormalFile == S_ISREG(st.st_mode));
		if (!S_ISDIR(st.st_mode))
		{
			assert(file.m_fileSize == st.st_size);
		}
#endif
    }
    
    return res;
#else
    DIR *dir = (DIR *)m_handle;
    bool res = false;
    if (m_first)
    {
        m_first = false;
        dir = opendir(m_path.c_str());
        m_handle = (void *)dir;
        if (dir == NULL)
        {
            return res;
        }
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        
        file.m_fileName = entry->d_name;
        file.m_fullPath = combinePath(m_path, file.m_fileName);
        
        struct stat st;
        stat(file.m_fullPath.c_str(), &st);

        file.m_fileSize = st.st_size;
        file.m_isDir = S_ISDIR(st.st_mode);
        file.m_isNormalFile = S_ISREG(st.st_mode);
        file.m_modifiedTime = st.st_mtime;
            
        res = true;
        break;
    }
    
    return res;
#endif
}

File::File() :
#ifdef _WIN32
m_file(INVALID_HANDLE_VALUE)
#else
    m_file(NULL)
#endif
{
}

File::~File()
{
    close();
}

bool File::open(const std::string& path, bool readOnly/* = true*/)
{
#ifdef _WIN32
    CW2T pszT(CA2W(path.c_str(), CP_UTF8));
    if (readOnly)
    {
        m_file = CreateFile((LPCTSTR)pszT, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    else
    {
        m_file = CreateFile((LPCTSTR)pszT, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    return (INVALID_HANDLE_VALUE != m_file);
#else
    m_file = fopen(path.c_str(), readOnly ? "rb" : "wb");
    return NULL != m_file;
#endif
}

bool File::read(unsigned char* buffer, size_t bytesToRead, size_t& bytesRead)
{
#ifdef _WIN32
    DWORD numberOfBytesRead = 0;
    BOOL res = ReadFile(m_file, buffer, (DWORD)bytesToRead, &numberOfBytesRead, NULL);
    if (res)
    {
        bytesRead = numberOfBytesRead;
    }
    return res;
#else
    if (m_file != NULL && bytesToRead == 0)
    {
        bytesRead = 0;
        return true;
    }
    size_t res = fread(buffer, 1, bytesToRead, m_file);
    if (res < bytesToRead)
    {
        bytesRead = res;
        if (feof(m_file))
        {
            return true;
        }
        
        return ferror(m_file) != 0;
    }
    return true;
#endif
    
}

bool File::write(const unsigned char* buffer, size_t bytesToWrite, size_t& bytesWritten)
{
#ifdef _WIN32
    DWORD numberOfBytesWritten = 0;
    BOOL res = WriteFile(m_file, buffer, static_cast<DWORD>(bytesToWrite), &numberOfBytesWritten, NULL);
    bytesWritten = res ? numberOfBytesWritten : 0;
    return (TRUE == res);
#else
    if (NULL != m_file && bytesToWrite == 0)
    {
        bytesWritten = 0;
        return true;
    }
    
    bytesWritten = fwrite(buffer, 1, bytesToWrite, m_file);
    return (bytesWritten == bytesToWrite);
#endif
}

void File::close()
{
#ifdef _WIN32
    if (INVALID_HANDLE_VALUE != m_file)
    {
        CloseHandle(m_file);
        m_file = INVALID_HANDLE_VALUE;
    }
#else
    if (NULL != m_file)
    {
        fclose(m_file);
        m_file = NULL;
    }
#endif
}
