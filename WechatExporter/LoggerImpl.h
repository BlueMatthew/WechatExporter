//
//  LoggerImpl.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/1.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "core/Logger.h"
#include <ctime>
#include "ViewController.h"

#ifndef LoggerImpl_h
#define LoggerImpl_h

class LoggerImpl : public Logger
{
protected:
    __weak ViewController *m_viewController;

public:
    LoggerImpl(ViewController* viewController)
    {
        m_viewController = viewController;
    }
    
    ~LoggerImpl()
    {
        m_viewController = nil;
    }
    
    void write(const std::string& log)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        std::string timeString = getTimestampString(false, true) + ": ";
#else
        std::string timeString = getCurrentTimestamp() + ": ";
#endif

        __block NSString *logString = [NSString stringWithUTF8String:(timeString + log).c_str()];
        __block __weak ViewController* viewController = m_viewController;
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong __typeof(viewController)strongVC = viewController;
            if (strongVC)
            {
                [strongVC writeLog:logString];
                strongVC = nil;
            }
        });
    }
    
    void debug(const std::string& log)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        write(log);
        NSString *logString = [NSString stringWithUTF8String:log.c_str()];
        NSLog(@"%@", logString);
#endif
    }
    
};

#endif /* LoggerImpl_h */
