//
//  WechatParser.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#ifndef WechatParser_h
#define WechatParser_h

#include <stdio.h>
#include <regex>
#include <string>
#include <vector>
#include <atomic>
#include <map>
#include "Utils.h"
#include "Downloader.h"
#include "ByteArrayLocater.h"
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "MessageParser.h"
#if !defined(NDEBUG) || defined(DBG_PERF)
#include "Logger.h"
#endif

template<class T>
class FilterBase
{
protected:
    std::string m_path;
    std::string m_pattern;

public:
    bool operator() (const ITunesFile* s1, const T& s2) const    // less
    {
        return !startsWith(s1->relativePath, m_path) && s1->relativePath < m_path;
    }
    bool operator() (const T& s2, const ITunesFile* s1) const    // greater
    {
        return !startsWith(s1->relativePath, m_path) && s1->relativePath > m_path;
    }
    bool operator==(const ITunesFile* s) const
    {
        return startsWith(s->relativePath, m_path) && (s->relativePath.find(m_pattern, m_path.size()) != std::string::npos);
    }
    std::string parse(const ITunesFile* s) const
    {
        if (*this == s)
        {
            return s->relativePath.substr(m_path.size());
        }
        return std::string("");
    }
};

template<class T>
class RegexFilterBase
{
protected:
    std::string m_path;
    std::regex m_pattern;

public:
    bool operator() (const ITunesFile* s1, const T& s2) const    // less
    {
        return !startsWith(s1->relativePath, m_path) && s1->relativePath < m_path;
    }
    bool operator() (const T& s2, const ITunesFile* s1) const    // greater
    {
        return !startsWith(s1->relativePath, m_path) && s1->relativePath > m_path;
    }
    bool operator==(const ITunesFile* s) const
    {
        std::smatch sm;
        return startsWith(s->relativePath, m_path) && std::regex_search(s->relativePath.begin() + m_path.size(), s->relativePath.end(), sm, m_pattern);
    }
    std::string parse(const ITunesFile* s) const
    {
        std::smatch sm;
        if (std::regex_search(s->relativePath.begin() + m_path.size(), s->relativePath.end(), sm, m_pattern))
        {
            return sm[1];
        }
        return std::string("");
    }
};

class MessageDbFilter : public RegexFilterBase<MessageDbFilter>
{
public:
    MessageDbFilter(const std::string& basePath) : RegexFilterBase()
    {
        std::string vpath = basePath;
        std::replace(vpath.begin(), vpath.end(), '\\', '/');
        if (!endsWith(vpath, "/"))
        {
            vpath += "/";
        }
        vpath += "DB/";
        
        m_path = vpath;
        m_pattern = "^(message_[0-9]{1,4}\\.sqlite)$";
    }
};

class UserFolderFilter : public RegexFilterBase<UserFolderFilter>
{
protected:
    std::string m_suffix;
    size_t m_pathLen;
    size_t m_suffixLen;
public:
    UserFolderFilter() : RegexFilterBase()
    {
        m_path = "Documents/";
        // m_pattern = "^([a-zA-Z0-9]{32})$";
        m_suffix = "/DB/MM.sqlite";
        m_pathLen = m_path.length();
        m_suffixLen = m_suffix.length();
    }
    
    bool operator==(const ITunesFile* s) const
    {
        if (/*(s->relativePath.size() != (m_path.size() + 32 + 13)) || */!startsWith(s->relativePath, m_path)|| !endsWith(s->relativePath, m_suffix))
        {
            return false;
        }
        // if (s->relativePath.find('/', m_path.size(), 32) != std::string::npos)
        {
            // return false;
        }
        
        return true;
    }
    
    std::string parse(const ITunesFile* s) const
    {
        return s->relativePath.substr(m_pathLen, s->relativePath.length() - m_pathLen - m_suffixLen);
        // return s->relativePath.substr(m_path.size()) : "";
        // return s->relativePath.size() > 32 ? s->relativePath.substr(m_path.size()) : "";
    }
};

class WechatInfoParser
{
private:
    ITunesDb *m_iTunesDb;
    
public:
    
    WechatInfoParser(ITunesDb *iTunesDb);
    
    bool parse(WechatInfo& wechatInfo);
    
protected:
    bool parsePreferences(WechatInfo& wechatInfo);
};

class SessionCellDataFilter : public FilterBase<SessionCellDataFilter>
{
public:
    SessionCellDataFilter(const std::string& cellDataBasePath, const std::string& cellDataVersion) : FilterBase()
    {
        std::string vpath = cellDataBasePath;
        std::replace(vpath.begin(), vpath.end(), '\\', '/');
        
        m_path = vpath;
        m_pattern = "celldata" + cellDataVersion;    // celldataV7
    }
};

