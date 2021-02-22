//
//  WechatParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "WechatParser.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sqlite3.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <json/json.h>
#include <plist/plist.h>

#include "WechatObjects.h"
#include "RawMessage.h"
#include "XmlParser.h"
#include "MMKVReader.h"

#include "OSDef.h"

#ifdef _WIN32
#include <atlconv.h>
#endif

template<class T>
bool parseMembers(const std::string& xml, T& f)
{
    bool result = false;
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr xpathCtx = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr xpathNodes = NULL;

    doc = xmlParseMemory(xml.c_str(), static_cast<int>(xml.size()));
    if (doc == NULL) { goto end; }

    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) { goto end; }
    
    xpathObj = xmlXPathEvalExpression(BAD_CAST("//RoomData/Member"), xpathCtx);
    if(xpathObj == NULL) { goto end; }
    
    xpathNodes = xpathObj->nodesetval; //从xpath object中得到node set
    if ((xpathNodes) && (xpathNodes->nodeNr > 0))
    {
        for (int i = 0; i < xpathNodes->nodeNr; i++)
        {
            xmlNode *cur = xpathNodes->nodeTab[i];
            xmlChar* uidPtr = xmlGetProp(cur, reinterpret_cast<const xmlChar *>("UserName"));
            if (NULL == uidPtr)
            {
                continue;
            }
            
            std::string uid = reinterpret_cast<char *>(uidPtr);
            xmlFree(uidPtr);
            
            std::string displayName;
            
            cur = cur->xmlChildrenNode;
            while (cur != NULL)
            {
                if ((!xmlStrcmp(cur->name, reinterpret_cast<const xmlChar *>("DisplayName"))))
                {
                    xmlChar *dnPtr = xmlNodeGetContent(cur);
                    if (NULL != dnPtr)
                    {
                        displayName = reinterpret_cast<char *>(dnPtr);
                        xmlFree(dnPtr);
                    }
                    break;
                }
                cur = cur->next;
            }
        
            std::string uidHash = md5(uid);
            f.addMember(uidHash, std::make_pair(uid, displayName));
        }
    }
    
    result = true;
    
    end:
    if (xpathObj) { xmlXPathFreeObject(xpathObj); }
    if (xpathCtx) { xmlXPathFreeContext(xpathCtx); }
    if (doc) { xmlFreeDoc(doc); }
    
    return result;
}

LoginInfo2Parser::LoginInfo2Parser(ITunesDb *iTunesDb) : m_iTunesDb(iTunesDb)
{
}

#ifndef NDEBUG
std::string LoginInfo2Parser::getError() const
{
    return m_error;
}
#endif

bool LoginInfo2Parser::parse(std::vector<Friend>& users)
{
    std::string loginInfo2 = "Documents/LoginInfo2.dat";
    std::string realPath = m_iTunesDb->findRealPath(loginInfo2);
    
    if (realPath.empty())
    {
        return false;
    }
    
    return parse(realPath, users);
}

bool LoginInfo2Parser::parse(const std::string& loginInfo2Path, std::vector<Friend>& users)
{
    RawMessage msg;
    if (!msg.mergeFile(loginInfo2Path))
    {
        return false;
    }
    
    std::string value1;
    if (!msg.parse("1", value1))
    {
        return false;
    }
    
    users.clear();
    int offset = 0;
    
    std::string::size_type length = value1.size();
    while (1)
    {
        int res = parseUser(value1.c_str() + offset, static_cast<int>(length - offset), users);
        if (res < 0)
        {
            break;
        }
        
        offset += res;
    }
    
    parseUserFromFolder(users);
    
    MMSettingInMMappedKVFilter filter;
    ITunesFileVector mmsettings = m_iTunesDb->filter(filter);
    
    std::map<std::string, std::string> mmsettingFiles;  // hash => usrName
    for (ITunesFilesConstIterator it = mmsettings.cbegin(); it != mmsettings.cend(); ++it)
    {
        std::string fileName = filter.parse((*it));
        fileName = fileName.substr(filter.getPrefix().size());
        if (fileName.empty())
        {
            continue;
        }
        
        std::string usrNameHash = md5(fileName);
        mmsettingFiles[usrNameHash] = fileName;
    }
    
    for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
    {
        MMSettingParser mmsettingParser(m_iTunesDb);
        if (mmsettingParser.parse(it->getHash()))
        {
            it->setUsrName(mmsettingParser.getUsrName());
            if (it->isDisplayNameEmpty())
            {
                it->setDisplayName(mmsettingParser.getDisplayName());
            }
            it->setPortrait(mmsettingParser.getPortrait());
            it->setPortraitHD(mmsettingParser.getPortraitHD());
        }
        else
        {
            // Check mmsettings.archive in MMappedKV folde
            if (it->getUsrName().empty())
            {
                std::map<std::string, std::string>::const_iterator it2 = mmsettingFiles.find(it->getHash());
                if (it2 != mmsettingFiles.cend())
                {
                    it->setUsrName(it2->second);
                }
            }
            if (!(it->getUsrName().empty()))
            {
                std::string realPath = m_iTunesDb->findRealPath("Documents/MMappedKV/mmsetting.archive." + it->getUsrName());
                std::string realCrcPath = m_iTunesDb->findRealPath("Documents/MMappedKV/mmsetting.archive." + it->getUsrName() + ".crc");
                if (!realPath.empty() && !realCrcPath.empty())
                {
                    MMKVParser parser;
                    if (parser.parse(realPath, realCrcPath))
                    {
                        if (it->isDisplayNameEmpty())
                        {
                            it->setDisplayName(parser.getDisplayName());
                        }
                        it->setPortrait(parser.getPortrait());
                        it->setPortraitHD(parser.getPortraitHD());
                    }
                }
            }
        }
    }
    
    auto it = users.begin();
    while (it != users.end())
    {
        if (it->getUsrName().empty())
        {
#ifndef NDEBUG
            m_error += "Erase: md5=" + it->getHash() + "* dn=" + it->getDisplayName() + "* ";
#endif
            // erase() invalidates the iterator, use returned iterator
            it = users.erase(it);

        }
        else
        {
            ++it;
        }
    }
    
    return true;
}

int LoginInfo2Parser::parseUser(const char* data, int length, std::vector<Friend>& users)
{
    uint32_t userBufferLen = 0;
    
    const char* p = calcVarint32Ptr(data, data + length, &userBufferLen);
    if (NULL == p || 0 == userBufferLen)
    {
        return -1;
    }
    RawMessage msg;
    if (!msg.merge(p, userBufferLen))
    {
        return -1;
    }
    
    Friend user;
    
    std::string value;
    if (msg.parse("1", value))
    {
        user.setUsrName(value);
    }
    if (msg.parse("3", value))
    {
        user.setDisplayName(value);
    }
#ifndef NDEBUG
    if (msg.parse("10.1.2", value))
    {
    }
    if (msg.parse("10.2.2.2", value))
    {
    }
#endif
    users.push_back(user);
    
#ifndef NDEBUG
    m_error += "LoginInfo2.dat: *" + user.getDisplayName() + "* ";
#endif
    
    return static_cast<int>(userBufferLen + (p - data));
}

bool LoginInfo2Parser::parseUserFromFolder(std::vector<Friend>& users)
{
    UserFolderFilter filter;
    ITunesFileVector folders = m_iTunesDb->filter(filter);
    
    for (ITunesFilesConstIterator it = folders.cbegin(); it != folders.cend(); ++it)
    {
        std::string fileName = filter.parse((*it));
#ifndef NDEBUG
        m_error += "User Folder: *" + fileName + "*  ";
#endif
        if (fileName == "00000000000000000000000000000000")
        {
            continue;
        }
        
        bool existing = false;
        std::vector<Friend>::const_iterator it2 = users.cbegin();
        for (; it2 != users.cend(); ++it2)
        {
            if (it2->getHash() == fileName)
            {
                existing = true;
                break;
            }
        }
        
        if (!existing)
        {
            users.emplace(users.end(), "", fileName);
#ifndef NDEBUG
            m_error += "New User From Folder: *" + fileName + "*  ";
#endif
        }
    }

    return true;
}

void MMSettings::clear()
{
    m_usrName.clear();
    m_displayName.clear();
    m_portrait.clear();
    m_portraitHD.clear();
}

std::string MMSettings::getUsrName() const
{
    return m_usrName;
}

std::string MMSettings::getDisplayName() const
{
    return m_displayName;
}

