//
//  Logger.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>

#ifndef Logger_h
#define Logger_h

class Logger
{
public:
    virtual void write(const std::string& log) = 0;
    virtual void debug(const std::string& log) = 0;
    virtual ~Logger() {}
};

#endif /* Logger_h */
