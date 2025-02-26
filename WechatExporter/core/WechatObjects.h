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
#include "FileSystem.h"

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
#ifndef NDEBUG
public:
#else
protected:
#endif
    std::string m_usrName;
    std::string m_wxName;   // WeiXin Hao
    std::string m_uidHash;
    std::string m_displayName;
    int m_userType;
    bool m_isChatroom;
	bool m_deleted;
    std::string m_portrait;
    std::string m_portraitHD;

    std::string m_outputFileName; // Use displayName first and then usrName
    std::string m_encodedOutputFileName; // Use displayName first and then usrName
    
    // std::map<std::string, std::pair<std::string, std::string>> m_members; // uidHash => <uid,NickName>
    std::map<std::string, std::string> m_members; // uid => NickName
    std::vector<std::string> m_memberUsrNames; // uid
    std::vector<std::string> m_tags;
    
public:
    
    Friend() : m_isChatroom(false), m_deleted(false)
    {
    }
    
    Friend(const std::string& uid, const std::string& hash) : m_usrName(uid), m_uidHash(hash), m_isChatroom(false), m_deleted(false)
    {
        m_isChatroom = isChatroom(uid);
    }
    
    static bool isSubscription(const std::string& usrName);
    static bool isChatroom(const std::string& usrName);
    
    static bool isDefaultAvatar(const std::string& path);
    static bool isDefaultAvatar(size_t fileSize, const std::string& path);
    
    inline bool isSubscription() const
    {
        return isSubscription(m_usrName);
    }
    
    inline std::string getUsrName() const { return m_usrName; }
    inline std::string getWxName() const { return m_wxName.empty() ? m_usrName : m_wxName; }
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

	void setEmptyUsrName(const std::string& usrName) { this->m_usrName = usrName; }
	
	bool isDeleted() const
	{
		return m_deleted;
	}

	void setDeleted(bool deleted)
	{
		m_deleted = deleted;
	}

    bool containMember(const std::string& usrName) const
    {
        auto it = m_members.find(usrName);
        return it != m_members.cend();
    }
    
    std::string getMemberName(const std::string& usrName) const
    {
        auto it = m_members.find(usrName);
        return it != m_members.cend() ? it->second : "";
    }

    void addMember(const std::string& usrName, const std::string& displayName)
    {
        auto it = m_members.find(usrName);
        if (it != m_members.end())
        {
            it->second = displayName;
        }
        else
        {
            m_members[usrName] = displayName;
            m_memberUsrNames.push_back(usrName);
        }
    }
    
    std::vector<std::string> getMemberUsrNames() const
    {
        return m_memberUsrNames;
    }
    
    inline std::string getDisplayName() const
    {
        return m_displayName.empty() ? (m_wxName.empty() ? m_usrName : m_wxName) : m_displayName;
    }
    
    inline bool isDisplayNameEmpty() const
    {
        return m_displayName.empty();
    }
    
    inline bool isWxNameEmpty() const
    {
        return m_wxName.empty();
    }
    
    inline void setWxName(const std::string& wxName)
    {
        m_wxName = wxName;
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
        m_encodedOutputFileName = encodeUrl(outputFileName);
    }
    
    inline std::string getEncodedOutputFileName() const
    {
        return m_encodedOutputFileName;
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
    
    void clearTags()
    {
        m_tags.clear();
    }
    
    void swapTags(std::vector<std::string> tags)
    {
        m_tags.swap(tags);
    }
    
    std::string buildTagDesc(const std::map<uint64_t, std::string>& tags) const
    {
        if (m_tags.empty())
        {
            return "";
        }
        
        std::vector<std::string> tagDesc(m_tags.size());
        for (auto it = m_tags.cbegin(); it != m_tags.cend(); ++it)
        {
            auto it2 = tags.find(std::stoull(*it));
            if (it2 != tags.cend())
            {
                tagDesc.push_back(it2->second);
            }
        }
        return join(tagDesc, " ");
    }
    
protected:
    bool update(const Friend& f)
    {
        if (m_usrName.empty() && m_uidHash.empty())
        {
            // Can't compare the object
            return false;
        }
        if (!m_usrName.empty())
        {
            if (m_usrName != f.m_usrName)
            {
                return false;
            }
        }
        else
        {
            if (m_uidHash != f.m_uidHash)
            {
                return false;
            }
        }
        
        bool result = false;
        
        if (isUsrNameEmpty())
        {
            setUsrName(f.getUsrName());
            result = true;
        }
        if (m_wxName.empty() && !f.m_wxName.empty())
        {
            m_wxName = f.m_wxName;
            result = true;
        }
        
        if (m_displayName.empty() && !f.m_displayName.empty())
        {
            m_displayName = f.m_displayName;
            result = true;
        }
        if (m_portrait.empty() && !f.m_portrait.empty())
        {
            m_portrait = f.m_portrait;
            result = true;
        }
        if (m_portraitHD.empty() && !f.m_portraitHD.empty())
        {
            m_portraitHD = f.m_portraitHD;
            result = true;
        }
        
        if (m_members.empty())
        {
            if (!f.m_members.empty())
            {
                m_members = f.m_members;
                m_memberUsrNames = f.m_memberUsrNames;
            }
        }
        else
        {
            for (auto it = m_members.begin(); it != m_members.end(); ++it)
            {
                if (it->second.empty())
                {
                    auto it2 = f.m_members.find(it->first);
                    if (it2 != f.m_members.cend())
                    {
                        it->second = it2->second;
                        result = true;
                    }
                }
            }
        }

        return result;
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

inline bool Friend::isDefaultAvatar(const std::string& path)
{
    return isDefaultAvatar(getFileSize(path), path);
}

inline bool Friend::isDefaultAvatar(size_t fileSize, const std::string& path)
{
    return (fileSize == 5875) && (md5File(path) == "e3e807760b01d24760eba724bac616d6");
}

inline bool Friend::isInvalidPortrait(const std::string& portrait)
{
    return !portrait.empty() && (!startsWith(portrait, "http://") && !startsWith(portrait, "https://") && !startsWith(portrait, "file://"));
}

struct FriendDisplayNameCompare
{
    bool operator()(const Friend& f1, const Friend& f2) const
    {
        return f1.getDisplayName().compare(f2.getDisplayName()) < 0;
    }
    
    bool operator()(const Friend& f1, const std::string& s2) const
    {
        return f1.getDisplayName().compare(s2) < 0;
    }
    
    bool operator()(const Friend* f1, const Friend* f2) const
    {
        return f1->getDisplayName().compare(f2->getDisplayName()) < 0;
    }
    
    bool operator()(const Friend* f1, const std::string& s2) const
    {
        return f1->getDisplayName().compare(s2) < 0;
    }
};

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
    
    void toArraySortedByDisplayName(std::vector<const Friend *>& friends) const
    {
        friends.reserve(this->friends.size());
        for (auto it = this->friends.cbegin(); it != this->friends.cend(); ++it)
        {
            friends.push_back(&(it->second));
        }
        
        std::sort(friends.begin(), friends.end(), FriendDisplayNameCompare());
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
    std::string m_lastMessage;
    std::string m_lastMessageUsrName;
    std::string m_lastMessageUserDisplayName;
    std::string m_extFileName;
    std::string m_dbFile;
    std::string m_memberIds;
    void *m_data;
    const Friend* m_owner;

public:
    Session(const Friend* owner) : Friend(), m_unreadCount(0), m_recordCount(0), m_createTime(0), m_lastMessageTime(0), m_data(NULL), m_owner(owner)
    {
    }
    
    Session(const std::string& uid, const std::string& hash, const Friend* owner) : Friend(uid, hash), m_unreadCount(0), m_recordCount(0), m_createTime(0), m_lastMessageTime(0), m_data(NULL), m_owner(owner)
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
    
    inline std::string getLastMessage() const
    {
        return m_lastMessage;
    }
    
    inline void setLastMessage(const std::string& lastMessage)
    {
        m_lastMessage = lastMessage;
    }
    
    inline void setLastMessage(const std::string& lastMessage, const std::string& lastMessageUsrName, const Friends& friends)
    {
        if (startsWith(lastMessage, lastMessageUsrName + ":\n"))
        {
            m_lastMessage = lastMessage.substr(lastMessageUsrName.size() + 2);
        }
        else
        {
            m_lastMessage = lastMessage;
        }
        setLastMessageUsrName(lastMessageUsrName, friends);
    }
    
    inline std::string getLastMessageUsrName() const
    {
        return m_lastMessageUsrName;
    }
    
    bool isTextMessage() const
    {
        return !m_lastMessage.empty() && !startsWith(m_lastMessage, "<?xml") && !startsWith(m_lastMessage, "<msg>") && !startsWith(m_lastMessage, "<voipinvitemsg>") && !startsWith(m_lastMessage, "{\"msgLocalID\":") && !startsWith(m_lastMessage, "<sysmsg");
    }
    
    inline void setLastMessageUsrName(const std::string& lastMessageUsrName, const Friends& friends)
    {
        m_lastMessageUsrName = lastMessageUsrName;
        // std::string uidHash = md5(m_lastMessageUsrName);
        auto it = m_members.find(m_lastMessageUsrName);
        // std::map<std::string, std::pair<std::string, std::string>> m_members; // uidHash => <uid,NickName>
        if (it != m_members.cend() && !it->second.empty())  // "Empty display name" means there is no specified nick name for chat group
        {
            m_lastMessageUserDisplayName = it->second;
        }
        else
        {
            const Friend* f = friends.getFriendByUid(m_lastMessageUsrName);
            if (NULL != f)
            {
                m_lastMessageUserDisplayName = f->getDisplayName();
            }
        }
    }
    
    inline void setLastMessageUsrName(const std::string& lastMessageUsrName, const std::string& lastMessageUserDsiplayName)
    {
        m_lastMessageUsrName = lastMessageUsrName;
        m_lastMessageUserDisplayName = lastMessageUserDsiplayName;
    }
    
    inline bool hasLastMessageUserDisplayName() const
    {
        return !m_lastMessageUserDisplayName.empty();
    }
    
    inline std::string getLastMessageUserDisplayName() const
    {
        return m_lastMessageUserDisplayName;
    }
    
    inline void setLastMessageUserDisplayName(const std::string& lastMessageUserDispalayName)
    {
        m_lastMessageUserDisplayName = lastMessageUserDispalayName;
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
    
    inline bool isMemberIdsEmpty() const
    {
        return m_members.empty() && m_memberUsrNames.empty();
    }
    
    inline std::string getMemberIds() const
    {
        return m_memberIds;
    }
    
    inline void setMemberIds(const std::string& memberIds)
    {
        m_memberIds = memberIds;
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
        bool result = Friend::update(f);
        if (!m_lastMessageUsrName.empty() && m_lastMessageUserDisplayName.empty())
        {
            std::string memberName = f.getMemberName(m_lastMessageUsrName);
            if (!memberName.empty())
            {
                m_lastMessageUserDisplayName = memberName;
                result = true;
            }
        }

        return result;
    }
    
    const Friend* getOwner() const
    {
        return m_owner;
    }

};

struct SessionUsrNameCompare
{
    bool operator()(const Session& s1, const Session& s2) const
    {
        return s1.getUsrName().compare(s2.getUsrName()) < 0;
    }
    
    bool operator()(const Session& s1, const std::string& s2) const
    {
        return s1.getUsrName().compare(s2) < 0;
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

struct WXMSG
{
    unsigned int createTime;
    std::string content;
    int des;
    int type;
    std::string msgId;
    int64_t msgIdValue;
    uint64_t msgSvrId;
    int status;
    int tableVersion;
};

struct WXAPPMSG
{
    const WXMSG *msg;
    int appMsgType;
    std::string appId;
    std::string appName;
    std::string localAppIcon;
    std::string senderUsrName;
};

struct WXFWDMSG
{
    const WXMSG *msg;
    std::string usrName;
    std::string displayName;
    std::string portrait;
    std::string portraitLD;
    std::string dataType;
    std::string subType;
    std::string dataId;
    std::string dataFormat;
    std::string msgTime;
    std::string srcMsgTime;
#if !defined(NDEBUG) || defined(DBG_PERF)
    std::string rawMessage;
#endif
};


#endif /* WechatObjects_h */