std::string MMSettings::getPortrait() const
{
    return m_portrait;
}

std::string MMSettings::getPortraitHD() const
{
    return m_portraitHD;
}

MMKVParser::MMKVParser()
{
}

bool MMKVParser::parse(const std::string& path, const std::string& crcPath)
{
    std::vector<unsigned char> contents;
    uint32_t lastActualSize = 0;
    // 86: usrName
    // 87: name
    // 88: DisplayName
    if (readFile(crcPath, contents))
    {
        memcpy(&lastActualSize, &contents[32], 4);
        contents.clear();
    }
    
    if (!readFile(path, contents))
    {
        return false;
    }
    
    uint32_t actualSize = 0;
    memcpy(&actualSize, &contents[0], 4);
    if (actualSize <= 0)
    {
        actualSize = lastActualSize;
    }
    
    if (actualSize <= 0)
    {
        return false;
    }

    actualSize += 4;
    
    MMKVReader reader(&contents[0], actualSize);
    reader.seek(8);
    
    while (!reader.isAtEnd())
    {
        // 
        const auto k = reader.readKey();
        if (k.empty())
        {
            break;
        }
        
        if (k == "86")
        {
            m_usrName = reader.readStringValue();
        }
        else if (k == "87")
        {
            m_name = reader.readStringValue();
        }
        else if (k == "88")
        {
            m_displayName = reader.readStringValue();
        }
        else if (k == "headimgurl")
        {
            m_portrait = reader.readStringValue();
        }
        else if (k == "headhdimgurl")
        {
            m_portraitHD = reader.readStringValue();
        }
        else
        {
            reader.skipValue();
        }
    }
    
    return true;
}

MMSettingParser::MMSettingParser(ITunesDb *iTunesDb) : m_iTunesDb(iTunesDb)
{
}

bool MMSettingParser::parse(const std::string& usrNameHash)
{
    clear();
    
    std::string vpath = "Documents/" + usrNameHash + "/mmsetting.archive";
    std::string mmsettingPath = m_iTunesDb->findRealPath(vpath);
    if (mmsettingPath.empty())
    {
        return false;
    }
    
    std::vector<unsigned char> data;
    if (!readFile(mmsettingPath, data))
    {
        return false;
    }
    
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
    if (NULL == node)
    {
        return false;
    }
    
    std::unique_ptr<void, decltype(&plist_free)> nodePtr(node, &plist_free);
    plist_t objectsNode = plist_access_path(node, 1, "$objects");
    if (NULL == objectsNode || !PLIST_IS_ARRAY(objectsNode))
    {
        return false;
    }

    plist_t keyedUidNodes = plist_array_get_item(objectsNode, 1);
    
    const char* keys[] = {"UsrName", "NickName", "AliasName"};
    for (int idx = 0; idx < sizeof(keys) / sizeof(const char *); ++idx)
    {
        plist_t keyedUidNode = plist_dict_get_item(keyedUidNodes, keys[idx]);
        if (keyedUidNode != NULL && PLIST_IS_UID(keyedUidNode))
        {
            uint64_t uid = 0;
            plist_get_uid_val(keyedUidNode, &uid);
            plist_t keyedItemNode = plist_array_get_item(objectsNode, static_cast<uint32_t>(uid));
            
            uint64_t valueLength = 0;
            const char* pValue = plist_get_string_ptr(keyedItemNode, &valueLength);
            if (pValue == NULL || valueLength == 0)
            {
                continue;
            }
            
            if (idx == 0)
            {
                m_usrName.assign(pValue, valueLength);
            }
            else if (idx == 1)
            {
                m_displayName.assign(pValue, valueLength);
            }
            else if (idx == 2)
            {
                if (m_displayName.empty())
                {
                    m_displayName.assign(pValue, valueLength);
                }
            }
        }
    }
    
    // "new_dicsetting"
    plist_t keyedUidNode = plist_dict_get_item(keyedUidNodes, "new_dicsetting");
    if (keyedUidNode != NULL && PLIST_IS_UID(keyedUidNode))
    {
        uint64_t uid = 0;
        plist_get_uid_val(keyedUidNode, &uid);
        
        plist_t settingNode = plist_array_get_item(objectsNode, static_cast<uint32_t>(uid));
        if (keyedUidNode != NULL && PLIST_IS_DICT(settingNode))
        {
            plist_t settingKeysNode = plist_dict_get_item(settingNode, "NS.keys");
            plist_t settingValuesNode = plist_dict_get_item(settingNode, "NS.objects");
            
            if (settingKeysNode != NULL && PLIST_IS_ARRAY(settingKeysNode) && settingValuesNode != NULL && PLIST_IS_ARRAY(settingValuesNode))
            {
                uint32_t settingKeysNodeSize = plist_array_get_size(settingKeysNode);
                uint32_t settingValuesNodeSize = plist_array_get_size(settingValuesNode);
                uint32_t minSize = std::min(settingKeysNodeSize, settingValuesNodeSize);
                for (uint32_t idx = 0; idx < minSize; ++idx)
                {
                    plist_t keyedUidNode = plist_array_get_item(settingKeysNode, idx);
                    if (keyedUidNode == NULL || !PLIST_IS_UID(keyedUidNode))
                    {
                        continue;
                    }
                    
                    uint64_t uid = 0;
                    plist_get_uid_val(keyedUidNode, &uid);
                    plist_t keyedItemNode = plist_array_get_item(objectsNode, static_cast<uint32_t>(uid));
                    if (keyedItemNode == NULL || !PLIST_IS_STRING(keyedItemNode))
                    {
                        continue;
                    }
                    
                    uint64_t valueLength = 0;
                    const char* pValue = plist_get_string_ptr(keyedItemNode, &valueLength);
                    
                    if (pValue == NULL || valueLength == 0)
                    {
                        continue;
                    }
                    
                    std::string settingKey(pValue, valueLength);
                    
                    if (settingKey != "headimgurl" && settingKey != "headhdimgurl")
                    {
                        continue;
                    }
                    
                    keyedUidNode = plist_array_get_item(settingValuesNode, idx);
                    if (keyedUidNode == NULL || !PLIST_IS_UID(keyedUidNode))
                    {
                        continue;
                    }
                    
                    uid = 0;
                    plist_get_uid_val(keyedUidNode, &uid);
                    keyedItemNode = plist_array_get_item(objectsNode, static_cast<uint32_t>(uid));
                    if (keyedItemNode == NULL || !PLIST_IS_STRING(keyedItemNode))
                    {
                        continue;
                    }
                    
                    valueLength = 0;
                    pValue = plist_get_string_ptr(keyedItemNode, &valueLength);
                    if (pValue == NULL || valueLength == 0)
                    {
                        continue;
                    }
                    
                    if (settingKey == "headimgurl")
                    {
                        m_portrait.assign(pValue, valueLength);
                    }
                    else if (settingKey == "headhdimgurl")
                    {
                        m_portraitHD.assign(pValue, valueLength);
                    }
                    
                    if (!m_portrait.empty() && !m_portraitHD.empty())
                    {
                        break;
                    }
                }
            }
        }
    }

    return true;
}

FriendsParser::FriendsParser(bool detailedInfo/* = true*/) : m_detailedInfo(detailedInfo)
{
}

