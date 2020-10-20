//
//  ShellImpl.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/10/1.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "ShellImpl.h"



bool ShellImpl::makeDirectory(const std::string& path) const
{
    return true;
}

bool ShellImpl::copyFile(const std::string& src, const std::string& dest, bool overwrite) const
{
    return true;
}

bool ShellImpl::convertPlist(const std::string& plist, const std::string& xml) const
{
    return true;
}

bool ShellImpl::convertSilk(const std::string& silk, const std::string& mp3) const
{
    return true;
}

int ShellImpl::exec(const std::string& cmd) const
{
    return 0;
}

