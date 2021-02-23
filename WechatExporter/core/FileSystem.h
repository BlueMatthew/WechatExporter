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

bool existsDirectory(const std::string& path);
bool makeDirectory(const std::string& path);
bool deleteFile(const std::string& path);
bool deleteDirectory(const std::string& path);
bool existsFile(const std::string& path);
bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories);
bool copyFile(const std::string& src, const std::string& dest, bool overwrite = true);
bool moveFile(const std::string& src, const std::string& dest, bool overwrite = true);
// ref: https://blackbeltreview.wordpress.com/2015/01/27/illegal-filename-characters-on-windows-vs-mac-os/
bool isValidFileName(const std::string& fileName);
std::string removeInvalidCharsForFileName(const std::string& fileName);


#endif /* FileSystem_h */
