//
//  PdfConverterImpl.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "PdfConverter.h"
#import <Cocoa/Cocoa.h>
#include "FileSystem.h"
#include "Utils.h"

#ifndef PdfConverterImpl_h
#define PdfConverterImpl_h

class PdfConverterImpl : public PdfConverter
{
public:
    PdfConverterImpl(const char *outputDir) : m_pdfSupported(false)
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
        
        if (NULL != outputDir)
        {
            initShellFile(outputDir);
        }
    }
    
    bool isPdfSupported() const
    {
        return m_pdfSupported;
    }

    ~PdfConverterImpl()
    {
    }
    
    void executeCommand()
    {
        NSString *shellPathString = [NSString stringWithUTF8String:m_shellPath.c_str()];
        if (![[NSFileManager defaultManager] fileExistsAtPath:shellPathString])
        {
            return;
        }

        NSURL *terminalUrl = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:@"com.apple.Terminal"];
        if (nil == terminalUrl)
        {
            return;
        }
        
        [[NSWorkspace sharedWorkspace] openFile:shellPathString withApplication:terminalUrl.path];
    }
    
    void setWorkDir(NSString* workDir)
    {
        m_workDir = [NSString stringWithString:workDir];
    }
    
    bool makeUserDirectory(const std::string& dirName)
    {
        // std::string command = replaceAll(dirName, " ", "\\ ");
        std::string command = NEW_LINE + "[ -d \"pdf/" + dirName + "\" ] || mkdir \"pdf/" + dirName + "\"" + NEW_LINE;
        
        appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(command.c_str()), command.size());
        
        return true;
    }
    
    bool convert(const std::string& htmlPath, const std::string& pdfPath)
    {
        if (!m_pdfSupported)
        {
            return false;
        }
        
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:htmlPath.c_str()]];
     
        std::string command = "chrome ";
        command += "--headless --disable-gpu --disable-extensions --print-to-pdf-no-header --print-to-pdf=\"";
        command += pdfPath;
        command += "\" \"";
        command += [[url absoluteString] UTF8String];
        command += "\" >/dev/null 2>&1" + NEW_LINE;
        command += "echo \"" + pdfPath.substr(m_output.size()) + "\"" + NEW_LINE;
                
        appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(command.c_str()), command.size());
        
        return true;
        
        /*
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
        NSString *appPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:@"com.google.Chrome"];
        if (nil != appPath)
        {
            appPath = [appPath stringByAppendingPathComponent:@"Contents/MacOS/Google Chrome"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:appPath])
            {
                m_assemblyPath = [NSString stringWithString:appPath];
                return true;
            }
        }
        
        return false;
    }

    bool detectEdgeInstalled()
    {
        NSString *appPath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:@"com.microsoft.edgemac"];
        if (nil != appPath)
        {
            appPath = [appPath stringByAppendingPathComponent:@"Contents/MacOS/Microsoft Edge"];
            if ([[NSFileManager defaultManager] fileExistsAtPath:appPath])
            {
                m_assemblyPath = [NSString stringWithString:appPath];
                return true;
            }
        }
        
        return false;
    }
    
    void initShellFile(const char *outputDir)
    {
        m_output = outputDir;
        if (!endsWith(m_output, "/"))
        {
            m_output += "/";
        }
        m_shellPath = combinePath(m_output, "pdf.sh");
        deleteFile(m_shellPath);

        std::string aliasCmd = [m_assemblyPath UTF8String];
        replaceAll(aliasCmd, " ", "\\ ");
        aliasCmd = "#!/bin/sh" + NEW_LINE + NEW_LINE + "alias chrome=\"" + aliasCmd + "\"" + NEW_LINE + NEW_LINE;
        
        std::string output = m_output;
        // replaceAll(output, " ", "\\ ");
        
        aliasCmd += "cd \"" + output + "\"" + NEW_LINE;
        aliasCmd += "[ -d pdf ] || mkdir pdf" + NEW_LINE;
        
        NSDictionary<NSFileAttributeKey, id> *attributes = @{NSFilePosixPermissions : [NSNumber numberWithShort:0777]};
        NSData *contents = [NSData dataWithBytes:aliasCmd.c_str() length:aliasCmd.size()];
        NSString *shellPath = [NSString stringWithUTF8String:m_shellPath.c_str()];
        [[NSFileManager defaultManager] createFileAtPath:shellPath contents:contents attributes:attributes];
        
        // appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(aliasCmd.c_str()), aliasCmd.size());
    }

private:
    bool m_pdfSupported;
    NSString *m_assemblyPath;
    NSArray *m_param;
    std::string m_output;
    std::string m_shellPath;
    NSString *m_workDir;
    
    const std::string NEW_LINE = "\n";
};


#endif /* PdfConverterImpl_h */
