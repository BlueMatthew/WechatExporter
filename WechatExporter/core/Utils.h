//
//  util.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>

#ifndef Utils_h
#define Utils_h

std::string replace_all(std::string input, std::string search, std::string format);

bool endsWith(const std::string& str, const std::string& suffix);
bool endsWith(const std::string& str, std::string::value_type ch);
bool startsWith(const std::string& str, const std::string& prefix);
bool startsWith(const std::string& str, const std::string::value_type ch);

std::string combinePath(const std::string& p1, const std::string& p2);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3);
std::string combinePath(const std::string& p1, const std::string& p2, const std::string& p3, const std::string& p4);

bool existsFile(const std::string &path);
int makePath(const std::string& path, mode_t mode);

std::string md5(const std::string& s);

std::string safeHTML(const std::string& s);

std::string removeCdata(const std::string& str);

std::string fromUnixTime(unsigned int unixtime);

const char* calcVarint32Ptr(const char* p, const char* limit, uint32_t* value);
const unsigned char* calcVarint32Ptr(const unsigned char* p, const unsigned char* limit, uint32_t* value);

std::string readFile(const std::string& path);
bool readFile(const std::string& path, std::vector<unsigned char>& data);

int GetBigEndianInteger(const unsigned char* data, int startIndex = 0);
int GetLittleEndianInteger(const unsigned char* data, int startIndex = 0);

#endif /* Utils_h */
