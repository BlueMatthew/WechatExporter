//
//  ShellImpl.h
//  WechatExporter
//
//  Created by Matthew on 2020/10/1.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "core/Shell.h"
#include <dirent.h>

#ifndef ShellImpl_h
#define ShellImpl_h


class ShellImpl : public Shell
{
public:
    
    ShellImpl()
    {
        
    }
    
    bool existsDirectory(const std::string& path) const
    {
        BOOL isDir = NO;
        NSString *ocPath = [NSString stringWithUTF8String: path.c_str()];
        
        BOOL existed = [[NSFileManager defaultManager] fileExistsAtPath:ocPath isDirectory:&isDir];
        return existed == YES && isDir == YES;
    }
    
    bool listSubDirectories(const std::string& path, std::vector<std::string>& subDirectories) const
	{
		struct dirent *entry;
	    DIR *dir = opendir(path.c_str());
	    if (dir == NULL)
	    {
	        return false;
	    }
	
	    while ((entry = readdir(dir)) != NULL)
	    {
	        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
	        {
	            continue;
	        }
	        std::string subDir = entry->d_name;
            // TODO: Check directory or not
            subDirectories.push_back(subDir);
	    }
	    closedir(dir);
    
		return true;
	}
    
    bool makeDirectory(const std::string& path) const
    {
        NSString *ocPath = [NSString stringWithUTF8String: path.c_str()];
        return [[NSFileManager defaultManager] createDirectoryAtPath:ocPath withIntermediateDirectories:YES attributes:nil error:nil] == YES;
    }
    
    bool copyFile(const std::string& src, const std::string& dest, bool overwrite) const
    {
        NSString *srcPath = [NSString stringWithUTF8String: src.c_str()];
        NSString *destPath = [NSString stringWithUTF8String: dest.c_str()];
        
        NSFileManager* fileManager = [NSFileManager defaultManager];
        
        if ([fileManager fileExistsAtPath:destPath])
        {
#ifndef NDEBUG
            return true;
#else
            [fileManager removeItemAtPath:destPath error:nil];
#endif
        }
        return [[NSFileManager defaultManager] copyItemAtPath:srcPath toPath:destPath error:nil] == YES;
    }
    
    bool openOutputFile(std::ofstream& ofs, const std::string& fileName, std::ios_base::openmode mode/* = std::ios::out*/) const
    {
        if (ofs.is_open())
        {
            return false;
        }

        ofs.open(fileName, mode);

        return ofs.is_open();
    }
    
    /*
    bool convertPlist(const std::vector<unsigned char>& bplist, std::string& xml) const
    {
        std::string bplistPath = std::tmpnam(NULL);
        if (!writeFile(bplistPath, bplist))
        {
            return false;
        }
        std::string xmlPath = std::tmpnam(NULL);
        
        std::string cmd = "plutil -convert xml1 -o " + xmlPath + " " + bplistPath;
        bool result = exec(cmd);
        if (result)
        {
            xml = readFile(xmlPath);
            std::remove(xmlPath.c_str());
        }
        
        std::remove(bplistPath.c_str());
        return result;
    }
    */
    
    int exec(const std::string& cmd) const
    {
        NSArray *arguments = [NSArray arrayWithObjects:
                                  @"-c" ,
                              [NSString stringWithUTF8String:cmd.c_str()],
                                  nil];
        
        int pid = [[NSProcessInfo processInfo] processIdentifier];
        NSPipe *pipe = [NSPipe pipe];
        NSFileHandle *file = pipe.fileHandleForReading;

        NSTask *task = [[NSTask alloc] init];
        task.launchPath = @"/bin/sh";
        [task setArguments:arguments];
        task.standardOutput = pipe;

        [task launch];
        
        [task waitUntilExit];

        NSData *data = [file readDataToEndOfFile];
        [file closeFile];

        NSString *grepOutput = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
        NSLog (@"grep returned:\n%@", grepOutput);
        
        return task.terminationStatus == 0;
    }

};

#endif /* ShellImpl_h */
