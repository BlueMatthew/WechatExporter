//
//  ShellImpl.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/1.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "core/Shell.h"

#ifndef ShellImpl_h
#define ShellImpl_h


class ShellImpl : public Shell
{
public:
    bool makeDirectory(const std::string& path) const;
    bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const;
    bool convertPlist(const std::string& plist, const std::string& xml) const;
    bool convertSilk(const std::string& silk, const std::string& mp3) const;
    int exec(const std::string& cmd) const;
};

#endif /* ShellImpl_h */
