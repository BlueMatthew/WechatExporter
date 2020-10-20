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
#include <sys/types.h>
#include <sys/stat.h>
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

int makePathImpl(const std::string::value_type *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0)
    {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    }
    else if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
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
    std::ifstream file(path.c_str(), std::ios::in|std::ios::binary|std::ios::ate);
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
