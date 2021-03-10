//
//  Updater.h
//  WechatExporter
//
//  Created by Matthew on 2021/3/6.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef Updater_h
#define Updater_h

#include <string>

class Updater
{
public:
    Updater(const std::string& currentVersion);
    ~Updater();
    
    void setUserAgent(const std::string& userAgent);
    
    bool checkUpdate();
    std::string getNewVersion() const;
    std::string getUpdateUrl() const;
    
private:
    std::string m_currentVersion;
    std::string m_latestVersion;
    std::string m_updateUrl;
    std::string m_userAgent;
};

#endif /* Updater_h */
