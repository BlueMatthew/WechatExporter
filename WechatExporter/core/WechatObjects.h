//
//  WechatObjects.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <vector>
#include <regex>
#include <map>
#include <algorithm>
#include <cmath>
#ifndef NDEBUG
#include <cassert>
#endif
#include "Utils.h"

#ifndef WechatObjects_h
#define WechatObjects_h

// https://www.theiphonewiki.com/wiki/Kernel#iOS
// https://en.wikipedia.org/wiki/Darwin_(operating_system)
// Major * 10000 + Minor * 100 + Patch
/*
6.0     13.0.0
7.0     14.0.0
9.0     15.0.0
9.3     15.4.0
9.3.2   15.5.0
9.3.3   15.6.0
10.0    16.0.0
10.1    16.1.0
10.2    16.3.0
10.3    16.5.0
10.3.2  16.6.0
10.3.3  16.7.0
11.0    17.0.0
11.1    17.2.0
11.2    17.3.0
11.2.5  17.4.0
11.3    17.5.0
11.4    17.6.0
11.4.1  17.7.0
12.0    18.0.0
12.1    18.2.0
12.2    18.5.0
12.3    18.6.0
12.4    18.7.0
13.0    19.0.0
13.3    19.2.0
13.3.1  19.3.0
13.4    19.4.0
13.4.5  19.5.0
13.5.5  19.6.0
14.0    20.0.0
14.2    20.1.0
14.3    20.2.0
14.4    20.3.0
14.5    20.4.0
*/

// CFNetwork - Darwin
// https://user-agents.net/applications/cfnetwork

class WechatInfo
{
private:
    std::string m_version;
    std::string m_osVersion;
    std::string m_shortVersion;
    std::string m_cellDataVersion;
    
    struct __less
    {
        bool operator()(const std::pair<int, std::string>& x, int y) const {return x.first < y;}
    };
    
public:
    void setVersion(const std::string& version)
    {
        m_version = version;
        m_shortVersion = version;
        
        std::vector<std::string> parts = split(version, ".");
        if (parts.size() > 3)
        {
            parts.erase(parts.begin() + 3, parts.end());
            m_shortVersion = join(parts, ".");
        }
    }
    
    void setOSVersion(const std::string& osVersion)
    {
        size_t pos = osVersion.find_first_not_of("0123456789.");
        if (pos == std::string::npos)
        {
            m_osVersion = osVersion;
        }
        else
        {
            m_osVersion = osVersion.substr(0, pos);
        }
    }
    
    std::string getVersion() const
    {
        return m_version;
    }
    
    std::string getShortVersion() const
    {
        return m_shortVersion;
    }
    void setCellDataVersion(const std::string& cellDataVersion)
    {
        m_cellDataVersion = cellDataVersion;
    }
    
    std::string getCellDataVersion() const
    {
        return m_cellDataVersion;
    }
    
