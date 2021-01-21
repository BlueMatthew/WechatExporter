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
    
    bool existsFile(const std::string& path) const
    {
        BOOL isDir = NO;
        NSString *ocPath = [NSString stringWithUTF8String: path.c_str()];
        
        BOOL existed = [[NSFileManager defaultManager] fileExistsAtPath:ocPath isDirectory:&isDir];
        return existed == YES;
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
    
    bool deleteFile(const std::string& path) const
    {
        NSString *ocPath = [NSString stringWithUTF8String: path.c_str()];
        return [[NSFileManager defaultManager] removeItemAtPath:ocPath error:nil] == YES;
    }
    
    bool deleteDirectory(const std::string& path) const
    {
        NSString *ocPath = [NSString stringWithUTF8String: path.c_str()];
        return deleteDirectory(ocPath);
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
            if (!overwrite)
            {
                return false;
            }
            [fileManager removeItemAtPath:destPath error:nil];
#endif
        }
        return [[NSFileManager defaultManager] copyItemAtPath:srcPath toPath:destPath error:nil] == YES;
    }
    
    bool moveFile(const std::string& src, const std::string& dest) const
    {
        return rename(src.c_str(), dest.c_str()) == 0;
    }

    std::string removeInvalidCharsForFileName(const std::string& fileName) const
    {
        /*
        Windows NT:
        Do not use any of the following characters when naming your files or folders:
            / ? < > \ : * | ” and any character you can type with the Ctrl key
        File and folder names may be up to 256 characters long
        The maximum length of a full path is 260 characters
        Placing a space at the end of the name
        Placing a period at the end of the name

         MAC:
         The only illegal character for file and folder names in Mac OS X is the colon “:”
         File and folder names are not permitted to begin with a dot “.”
         File and folder names may be up to 255 characters in length
         */
        
        std::string validFileName = fileName;
        
        static std::string invalidChars = "[\\/:*?\"<>|]";

        for (unsigned int i = 0; i < invalidChars.size(); ++i)
        {
            validFileName.erase(remove(validFileName.begin(), validFileName.end(), invalidChars[i]), validFileName.end());
        }

        size_t pos = validFileName.find_first_not_of(".");
        if (pos == std::string::npos)
        {
            return "";
        }
        else if (pos != 0)
        {
            validFileName.erase(0, pos);
        }

        pos = validFileName.find_last_not_of(" .");
        if (pos == std::string::npos)
        {
            return "";
        }
        else
        {
            validFileName.erase(pos + 1);
        }

        if (validFileName.size() > 255)
        {
            validFileName = validFileName.substr(0, 255);
        }
        return validFileName;
    }
    
protected:
    bool deleteDirectory(NSString *path) const
    {
        NSFileManager* fileManager = [NSFileManager defaultManager];
        NSArray *files = [fileManager contentsOfDirectoryAtPath:path error:nil];
        BOOL isDir = NO;
        for (NSString *file in files)
        {
            NSString *fullPath = [path stringByAppendingPathComponent:file];
            BOOL existed = [[NSFileManager defaultManager] fileExistsAtPath:fullPath isDirectory:&isDir];
            if (isDir)
            {
                deleteDirectory(fullPath);
            }
            else
            {
                [fileManager removeItemAtPath:fullPath error:nil];
            }
        }
        
        [fileManager removeItemAtPath:path error:nil];
        return true;
    }

};

#endif /* ShellImpl_h */
