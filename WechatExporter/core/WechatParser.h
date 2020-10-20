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
#include <map>
// #include <sys/md5.h>
#include "WechatObjects.h"
#include "ITunesParser.h"
#include "Utils.h"
#include "ByteArrayLocater.h"
#include "Shell.h"
#include "Logger.h"

#include "DownloadPool.h"

using StringPair = std::pair<std::string, std::string>;

template<class T>
class FilterBase
{
protected:
    std::string m_path;
    std::string m_pattern;

public:
    bool operator() (const StringPair& s1, const T& s2) const    // less
    {
        return !startsWith(s1.first, m_path) && s1.first < m_path;
    }
    bool operator() (const T& s2, const StringPair& s1) const    // greater
    {
        return !startsWith(s1.first, m_path) && s1.first > m_path;
    }
    bool operator==(const StringPair& s) const
    {
        return startsWith(s.first, m_path) && (s.first.find(m_pattern, m_path.size()) != std::string::npos);
    }
    std::string parse(const StringPair& s) const
    {
        if (*this == s)
        {
            return s.first.substr(m_path.size());
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
    bool operator() (const StringPair& s1, const T& s2) const    // less
    {
        return !startsWith(s1.first, m_path) && s1.first < m_path;
    }
    bool operator() (const T& s2, const StringPair& s1) const    // greater
    {
        return !startsWith(s1.first, m_path) && s1.first > m_path;
    }
    bool operator==(const StringPair& s) const
    {
        std::smatch sm;
        return startsWith(s.first, m_path) && std::regex_search(s.first, sm, m_pattern);
    }
    std::string parse(const StringPair& s) const
    {
        std::smatch sm;
        if (std::regex_search(s.first, sm, m_pattern))
        {
            return sm[1];
        }
        return std::string("");
    }
};

class UserDirectoryFilter : public RegexFilterBase<UserDirectoryFilter>
{
public:
    UserDirectoryFilter() : RegexFilterBase()
    {
        m_path = "Documents\\/";
        m_pattern = "Documents\\/([0-9a-f]{32})\\/";
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
        m_pattern = "^\\/?(message_[0-9]{1,4}\\.sqlite)$";
    }
};

class SessionCellDataFilter : public FilterBase<SessionCellDataFilter>
{
    SessionCellDataFilter(const std::string& cellDataBasePath) : FilterBase()
    {
        std::string vpath = cellDataBasePath;
        std::replace(vpath.begin(), vpath.end(), '\\', '/');
        
        m_path = vpath;
        m_pattern = "celldataV7";
    }
};

class LoginInfo2Parser
{
private:
    ITunesDb *m_iTunesDb;
    
public:
    LoginInfo2Parser(ITunesDb *iTunesDb);
    
    bool parse(std::vector<Friend>& users);
    
    bool parse(const std::string& loginInfo2Path, std::vector<Friend>& users);
    
private:
    int parseUser(const char* data, int length, std::vector<Friend>& users);
};

class MMSettingInMMappedKVFilter : public FilterBase<MMSettingInMMappedKVFilter>
{
protected:
    std::string m_suffix;
    
public:
    MMSettingInMMappedKVFilter(const std::string& uid) : FilterBase()
    {
        m_path = "Documents/MMappedKV/";
        m_pattern = m_path + "mmsetting.archive." + uid;
        m_suffix = ".crc";
    }
    
    bool operator==(const std::string& s) const
    {
        return startsWith(s, m_pattern) && !endsWith(s, m_suffix);
    }
    std::string parse(const std::string& s) const
    {
        if (*this == s)
        {
            return s.substr(m_path.size());
        }
        return std::string("");
    }
};

class MMKVParser
{
private:
    std::vector<unsigned char> m_contents;
public:
    MMKVParser(const std::string& path);
    std::string findValue(const std::string& key);
};

class UserParser
{
private:
    std::string m_backupPath;
    ITunesDb *m_iTunesDb;
    Shell*      m_shell;
    
public:
    bool parse(Friend& myself);
};



class FriendsParser
{
public:
    bool parseWcdb(const std::string& mmPath, Friends& friends);
    
private:
    bool parseRemark(const void *data, int length, Friend& f);
    bool parseHeadImage(const void *data, int length, Friend& f);
    bool parseChatroom(const void *data, int length, Friend& f);
    // bool parseMembers(const std::string& xml, Friend& f);
};

class SessionsParser
{
private:
    ITunesDb *m_iTunesDb;

public:
    SessionsParser(ITunesDb *iTunesDb);
    
    bool parse(const std::string& userRoot, std::vector<Session>& sessions);

private:
    bool parseCellData(const std::string& userRoot, Session session);
    bool parseMessageDbs(const std::string& userRoot, std::vector<std::pair<std::string, std::string>>& sessions);
    bool parseMessageDb(const std::string& mmPath, std::vector<std::pair<std::string, std::string>>& sessions);
};

struct Record
{
    int createTime;
    std::string message;
    int des;
    int type;
    int msgid;
};


class SessionParser
{
private:
    const std::map<std::string, std::string> m_templates;
    Friends m_friends;
    // std::set<DownloadTask>& emojidown;
    const ITunesDb& m_iTunesDb;
    const Shell&  m_shell;
    Logger& m_logger;
    DownloadPool& m_downloadPool;
    Friend m_myself;
    
public:
    
    SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, Logger& logger, std::map<std::string, std::string>& templates, DownloadPool& dlPool);
    
    // bool parse(const std::string& chatId, std::string& message);
    int parse(const std::string& userBase, const std::string& path, const Session& session, Friend& f);
    
private:
    std::string getTemplate(std::string key) const;
    std::string getDisplayTime(int ms) const;
    bool requireResource(std::string vpath, std::string dest);
    bool parseRow(Record& record, const std::string& userBase, const std::string& path, const Session& session, std::string& templateKey, std::map<std::string, std::string>& templateValues);
    
};

class WechatParser
{
private:
    std::string backupPath;
    std::string saveBase;
    // Logger& logger;
    
    Friends m_friends;
    
public:
    
    WechatParser(const std::string& documentsPath, const std::string& uidMd5);
    
    bool parse();
    
};



#endif /* WechatParser_h */