    std::string buildUserAgent() const
    {
        std::vector<std::pair<int, std::string>> versionMapping = {
            {0, "11.0.0 485.12.7"},
            {60000, "13.0.0 609.1.4"},
            {70000, "14.0.0 711.3.18"},
            {90000, "15.0.0 758.2.8"},
            {90300, "15.4.0 758.3.15"},
            {90302, "15.5.0 758.4.3"},
            {90303, "15.6.0 758.5.3"},
            {100000, "16.0.0 808.0.2"},
            {100100, "16.1.0 808.1.4"},
            {100200, "16.3.0 808.3"},
            {100300, "16.5.0 811.4.18"},
            {100302, "16.6.0 811.5.4"},
            {100303, "16.7.0 811.5.4"},
            {110000, "17.0.0 887"},
            {110100, "17.2.0 889.9"},
            {110200, "17.3.0 893.14.2"},
            {110205, "17.4.0 894"},
            {110300, "17.5.0 897.15"},
            {110400, "17.6.0 901.1"},
            {110401, "17.7.0 902.2"},
            {120000, "18.0.0 974.2.1"},
            {120100, "18.2.0 975.0.3"},
            {120200, "18.5.0 978.0.7"},
            {120300, "18.6.0 978.0.7"},
            {120400, "18.7.0 978.0.7"},
            {130000, "19.0.0 1120"},
            {130300, "19.2.0 1121.2.2"},
            {130301, "19.3.0 1121.2.2"},
            {130400, "19.4.0 978.0.7"},
            {130405, "19.5.0 1126"},
            {130505, "19.6.0 1128.0.1"},
            {140000, "20.0.0 1197"},
            {140200, "20.1.0 1206"},
            {140300, "20.2.0 1209"},
            {140400, "20.3.0 1220.1"},
            {140500, "20.4.0 1237"},
            {140600, "20.5.0 1240.0.4"},
            {140700, "20.6.0 1240.0.4"},
            {150000, "21.0.0 1300.1"},
        };
        
        int osVersion = getOSVersionNumber();
        std::vector<std::pair<int, std::string>>::iterator it = std::lower_bound(versionMapping.begin(), versionMapping.end(), osVersion, __less());
        if (it == versionMapping.cend() || it->first != osVersion)
        {
            --it;
        }
        size_t pos = it->second.find(' ');
        if (pos != std::string::npos)
        {
            std::string darwinVersion = it->second.substr(0, pos);
            std::string cfVersion = it->second.substr(pos + 1);
            return "WeChat/" + (m_version.empty() ? "7.0.15.33" : m_version) +
                " CFNetwork/" + (cfVersion.empty() ? "978.0.7" : cfVersion) +
                " Darwin/" + (darwinVersion.empty() ? "18.6.0" : darwinVersion);
        }
        
        return "WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0"; // default
    }
    
protected:
    int getOSVersionNumber() const
    {
        int versionNumber = 0;
        
        if (!m_osVersion.empty())
        {
            std::vector<std::string> parts = split(m_osVersion, ".");
            for (int idx = 0; idx < std::min(3, static_cast<int>(parts.size())); ++idx)
            {
                if (!parts[idx].empty())
                {
                    versionNumber += std::pow(100, (2 - idx)) * std::stoi(parts[idx]);
                }
            }
        }
        return versionNumber;
    }
};

class Friend
{
protected:
    std::string m_usrName;
    std::string m_uidHash;
    std::string m_displayName;
    int m_userType;
    bool m_isChatroom;
    std::string m_portrait;
    std::string m_portraitHD;
    
    std::string m_outputFileName; // Use displayName first and then usrName
    
    std::map<std::string, std::pair<std::string, std::string>> m_members; // uidHash => <uid,NickName>
    
public:
    
    Friend() : m_isChatroom(false)
    {
    }
    
    Friend(const std::string& uid, const std::string& hash) : m_usrName(uid), m_uidHash(hash)
    {
        m_isChatroom = isChatroom(uid);
    }
    
    static bool isSubscription(const std::string& usrName);
    static bool isChatroom(const std::string& usrName);
    
    inline bool isSubscription() const
    {
        return isSubscription(m_usrName);
    }
    
    inline std::string getUsrName() const { return m_usrName; }
    inline std::string getHash() const { return m_uidHash; }
    void setUsrName(const std::string& usrName) { this->m_usrName = usrName; m_uidHash = md5(usrName);  m_outputFileName = m_uidHash; m_isChatroom = isChatroom(usrName); }
    inline bool isUsrNameEmpty() const
    {
        return m_usrName.empty();
    }
    inline bool isHashEmpty() const
    {
        return m_uidHash.empty();
    }

    bool containMember(const std::string& uidHash) const
    {
        std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = m_members.find(uidHash);
        return it != m_members.cend();
    }
    
    std::string getMemberName(const std::string& uidHash) const
    {
        std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = m_members.find(uidHash);
        return it != m_members.cend() ? it->second.second : "";
    }

    void addMember(const std::string& uidHash, const std::pair<std::string, std::string>& uidAndDisplayName)
    {
        typename std::map<std::string, std::pair<std::string, std::string>>::iterator it = m_members.find(uidHash);
        if (it != m_members.end())
        {
            if (it->second.second != uidAndDisplayName.second)
            {
                it->second.second = uidAndDisplayName.second;
            }
        }
        else
        {
            m_members[uidHash] = uidAndDisplayName;
        }
    }
    
    inline std::string getDisplayName() const
    {
        return m_displayName.empty() ? m_usrName : m_displayName;
    }
    
    inline bool isDisplayNameEmpty() const
    {
        return m_displayName.empty();
    }
    
    inline void setDisplayName(const std::string& displayName)
    {
        m_displayName = displayName;
    }
    
    inline void setUserType(int userType)
    {
        m_userType = userType;
    }
    
    static bool isInvalidPortrait(const std::string& portrait);
    
    inline void setPortrait(const std::string& portrait)
    {
#ifndef NDEBUG
        if (isInvalidPortrait(portrait))
        {
            assert(false);
        }
#endif
        m_portrait = portrait;
    }
    
