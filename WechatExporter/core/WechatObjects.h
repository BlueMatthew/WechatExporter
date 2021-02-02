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
#include "Utils.h"

#ifndef WechatObjects_h
#define WechatObjects_h

class WechatInfo
{
private:
    std::string m_version;
    std::string m_shortVersion;
    std::string m_cellDataVersion;
    
public:
    void setVersion(std::string version)
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
    
    std::string getVersion() const
    {
        return m_version;
    }
    
    std::string getShortVersion() const
    {
        return m_shortVersion;
    }
    void setCellDataVersion(std::string cellDataVersion)
    {
        m_cellDataVersion = cellDataVersion;
    }
    
    std::string getCellDataVersion() const
    {
        return m_cellDataVersion;
    }
    
    std::string buildUserAgent() const
    {
        if (m_version.empty()) return "WeChat/7.0.15.33 CFNetwork/978.0.7 Darwin/18.6.0"; // default
        return "WeChat/" + m_version + " CFNetwork/978.0.7 Darwin/18.6.0";
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
    
    inline void setPortrait(const std::string& portrait)
    {
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

class Friends
{
public:
    std::map<std::string, Friend> friends;  // uidHash => Friend
    std::map<std::string, std::string> hashes;  // uid => Hash
    
    template <class THandler>
    void handleFriend(THandler handler)
    {
        for (std::map<std::string, Friend>::iterator it = friends.begin(); it != friends.end(); ++it)
        {
            handler(it->second);
        }
    }
    
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
    
public:
    Session() : Friend(), m_unreadCount(0), m_recordCount(0), m_createTime(0), m_lastMessageTime(0)
    {
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

    bool update(const Friend& f)
    {
        return Friend::update(f);
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