class LoginInfo2Parser
{
private:
    ITunesDb *m_iTunesDb;
#if !defined(NDEBUG) || defined(DBG_PERF)
    std::string m_error;
    Logger*     m_logger;
#endif
    
public:
    LoginInfo2Parser(ITunesDb *iTunesDb
#if !defined(NDEBUG) || defined(DBG_PERF)
                     , Logger* logger
#endif
    );
    
    bool parse(std::vector<Friend>& users);
    bool parse(const std::string& loginInfo2Path, std::vector<Friend>& users);
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    std::string getError() const;
#endif
    
private:
    int parseUser(const char* data, int length, std::vector<Friend>& users);
    bool parseUserFromFolder(std::vector<Friend>& users);
    bool parseMMSettingsFromMMKV(std::map<std::string, std::pair<std::string, std::string>>& mmsettingFiles);
};

class MMSettingInMMappedKVFilter : public FilterBase<MMSettingInMMappedKVFilter>
{
protected:
    std::string m_suffix;
    
public:
    MMSettingInMMappedKVFilter(const std::string& uid) : FilterBase()
    {
        m_pattern = "Documents/MMappedKV/";
        m_path = m_pattern + "mmsetting.archive." + uid;
        m_suffix = ".crc";
    }
    
    MMSettingInMMappedKVFilter() : FilterBase()
    {
        m_pattern = "Documents/MMappedKV/";
        m_path = m_pattern + "mmsetting.archive.";
        m_suffix = ".crc";
    }
    
    std::string getPrefix() const
    {
        return "mmsetting.archive.";
    }
    
    bool operator==(const ITunesFile* s) const
    {
        return startsWith(s->relativePath, m_path) && !endsWith(s->relativePath, m_suffix);
    }
    std::string parse(const ITunesFile* s) const
    {
        if (*this == s)
        {
            return s->relativePath.substr(m_pattern.size());
        }
        return std::string("");
    }
};

class MMSettings
{
protected:
    std::string m_usrName;
    std::string m_name;
    std::string m_displayName;
    std::string m_portrait;
    std::string m_portraitHD;
public:
    
    std::string getUsrName() const;
    std::string getDisplayName() const;
    std::string getPortrait() const;
    std::string getPortraitHD() const;
    
protected:
    void clear();
};

class MMKVParser : public MMSettings
{
private:
#if !defined(NDEBUG) || defined(DBG_PERF)
    Logger*     m_logger;
#endif
    
public:
    MMKVParser(
#if !defined(NDEBUG) || defined(DBG_PERF)
               Logger* logger
#endif
    );
    bool parse(const std::string& path, const std::string& crcPath);
};

class MMSettingParser : public MMSettings
{
private:
    ITunesDb *m_iTunesDb;

public:
    MMSettingParser(ITunesDb *iTunesDb);
    bool parse(const std::string& usrNameHash);
};

class FriendsParser
{
public:
    FriendsParser(bool detailedInfo = true);
    bool parseWcdb(const std::string& mmPath, Friends& friends);
#ifndef NDEBUG
    void setOutputPath(const std::string& outputPath);
#endif
    
private:
    bool parseRemark(const void *data, int length, Friend& f);
    bool parseAvatar(const void *data, int length, Friend& f);
    bool parseChatroom(const void *data, int length, Friend& f);
    
private:
    bool m_detailedInfo;
#ifndef NDEBUG
    std::string m_outputPath;
#endif
};

class SessionsParser
{
private:
    ITunesDb *m_iTunesDb;
    ITunesDb *m_iTunesDbShare;
    std::string m_cellDataVersion;
    bool        m_detailedInfo;

public:
    SessionsParser(ITunesDb *iTunesDb, ITunesDb *iTunesDbShare, const std::string& cellDataVersion, bool detailedInfo = true);
    
    bool parse(const Friend& user, const Friends& friends, std::vector<Session>& sessions);

private:
    bool parseCellData(const std::string& userRoot, Session& session);
    bool parseMessageDbs(const Friend& user, const std::string& userRoot, std::vector<Session>& sessions);
    bool parseMessageDb(const Friend& user, const std::string& mmPath, std::vector<Session>& sessions, std::vector<Session>& deletedSessions);
    
    bool parseSessionsInGroupApp(const std::string& userRoot, std::vector<Session>& sessions);
};

class SessionParser
{
public:
    class MessageEnumerator
    {
    protected:
        MessageEnumerator(const Session& session, int options, int64_t minId);
        
        friend SessionParser;
    public:
        bool isInvalid() const;
        bool nextMessage(WXMSG& msg);
        
        ~MessageEnumerator();
    private:
        
        void* m_context;
    };
    
private:
    
    int m_options;
    
public:
    SessionParser(int options);

    MessageEnumerator* buildMsgEnumerator(const Session& session, uint64_t minId);
};


#endif /* WechatParser_h */
