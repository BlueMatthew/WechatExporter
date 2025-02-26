//
//  util.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <locale>

#ifdef _WIN32
#include <io.h>
typedef int mode_t;
#endif

#define ENABLE_AUDIO_CONVERTION

#ifndef Utils_h
#define Utils_h

int replaceAll(std::string& input, const std::string& search, const std::string& replace);
int replaceAll(std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs);
// std::string replaceAll(const std::string& input, const std::string& search, const std::string& replace);
// std::string replaceAll(const std::string& input, const std::vector<std::pair<std::string, std::string>>& pairs);


bool endsWith(const std::string& str, const std::string& suffix);
bool endsWith(const std::string& str, std::string::value_type ch);
bool startsWith(const std::string& str, const std::string& prefix, int pos = 0);
bool startsWith(const std::string& str, const std::string::value_type ch);

std::string toUpper(const std::string& str);
std::string toLower(const std::string& str);

std::vector<std::string> split(const std::string& str, const std::string& delim);
std::string join(const std::vector<std::string>& elements, const char *const delimiter);
std::string join(std::vector<std::string>::const_iterator b, std::vector<std::string>::const_iterator e, const char *const delimiter);

template <typename ...Args>
std::string formatString(const std::string& format, Args && ...args)
{
    auto size = std::snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
    std::string output(size, '\0');
    std::sprintf(&output[0], format.c_str(), std::forward<Args>(args)...);
    return output;
}

// bool existsFile(const std::string &path);
// int makePath(const std::string& path, mode_t mode);

std::string md5(const std::string& s);
std::string sha1(const std::string& s);
std::string md5File(const std::string& path);

std::string safeHTML(const std::string& s);
void removeHtmlTags(std::string& html);

std::string removeCdata(const std::string& str);

std::string fromUnixTime(unsigned int unixtime, bool localTime = true);
uint32_t getUnixTimeStamp();

const char* calcVarint32Ptr(const char* p, const char* limit, uint32_t* value);
const unsigned char* calcVarint32Ptr(const unsigned char* p, const unsigned char* limit, uint32_t* value);

// bool moveFile(const std::string& src, const std::string& dest, bool overwrite = true);
// bool copyFile(const std::string& src, const std::string& dest);

#ifdef _WIN32
std::string utf8ToLocalAnsi(const std::string& utf8Str);
#else
#define utf8ToLocalAnsi(utf8Str) utf8Str
#endif
void updateFileTime(const std::string& path, time_t mtime);
// bool deleteFile(const std::string& fileName);

bool isBigEndian();
int GetBigEndianInteger(const unsigned char* data, int startIndex = 0);
int GetLittleEndianInteger(const unsigned char* data, int startIndex = 0);

int16_t bigEndianToNative(int16_t n);
int32_t bigEndianToNative(int32_t n);
int64_t bigEndianToNative(int64_t n);
uint16_t bigEndianToNative(uint16_t n);
uint32_t bigEndianToNative(uint32_t n);
uint64_t bigEndianToNative(uint64_t n);

struct sqlite3;
int openSqlite3Database(const std::string& path, sqlite3 **ppDb, bool readOnly = true);

std::string encodeUrl(const std::string& url);
std::string decodeUrl(const std::string& url);

// std::string utcToLocal(const std::string& utcTime);
std::string getTimestampString(bool includingYMD = false, bool includingMs = false);

bool amrToPcm(const std::string& silkPath, std::vector<unsigned char>& pcmData, std::string* error = NULL);
bool amrToPcm(const std::string& silkPath, const std::string& pcmPath, std::string* error = NULL);

bool silkToPcm(const std::string& silkPath, std::vector<unsigned char>& pcmData, bool& isSilk, std::string* error = NULL);
bool silkToPcm(const std::string& silkPath, const std::string& pcmPath, bool& isSilk, std::string* error = NULL);

bool pcmToMp3(const std::string& pcmPath, const std::string& mp3Path, std::string* error = NULL);
bool pcmToMp3(const std::vector<unsigned char>& pcmData, const std::string& mp3Path, std::string* error = NULL);

bool amrPcmToMp3(const std::string& pcmPath, const std::string& mp3Path, std::string* error = NULL);
bool amrPcmToMp3(const std::vector<unsigned char>& pcmData, const std::string& mp3Path, std::string* error = NULL);

void setThreadName(const char* threadName);
bool isNumber(const std::string &s);

std::string makeUuid();

std::string toHex(unsigned char* data, size_t length);
std::string toHex(char* data, size_t length);

#endif /* Utils_h */
