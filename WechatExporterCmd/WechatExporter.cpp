//
//  main.cpp
//  WechatExporterCmd
//
//  Created by Matthew on 2022/3/4.
//

#include <iostream>
#include <string>
#include <vector>
#include <thread>


#ifdef _WIN32
#elif defined(__APPLE__)
#include <libgen.h>
#include <mach-o/dyld.h>
#include "Utils.h"
#else
#endif

#include "WechatExporterCmd.h"


std::string getCurrentLanguageCode();
std::string getExecutablePath();
std::string getExecutableDir();


class LoggerImpl : public Logger
{
public:
    
    void write(const std::string& log)
    {
        std::cout << log << std::endl;
    }
    
    void debug(const std::string& log)
    {
        std::cout << "DBG:: " << log << std::endl;
    }
    
};

int main(int argc, const char * argv[]) {
    
    const char * fullPath = argv[0];
    
    const char *executableName = basename((char *)argv[0]);
    int outputFormat = OUTPUT_FORMAT_HTML;
    int asyncLoading = HTML_OPTION_ONSCROLL;
    bool outputFilter = false;
    std::string backupDir;
    std::string outputDir;
    std::string account;
    std::vector<std::string> sessions;
    
    for (int idx = 1; idx < argc; idx++)
    {
        if (strcmp("--help", argv[idx]) == 0)
        {
            printHelp(executableName);
            return 0;
        }
        
        const char* equals_pos = strchr(argv[idx], '=');
        if (equals_pos == NULL)
        {
            continue;
        }
        
        std::string name = std::string(argv[idx], equals_pos - argv[idx]);
        if (name == "--backup")
        {
            backupDir = parseArgumentwithQuato(equals_pos + 1);
        }
        else if (name == "--output")
        {
            outputDir = parseArgumentwithQuato(equals_pos + 1);
        }
        else if (name == "--account")
        {
            account = parseArgumentwithQuato(equals_pos + 1);
        }
        else if (name == "--session")
        {
            sessions.push_back(parseArgumentwithQuato(equals_pos + 1));
        }
        else if (name == "--asyncloading")
        {
            if (strcmp("sync", equals_pos + 1) == 0)
            {
                asyncLoading = HTML_OPTION_SYNC;
            }
            else if (strcmp("oninit", equals_pos + 1) == 0)
            {
                asyncLoading = HTML_OPTION_ONINIT;
            }
        }
        else if (name == "--filter")
        {
            if (strcmp("yes", equals_pos + 1) == 0)
            {
                outputFilter = true;
            }
        }
    }
    
    if (backupDir.empty() || !existsDirectory(backupDir))
    {
        std::cout << "Please input valid iTunes backup directory." << std::endl;
        return 1;
    }
    if (outputDir.empty() || !existsDirectory(outputDir))
    {
        std::cout << "Please input valid output directory." << std::endl;
        return 1;
    }
    if (account.empty())
    {
        std::cout << "Please input account name." << std::endl;
        return 1;
    }
    
    std::string workDir(argv[0], 0, strlen(argv[0]) - strlen(executableName));
    
    workDir = getExecutableDir();
    if (!endsWith(workDir, "/"))
    {
        workDir += "/";
    }
    
    std::string languageCode = getCurrentLanguageCode();
    LoggerImpl logger;
    
    return exportSessions(languageCode, &logger, workDir, backupDir, outputDir, account, sessions, outputFormat, asyncLoading, outputFilter);
}

std::string getExecutablePath()
{
    char rawPathName[PATH_MAX];
    char realPathName[PATH_MAX];
    uint32_t rawPathSize = (uint32_t)sizeof(rawPathName);

    if(!_NSGetExecutablePath(rawPathName, &rawPathSize)) {
        realpath(rawPathName, realPathName);
    }
    return  std::string(realPathName);
}

std::string getExecutableDir()
{
    std::string executablePath = getExecutablePath();
    char *executablePathStr = new char[executablePath.length() + 1];
    strcpy(executablePathStr, executablePath.c_str());
    char* executableDir = dirname(executablePathStr);
    delete [] executablePathStr;
    return std::string(executableDir);
}

std::string getCurrentLanguageCode()
{
    std::locale loc = std::locale("");
    
    std::string name = loc.name();
    return loc.name();
    
    // NSString *preferredLanguage = [[NSLocale preferredLanguages] objectAtIndex:0];
    // if ([preferredLanguage hasPrefix:@"zh-Hans"])
    {
        // preferredLanguage = @"zh-Hans";
    }
    
    // return preferredLanguage;
}

