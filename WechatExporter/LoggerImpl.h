//
//  LoggerImpl.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/1.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "core/Logger.h"

#ifndef LoggerImpl_h
#define LoggerImpl_h

class LoggerImpl : public Logger
{
protected:
    NSTextView* m_textView;
public:
    LoggerImpl(NSTextView* textField)
    {
        m_textView = textField;
    }
    
    ~LoggerImpl()
    {
        m_textView = NULL;
    }
    
    void write(const std::string& log)
    {
        __block NSString *logString = [NSString stringWithUTF8String:log.c_str()];
        __block NSTextView* txtView = m_textView;
        
        dispatch_async(dispatch_get_main_queue(), ^{
           NSString *oldLog = txtView.string;
           
           if (nil == oldLog || oldLog.length == 0)
           {
               oldLog = logString;
           }
           else
           {
               oldLog = [NSString stringWithFormat:@"%@\n%@", oldLog, logString];
           }
           txtView.string = oldLog;
           
           // Smart Scrolling
           BOOL scroll = (NSMaxY(txtView.visibleRect) == NSMaxY(txtView.bounds));
           if (scroll) // Scroll to end of the textview contents
           {
               [txtView scrollRangeToVisible: NSMakeRange(txtView.string.length, 0)];
           }
        });

    }
    
    void debug(const std::string& log)
    {
        write(log);
    }
    
};

#endif /* LoggerImpl_h */
