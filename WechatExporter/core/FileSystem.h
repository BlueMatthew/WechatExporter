//
//  FileSystem.h
//  WechatExporter
//
//  Created by Matthew on 2021/1/21.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef FileSystem_h
#define FileSystem_h

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#endif

#ifdef _WIN32
#define DIR_SEP '\\'
#define DIR_SEP_STR "\\"
#define ALT_DIR_SEP '/'
#else
#define DIR_SEP '/'
#define DIR_SEP_STR "/"
#define ALT_DIR_SEP '\\'
#endif

size_t getFileSize(const std::string& path);
bool existsDirectory(const std::string& path);
bool makeDirectory(const std::string& path);
bool deleteFile(const std::string& path);
bool deleteDirectory(const std::string& path);
bool existsFile(const std::string& path);
bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories);
bool listDirectory(const std::string& path, std::vector<std::string>& subDirectories, std::vector<std::string>& subFiles);
bool copyFile(const std::string& src, const std::string& dest, bool overwrite = true);
bool copyFileIfNewer(const std::string& src, const std::string& dest);
bool copyDirectory(const std::string& src, const std::string& dest);
bool moveFile(const std::string& src, const std::string& dest, bool overwrite = true);
// ref: https://blackbeltreview.wordpress.com/2015/01/27/illegal-filename-characters-on-windows-vs-mac-os/
bool isValidFileName(const std::string& fileName);
std::string removeInvalidCharsForFileName(const std::string& fileName);

std::string readFile(const std::string& path);
bool readFile(const std::string& path, std::vector<unsigned char>& data);
bool writeFile(const std::string& path, const std::vector<unsigned char>& data);
bool writeFile(const std::string& path, const std::string& data);
bool writeFile(const std::string& path, const unsigned char* data, size_t dataLength);
bool appendFile(const std::string& path, const std::string& data);
bool appendFile(const std::string& path, const unsigned char* data, size_t dataLength);



std::string combinePath(const std::string& p1, const std::string& p2);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3, const std::string& p4);

std::string normalizePath(const std::string& path);
void normalizePath(std::string& path);

// return zero for succeeding and non-zero for failure
int calcFreeSpace(const std::string& path, uint64_t& freeSpace);

#ifdef _WIN32
time_t FileTimeToTime(FILETIME ft);
#endif

class FileEnumerator
{
public:
    class File
    {
    public:
        
        friend FileEnumerator;
        
        File();
        
        const std::string& getFileName() const;
        const std::string& getFullPath() const;
        bool isDirectory() const;
        bool isNormalFile() const;
        time_t getModifiedTime() const;
        uint64_t getFileSize() const;
        
    private:
        std::string m_fileName;
        std::string m_fullPath;
        time_t m_modifiedTime;
        bool m_isDir;
        bool m_isNormalFile;
        uint64_t m_fileSize;
    };
    
    FileEnumerator(const std::string& path);
    ~FileEnumerator();
    
    bool isValid() const;
    
    bool nextFile(File& file);
    
private:
    std::string m_path;
    void* m_handle;
    bool m_first;
};

class File
{
public:
    File();
    // Will close the file
    ~File();
    
    bool open(const std::string& path, bool readOnly = true);
    bool read(unsigned char* buffer, size_t bytesToRead, size_t& bytesRead);
    bool write(const unsigned char* buffer, size_t bytesToWrite, size_t& bytesWritten);
    
    void close();
    
private:
#ifdef _WIN32
    HANDLE m_file;
#else
    FILE* m_file;
#endif
};


#endif /* FileSystem_h */
