//
//  PdfConverterImpl.h
//  WechatExporter
//
//  Created by Matthew on 2021/4/22.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "PdfConverter.h"
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
    
    void setWorkDir(NSString* workDir)
    {
        m_workDir = [NSString stringWithString:workDir];
    }
    
    bool makeUserDirectory(const std::string& dirName)
    {
        std::string command = replaceAll(dirName, " ", "\\ ");
        command = NEW_LINE + "[ -d \"pdf/" + command + "\" ] || mkdir \"pdf/" + command + "\"" + NEW_LINE;
        
        appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(command.c_str()), command.size());
        
        return true;
    }
    
    bool convert(const std::string& htmlPath, const std::string& pdfPath)
    {
        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:htmlPath.c_str()]];
     
        std::string command = "chrome ";
        command += "--headless --disable-gpu --disable-extensions --print-to-pdf-no-header --print-to-pdf=\"";
        command += pdfPath;
        command += "\" ";
        command += [[url absoluteString] UTF8String];
        command += NEW_LINE;
                
        appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(command.c_str()), command.size());
        
        return true;
        
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
    
    void initShellFile(const char *outputDir)
    {
        m_output = outputDir;
        m_shellPath = combinePath(m_output, "pdf.sh");
        deleteFile(m_shellPath);

        std::string aliasCmd = [m_assemblyPath UTF8String];
        replaceAll(aliasCmd, " ", "\\ ");
        aliasCmd = "#/bin/sh" + NEW_LINE + NEW_LINE + "alias chrome=\"" + aliasCmd + "\"" + NEW_LINE + NEW_LINE;
        
        std::string output = m_output;
        replaceAll(output, " ", "\\ ");
        
        aliasCmd += "cd \"" + output + "\"" + NEW_LINE;
        aliasCmd += "[ -d pdf ] || mkdir pdf" + NEW_LINE;
        
        appendFile(m_shellPath, reinterpret_cast<const unsigned char *>(aliasCmd.c_str()), aliasCmd.size());
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
