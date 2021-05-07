//
//  PdfConverterImpl.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "PdfConverter.h"
#include "FileSystem.h"

#ifndef PdfConverterImpl_h
#define PdfConverterImpl_h

class PdfConverterImpl : public PdfConverter
{
public:
    PdfConverterImpl() : m_pdfSupported(false)
    {
        if (detectChromeInstalled())
        {
            m_pdfSupported = true;
            m_param = @[@"--headless", @"--disable-extensions", @"--disable-gpu", @"--print-to-pdf-no-header"];
            // m_param = "--headless --disable-extensions --disable-gpu --print-to-pdf=\"%%DEST%%\" --print-to-pdf-no-header \"file://%%SRC%%\"";
        }
        else if (detectEdgeInstalled())
        {
            m_pdfSupported = true;
            // m_param = @[@"--headless", @"--disable-extensions", @"--disable-gpu", @"--print-to-pdf-no-header"];
            m_param = @[@"--headless", @"--print-to-pdf-no-header"];
        }
    }
    
    bool isPdfSupported() const
    {
        return false;
        // return m_pdfSupported;
    }

    ~PdfConverterImpl()
    {
    }
    
    void setWorkDir(NSString* workDir)
    {
        m_workDir = [NSString stringWithString:workDir];
    }
    
    void initShellFile(const std::string& output)
    {
        
    }
    
    void appendConvertCommand(const std::string& htmlPath, const std::string& pdfPath)
    {
        if (!m_pdfSupported)
        {
            return;
        }

        std::string command = [m_assemblyPath UTF8String];
        command += " --headless --print-to-pdf-no-header --print-to-pdf=\"" + pdfPath + "\" ";
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:htmlPath.c_str()]];
        
        command += [[url absoluteString] UTF8String];
        
        // return command;
    }

    bool convert(const std::string& htmlPath, const std::string& pdfPath)
    {
        return false;
        
        /*
        if (!m_pdfSupported)
        {
            return false;
        }

        std::string command = [m_assemblyPath UTF8String];
        command += " ";
        
        NSString *src = [NSString stringWithUTF8String:htmlPath.c_str()];
        NSString *dest = [NSString stringWithUTF8String:pdfPath.c_str()];
        NSURL *url = [NSURL fileURLWithPath:src];
        
        NSMutableArray *params = [NSMutableArray arrayWithArray:m_param];
        [params addObject:[NSString stringWithFormat:@"--print-to-pdf=\"%@\"", dest]];
        [params addObject:[url absoluteString]];
        
        NSString* args = [params componentsJoinedByString:@" "];
        NSString* arg1 = [NSString stringWithFormat:@"--print-to-pdf=\"%@\"", dest];
        
        command += [args UTF8String];
        
        // system(command.c_str());
        
        execlp([m_assemblyPath UTF8String], [m_assemblyPath UTF8String], "--headless", "--print-to-pdf-no-header", [arg1 UTF8String], [[url absoluteString] UTF8String], NULL);
        */
        
        /*
        // std::string param = m_param;
        // replaceAll(param, "%%SRC%%", htmlPath);
        // replaceAll(param, "%%DEST%%", pdfPath);

        NSTask *task = [[NSTask alloc] init];
        task.launchPath = m_assemblyPath;
        task.arguments = params;
        task.currentDirectoryPath = m_workDir;
        // task.standardOutput = pipe;
        NSPipe *pipe = [NSPipe pipe];
        NSFileHandle *file = pipe.fileHandleForReading;
        
        task.standardOutput = pipe;
        
        [task launch];

        NSData *data = [file readDataToEndOfFile];
        [file closeFile];
        
        // [[outputPipe fileHandleForReading] readToEndOfFileInBackgroundAndNotify];
        
        [task waitUntilExit];
        int status = [task terminationStatus];
        
        return status == 0;
         */
    }

protected:
    bool detectChromeInstalled()
    {
        NSString *appPath = @"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
        if ([[NSFileManager defaultManager] fileExistsAtPath:appPath])
        {
            m_assemblyPath = [NSString stringWithString:appPath];
#ifndef NDEBUG
            return false;
#endif
            return true;
        }
        
        return false;
    }

    bool detectEdgeInstalled()
    {
        NSString *appPath = @"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge";
        if ([[NSFileManager defaultManager] fileExistsAtPath:appPath])
        {
            m_assemblyPath = [NSString stringWithString:appPath];
            return true;
        }
        
        return false;
    }

private:
    bool m_pdfSupported;
    NSString *m_assemblyPath;
    NSArray *m_param;
    NSString *m_workDir;
};


#endif /* PdfConverterImpl_h */
