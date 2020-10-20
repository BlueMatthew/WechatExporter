//
//  Exporter.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <stdio.h>
#include <string>

#include "Logger.h"
#include "Shell.h"
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "DownloadPool.h"

#ifndef Exporter_h
#define Exporter_h



class Exporter
{
protected:
    std::string m_workDir;
    
    std::string m_backup;
    std::string m_output;
    Logger* m_logger;
    Shell* m_shell;
    
    ITunesDb *m_iTunesDb;
    
    std::map<std::string, std::string> m_templates;
    
    DownloadPool m_downloadPool;
    
public:
    Exporter(const std::string& workDir, const std::string& backup, const std::string& output, Shell* shell, Logger* logger);
    ~Exporter();
    
    bool run();
    
    bool exportUser(Friend& user);
    bool exportSession(Friend& user, Friends& friends, const Session& sessionId);
    
protected:
    bool fillUser(Friend& user);
    bool loadTemplates();
};

#endif /* Exporter_h */
