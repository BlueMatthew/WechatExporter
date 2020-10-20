//
//  Shell.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//
#include <string>

#ifndef Shell_h
#define Shell_h

class Shell
{
public:
    
    
    virtual bool makeDirectory(const std::string& path) const = 0;
    virtual bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const = 0;
    virtual bool convertPlist(const std::string& plist, const std::string& xml) const = 0;
    virtual bool convertSilk(const std::string& silk, const std::string& mp3) const = 0;
    virtual int exec(const std::string& cmd) const = 0;
    virtual ~Shell() {}
    
    
};

#endif /* Shell_h */
