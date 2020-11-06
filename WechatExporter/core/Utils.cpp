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
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <curl/curl.h>
#include "OSDef.h"


std::string replace_all(std::string input, std::string search, std::string replace)
{
    std::string result = input;
    size_t pos = 0;
    while((pos = result.find(search, pos)) != std::string::npos)
    {
         result.replace(pos, search.length(), replace);
         pos += replace.length();
    }
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
/*
std::string stringWithFormat(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    
    auto size = std::vsnprintf(nullptr, 0, format.c_str(), ap);
    std::string output(size + 1, '\0');
    std::vsnprintf(&output[0], format.c_str(), ap);
    va_end(ap);
    return result;
}
 */

std::string utf8ToString(const std::string& utf8str, const std::locale& loc)
{
	// UTF-8 to wstring
	std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;
	std::wstring wstr = wconv.from_bytes(utf8str);
	// wstring to string
	std::vector<char> buf(wstr.size());
	std::use_facet<std::ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '?', buf.data());
	return std::string(buf.data(), buf.size());
}

std::string combinePath(const std::string& p1, const std::string& p2)
{
    if (p1.empty() && p2.empty())
    {
        return "";
    }
    
    std::string path;
    if (!p1.empty())
    {
        path = p1;
        // std::replace(path.begin(), path.end(), '\\', '/');
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
    
    // std::replace(path.begin(), path.end(), DIR_SEP_R, DIR_SEP);
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

std::string safeHTML(const std::string& s)
{
    std::string result = replace_all(s, "&", "&amp;");
    result = replace_all(result, " ", "&nbsp;");
    result = replace_all(result, "<", "&lt;");
    result = replace_all(result, ">", "&gt;");
    result = replace_all(result, "\r\n", "<br/>");
    result = replace_all(result, "\r", "<br/>");
    result = replace_all(result, "\n", "<br/>");
    return result;
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
    std::tm* t = std::gmtime(&temp);
    std::stringstream ss; // or if you're going to print, just input directly into the output stream
    ss << std::put_time(t, "%Y-%m-%d %I:%M:%S %p");
    
    return ss.str();
}

bool existsFile(const std::string &path)
{
    struct stat buffer;
    return (stat (path.c_str(), &buffer) == 0);
}

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
    std::ifstream file(path, std::ios::in|std::ios::binary|std::ios::ate);
    if (file.is_open())
    {
        std::streampos size = file.tellg();
        std::vector<std::string::value_type> buffer;
        data.resize(size);
        
        file.seekg (0, std::ios::beg);
        file.read((char *)(&data[0]), size);
        file.close();
        
        return true;
    }
    
    return false;
}

bool writeFile(const std::string& path, const std::vector<unsigned char>& data)
{
    std::ofstream ofs;
    ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (ofs.is_open())
    {
        ofs.write(reinterpret_cast<const char *>(&(data[0])), data.size());
        ofs.close();
        return true;
    }
    
    return false;
}

bool isValidFileName(const std::string& fileName)
{
    char const *tmpdir = getenv("TMPDIR");
    if (tmpdir == 0)
    {
#if defined(_WIN32)
        // tmpdir = "/tmp";
#else
        tmpdir = "/tmp";

#endif
    }
    
    // std::string tmpname = std::tmpnam(nullptr);
    
    std::string path = combinePath(tmpdir, fileName);
    
#ifdef _WIN32
	int status = mkdir(path.c_str());
#else
    int status = mkdir(path.c_str(), 0);
#endif
    int lastErrorNo = errno;
    if (status == 0)
    {
        remove(path.c_str());
    }
    
    return status == 0 || lastErrorNo == EEXIST;
}

int openSqlite3ReadOnly(const std::string& path, sqlite3 **ppDb)
{
    std::string pathWithQuery = "file:" + path;
    pathWithQuery += "?immutable=1&mode=ro";
    
    return sqlite3_open_v2(pathWithQuery.c_str(), ppDb, SQLITE_OPEN_READONLY, NULL);
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
