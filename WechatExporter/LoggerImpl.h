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
    NSLock *m_lock;
    char m_logFile[1024];
    
public:
    LoggerImpl(ViewController* viewController)
    {
        m_viewController = viewController;
        m_lock = [[NSLock alloc] init];
    }
    
    ~LoggerImpl()
    {
        m_viewController = nil;
        m_lock = nil;
    }
    
    void setLogPath(const char* logPath)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        [m_lock lock];
        strcpy(m_logFile, logPath);
        if (!endsWith(logPath, DIR_SEP_STR))
        {
            strcat(m_logFile, DIR_SEP_STR);
        }
        strcat(m_logFile, "log.txt");
        FILE *file = fopen(m_logFile, "w");
        if (NULL != file)
        {
            fclose(file);
        }
        [m_lock unlock];
#endif
    }
    
    void write(const std::string& log)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        std::string timeString = getTimestampString(false, true) + ": ";
#else
        std::string timeString = getTimestampString() + ": ";
#endif
        
#if !defined(NDEBUG) || defined(DBG_PERF)
        [m_lock lock];
        if (strlen(m_logFile) > 0)
        {
            FILE *file = fopen(m_logFile, "a");
            if (NULL != file)
            {
                fputs(timeString.c_str(), file);
                fputs(log.c_str(), file);
                fputs("\r", file);
                fclose(file);
            }
        }
        [m_lock unlock];
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
        write(log);
#if !defined(NDEBUG) || defined(DBG_PERF)
        NSString *logString = [NSString stringWithUTF8String:log.c_str()];
        NSLog(@"%@", logString);
#endif
    }
    
};

#endif /* LoggerImpl_h */
