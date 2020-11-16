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

class Friend
{
public:
    std::string NickName;
    std::string Alias;
    std::string ConRemark;
    int UserType;
    std::string ConChatRoomMem;
    std::string ConStrRes2;
    std::string Portrait;
    std::string PortraitHD;
    mutable bool PortraitRequired;
    std::string DefaultProfileHead;
    mutable bool IsChatroom;
    
    std::string outputFileName;
    
    std::map<std::string, std::pair<std::string, std::string>> Members; // uidHash => <uid,NickName>
    
private:
    std::string usrName;
    std::string uidHash;

public:
    
    Friend(const std::string& uid, const std::string& hash) : usrName(uid), uidHash(hash)
    {
        IsChatroom = endsWith(uid, "@chatroom");
    }
    
    const std::string& getUsrName() const { return usrName; }
    const std::string& getUidHash() const { return uidHash; }
    void setUsrName(const std::string& usrName) { this->usrName = usrName; uidHash = md5(usrName);  outputFileName = uidHash; IsChatroom = endsWith(usrName, "@chatroom"); }
    
    Friend() : PortraitRequired(false), DefaultProfileHead("DefaultProfileHead@2x.png"), IsChatroom(false)
    {
    }
    
    bool containMember(const std::string& uidHash) const
    {
        std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = Members.find(uidHash);
        return it != Members.cend();
    }
    
    std::string getMemberName(const std::string& uidHash) const
    {
        std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = Members.find(uidHash);
        return it != Members.cend() ? it->second.second : "";
    }
    
    void ProcessConStrRes2()
    {
        static std::regex aliasPattern("<alias>(.*?)<\\/alias>");
        
        std::smatch sm;
        bool matched = std::regex_search(ConStrRes2, sm, aliasPattern);
        if (matched)
        {
            Alias = sm[1];
        }
        // alias = match.Success ? match.Groups[1].Value : null;
        static std::regex headImgUrlPattern("<HeadImgUrl>(.+?)<\\/HeadImgUrl>");
        
        std::smatch sm2;
        matched = std::regex_search(ConStrRes2, sm2, headImgUrlPattern);
        if (matched)
        {
            Portrait = sm[1];
        }
        static std::regex headImgHDUrlPattern("<HeadImgHDUrl>(.+?)<\\/HeadImgHDUrl>");
        
        std::smatch sm3;
        matched = std::regex_search(ConStrRes2, sm3, headImgHDUrlPattern);
        if (matched)
        {
            PortraitHD = sm[1];
        }
    }

    std::string DisplayName() const
    {
        if (ConRemark != "") return ConRemark;
        if (NickName != "") return NickName;
        return ID();
    }
    
    std::string ID() const
    {
        if (Alias != "") return Alias;
        if (usrName != "") return usrName;
        return std::string("");
    }
    
    std::string getPortraitUrl() const
    {
        return PortraitHD.empty() ? Portrait : PortraitHD;
    }
    
    std::string getLocalPortrait() const
    {
        return usrName + ".jpg";
    }
    
};

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
        return getFriend(hash);
    }
    Friend* getFriendByUid(const std::string& uid)
    {
        std::map<std::string, std::string>::const_iterator it = hashes.find(uid);
        std::string hash = it == hashes.cend() ? md5(uid) : it->second;
        return getFriend(hash);
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

struct Session
{
    std::string UsrName;
    std::string Hash;
    std::string DisplayName;
    std::string Portrait;
    unsigned int CreateTime;
    unsigned int LastMessageTime;
    std::string ExtFileName;
    
    int UnreadCount = 0;
    
    std::string dbFile;
    
    std::string outputFileName;
    
    bool isChatroom() const { return endsWith(UsrName, "@chatroom") || endsWith(UsrName, "@im.chatroom"); }
    bool isSubscription() const { return isSubscription(UsrName); }
    
    static bool isSubscription(const std::string& usrName);
    
    std::map<std::string, std::pair<std::string, std::string>> Members; // uidHash => <uid,NickName>
    
    void setUsrName(const std::string& usrName) { this->UsrName = usrName; Hash = md5(UsrName); outputFileName = UsrName; }
    
    std::string getMemberName(const std::string& uidHash) const
    {
        std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = Members.find(uidHash);
        return it != Members.cend() ? it->second.second : "";
    }
    
    bool copyInfoFromFriend(const Friend& f)
    {
        if (UsrName != f.getUsrName())
        {
            return false;
        }
        
        if (DisplayName.empty())
        {
            DisplayName = f.DisplayName();
        }
        if (Portrait.empty())
        {
            Portrait = f.getPortraitUrl();
        }
        
        return true;
    }
    
    bool exportToFriend(Friend& f)
    {
        if (UsrName != f.getUsrName() && !isChatroom())
        {
            return false;
        }
        
        if (f.NickName.empty())
        {
            f.NickName = DisplayName;
        }
        
        for (typename std::map<std::string, std::pair<std::string, std::string>>::const_iterator it = Members.cbegin(); it != Members.cend(); ++it)
        {
            typename std::map<std::string, std::pair<std::string, std::string>>::iterator itFriend = f.Members.find(it->first);
            if (itFriend != f.Members.end())
            {
                if (itFriend->second.second != it->second.second)
                {
                    itFriend->second.second = it->second.second;
                }
            }
            else
            {
                f.Members[it->first] = it->second;
            }
        }
        
        return true;
    }

};

inline bool Session::isSubscription(const std::string& usrName)
{
    return startsWith(usrName, "gh_") || (usrName.compare("brandsessionholder") == 0);
}

struct SessionHashCompare
{
    bool operator()(const Session& s1, const Session& s2) const
    {
        return s1.Hash.compare(s2.Hash) < 0;
    }
    
    bool operator()(const Session& s1, const std::string& s2) const
    {
        return s1.Hash.compare(s2) < 0;
    }
};

struct SessionLastMsgTimeCompare
{
    bool operator()(const Session& s1, const Session& s2) const
    {
        return s1.LastMessageTime > s2.LastMessageTime;
    }
};


#endif /* WechatObjects_h */