bool FriendsParser::parseWcdb(const std::string& mmPath, Friends& friends)
{
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(mmPath, &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT userName,dbContactRemark,dbContactChatRoom,dbContactHeadImage,type FROM Friend";
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int userType = sqlite3_column_int(stmt, 4);
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL == val)
        {
            continue;
        }
        std::string uid = val;
        
        if (Friend::isSubscription(uid))
        {
            continue;
        }
        
        Friend& f = friends.addFriend(uid);
        f.setUserType(userType);
        
        parseRemark(sqlite3_column_blob(stmt, 1), sqlite3_column_bytes(stmt, 1), f);
        if (m_detailedInfo)
        {
            parseAvatar(sqlite3_column_blob(stmt, 3), sqlite3_column_bytes(stmt, 3), f);
            parseChatroom(sqlite3_column_blob(stmt, 2), sqlite3_column_bytes(stmt, 2), f);
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return true;
}

bool FriendsParser::parseRemark(const void *data, int length, Friend& f)
{
    RawMessage msg;
    if (!msg.merge(reinterpret_cast<const char *>(data), length))
    {
        return false;
    }
    
    std::string value;
    if (msg.parse("1", value))
    {
        f.setDisplayName(value);
    }
    /*
    if (msg.parse("6", value))
    {
    }
    */
    
    return true;
}

bool FriendsParser::parseAvatar(const void *data, int length, Friend& f)
{
    RawMessage msg;
    if (!msg.merge(reinterpret_cast<const char *>(data), length))
    {
        return false;
    }
    
    std::string value;
    if (msg.parse("2", value))
    {
        f.setPortrait(value);
    }
    if (msg.parse("3", value))
    {
        f.setPortraitHD(value);
    }

    return true;
}

bool FriendsParser::parseChatroom(const void *data, int length, Friend& f)
{
    RawMessage msg;
    if (!msg.merge(reinterpret_cast<const char *>(data), length))
    {
        return false;
    }
    
    std::string value;
    if (msg.parse("6", value))
    {
        parseMembers(value, f);
    }

    return true;
}

WechatInfoParser::WechatInfoParser(ITunesDb *iTunesDb) : m_iTunesDb(iTunesDb)
{
}

bool WechatInfoParser::parse(WechatInfo& wechatInfo)
{
    wechatInfo.setOSVersion(m_iTunesDb->getIOSVersion());
    return parsePreferences(wechatInfo);
}

bool WechatInfoParser::parsePreferences(WechatInfo& wechatInfo)
{
    std::string preferencesPath = m_iTunesDb->findRealPath("Library/Preferences/com.tencent.xin.plist");
    if (preferencesPath.empty())
    {
        return false;
    }
    
    std::vector<unsigned char> data;
    if (!readFile(preferencesPath, data))
    {
        return false;
    }
    
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
    if (NULL == node)
    {
        return false;
    }
    
    plist_t startupVersions = plist_access_path(node, 1, "prevStartupVersions");
    if (NULL != startupVersions && PLIST_IS_ARRAY(startupVersions))
    {
        uint32_t startupVersionsSize = plist_array_get_size(startupVersions);
        if (startupVersionsSize > 0)
        {
            plist_t currentVersion = plist_array_get_item(startupVersions, startupVersionsSize - 1);
            if (NULL != currentVersion)
            {
                uint64_t versionLength = 0;
                const char* pVersion = plist_get_string_ptr(currentVersion, &versionLength);
                if (NULL != pVersion && versionLength > 0)
                {
                    std::string version;
                    version.assign(pVersion, versionLength);
                    wechatInfo.setVersion(version);
                }
            }
        }
    }
    
    plist_t cellDataVersion = plist_access_path(node, 1, "FrameCellDataVersion");
    if (NULL != cellDataVersion)
    {
        uint64_t versionLength = 0;
        const char* pVersion = plist_get_string_ptr(cellDataVersion, &versionLength);
        if (NULL != pVersion && versionLength > 0)
        {
            std::string cellDataVersion;
            cellDataVersion.assign(pVersion, versionLength);
            wechatInfo.setCellDataVersion(cellDataVersion);
        }
    }
    
    plist_free(node);
    
    return true;
}

SessionsParser::SessionsParser(ITunesDb *iTunesDb, ITunesDb *iTunesDbShare, Shell* shell, const std::string& cellDataVersion, bool detailedInfo/* = true*/) : m_iTunesDb(iTunesDb), m_iTunesDbShare(iTunesDbShare), m_shell(shell), m_cellDataVersion(cellDataVersion), m_detailedInfo(detailedInfo)
{
    if (cellDataVersion.empty())
    {
        m_cellDataVersion = "V";
    }
}

bool SessionsParser::parse(const Friend& user, std::vector<Session>& sessions, const Friends& friends)
{
    std::string usrNameHash = user.getHash();
    std::string userRoot = "Documents/" + usrNameHash;
    std::string sessionDbPath = m_iTunesDb->findRealPath(combinePath(userRoot, "session", "session.db"));
	if (sessionDbPath.empty())
	{
		return false;
	}

    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(sessionDbPath, &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT ConStrRes1,CreateTime,unreadcount,UsrName FROM SessionAbstract";

    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *usrName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (NULL == usrName || Session::isSubscription(usrName))
        {
            continue;
        }
        
        std::vector<Session>::iterator it = sessions.emplace(sessions.cend(), &user);
        Session& session = (*it);
        
        session.setUsrName(usrName);
        session.setCreateTime(static_cast<unsigned int>(sqlite3_column_int(stmt, 1)));
        const char* extFileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != extFileName) session.setExtFileName(extFileName);
        session.setUnreadCount(sqlite3_column_int(stmt, 2));

        if (!session.isExtFileNameEmpty())
        {
            parseCellData(userRoot, session);
        }
        const Friend* f = friends.getFriend(session.getHash());
        if (NULL != f)
        {
            if (!session.isChatroom())
            {
                session.update(*f);
            }
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::string shareUserRoot = "share/" + usrNameHash;
    parseSessionsInGroupApp(shareUserRoot, sessions);
    
    // if (m_detailedInfo)
    {
        parseMessageDbs(userRoot, sessions);
    }
    
    
    return true;
}

bool SessionsParser::parseMessageDbs(const std::string& userRoot, std::vector<Session>& sessions)
{
	SessionHashCompare comp;
	std::sort(sessions.begin(), sessions.end(), comp);

    MessageDbFilter filter(userRoot);
    ITunesFileVector dbs = m_iTunesDb->filter(filter);
    
    std::string dbPath = m_iTunesDb->findRealPath(combinePath(userRoot, "DB", "MM.sqlite"));

    std::vector<std::pair<std::string, int>> sessionIds;
    parseMessageDb(dbPath, sessionIds);

	for (typename std::vector<std::pair<std::string, int>>::const_iterator it = sessionIds.cbegin(); it != sessionIds.cend(); ++it)
	{
		std::vector<Session>::iterator itSession = std::lower_bound(sessions.begin(), sessions.end(), it->first, comp);
		if (itSession != sessions.end() && itSession->getHash() == it->first)
		{
			itSession->setDbFile(dbPath);
            itSession->setRecordCount(it->second);
		}
	}

    for (ITunesFilesConstIterator it = dbs.cbegin(); it != dbs.cend(); ++it)
    {
        dbPath = m_iTunesDb->fileIdToRealPath((*it)->fileId);
		sessionIds.clear();
        parseMessageDb(dbPath, sessionIds);

		for (typename std::vector<std::pair<std::string, int>>::const_iterator itId = sessionIds.cbegin(); itId != sessionIds.cend(); ++itId)
		{
			std::vector<Session>::iterator itSession = std::lower_bound(sessions.begin(), sessions.end(), itId->first, comp);
			if (itSession != sessions.end() && itSession->getHash() == itId->first)
			{
				itSession->setDbFile(dbPath);
                itSession->setRecordCount(itId->second);
			}
		}
    }

    return true;
}

bool SessionsParser::parseMessageDb(const std::string& mmPath, std::vector<std::pair<std::string, int>>& sessionIds)
{
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(mmPath, &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";

    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* pName = sqlite3_column_text(stmt, 0);
        if (pName == NULL)
        {
            continue;
        }
        std::string name = reinterpret_cast<const char*>(pName);
        // "^Chat_([0-9a-f]{32})$"
        if (startsWith(name, "Chat_"))
        {
            std::string chatId = name.substr(5);
            int recordCount = 0;
            std::string sql2 = "SELECT COUNT(*) AS rc FROM " + name;
            sqlite3_stmt* stmt2 = NULL;
            rc = sqlite3_prepare_v2(db, sql2.c_str(), (int)(sql2.size()), &stmt2, NULL);
            if (rc == SQLITE_OK)
            {
                if (sqlite3_step(stmt2) == SQLITE_ROW)
                {
                    recordCount = sqlite3_column_int(stmt2, 0);
                }
                
                sqlite3_finalize(stmt2);
            }
            
            sessionIds.push_back(std::make_pair<>(chatId, recordCount));
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return true;
}

bool SessionsParser::parseCellData(const std::string& userRoot, Session& session)
{
	std::string fileName = session.getExtFileName();
	if (startsWith(fileName, DIR_SEP) || startsWith(fileName, DIR_SEP_R))
	{
		fileName = fileName.substr(1);
	}
    std::string cellDataPath = combinePath(userRoot, fileName);
	fileName = m_iTunesDb->findRealPath(cellDataPath);
	
	if (fileName.empty())
	{
		return false;
	}


	RawMessage msg;
	if (!msg.mergeFile(fileName))
	{
		return false;
	}

	std::string value;
	int value2;
    if (msg.parse("1.1.6", value))
    {
        session.setDisplayName(value);
    }
    if (msg.parse("1.1.4", value))
    {
        if (session.isDisplayNameEmpty())
        {
            session.setDisplayName(value);
        }
    }
	if (msg.parse("1.1.14", value))
	{
		session.setPortrait(value);
	}
	if (msg.parse("1.5", value))
	{
		parseMembers(value, session);
	}
	if (msg.parse("2.7", value2))
	{
		session.setLastMessageTime(static_cast<unsigned int>(value2));
	}
    if (msg.parse("2.2", value2))
    {
        session.setRecordCount(value2);
    }
    
    if (session.isDisplayNameEmpty())
    {
        SessionCellDataFilter filter(cellDataPath, m_cellDataVersion);
        ITunesFileVector items = m_iTunesDb->filter(filter);

        unsigned int lastModifiedTime = 0;
        for (ITunesFilesConstIterator it = items.cbegin(); it != items.cend(); ++it)
        {
            fileName = m_iTunesDb->fileIdToRealPath((*it)->fileId);
            if (fileName.empty())
            {
                continue;
            }
            
            RawMessage msg2;
            if (!msg2.mergeFile(fileName))
            {
                continue;
            }
            std::string displayName;
            std::string msgTime;
            if (msg2.parse("10", value))
            {
                msgTime = value;
            }
            
            if (msg2.parse("7", value))
            {
                displayName = value;
            }
            
            unsigned int modifiedTime = 0;
            if (items.size() > 1)
            {
                modifiedTime = ITunesDb::parseModifiedTime((*it)->blob);
            }
            if (session.isDisplayNameEmpty() || (!displayName.empty() && modifiedTime > lastModifiedTime))
            {
                session.setDisplayName(displayName);
                lastModifiedTime = modifiedTime;
            }
        }
    }

	return true;
}

bool SessionsParser::parseSessionsInGroupApp(const std::string& userRoot, std::vector<Session>& sessions)
{
    std::map<std::string, Session*> sessionMap;
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        sessionMap.insert(sessionMap.cend(), std::make_pair<>(it->getUsrName(), &(*it)));
        // sessionMap[it->getUsrName()] = &(*it);
    }
    
    std::string sessionDataArchivePath = m_iTunesDbShare->findRealPath(combinePath(userRoot, "session", "sessionData.archive"));
    if (!sessionDataArchivePath.empty())
    {
        std::vector<unsigned char> contents;
        if (readFile(sessionDataArchivePath, contents))
        {
            RawMessage msg;
            if (msg.merge(reinterpret_cast<const char *>(&contents[0]), static_cast<int>(contents.size())))
            {
                std::string value;
                if (msg.parse("1", value))
                {
                    uint32_t blockLen = 0;
                    const char * data = value.c_str();
                    const char * data1 = NULL;
                    while ((data1 = calcVarint32Ptr(data, value.c_str() + value.size(), &blockLen)) != NULL)
                    {
                        RawMessage msg2;
                        if (msg2.merge(data1, static_cast<int>(blockLen)))
                        {
                            std::string value2;
                            if (msg2.parse("1", value2))
                            {
                                std::map<std::string, Session*>::iterator it = sessionMap.find(value2);
                                if (it != sessionMap.end())
                                {
                                    std::string name;
                                    if (msg2.parse("2", value2))
                                    {
                                        name = value2;
                                    }
                                    if (msg2.parse("3", value2))
                                    {
                                        it->second->setDisplayName(value2.empty() ? name : value2);
                                    }
                                }
                            }
                        }
            
                        data = data1 + blockLen;
                    }
                }
            }
        }
    }
    
    std::string extraSessionDataArchivePath = m_iTunesDbShare->findRealPath(combinePath(userRoot, "session", "extraSessionExtraSessionData.archive"));
    if (!extraSessionDataArchivePath.empty())
    {
        std::vector<unsigned char> contents;
        if (readFile(extraSessionDataArchivePath, contents))
        {
            RawMessage msg;
            if (msg.merge(reinterpret_cast<const char *>(&contents[0]), static_cast<int>(contents.size())))
            {
                std::string value;
                if (msg.parse("1", value))
                {
                    uint32_t blockLen = 0;
                    const char * data = value.c_str();
                    const char * data1 = NULL;
                    while ((data1 = calcVarint32Ptr(data, value.c_str() + value.size(), &blockLen)) != NULL)
                    {
                        RawMessage msg2;
                        if (msg2.merge(data1, static_cast<int>(blockLen)))
                        {
                            std::string value2;
                            if (msg2.parse("1", value2))
                            {
                                std::map<std::string, Session*>::iterator it = sessionMap.find(value2);
                                if (it != sessionMap.end())
                                {
                                    std::string name;
                                    if (msg2.parse("2", value2))
                                    {
                                        name = value;
                                    }
                                    if (msg2.parse("3", value2))
                                    {
                                        // it->second->setDisplayName(value2.empty() ? name : value2);
                                    }
                                }
                            }
                        }
            
                        data = data1 + blockLen;
                    }
                }
            }
        }
    }
    
    // copy avatar
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        std::string avatarPath = m_iTunesDbShare->findRealPath(combinePath(userRoot, "session", "headImg", it->getHash() + ".pic"));
        if (!avatarPath.empty())
        {
            it->setPortrait("file://" + avatarPath);
        }
    }

    return true;
}

SessionParser::SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, int options, Downloader& downloader, std::function<std::string(const std::string&)> localeFunc) : m_options(options), m_myself(myself), m_friends(friends), m_iTunesDb(iTunesDb), m_shell(shell), m_downloader(downloader)
{
    m_localFunction = std::move(localeFunc);
}

int SessionParser::parse(const std::string& userBase, const std::string& outputBase, const Session& session, std::function<bool(const std::vector<TemplateValues>&)> handler)
{
    int count = 0;
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(session.getDbFile(), &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT CreateTime,Message,Des,Type,MesLocalID FROM Chat_" + session.getHash() + " ORDER BY CreateTime";
    if ((m_options & SPO_DESC) == SPO_DESC)
    {
        sql += " DESC";
    }
    
    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }

    std::vector<TemplateValues> tvs;
    MsgRecord record;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        tvs.clear();
        
        record.createTime = sqlite3_column_int(stmt, 0);
        const unsigned char* pMessage = sqlite3_column_text(stmt, 1);
        
        record.message = pMessage != NULL ? reinterpret_cast<const char*>(pMessage) : "";
        record.des = sqlite3_column_int(stmt, 2);
        record.type = sqlite3_column_int(stmt, 3);
        record.msgId = sqlite3_column_int(stmt, 4);
        if (parseRow(record, userBase, outputBase, session, tvs))
        {
            count++;
            if (handler(tvs))
            {
                // cancelled
                break;
            }
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return count;
}

bool SessionParser::parseRow(MsgRecord& record, const std::string& userBase, const std::string& outputPath, const Session& session, std::vector<TemplateValues>& tvs)
{
    TemplateValues& templateValues = *(tvs.emplace(tvs.end(), "msg"));
    
	std::string msgIdStr = std::to_string(record.msgId);
    std::string assetsDir = combinePath(outputPath, session.getOutputFileName() + "_files");
    
    templateValues["%%MSGID%%"] = std::to_string(record.msgId);
	templateValues["%%NAME%%"] = "";
	templateValues["%%TIME%%"] = fromUnixTime(record.createTime);
	templateValues["%%MESSAGE%%"] = "";
    
    std::string forwardedMsg;
    std::string forwardedMsgTitle;

    std::string senderId = "";
    if (session.isChatroom())
    {
        if (record.des != 0)
        {
            std::string::size_type enter = record.message.find(":\n");
            if (enter != std::string::npos && enter + 2 < record.message.size())
            {
                senderId = record.message.substr(0, enter);
                record.message = record.message.substr(enter + 2);
            }
        }
    }

#ifndef NDEBUG
    writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + ".txt"), record.message);
#endif
    
    /*
     MSGTYPE_TEXT: 1,
     MSGTYPE_IMAGE: 3,
     MSGTYPE_VOICE: 34,
     MSGTYPE_VIDEO: 43,
     MSGTYPE_MICROVIDEO: 62,
     MSGTYPE_EMOTICON: 47,
     MSGTYPE_APP: 49,
     MSGTYPE_VOIPMSG: 50,
     MSGTYPE_VOIPNOTIFY: 52,
     MSGTYPE_VOIPINVITE: 53,
     MSGTYPE_LOCATION: 48,
     MSGTYPE_STATUSNOTIFY: 51,
     MSGTYPE_SYSNOTICE: 9999,
     MSGTYPE_POSSIBLEFRIEND_MSG: 40,
     MSGTYPE_VERIFYMSG: 37,
     MSGTYPE_SHARECARD: 42,
     MSGTYPE_SYS: 10000,
     MSGTYPE_RECALLED: 10002,
     */
    if (record.type == 10000 || record.type == 10002 /*recall*/)
    {
        templateValues.setName("system");
        std::string sysMsg = record.message;
        removeHtmlTags(sysMsg);
        templateValues["%%MESSAGE%%"] = sysMsg;
    }
    else if (record.type == 1)
    {
        if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
        {
            templateValues["%%MESSAGE%%"] = safeHTML(record.message);
        }
        else
        {
            templateValues["%%MESSAGE%%"] = record.message;
        }
    }
    else if (record.type == 34)
    {
        std::string audioSrc;
        int voicelen = -1;
        const ITunesFile* audioSrcFile = NULL;
        if ((m_options & SPO_IGNORE_AUDIO) == 0)
        {
            std::string vlenstr;
            XmlParser xmlParser(record.message);
            if (xmlParser.parseAttributeValue("/msg/voicemsg", "voicelength", vlenstr) && !vlenstr.empty())
            {
                voicelen = std::stoi(vlenstr);
            }
            
            audioSrcFile = m_iTunesDb.findITunesFile(combinePath(userBase, "Audio", session.getHash(), msgIdStr + ".aud"));
            if (NULL != audioSrcFile)
            {
                audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
            }
        }
        if (audioSrc.empty())
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = voicelen == -1 ? getLocaleString("[Audio]") : formatString(getLocaleString("[Audio %s]"), getDisplayTime(voicelen).c_str());
        }
        else
        {
            m_pcmData.clear();
            std::string mp3Path = combinePath(assetsDir, msgIdStr + ".mp3");

            silkToPcm(audioSrc, m_pcmData);
            
            ensureDirectoryExisted(assetsDir);
            pcmToMp3(m_pcmData, mp3Path);
            if (audioSrcFile != NULL)
            {
                updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
            }

            templateValues.setName("audio");
            templateValues["%%AUDIOPATH%%"] = session.getOutputFileName() + "_files/" + msgIdStr + ".mp3";
        }
    }
    else if (record.type == 47)
    {
		std::string url;

        // static std::regex pattern47_1("cdnurl ?= ?\"(.*?)\"");
        // xml is quicker than regex
        if ((m_options & SPO_IGNORE_EMOJI) == 0)
        {
            XmlParser xmlParser(record.message);
            if (!xmlParser.parseAttributeValue("/msg/emoji", "cdnurl", url))
            {
                url.clear();
            }
            else
            {
                if (!startsWith(url, "http") && startsWith(url, "https"))
                {
                    if (!xmlParser.parseAttributeValue("/msg/emoji", "thumburl", url))
                    {
                        url.clear();
                    }
                }
            }
        }

		if (startsWith(url, "http") || startsWith(url, "https"))
        {
            std::string localfile = url;
            std::smatch sm2;
            static std::regex pattern47_2("\\/(\\w+?)\\/\\w*$");
            if (std::regex_search(localfile, sm2, pattern47_2))
            {
                localfile = sm2[1];
            }
            else
            {
                static int uniqueFileName = 1000000000;
                localfile = std::to_string(uniqueFileName++);
            }
            
            std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
            localfile = emojiPath + localfile + ".gif";
            ensureDirectoryExisted(outputPath);
            m_downloader.addTask(url, combinePath(outputPath, localfile), record.createTime);
            templateValues.setName("emoji");
            templateValues["%%EMOJIPATH%%"] = localfile;
        }
        else
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = getLocaleString("[Emoji]");
        }
    }
    else if (record.type == 62 || record.type == 43)
    {
        // Video
        std::map<std::string, std::string> attrs = { {"fromusername", ""}, {"cdnthumbwidth", ""}, {"cdnthumbheight", ""} };
        XmlParser xmlParser(record.message);
        if (xmlParser.parseAttributesValue("/msg/videomsg", attrs))
        {
        }
        
        if (senderId.empty())
        {
            senderId = attrs["fromusername"];
        }
        
        std::string vfile = combinePath(userBase, "Video", session.getHash(), msgIdStr);
        parseVideo(outputPath, session.getOutputFileName() + "_files", vfile + ".mp4", msgIdStr + ".mp4", vfile + ".video_thum", msgIdStr + "_thum.jpg", attrs["cdnthumbwidth"], attrs["cdnthumbheight"], templateValues);
    }
    else if (record.type == 50)
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Video/Audio Call]");
    }
    else if (record.type == 64)
    {
        templateValues.setName("notice");

		Json::Reader reader;
		Json::Value root;
		if (reader.parse(record.message, root))
		{
			templateValues["%%MESSAGE%%"] = root["msgContent"].asString();
		}
    }
    else if (record.type == 3)
    {
		std::string vfile = combinePath(userBase, "Img", session.getHash(), msgIdStr);
        parseImage(outputPath, session.getOutputFileName() + "_files", vfile + ".pic", "", msgIdStr + ".jpg", vfile + ".pic_thum", msgIdStr + "_thumb.jpg", templateValues);
    }
    else if (record.type == 48)
    {
        std::map<std::string, std::string> attrs = { {"x", ""}, {"y", ""}, {"label", ""} };
        
        XmlParser xmlParser(record.message);
        if (xmlParser.parseAttributesValue("/msg/location", attrs) && !attrs["x"].empty() && !attrs["y"].empty() && !attrs["label"].empty())
        {
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Location (%s,%s) %s]"), attrs["x"].c_str(), attrs["y"].c_str(), attrs["label"].c_str());
        }
        else
        {
            templateValues["%%MESSAGE%%"] = getLocaleString("[Location]");
        }
        templateValues.setName("msg");
    }
    else if (record.type == 49)
    {
        /*
         "APPMSGTYPE_TEXT": 1,
         "APPMSGTYPE_IMG": 2,
         "APPMSGTYPE_AUDIO": 3,
         "APPMSGTYPE_VIDEO": 4,
         "APPMSGTYPE_URL": 5,
         "APPMSGTYPE_ATTACH": 6,
         "APPMSGTYPE_OPEN": 7,
         "APPMSGTYPE_EMOJI": 8,
         "APPMSGTYPE_VOICE_REMIND": 9,
         "APPMSGTYPE_SCAN_GOOD": 10,
         "APPMSGTYPE_GOOD": 13,
         "APPMSGTYPE_EMOTION": 15,
         "APPMSGTYPE_CARD_TICKET": 16,
         "APPMSGTYPE_REALTIME_SHARE_LOCATION": 17,
         "APPMSGTYPE_TRANSFERS": 2000,
         "APPMSGTYPE_RED_ENVELOPES": 2001,
         "APPMSGTYPE_READER_TYPE": 100001,
         */
        // std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
        // if (xmlParser.parseNodesValue("/msg/appmsg/*", nodes))
        
        XmlParser xmlParser(record.message, true);
        std::string appMsgType;
        if (xmlParser.parseNodeValue("/msg/appmsg/type", appMsgType))
        {
#ifndef NDEBUG
            writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_" + appMsgType + ".txt"), record.message);
#endif
            if (appMsgType == "2001") templateValues["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
            else if (appMsgType == "2000") templateValues["%%MESSAGE%%"] = getLocaleString("[Transfer]");
            else if (appMsgType == "17") templateValues["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
            else if (appMsgType == "6")
            {
                std::string title;
                std::string attachFileExtName;
                xmlParser.parseNodeValue("/msg/appmsg/title", title);
                xmlParser.parseNodeValue("/msg/appmsg/appattach/fileext", attachFileExtName);
                
                std::string attachFileName = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr;
                if (!attachFileExtName.empty())
                {
                    attachFileName += "." + attachFileExtName;
                }
                
                std::string attachOutputFileName = msgIdStr + "_" + title;
                parseFile(outputPath, session.getOutputFileName() + "_files", attachFileName, attachOutputFileName, title, templateValues);
            }
            else if (appMsgType == "19")    // Forwarded Msgs
            {
                std::string title;
                xmlParser.parseNodeValue("/msg/appmsg/title", title);
                xmlParser.parseNodeValue("/msg/appmsg/recorditem", forwardedMsg);
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_" + appMsgType + ".txt"), forwardedMsg);
#endif
                templateValues.setName("msg");
                templateValues["%%MESSAGE%%"] = title;

                forwardedMsgTitle = title;
            }
            else if (appMsgType == "57")
            {
                // Refer Message
                std::string title;
                xmlParser.parseNodeValue("/msg/appmsg/title", title);
                std::map<std::string, std::string> nodes = { {"displayname", ""}, {"content", ""}, {"type", ""}};
                if (xmlParser.parseNodesValue("/msg/appmsg/refermsg/*", nodes))
                {
#ifndef NDEBUG
                    writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_" + appMsgType + "_ref_" + nodes["type"] + " .txt"), nodes["content"]);
#endif
                    templateValues.setName("refermsg");
                    templateValues["%%MESSAGE%%"] = title;
                    templateValues["%%REFERNAME%%"] = nodes["displayname"];
                    if (nodes["type"] == "43")
                    {
                        templateValues["%%REFERMSG%%"] = getLocaleString("[Video]");
                    }
                    else if (nodes["type"] == "1")
                    {
                        templateValues["%%REFERMSG%%"] = nodes["content"];
                    }
                    else if (nodes["type"] == "3")
                    {
                        templateValues["%%REFERMSG%%"] = getLocaleString("[Photo]");
                    }
                    else if (nodes["type"] == "49")
                    {
                        // APPMSG
                        XmlParser subAppMsgXmlParser(nodes["content"], true);
                        std::string subAppMsgTitle;
                        subAppMsgXmlParser.parseNodeValue("/msg/appmsg/title", subAppMsgTitle);
                        templateValues["%%REFERMSG%%"] = subAppMsgTitle;
                    }
                    else
                    {
                        templateValues["%%REFERMSG%%"] = nodes["content"];
                    }
                }
                else
                {
                    templateValues.setName("msg");
                    templateValues["%%MESSAGE%%"] = title;
                }
            }
            else if (appMsgType == "50")
            {
                // Channel Card
                std::map<std::string, std::string> nodes = { {"username", ""}, {"avatar", ""}, {"nickname", ""}};
                xmlParser.parseNodesValue("/msg/appmsg/findernamecard/*", nodes);
                
                std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
                
                parseChannelCard(outputPath, portraitDir, nodes["username"], nodes["avatar"], nodes["nickname"], templateValues);
            }
            else if (appMsgType == "51")
            {
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_" + appMsgType + "_" + msgIdStr + ".txt"), record.message);
#endif
                // Channels SHI PIN HAO
                std::map<std::string, std::string> nodes = { {"objectId", ""}, {"nickname", ""}, {"avatar", ""}, {"desc", ""}, {"mediaCount", ""}, {"feedType", ""}, {"desc", ""}, {"username", ""}};
                xmlParser.parseNodesValue("/msg/appmsg/finderFeed/*", nodes);
                std::map<std::string, std::string> videoNodes = { {"mediaType", ""}, {"url", ""}, {"thumbUrl", ""}, {"coverUrl", ""}, {"videoPlayDuration", ""}};
                xmlParser.parseNodesValue("/msg/appmsg/finderFeed/mediaList/media/*", videoNodes);
                std::string thumbUrl;
                if ((m_options & SPO_IGNORE_SHARING) == 0)
                {
                    thumbUrl = videoNodes["thumbUrl"].empty() ? videoNodes["coverUrl"] : videoNodes["thumbUrl"];
                }
                
                std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
                
                templateValues["%%CARDNAME%%"] = nodes["nickname"];
                templateValues["%%CHANNELS%%"] = getLocaleString("Channels");
                templateValues["%%MESSAGE%%"] = nodes["desc"];
                templateValues["%%EXTRA_CLS%%"] = "channels";
                
                if (!thumbUrl.empty())
                {
                    templateValues.setName("channels");
                    
                    std::string thumbFile = session.getOutputFileName() + "_files/" + msgIdStr + ".jpg";
                    templateValues["%%CHANNELTHUMBPATH%%"] = thumbFile;
                    ensureDirectoryExisted(combinePath(outputPath, session.getOutputFileName() + "_files"));
                    m_downloader.addTask(thumbUrl, combinePath(outputPath, thumbFile), 0);
                    
                    if (!nodes["avatar"].empty())
                    {
                        templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + nodes["username"] + ".jpg";
                        std::string localfile = combinePath(portraitDir, nodes["username"] + ".jpg");
                        ensureDirectoryExisted(portraitDir);
                        m_downloader.addTask(nodes["avatar"], combinePath(outputPath, localfile), 0);
                    }

                    templateValues["%%CHANNELURL%%"] = videoNodes["url"];
                }
            }
            else
            {
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_unknwn_" + appMsgType + ".txt"), record.message);
#endif
                std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
                xmlParser.parseNodesValue("/msg/appmsg/*", nodes);
                
                if (!nodes["title"].empty() && !nodes["url"].empty())
                {
                    templateValues.setName(nodes["thumburl"].empty() ? "plainshare" : "share");

                    templateValues["%%SHARINGIMGPATH%%"] = nodes["thumburl"];
                    templateValues["%%SHARINGURL%%"] = nodes["url"];
                    templateValues["%%SHARINGTITLE%%"] = nodes["title"];
                    templateValues["%%MESSAGE%%"] = nodes["des"];
                }
                else if (!nodes["title"].empty())
                {
                    templateValues["%%MESSAGE%%"] = nodes["title"];
                }
                else
                {
                    templateValues["%%MESSAGE%%"] = getLocaleString("[Link]");
                }
            }
        }
        else
        {
            // Failed to parse APPMSG type
#ifndef NDEBUG
            writeFile(combinePath(outputPath, "../dbg", "msg" + std::to_string(record.type) + "_app_invld_" + msgIdStr + ".txt"), record.message);
#endif
            templateValues["%%MESSAGE%%"] = getLocaleString("[Link]");
        }
    }
    else if (record.type == 42)
    {
        std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
        parseCard(outputPath, portraitDir, record.message, templateValues);
    }
    else
    {
#ifndef NDEBUG
        writeFile(combinePath(outputPath, "../dbg", "msg_unknwn_type_" + std::to_string(record.type) + msgIdStr + ".txt"), record.message);
#endif
        if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
        {
            templateValues["%%MESSAGE%%"] = safeHTML(record.message);
        }
        else
        {
            templateValues["%%MESSAGE%%"] = record.message;
        }
    }
    
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Emoji/" : "Emoji/";
    
    std::string localPortrait;
    std::string remotePortrait;
    if (session.isChatroom())
    {
        if (record.des == 0)
        {
            templateValues["%%ALIGNMENT%%"] = "right";
            templateValues["%%NAME%%"] = m_myself.getDisplayName();    // Don't show name for self
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            templateValues["%%AVATAR%%"] = localPortrait;
            remotePortrait = m_myself.getPortrait();
        }
        else
        {
            templateValues["%%ALIGNMENT%%"] = "left";
            if (!senderId.empty())
            {
                std::string txtsender = session.getMemberName(md5(senderId));
                const Friend *f = m_friends.getFriendByUid(senderId);
                if (txtsender.empty() && NULL != f)
                {
                    txtsender = f->getDisplayName();
                }
                templateValues["%%NAME%%"] = txtsender.empty() ? senderId : txtsender;
                localPortrait = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
                remotePortrait = (NULL != f) ? f->getPortrait() : "";
                templateValues["%%AVATAR%%"] = localPortrait;
            }
            else
            {
                templateValues["%%NAME%%"] = senderId;
                templateValues["%%AVATAR%%"] = "";
            }
        }
    }
    else
    {
        if (record.des == 0 || session.getUsrName() == m_myself.getUsrName())
        {
            templateValues["%%ALIGNMENT%%"] = "right";
            templateValues["%%NAME%%"] = m_myself.getDisplayName();
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            remotePortrait = m_myself.getPortrait();
            templateValues["%%AVATAR%%"] = localPortrait;
        }
        else
        {
            templateValues["%%ALIGNMENT%%"] = "left";

            const Friend *f = m_friends.getFriend(session.getHash());
            if (NULL == f)
            {
                templateValues["%%NAME%%"] = session.getDisplayName();
                localPortrait = portraitPath + (session.isPortraitEmpty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
                remotePortrait = session.getPortrait();
                templateValues["%%AVATAR%%"] = localPortrait;
            }
            else
            {
                templateValues["%%NAME%%"] = f->getDisplayName();
                localPortrait = portraitPath + f->getLocalPortrait();
                remotePortrait = f->getPortrait();
                templateValues["%%AVATAR%%"] = localPortrait;
            }
        }
    }

    if ((m_options & SPO_IGNORE_AVATAR) == 0)
    {
        if (!remotePortrait.empty() && !localPortrait.empty())
        {
            
            m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
        }
    }
    
    if ((m_options & SPO_IGNORE_HTML_ENC) == 0)
    {
        templateValues["%%NAME%%"] = safeHTML(templateValues["%%NAME%%"]);
    }

    if (!forwardedMsg.empty())
    {
        // This funtion will change tvs and causes templateValues invalid, so we do it at last
        parseForwardedMsgs(userBase, outputPath, session, record, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return true;
}

void SessionParser::parseVideo(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& srcVideo, const std::string& destVideo, const std::string& srcThumb, const std::string& destThumb, const std::string& width, const std::string& height, TemplateValues& templateValues)
{
    bool hasThumb = false;
    bool hasVideo = false;
    
    if ((m_options & SPO_IGNORE_VIDEO) == 0)
    {
        std::string fullAssertsPath = combinePath(sessionPath, sessionAssertsPath);
        ensureDirectoryExisted(fullAssertsPath);
        hasThumb = requireFile(srcThumb, combinePath(fullAssertsPath, destThumb));
        hasVideo = requireFile(srcVideo, combinePath(fullAssertsPath, destVideo));
    }

    if (hasVideo)
    {
        templateValues.setName("video");
        templateValues["%%THUMBPATH%%"] = hasThumb ? (sessionAssertsPath + "/" + destThumb) : "";
        templateValues["%%VIDEOPATH%%"] = sessionAssertsPath + "/" + destVideo;
        
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        templateValues["%%MESSAGE%%"] = getLocaleString("(Video Missed)");
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Video]");
    }
    
    templateValues["%%VIDEOWIDTH%%"] = width;
    templateValues["%%VIDEOHEIGHT%%"] = height;
}

void SessionParser::parseImage(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues)
{
    bool hasThumb = false;
    bool hasImage = false;
    if ((m_options & SPO_IGNORE_VIDEO) == 0)
    {
        hasThumb = requireFile(srcThumb, combinePath(sessionPath, sessionAssertsPath, destThumb));
        if (!srcPre.empty())
        {
            hasImage = requireFile(srcPre, combinePath(sessionPath, sessionAssertsPath, dest));
        }
        if (!hasImage)
        {
            hasImage = requireFile(src, combinePath(sessionPath, sessionAssertsPath, dest));
        }
    }

    if (hasImage)
    {
        templateValues.setName("image");
        templateValues["%%IMGPATH%%"] = sessionAssertsPath + "/" + dest;
        templateValues["%%IMGTHUMBPATH%%"] = hasThumb ? (sessionAssertsPath + "/" + destThumb) : (sessionAssertsPath + "/" + dest);
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = sessionAssertsPath + "/" + destThumb;
        templateValues["%%MESSAGE%%"] = "";
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Photo]");
    }
}

void SessionParser::parseFile(const std::string& sessionPath, const std::string& sessionAssertsPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& templateValues)
{
    bool hasFile = false;
    if ((m_options & SPO_IGNORE_FILE) == 0)
    {
        hasFile = requireFile(src, combinePath(sessionPath, sessionAssertsPath, dest));
    }

    if (hasFile)
    {
        templateValues.setName("plainshare");
        templateValues["%%SHARINGURL%%"] = sessionAssertsPath + "/" + dest;
        templateValues["%%SHARINGTITLE%%"] = fileName;
        templateValues["%%MESSAGE%%"] = "";
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[File: %s]"), fileName.c_str());
    }
}

void SessionParser::parseCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& cardMessage, TemplateValues& templateValues)
{
    // static std::regex pattern42_1("nickname ?= ?\"(.+?)\"");
    // static std::regex pattern42_2("smallheadimgurl ?= ?\"(.+?)\"");
    std::map<std::string, std::string> attrs;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        attrs = { {"nickname", ""}, {"smallheadimgurl", ""}, {"bigheadimgurl", ""}, {"username", ""} };
    }
    else
    {
        attrs = { {"nickname", ""}, {"username", ""} };
    }

    templateValues["%%CARDTYPE%%"] = getLocaleString("[Contact Card]");
    XmlParser xmlParser(cardMessage, true);
    if (xmlParser.parseAttributesValue("/msg", attrs) && !attrs["nickname"].empty())
    {
        std::string portraitUrl = attrs["bigheadimgurl"].empty() ? attrs["smallheadimgurl"] : attrs["bigheadimgurl"];
        if (!attrs["username"].empty() && !portraitUrl.empty())
        {
            templateValues.setName("card");
            templateValues["%%CARDNAME%%"] = attrs["nickname"];
            templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + attrs["username"] + ".jpg";
            std::string localfile = combinePath(portraitDir, attrs["username"] + ".jpg");
            ensureDirectoryExisted(portraitDir);
            m_downloader.addTask(portraitUrl, combinePath(sessionPath, localfile), 0);
        }
        else
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Contact Card] %s"), attrs["username"].c_str());
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
    }
    templateValues["%%EXTRA_CLS%%"] = "contact-card";
}

