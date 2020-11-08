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
    NSScrollView* m_scrollView;
    NSTextView* m_textView;
public:
    LoggerImpl(NSScrollView* scrollView, NSTextView* textField)
    {
        m_scrollView = scrollView;
        m_textView = textField;
    }
    
    ~LoggerImpl()
    {
        m_textView = NULL;
        m_scrollView = NULL;
    }
    
    void write(const std::string& log)
    {
        __block NSString *logString = [NSString stringWithUTF8String:log.c_str()];
        __block NSScrollView* scrollView = m_scrollView;
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
           
        
            NSPoint newScrollOrigin;
             
            // assume that the scrollview is an existing variable
            if ([[scrollView documentView] isFlipped])
            {
                newScrollOrigin=NSMakePoint(0.0,NSMaxY([[scrollView documentView] frame])
                                               -NSHeight([[scrollView contentView] bounds]));
            }
            else
            {
                newScrollOrigin = NSMakePoint(0.0,0.0);
            }

            [[scrollView documentView] scrollPoint:newScrollOrigin];

        
           // Smart Scrolling
           // BOOL scroll = (NSMaxY(txtView.visibleRect) == NSMaxY(txtView.bounds));
           // if (scroll) // Scroll to end of the textview contents
           {
               // [txtView scrollRangeToVisible: NSMakeRange(txtView.string.length, 0)];
           }
        });

    }
    
    void debug(const std::string& log)
    {
        write(log);
        NSString *logString = [NSString stringWithUTF8String:log.c_str()];
        
        NSLog(@"%@", logString);
    }
    
};

#endif /* LoggerImpl_h */