    inline void setPortraitHD(const std::string& portraitHD)
    {
        m_portraitHD = portraitHD;
    }
    
    inline std::string getOutputFileName() const
    {
        return m_outputFileName;
    }
    
    inline void setOutputFileName(const std::string& outputFileName)
    {
        m_outputFileName = outputFileName;
    }
    
    inline bool isChatroom() const
    {
        return m_isChatroom;
    }
    
    inline bool isPortraitEmpty() const
    {
        return m_portrait.empty() && m_portraitHD.empty();
    }
    
    inline std::string getPortrait() const
    {
        return m_portraitHD.empty() ? m_portrait : m_portraitHD;
    }
    
    inline std::string getSecondaryPortrait() const
    {
        return (m_portraitHD.empty() || m_portrait.empty()) ? "" : m_portrait;
    }
    
    inline std::string getLocalPortrait() const
    {
        return m_usrName + ".jpg";
    }
    
protected:
    bool update(const Friend& f)
    {
        if (m_usrName != f.m_usrName)
        {
            return false;
        }
        
        if (m_displayName.empty() && !f.m_displayName.empty())
        {
            m_displayName = f.m_displayName;
        }
        if (m_portrait.empty() && !f.m_portrait.empty())
        {
            m_portrait = f.m_portrait;
        }
        if (m_portraitHD.empty() && !f.m_portraitHD.empty())
        {
            m_portraitHD = f.m_portraitHD;
        }
        for (std::map<std::string, std::pair<std::string, std::string>>::iterator it = m_members.begin(); it != m_members.end(); ++it)
        {
            if (it->second.second.empty())
            {
                std::map<std::string, std::pair<std::string, std::string>>::const_iterator it2 = f.m_members.find(it->first);
                if (it2 != f.m_members.cend())
                {
                    it->second.second = it2->second.second;
                }
            }
        }
        
        return true;
    }
    
};

inline bool Friend::isSubscription(const std::string& usrName)
{
    /*
     newsapp,fmessage,filehelper,weibo,qqmail,fmessage,tmessage,qmessage,qqsync,floatbottle,lbsapp,shakeapp,medianote,qqfriend,readerapp,blogapp,facebookapp,masssendapp,meishiapp,feedsapp,voip,blogappweixin,weixin,brandsessionholder,weixinreminder,wxid_novlwrv3lqwv11,gh_22b87fa7cb3c,officialaccounts,notification_messages,wxid_novlwrv3lqwv11,gh_22b87fa7cb3c,wxitil,userexperience_alarm,notification_messages
     */
    return startsWith(usrName, "gh_")
        || (usrName.compare("brandsessionholder") == 0)
        || (usrName.compare("newsapp") == 0)
        || (usrName.compare("weixin") == 0)
        || (usrName.compare("notification_messages") == 0);
}

inline bool Friend::isChatroom(const std::string& usrName)
{
    return endsWith(usrName, "@chatroom")
        || endsWith(usrName, "@im.chatroom");
}

inline bool Friend::isInvalidPortrait(const std::string& portrait)
{
    return !portrait.empty() && (!startsWith(portrait, "http://") && !startsWith(portrait, "https://") && !startsWith(portrait, "file://"));
}

class Friends
{
public:
    std::map<std::string, Friend> friends;  // uidHash => Friend
    std::map<std::string, std::string> hashes;  // uid => Hash
    
    /*
    template <class THandler>
    void handleFriend(THandler handler)
    {
        for (std::map<std::string, Friend>::iterator it = friends.begin(); it != friends.end(); ++it)
        {
            handler(it->second);
        }
    }
     */
    
    bool hasFriend(const std::string& hash) const { return friends.find(hash) != friends.end(); }
    const Friend* getFriend(const std::string& uidHash) const
    {
        std::map<std::string, Friend>::const_iterator it = friends.find(uidHash);
        if (it == friends.cend())
        {
            return NULL;
        }
        return &(it->second);
    }
    Friend* getFriend(const std::string& uidHash)
    {
        std::map<std::string, Friend>::iterator it = friends.find(uidHash);
        if (it == friends.end())
        {
            return NULL;
        }
        return &(it->second);
    }
    const Friend* getFriendByUid(const std::string& uid) const
    {
        std::map<std::string, std::string>::const_iterator it = hashes.find(uid);
        std::string hash = it == hashes.cend() ? md5(uid) : it->second;
        
        std::map<std::string, Friend>::const_iterator it2 = friends.find(hash);
        if (it2 == friends.cend())
        {
            return NULL;
        }
        return &(it2->second);
        // return getFriend(hash);
    }
    Friend* getFriendByUid(const std::string& uid)
    {
        std::map<std::string, std::string>::const_iterator it = hashes.find(uid);
        std::string hash = it == hashes.cend() ? md5(uid) : it->second;
        
        std::map<std::string, Friend>::iterator it2 = friends.find(hash);
        if (it2 == friends.end())
        {
            return NULL;
        }
        return &(it2->second);
        // return getFriend(hash);
    }
    