void SessionParser::parseChannelCard(const std::string& sessionPath, const std::string& portraitDir, const std::string& usrName, const std::string& avatar, const std::string& name, TemplateValues& templateValues)
{
    bool hasImg = false;
    if ((m_options & SPO_IGNORE_SHARING) == 0)
    {
        hasImg = (!usrName.empty() && !avatar.empty());
    }
    templateValues["%%CARDTYPE%%"] = getLocaleString("[Channel Card]");
    if (!name.empty())
    {
        if (hasImg)
        {
            templateValues.setName("card");
            templateValues["%%CARDNAME%%"] = name;
            templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + usrName + ".jpg";
            std::string localfile = combinePath(portraitDir, usrName + ".jpg");
            ensureDirectoryExisted(portraitDir);
            m_downloader.addTask(avatar, combinePath(sessionPath, localfile), 0);
        }
        else
        {
            templateValues.setName("msg");
            templateValues["%%MESSAGE%%"] = formatString(getLocaleString("[Channel Card] %s"), name.c_str());
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = getLocaleString("[Channel Card]");
    }
    templateValues["%%EXTRA_CLS%%"] = "channel-card";
}

class ForwardMsgsHandler
{
private:
    XmlParser& m_xmlParser;
    int m_msgId;
    std::vector<ForwardMsg>& m_forwardedMsgs;
public:

    ForwardMsgsHandler(XmlParser& xmlParser, int msgId, std::vector<ForwardMsg>& forwardedMsgs) : m_xmlParser(xmlParser), m_msgId(msgId), m_forwardedMsgs(forwardedMsgs)
    {
    }
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        for (int idx = 0; idx < xpathNodes->nodeNr; ++idx)
        {
            ForwardMsg fmsg = {m_msgId};
            
            // templateValues.setName("msg");
            // templateValues["%%ALIGNMENT%%"] = "left";
            
            xmlNode *cur = xpathNodes->nodeTab[idx];
            
            XmlParser::getNodeAttributeValue(cur, "datatype", fmsg.dataType);
            XmlParser::getNodeAttributeValue(cur, "dataid", fmsg.dataId);
            XmlParser::getNodeAttributeValue(cur, "subtype", fmsg.subType);
            
            xmlNodePtr childNode = xmlFirstElementChild(cur);
            bool hasDataTitle = false;
            while (NULL != childNode)
            {
                if (xmlStrcmp(childNode->name, BAD_CAST("sourcename")) == 0)
                {
                    fmsg.displayName = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("sourcetime")) == 0)
                {
                    fmsg.msgTime = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datadesc")) == 0)
                {
                    if (!hasDataTitle)
                    {
                        fmsg.message = XmlParser::getNodeInnerText(childNode);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("dataitemsource")) == 0)
                {
                    if (!XmlParser::getChildNodeContent(childNode, "realchatname", fmsg.usrName))
                    {
                        XmlParser::getChildNodeContent(childNode, "fromusr", fmsg.usrName);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("srcMsgCreateTime")) == 0)
                {
                    fmsg.srcMsgTime = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datafmt")) == 0)
                {
                    fmsg.dataFormat = XmlParser::getNodeInnerText(childNode);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("weburlitem")) == 0)
                {
                    XmlParser::getChildNodeContent(childNode, "title", fmsg.message);
                    XmlParser::getChildNodeContent(childNode, "link", fmsg.link);
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("datatitle")) == 0)
                {
                    fmsg.message = XmlParser::getNodeInnerText(childNode);
                    hasDataTitle = true;
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("recordxml")) == 0)
                {
                    xmlNodePtr nodeRecordInfo = XmlParser::getChildNode(childNode, "recordinfo");
                    if (NULL != nodeRecordInfo)
                    {
                        fmsg.nestedMsgs = XmlParser::getNodeOuterXml(nodeRecordInfo);
                    }
                }
                else if (xmlStrcmp(childNode->name, BAD_CAST("locitem")) == 0)
                {
                    fmsg.nestedMsgs = XmlParser::getNodeOuterXml(childNode);
                }

                childNode = childNode->next;
            }
            
            m_forwardedMsgs.push_back(fmsg);
        }
        return true;
    }
};

