//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "Utils.h"
#include <ctime>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cstdio>
#ifdef _WIN32
#include <direct.h>
#include <atlstr.h>
#include <sys/utime.h>
#include "Shlwapi.h"
#else
#include <utime.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include "OSDef.h"

void replaceAll(std::string& input, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while((pos = input.find(search, pos)) != std::string::npos)
    {
        input.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}

void replaceAll(std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs)
{
    for (std::vector<std::pair<std::string, std::string>>::const_iterator it = pairs.cbegin(); it != pairs.cend(); ++it)
    {
        size_t pos = 0;
        while((pos = input.find(it->first, pos)) != std::string::npos)
        {
            input.replace(pos, it->first.length(), it->second);
            pos += it->second.length();
        }
    }
}

std::string replaceAll(const std::string& input, const std::string& search, const std::string& replace)
{
    std::string result = input;
    replaceAll(result, search, replace);
    return result;
}

std::string replaceAll(const std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs)
{
    std::string result = input;
    replaceAll(result, pairs);

    return result;
}

bool endsWith(const std::string& str, const std::string& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

bool endsWith(const std::string& str, std::string::value_type ch)
{
    return !str.empty() && str[str.size() - 1] == ch;
}

bool startsWith(const std::string& str, const std::string& prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

bool startsWith(const std::string& str, std::string::value_type ch)
{
    return !str.empty() && str[0] == ch;
}

std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delimiter, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delimiter.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string join(const std::vector<std::string>& elements, const char *const delimiter)
{
    return join(std::cbegin(elements), std::cend(elements), delimiter);
}

std::string join(std::vector<std::string>::const_iterator b, std::vector<std::string>::const_iterator e, const char *const delimiter)
{
    std::ostringstream os;
    if (b != e)
    {
        auto pe = prev(e);
        for (std::vector<std::string>::const_iterator it = b; it != pe; ++it)
        {
            os << *it;
            os << delimiter;
        }
        b = pe;
    }
    if (b != e)
    {
        os << *b;
    }

    return os.str();
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

	::PathCombine(buffer, pszT1, pszT2);

	CW2A pszU8(CT2W(buffer), CP_UTF8);
	path = pszU8;
#else
    
    if (!p1.empty())
    {
        path = p1;
        if (path[path.size() - 1] != DIR_SEP && path[path.size() - 1] != DIR_SEP_R)
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
    std::replace(path.begin(), path.end(), DIR_SEP_R, DIR_SEP);
}

std::string safeHTML(const std::string& s)
{
    static std::vector<std::pair<std::string, std::string>> replaces =
    { {"&", "&amp;"}, {" ", "&nbsp;"}, {"<", "&lt;"}, {">", "&gt;"}, {"\r\n", "<br/>"}, {"\r", "<br/>"}, {"\n", "<br/>"} };
    return replaceAll(s, replaces);
}

void removeHtmlTags(std::string& html)
{
    std::string::size_type startpos = 0;
    while ((startpos = html.find("<", startpos)) != std::string::npos)
    {
        // auto startpos = html.find("<");
        auto endpos = html.find(">", startpos + 1);
        if (endpos != std::string::npos)
        {
            html.erase(startpos, endpos - startpos + 1);
        }
    }
}

std::string removeCdata(const std::string& str)
{
    if (startsWith(str, "<![CDATA[") && endsWith(str, "]]>")) return str.substr(9, str.size() - 12);
    return str;
}

std::string fromUnixTime(unsigned int unixtime)
{
    std::uint32_t time_date_stamp = unixtime;
    std::time_t temp = time_date_stamp;
    std::tm* t = std::localtime(&temp);
    std::stringstream ss; // or if you're going to print, just input directly into the output stream
    ss << std::put_time(t, "%Y-%m-%d %I:%M:%S %p");
    
    return ss.str();
}

/*
bool existsFile(const std::string &path)
{
#ifdef _WIN32
    struct stat buffer;
	CW2A pszA(CA2W(path.c_str(), CP_UTF8));
    return (stat ((LPCSTR)pszA, &buffer) == 0);
#else
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
#endif
}
*/

/*
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

int makePath(const std::string& path, mode_t mode)
{
    std::vector<std::string::value_type> copypath;
    copypath.reserve(path.size() + 1);
    std::copy(path.begin(), path.end(), std::back_inserter(copypath));
    copypath.push_back('\0');
    std::replace(copypath.begin(), copypath.end(), '\\', '/');
    
    std::vector<std::string::value_type>::iterator itStart = copypath.begin();
    std::vector<std::string::value_type>::iterator it;
    
    int status = 0;
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
*/
/*
bool moveFile(const std::string& src, const std::string& dest, bool overwrite)
{
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));
	if (overwrite)
	{
		::DeleteFile(pszDest);
	}
	BOOL bErrorFlag = ::MoveFile(pszSrc, pszDest);
	return (TRUE == bErrorFlag);
#else
	if (overwrite)
	{
		remove(dest.c_str());
	}
	return 0 == rename(src.c_str(), dest.c_str());
#endif
}

bool copyFile(const std::string& src, const std::string& dest)
{
#ifdef _WIN32
	CW2T pszSrc(CA2W(src.c_str(), CP_UTF8));
	CW2T pszDest(CA2W(dest.c_str(), CP_UTF8));

	BOOL bErrorFlag = ::CopyFile(pszSrc, pszDest, FALSE);
	return (TRUE == bErrorFlag);
#else
	std::ifstream  ss(src, std::ios::binary);
	std::ofstream  ds(dest, std::ios::binary);

	ds << ss.rdbuf();

	return true;
#endif
}
*/

#ifdef _WIN32
std::string utf8ToLocalAnsi(const std::string& utf8Str)
{
	CW2A pszA(CA2W(utf8Str.c_str(), CP_UTF8));
	return std::string((LPCSTR)pszA);
}
#else
#endif

void updateFileTime(const std::string& path, time_t mtime)
{
    const std::string& p = utf8ToLocalAnsi(path);
    
    struct stat st;
    struct utimbuf new_times;

    stat(p.c_str(), &st);
    
    new_times.actime = st.st_atime; /* keep atime unchanged */
    new_times.modtime = mtime;
    utime(p.c_str(), &new_times);
}

/*
bool deleteFile(const std::string& fileName)
{
    return 0 == std::remove(fileName.c_str());
}
*/

int openSqlite3ReadOnly(const std::string& path, sqlite3 **ppDb)
{
    std::string sep(1, DIR_SEP);
    std::string encodedPath;
#ifdef _WIN32
    TCHAR szDriver[_MAX_DRIVE] = { 0 };
    TCHAR szDir[_MAX_DIR] = { 0 };
    
    CW2T pszT(CA2W(normalizePath(path).c_str(), CP_UTF8));
    
    _tsplitpath(pszT, szDriver, szDir, NULL, NULL);
    size_t driveLen = _tcslen(szDriver);
    if (driveLen == 0)
    {
        // NO driver
        encodedPath = path;
    }
    else
    {
        CW2A pszU8(CT2W(&pszT[driveLen]), CP_UTF8);
        encodedPath = pszU8;
    }
#else
    encodedPath = normalizePath(path);
#endif

    std::vector<std::string> parts = split(encodedPath, sep);
    std::vector<std::string> encodedParts;
    encodedPath.reserve(parts.size() + 1);

    CURL *curl = curl_easy_init();
    if (curl)
    {
        for (std::vector<std::string>::const_iterator it = parts.cbegin(); it != parts.cend(); ++it)
        {
            char *ptr = curl_easy_escape(curl, it->c_str(), static_cast<int>(it->size()));
            if (ptr)
            {
                encodedParts.push_back(std::string(ptr));
                curl_free(ptr);
            }
        }

        curl_easy_cleanup(curl);

        encodedPath = join(encodedParts, sep.c_str());
    }

#ifdef _WIN32
    if (driveLen == 0)
    {
        if (_tcslen(szDir) > 0 && szDir[0] == DIR_SEP)
        {
            encodedPath = sep + encodedPath;
        }
    }
    else
    {
        CW2A pszU8(CT2W(szDriver), CP_UTF8);
        encodedPath = (LPCSTR)pszU8 + sep + encodedPath;
    }
#else
    if (startsWith(path, sep))
    {
        encodedPath = sep + encodedPath;
    }
#endif
    
    std::string pathWithQuery = "file:" + encodedPath;
    // std::string pathWithQuery = "file:" + path;
    pathWithQuery += "?immutable=1&mode=ro";
    
    // return sqlite3_open_v2(path.c_str(), ppDb, SQLITE_OPEN_READONLY, NULL);
    return sqlite3_open_v2(pathWithQuery.c_str(), ppDb, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, NULL);
}

int GetBigEndianInteger(const unsigned char* data, int startIndex/* = 0*/)
{
    return (data[startIndex] << 24)
         | (data[startIndex + 1] << 16)
         | (data[startIndex + 2] << 8)
         | data[startIndex + 3];
}

int GetLittleEndianInteger(const unsigned char* data, int startIndex/* = 0*/)
{
    return (data[startIndex + 3] << 24)
         | (data[startIndex + 2] << 16)
         | (data[startIndex + 1] << 8)
         | data[startIndex];
}

std::string encodeUrl(const std::string& url)
{
    std::string encodedUrl = url;
    CURL *curl = curl_easy_init();
    if(curl)
    {
        char *output = curl_easy_escape(curl, url.c_str(), static_cast<int>(url.size()));
        if(output)
        {
            encodedUrl = output;
            curl_free(output);
        }
        
        curl_easy_cleanup(curl);
    }
    
    return encodedUrl;
}

long long diff_tm(struct tm *a, struct tm *b) {
    return a->tm_sec - b->tm_sec
        + 60LL * (a->tm_min - b->tm_min)
        + 3600LL * (a->tm_hour - b->tm_hour)
        + 86400LL * (a->tm_yday - b->tm_yday)
        + (a->tm_year - 70) * 31536000LL
        - (a->tm_year - 69) / 4 * 86400LL
        + (a->tm_year - 1) / 100 * 86400LL
        - (a->tm_year + 299) / 400 * 86400LL
        - (b->tm_year - 70) * 31536000LL
        + (b->tm_year - 69) / 4 * 86400LL
        - (b->tm_year - 1) / 100 * 86400LL
        + (b->tm_year + 299) / 400 * 86400LL;
}

std::string utcToLocal(const std::string& utcTime)
{
    struct std::tm tp;
    std::istringstream ss(utcTime);
    ss >> std::get_time(&tp, "%Y-%m-%dT%H:%M:%SZ");
    tp.tm_isdst = -1;
    time_t utc = mktime(&tp);
    struct std::tm e0 = { 0 };
    e0.tm_year = tp.tm_year;
    e0.tm_mday = tp.tm_mday;
    e0.tm_mon = tp.tm_mon;
    e0.tm_hour = tp.tm_hour;
    e0.tm_isdst = -1;
    std::time_t pseudo = mktime(&e0);
    struct std::tm e1 = *gmtime(&pseudo);
    e0.tm_sec += utc - diff_tm(&e1, &e0);
    time_t local = e0.tm_sec;
    struct tm localt = *localtime(&local);
    char buf[30] = { 0 };
    strftime(buf, 30, "%Y-%m-%d %H:%M:%S", &localt);

    return std::string(buf);
}

std::string getCurrentTimestamp(bool includingYMD/* = false*/, bool includingMs/* = false*/)
{
    using std::chrono::system_clock;
    auto currentTime = std::chrono::system_clock::now();
    char buffer[80];

    std::time_t tt;
    tt = system_clock::to_time_t ( currentTime );
    auto timeinfo = localtime (&tt);
    strftime (buffer, 80, includingYMD ? "%F %H:%M:%S" : "%H:%M:%S", timeinfo);
    if (includingMs)
    {
        auto transformed = currentTime.time_since_epoch().count() / 1000000;
        auto millis = transformed % 1000;
        sprintf(buffer, "%s.%03d", buffer, (int)millis);
    }

    return std::string(buffer);
}