    Friend& addFriend(const std::string& uid)
    {
        std::map<std::string, std::string>::const_iterator it = hashes.find(uid);
        std::string hash = it == hashes.cend() ? md5(uid) : it->second;
        if (it == hashes.cend())
        {
            hashes[uid] = hash;
        }
        
        friends[hash] = Friend(uid, hash);
        return friends[hash];
    }
    
    void addHash(const std::string& uid)
    {
        std::map<std::string, std::string>::const_iterator it = hashes.find(uid);
        if (it == hashes.cend())
        {
            hashes[uid] = md5(uid);
        }
    }
    
};

class Session : public Friend
{
protected:
    int m_unreadCount;
    int m_recordCount;
    
    unsigned int m_createTime;
    unsigned int m_lastMessageTime;
    std::string m_extFileName;
    std::string m_dbFile;
    void *m_data;
    bool m_deleted;
    const Friend* m_owner;
    
public:
    Session(const Friend* owner) : Friend(), m_unreadCount(0), m_recordCount(0), m_createTime(0), m_lastMessageTime(0), m_data(NULL), m_deleted(false), m_owner(owner)
    {
    }
    
    Session(const std::string& uid, const std::string& hash, const Friend* owner) : Friend(uid, hash), m_unreadCount(0), m_recordCount(0), m_createTime(0), m_lastMessageTime(0), m_data(NULL), m_deleted(false), m_owner(owner)
    {
    }
    
    inline std::string getDisplayName() const
    {
        return m_deleted ? Friend::getDisplayName() + "-deleted" : Friend::getDisplayName();
    }
    
    inline unsigned int getCreateTime() const
    {
        return m_createTime;
    }
    
    inline void setCreateTime(unsigned int createTime)
    {
        m_createTime = createTime;
    }
    
    inline unsigned int getLastMessageTime() const
    {
        return m_lastMessageTime;
    }
    
    inline void setLastMessageTime(unsigned int lastMessageTime)
    {
        m_lastMessageTime = lastMessageTime;
    }
    
    inline bool isExtFileNameEmpty() const
    {
        return m_extFileName.empty();
    }
    
    inline std::string getExtFileName() const
    {
        return m_extFileName;
    }
    
    inline void setExtFileName(const std::string& extFileName)
    {
        m_extFileName = extFileName;
    }
    
    inline int getUnreadCount() const
    {
        return m_unreadCount;
    }
    
    inline void setUnreadCount(int unreadCount)
    {
        m_unreadCount = unreadCount;
    }
    
    inline int getRecordCount() const
    {
        return m_recordCount;
    }
    
    inline void setRecordCount(int rc)
    {
        m_recordCount = rc;
    }
    
    inline bool isDbFileEmpty() const
    {
        return m_dbFile.empty();
    }
    
    inline std::string getDbFile() const
    {
        return m_dbFile;
    }
    
    inline void setDbFile(const std::string& dbFile)
    {
        m_dbFile = dbFile;
    }
    
    void *getData() const
    {
        return m_data;
    }
    
    void setData(void *data)
    {
        m_data = data;
    }

    bool update(const Friend& f)
    {
        return Friend::update(f);
    }
    
    bool isDeleted() const
    {
        return m_deleted;
    }
    
    void setDeleted(bool deleted)
    {
        m_deleted = deleted;
    }
    
    const Friend* getOwner() const
    {
        return m_owner;
    }
    
    
};

struct SessionHashCompare
{
    bool operator()(const Session& s1, const Session& s2) const
    {
        return s1.getHash().compare(s2.getHash()) < 0;
    }
    
    bool operator()(const Session& s1, const std::string& s2) const
    {
        return s1.getHash().compare(s2) < 0;
    }
};

struct SessionLastMsgTimeCompare
{
    bool operator()(const Session& s1, const Session& s2) const
    {
        return s1.getLastMessageTime() > s2.getLastMessageTime();
    }
};


#endif /* WechatObjects_h */
