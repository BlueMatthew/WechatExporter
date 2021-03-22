//
//  Updater.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/3/6.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "Updater.h"
#include "Downloader.h"
#include "Utils.h"

Updater::Updater(const std::string& currentVersion) : m_currentVersion(currentVersion)
{
}

Updater::~Updater()
{
}

void Updater::setUserAgent(const std::string& userAgent)
{
    m_userAgent = userAgent;
}

std::string Updater::getNewVersion() const
{
    return m_latestVersion;
}

std::string Updater::getUpdateUrl() const
{
    return m_updateUrl;
}

bool Updater::checkUpdate()
{
    m_latestVersion.clear();
    m_updateUrl.clear();
    
    std::vector<unsigned char> body;
    std::vector<std::pair<std::string, std::string>> headers;
    if (!m_userAgent.empty())
    {
        headers.push_back(std::pair<std::string, std::string>("User-Agent", m_userAgent));
    }
    
    std::string url = "https://src.wakin.org/github/wxexp/update.conf?v=" + encodeUrl(m_currentVersion);
#ifndef NDEBUG
    headers.push_back(std::pair<std::string, std::string>("RESOLVE", "src.wakin.org:443:127.0.0.1"));
    url += "&dbg=1";
#endif
    long httpStatus = 0;
    if (!Downloader::httpGet(url, headers, httpStatus, body) || httpStatus != 200 || body.empty())
    {
        return false;
    }
    
    std::string bodyStr;
    bodyStr.assign(reinterpret_cast<char *>(&body[0]), body.size());
    replaceAll(bodyStr, "\r\n", "\n");
    replaceAll(bodyStr, "\r", "\n");

    std::vector<std::string> parts = split(bodyStr, "\n");
    if (parts.empty() || parts[0].empty())
    {
        return false;
    }
    
    std::vector<std::string> versionParts = split(parts[0], ".");
    if (versionParts.size() != 4 || !isNumber(versionParts[0]) || !isNumber(versionParts[1]) || !isNumber(versionParts[2]) || !isNumber(versionParts[3]))
    {
        return false;
    }
    m_latestVersion = parts[0];
    
    std::vector<std::string> curVersionParts = split(m_currentVersion, ".");
    if (curVersionParts.size() != 4 || !isNumber(curVersionParts[0]) || !isNumber(curVersionParts[1]) || !isNumber(curVersionParts[2]) || !isNumber(curVersionParts[3]))
    {
        return false;
    }
    
    if (parts.size() > 1 && !parts[1].empty())
    {
        m_updateUrl = parts[1];
    }

    for (int idx = 0; idx < 4; ++idx)
    {
        int v = std::atoi(versionParts[idx].c_str());
        int cv = std::atoi(curVersionParts[idx].c_str());
        if (v > cv)
        {
            return true;
        }
    }
    
    return false;
}