bool SessionParser::parseForwardedMsgs(const std::string& userBase, const std::string& outputPath, const Session& session, const MsgRecord& record, const std::string& title, const std::string& message, std::vector<TemplateValues>& tvs)
{
    XmlParser xmlParser(message);
    std::vector<ForwardMsg> forwardedMsgs;
    ForwardMsgsHandler handler(xmlParser, record.msgId, forwardedMsgs);
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait/" : "Portrait/";
    std::string localPortrait;
    std::string remotePortrait;
    
    std::string msgIdStr = std::to_string(record.msgId);
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(getLocaleString("<< %s"), title.c_str());
    beginTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg

    if (xmlParser.parseWithHandler("/recordinfo/datalist/dataitem", handler))
    {
        for (std::vector<ForwardMsg>::const_iterator it = forwardedMsgs.begin(); it != forwardedMsgs.end(); ++it)
        {
            TemplateValues& tv = *(tvs.emplace(tvs.end(), "msg"));
            tv["%%ALIGNMENT%%"] = "left";
            tv["%%EXTRA_CLS%%"] = "fmsg";   // forwarded msg
            // 1: message
            // 2: image
            // 4: video
            // 5: link
            // 6: location
            // 8: File
            // 16: Card
            // 17: Nested Forwarded Messages
            // 19: mini program
            // 22: Channels
             
            if (it->dataType == "1")
            {
                tv["%%MESSAGE%%"] = replaceAll(replaceAll(replaceAll(it->message, "\r\n", "<br />"), "\r", "<br />"), "\n", "<br />");
            }
            else if (it->dataType == "2")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseImage(outputPath, session.getOutputFileName() + "_files/" + msgIdStr, vfile + fileExtName, vfile + fileExtName + "_pre3", it->dataId + ".jpg", vfile + ".record_thumb", it->dataId + "_thumb.jpg", tv);
            }
            else if (it->dataType == "3")
            {
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "4")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseVideo(outputPath, session.getOutputFileName() + "_files/" + msgIdStr, vfile + fileExtName, it->dataId + fileExtName, vfile + ".record_thumb", it->dataId + "_thumb.jpg", "", "", tv);
                
            }
            else if (it->dataType == "5")
            {
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId + ".record_thumb";
                std::string dest = session.getOutputFileName() + "_files/" + msgIdStr + "/" + it->dataId + "_thumb.jpg";
                bool hasThumb = false;
                if ((m_options & SPO_IGNORE_SHARING) == 0)
                {
                    hasThumb = requireFile(vfile, combinePath(outputPath, dest));
                }
                
                if (!(it->link.empty()))
                {
                    tv.setName(hasThumb ? "share" : "plainshare");

                    tv["%%SHARINGIMGPATH%%"] = dest;
                    tv["%%SHARINGURL%%"] = it->link;
                    tv["%%SHARINGTITLE%%"] = it->message;
                    // tv["%%MESSAGE%%"] = nodes["des"];
                }
                else
                {
                    tv["%%MESSAGE%%"] = it->message;
                }
            }
            else if (it->dataType == "6")
            {
                // Location
                std::map<std::string, std::string> attrs = { {"poiname", ""}, {"lng", ""}, {"lat", ""}, {"label", ""} };
                
                XmlParser xmlParser(it->nestedMsgs);
                if (xmlParser.parseNodesValue("/locitem/*", attrs) && !attrs["lat"].empty() && !attrs["lng"].empty() && !attrs["poiname"].empty())
                {
                    tv["%%MESSAGE%%"] = formatString(getLocaleString("[Location (%s,%s) %s]"), attrs["lat"].c_str(), attrs["lng"].c_str(), attrs["poiname"].c_str());
                }
                else
                {
                    tv["%%MESSAGE%%"] = getLocaleString("[Location]");
                }
                tv.setName("msg");
            }
            else if (it->dataType == "8")
            {
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseFile(outputPath, session.getOutputFileName() + "_files/" + msgIdStr, vfile + fileExtName, it->dataId + fileExtName, it->message, tv);
            }
            else if (it->dataType == "16")
            {
                // Card
                std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getOutputFileName() + "_files/Portrait" : "Portrait";
                parseCard(outputPath, portraitDir, it->message, tv);
            }
            else if (it->dataType == "17")
            {
                // parseForwardedMsgs(userBase, outputPath, session, record, title, it->message, tvs);
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "19")
            {
                // Mini Program
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "22")
            {
                // Channels
                
                tv["%%MESSAGE%%"] = it->message;
            }
            else
            {
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "../dbg", "fwdmsg_unknwn_" + it->dataType + ".txt"), it->message);
#endif
                tv["%%MESSAGE%%"] = it->message;
            }
            
            tv["%%NAME%%"] = it->displayName;
            tv["%%MSGID%%"] = msgIdStr + "_" + it->dataId;
            tv["%%TIME%%"] = it->srcMsgTime.empty() ? it->msgTime : fromUnixTime(static_cast<unsigned int>(std::atoi(it->srcMsgTime.c_str())));
            
            localPortrait = portraitPath + (it->protrait.empty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
            remotePortrait = it->protrait;
            tv["%%AVATAR%%"] = localPortrait;
            if (!it->usrName.empty() && it->protrait.empty())
            {
                const Friend *f = (m_myself.getUsrName() == it->usrName) ? &m_myself : m_friends.getFriendByUid(it->usrName);
                std::string localPortrait = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
                remotePortrait = (NULL != f) ? f->getPortrait() : "";
                
                tv["%%AVATAR%%"] = localPortrait;
                
                if ((m_options & SPO_IGNORE_AVATAR) == 0)
                {
                    if (!remotePortrait.empty() && !localPortrait.empty())
                    {
                        m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
                    }
                }
            }
            
            if ((it->dataType == "17") && !it->nestedMsgs.empty())
            {
                parseForwardedMsgs(userBase, outputPath, session, record, it->message, it->nestedMsgs, tvs);
            }
        }
        
    }
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& endTv = tvs.back();
    endTv["%%MESSAGE%%"] = formatString(getLocaleString("%s Ends >>"), title.c_str());
    endTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    return true;
}

bool SessionParser::requireFile(const std::string& vpath, const std::string& dest) const
{
#ifndef NDEBUG
    // Make debug more effective
    if (m_shell.existsFile(normalizePath(dest)))
    {
        return true;
    }
#endif
    
    const ITunesFile* file = m_iTunesDb.findITunesFile(vpath);
    if (NULL != file)
    {
        std::string srcPath = m_iTunesDb.getRealPath(*file);
        if (!srcPath.empty())
        {
            normalizePath(srcPath);
            std::string destPath = normalizePath(dest);
            bool result = m_shell.copyFile(srcPath, destPath, true);
            if (result)
            {
                updateFileTime(dest, ITunesDb::parseModifiedTime(file->blob));
            }
            return result;
        }
    }
    
    return false;
}

std::string SessionParser::getDisplayTime(int ms) const
{
    if (ms < 1000) return "1\"";
    return std::to_string(std::round((double)ms)) + "\"";
}

void SessionParser::ensureDirectoryExisted(const std::string& path)
{
    if (m_shell.existsDirectory(path))
    {
        m_shell.makeDirectory(path);
    }
}
