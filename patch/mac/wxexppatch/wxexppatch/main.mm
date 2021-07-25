//
//  main.m
//  wxexppatch
//
//  Created by Matthew on 2021/7/24.
//

#import <Foundation/Foundation.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <vector>

void addPathSlash(std::string& path)
{
    if (!path.empty())
    {
        if (path[path.size() - 1] != '/' && path[path.size() - 1] != '\\')
        {
            path += '/';
        }
    }
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
        if (path[path.size() - 1] != '/' && path[path.size() - 1] != '\\')
        {
            path += '/';
        }
        
        path += p2;
    }
    else
    {
        path = p2;
    }

    return path;
}

std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3)
{
    return combinePath(combinePath(p1, p2), p3);
}

bool writeFile(const std::string& path, const char* data, size_t dataLength)
{
    std::ofstream ofs;
    ofs.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (ofs.is_open())
    {
        ofs.write(reinterpret_cast<const char *>(data), dataLength);
        ofs.close();
        return true;
    }

    return false;
}

bool appendFile(const std::string& path, const char* data, size_t dataLength)
{
    std::ofstream ofs;
    ofs.open(path, std::fstream::in | std::fstream::out | std::fstream::app | std::fstream::binary);
    if (ofs.is_open())
    {
        ofs.write(reinterpret_cast<const char *>(data), dataLength);
        ofs.close();
        return true;
    }

    return false;
}


bool findInvalidHtmlContent(const char *buffer, size_t& startPos, size_t& endPos)
{
    const char* str = strstr(buffer, "loadMsgsForNextPage");
    if (str == NULL)
    {
        return false;
    }

    const char* errorContents = "for (var idx = 0; idx < window.moreWechatMsgs.length; idx++)";
    str = strstr(buffer, errorContents);
    if (str == NULL)
    {
        return false;
    }
    startPos = str - buffer;
    endPos = strlen(errorContents) + startPos;

    return true;
}

bool doPatch(const char* root)
{
    std::string rootPath(root);
    
    addPathSlash(rootPath);
    
    std::vector<char> buffer;
    std::queue<std::string> directories;
    directories.push("");

    // char fullPath[PATH_MAX] = { 0 };
    size_t startPos = 0;
    size_t endPos = 0;
    const char* fixedContents = "while (window.moreWechatMsgs.length > 0)";
    const char* logTitle = "Fixed File List:\r\n";
    struct stat statbuf;
    size_t numberOfBytesRead = 0;
    
    char logPath[PATH_MAX] = { 0 };
    char log[PATH_MAX] = { 0 };
    strcpy(logPath, rootPath.c_str());
    strcat(logPath, "patch.log");

    writeFile(logPath, logTitle, strlen(logTitle));

    while (!directories.empty())
    {
        const std::string& dirName = directories.front();

        std::string path = combinePath(rootPath, dirName);
        // PathCombine(szFindPath, szRoot, dirName);
        // PathAddBackslash(szFindPath);
        // PathAppend(szFindPath, TEXT("*.*"));

        struct dirent *entry = NULL;
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

            std::string relativePath = dirName + entry->d_name;
            std::string fullPath = rootPath + relativePath;
            lstat(fullPath.c_str(), &statbuf);
            bool isDir = S_ISDIR(statbuf.st_mode);
            
            size_t len = 0;
            if (isDir)
            {
                addPathSlash(relativePath);
                directories.emplace(relativePath);
            }
            else if (((len = strlen(entry->d_name)) > 5) && strcmp(entry->d_name + len - 5, ".html") == 0)
            {
                // .html
                FILE* file = fopen(fullPath.c_str(), "r");
                if (file != NULL)
                {
                    buffer.resize(statbuf.st_size + 1);
                    buffer[statbuf.st_size] = 0;
                    if ((numberOfBytesRead = fread(&buffer[0], 1, statbuf.st_size, file)) > 0)
                    {
                        buffer[numberOfBytesRead] = 0;
                        if (findInvalidHtmlContent(&buffer[0], startPos, endPos))
                        {
                            file = freopen (fullPath.c_str(), "w+", file);
                            fwrite(&buffer[0], 1, startPos, file);
                            fwrite(fixedContents, 1, strlen(fixedContents), file);
                            fwrite(&buffer[endPos], 1, buffer.size() - 1 - endPos, file);
                            
                            strcpy(log, relativePath.c_str());
                            strcat(log, "\r");
                            
                            appendFile(logPath, log, strlen(log));
                        }
                    }

                    fclose(file);
                }
            }

        }
        closedir(dir);

        directories.pop();
    }

    std::cout << "All files have been fixed. May check patch_log.txt for more details." << std::endl;

    return true;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        doPatch([[[NSFileManager defaultManager] currentDirectoryPath] UTF8String]);
    }
    return 0;
}
