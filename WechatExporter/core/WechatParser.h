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
#include <map>
#include "Utils.h"
#include "Shell.h"
#include "Downloader.h"
#include "ByteArrayLocater.h"
#include "WechatObjects.h"
#include "ITunesParser.h"

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

class SessionCellDataFilter : public FilterBase<SessionCellDataFilter>
{
public:
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

class FriendsParser
{
public:
    bool parseWcdb(const std::string& mmPath, Friends& friends);
    
private:
    bool parseRemark(const void *data, int length, Friend& f);
    bool parseHeadImage(const void *data, int length, Friend& f);
    bool parseChatroom(const void *data, int length, Friend& f);
};

class SessionsParser
{
private:
    ITunesDb *m_iTunesDb;
    Shell*      m_shell;

public:
    SessionsParser(ITunesDb *iTunesDb, Shell* shell);
    
    bool parse(const std::string& userRoot, std::vector<Session>& sessions, const Friends& friends);

private:
    bool parseCellData(const std::string& userRoot, Session& session);
    bool parseMessageDbs(const std::string& userRoot, std::vector<Session>& sessions);
    bool parseMessageDb(const std::string& mmPath, std::vector<std::string>& sessionIds);
    unsigned int parseModifiedTime(std::vector<unsigned char>& data);
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
    const std::map<std::string, std::string>& m_templates;
	const std::map<std::string, std::string>& m_localeStrings;

    Friends& m_friends;
    const ITunesDb& m_iTunesDb;
    const Shell&  m_shell;
    Downloader& m_downloader;
    Friend m_myself;
    
    
    mutable std::vector<unsigned char> m_pcmData;  // buffer
    
public:
    
    SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, const std::map<std::string, std::string>& templates, const std::map<std::string, std::string>& localeStrings, Downloader& dlPool);
    int parse(const std::string& userBase, const std::string& outputBase, const Session& session, std::string& contents) const;
    
private:
    std::string getTemplate(const std::string& key) const;
	std::string getLocaleString(const std::string& key) const;
    std::string getDisplayTime(int ms) const;
    bool requireResource(const std::string& vpath, const std::string& dest) const;
    bool parseRow(Record& record, const std::string& userBase, const std::string& path, const Session& session, std::string& templateKey, std::map<std::string, std::string>& templateValues) const;
};


#endif /* WechatParser_h */
