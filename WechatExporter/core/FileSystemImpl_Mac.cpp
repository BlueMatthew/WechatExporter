//
//  FileSystem_Mac.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef _WIN32

#include "FileSystem.h"
#include "Utils.h"
// #include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fts.h>

bool existsDirectory(const std::string& path)
{
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode));
}

bool existsFile(const std::string& path)
{
    struct stat sb;
    return (stat(path.c_str(), &sb) == 0);
}

bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories)
{
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

    return true;
}

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

bool makeDirectory(const std::string& path)
{
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
    
    return status;
}

bool deleteFile(const std::string& path)
{
    return 0 == std::remove(path.c_str());
}

bool deleteDirectory(const std::string& path)
{
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
}

bool copyFile(const std::string& src, const std::string& dest, bool overwrite/* = true*/)
{
    std::ifstream ss(src, std::ios::binary);
    std::ofstream ds(dest, std::ios::binary);

    ds << ss.rdbuf();

    return true;
}

bool moveFile(const std::string& src, const std::string& dest, bool overwrite/* = true*/)
{
    if (overwrite)
    {
        remove(dest.c_str());
    }
    return rename(src.c_str(), dest.c_str()) == 0;
}

bool isValidFileName(const std::string& fileName)
{
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
}


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
}

#endif // #ifndef _WIN32
