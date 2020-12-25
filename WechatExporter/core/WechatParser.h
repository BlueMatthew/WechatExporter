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


class MMSettingParser
{
private:
    ITunesDb *m_iTunesDb;
    
    std::string m_usrName;
    std::string m_displayName;
    std::string m_portrait;
    std::string m_portraitHD;
public:
    MMSettingParser(ITunesDb *iTunesDb);
    bool parse(const std::string& usrNameHash);
    
    std::string getUsrName() const;
    std::string getDisplayName() const;
    std::string getPortrait() const;
    std::string getPortraitHD() const;
    
};

class FriendsParser
{
public:
    FriendsParser(bool detailedInfo = true);
    bool parseWcdb(const std::string& mmPath, Friends& friends);
    
private:
    bool parseRemark(const void *data, int length, Friend& f);
    bool parseAvatar(const void *data, int length, Friend& f);
    bool parseChatroom(const void *data, int length, Friend& f);
    
private:
    bool m_detailedInfo;
};

class SessionsParser
{
private:
    ITunesDb *m_iTunesDb;
    Shell*      m_shell;
    std::string m_cellDataVersion;
    bool        m_detailedInfo;

public:
    SessionsParser(ITunesDb *iTunesDb, Shell* shell, const std::string& cellDataVersion, bool detailedInfo = true);
    
    bool parse(const std::string& userRoot, std::vector<Session>& sessions, const Friends& friends);

private:
    bool parseCellData(const std::string& userRoot, Session& session);
    bool parseMessageDbs(const std::string& userRoot, std::vector<Session>& sessions);
    bool parseMessageDb(const std::string& mmPath, std::vector<std::string>& sessionIds);
};

struct MsgRecord
{
    int createTime;
    std::string message;
    int des;
    int type;
    int msgId;
};


struct ForwardMsg
{
    int msgid;
    std::string usrName;
    std::string displayName;
    std::string protrait;
    std::string dataType;
    std::string subType;
    std::string dataId;
    std::string dataFormat;
    std::string msgTime;
    std::string srcMsgTime;
    std::string message;
    std::string link;
    std::string nestedMsgs;
};

enum SessionParsingOption
{
    SPO_IGNORE_AUDIO = 1,
    SPO_DESC = 2,
    SPO_ICON_IN_SESSION = 4     // Put Head Icon and Emoji files in the folder of session
};

class TemplateValues
{
private:
    std::string m_name;
    std::map<std::string, std::string> m_values;
    
public:
    using const_iterator = std::map<std::string, std::string>::const_iterator;
 
public:
    TemplateValues()
    {
    }
    TemplateValues(const std::string& name) : m_name(name)
    {
    }
    std::string getName() const
    {
        return m_name;
    }
    void setName(const std::string& name)
    {
        m_name = name;
    }
    std::string& operator[](const std::string& k)
    {
        return m_values[k];
    }
    bool hasValue(const std::string& key) const
    {
        return m_values.find(key) != m_values.cend();
    }
    
    const_iterator cbegin() const
    {
        return m_values.cbegin();
    }
    
    const_iterator cend() const
    {
        return m_values.cend();
    }
    
    void clear()
    {
        m_values.clear();
    }
    
    void clearName()
    {
        m_name.clear();
    }
};

class SessionParser
{
private:
    const std::map<std::string, std::string>& m_templates;
	const std::map<std::string, std::string>& m_localeStrings;

    int m_options;
    Friends& m_friends;
    const ITunesDb& m_iTunesDb;
    const Shell&  m_shell;
    Downloader& m_downloader;
    Friend m_myself;
    std::atomic<bool>& m_cancelled;
    
    mutable std::vector<unsigned char> m_pcmData;  // buffer
    
public:
    
    SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, const std::map<std::string, std::string>& templates, const std::map<std::string, std::string>& localeStrings, int options, Downloader& downloader, std::atomic<bool>& cancelled);
    void ignoreAudio(bool ignoreAudio = true)
    {
        if (ignoreAudio)
            m_options |= SPO_IGNORE_AUDIO;
        else
            m_options &= ~SPO_IGNORE_AUDIO;
    }
    void setOrder(bool asc = true)
    {
        if (asc)
            m_options &= ~SPO_DESC;
        else
            m_options |= SPO_DESC;
    }
    
    int parse(const std::string& userBase, const std::string& outputPath, const Session& session, std::string& contents);

private:
    std::string getTemplate(const std::string& key) const;
	std::string getLocaleString(const std::string& key) const;
    std::string getDisplayTime(int ms) const;
    bool requireFile(const std::string& vpath, const std::string& dest) const;
    bool parseRow(MsgRecord& record, const std::string& userBase, const std::string& path, const Session& session, std::vector<TemplateValues>& tvs);
    bool parseForwardedMsgs(const std::string& userBase, const std::string& outputPath, const Session& session, const MsgRecord& record, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs);
    std::string buildContentFromTemplateValues(const TemplateValues& values) const;
    void parseImage(const std::string& sessionPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues);
    void parseVideo(const std::string& sessionPath, const std::string& src, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues);
    void parseFile(const std::string& sessionPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& templateValues);
    
    void parseCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& templateValues);
    
};


#endif /* WechatParser_h */
