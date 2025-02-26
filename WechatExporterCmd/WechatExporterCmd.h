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
#include "Utils.h"
#include "Exporter.h"
#include "IDeviceBackup.h"
#include "WechatSource.h"
#else
#endif

#ifndef WechatExporterCmd_h
#define WechatExporterCmd_h

#define OUTPUT_FORMAT_HTML      0
#define OUTPUT_FORMAT_TEXT      1

#define HTML_OPTION_SYNC        0
#define HTML_OPTION_ONSCROLL    1
#define HTML_OPTION_ONINIT      2

class Logger;

void printHelp(const char *executableName)
{
    std::cout
          <<
          "Usage: " << executableName
          << " [OPTION]\n"
             "Export Wechat chat history based on the options given:\n"
             "  --backup=PATH       Specify the directory of iTunes Backup\n"
             "  --output=PATH       Specify the directory in that Wechat chat history will be exported.\n"
             "  --format=FORMAT     FORMAT may be one of 'html', 'text'. 'html' is default.\n"
             "  --account=ACCOUNT   Specify the WeChat account which will be exported.\n"
             "  --session=SESSION   Friend name or chat group name which will be exported. May be specified multiple times\n"
             "                      If no session is specified, all sessions of the account will be exported.\n"
             "  --asyncloading=[HTML LOADING OPTION]\n"
             "                      [HTML LOADING OPTION] may be one of 'sync', 'onscroll', 'oninit'. 'onscroll' is default.\n"
             "  --filter=FILTER     FILTER may be one of 'no', 'yes'. 'no' is default.\n"
             "  --help              Show this help.\n"
          << std::endl;
}

std::string parseArgumentwithQuato(const char *path)
{
    std::string parsedPath;
    if (NULL == path)
    {
        return parsedPath;
    }
    
    size_t start = 0;
    size_t length = strlen(path);
    if (length > 1 && path[length - 1] == '"')
    {
        length--;
    }
    if (path[0] == '"')
    {
        start = 1;
        length--;
    }
    
    parsedPath = std::string(path, start, length);
    return parsedPath;
}

int exportSessions(const std::string& languageCode, Logger* logger, const std::string& workDir, const std::string& backupDir, const std::string& outputDir, const std::string& account, const std::vector<std::string>& sessions, int outputFormat, int asyncLoading, bool outputFilter)
{
    // const std::string& workDir, const std::string& backup, const std::string& output, Logger* logger, PdfConverter* pdfConverter
    
    Exporter exp(workDir, backupDir, outputDir, logger, NULL);
    exp.setLanguageCode(languageCode);
    ExportOption options;
    
    if (outputFormat == OUTPUT_FORMAT_TEXT)
    {
        options.setTextMode();
        exp.setExtName("txt");
        exp.setTemplatesName("templates_txt");
    }
    else
    {
        exp.setExtName("html");
        exp.setTemplatesName("templates");
        if (asyncLoading == HTML_OPTION_SYNC)
        {
            options.setTextMode();
            // exp.setSyncLoading();
        }
        else
        {
            if (asyncLoading != HTML_OPTION_ONINIT)
            {
                options.setLoadingDataOnScroll();
            }
        }
    }
    if (outputFilter)
    {
        options.supportsFilter();
    }
    options.filterByName();
    
    exp.setOptions(options);
    
    std::map<std::string, std::map<std::string, void *>> usersAndSessions;
    
    std::map<std::string, void *> mapSessions;
    
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        mapSessions[*it] = NULL;
    }
    
    usersAndSessions[account] = mapSessions;
    
    exp.filterUsersAndSessions(usersAndSessions);
    
    exp.run();
    
    exp.waitForComplition();
    
    return 0;
}

#endif // WechatExporterCmd_h
