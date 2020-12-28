//
//  WechatParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "WechatParser.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cmath>
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
    while(1)
    {
        int res = parseUser(value1.c_str() + offset, static_cast<int>(length - offset), users);
        if (res < 0)
        {
            break;
        }
        
        offset += res;
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
    
    return static_cast<int>(userBufferLen + (p - data));
}

MMKVParser::MMKVParser(const std::string& path)
{
    if (!readFile(path, m_contents))
    {
        m_contents.clear();
    }
}

std::string MMKVParser::findValue(const std::string& key)
{
    std::string value;

    ByteArrayLocater locator;
    int keyLength = static_cast<int>(key.length());
    const unsigned char* data = &m_contents[0];
    int length = static_cast<int>(m_contents.size());
    
    std::vector<int> positions = locator.locate(data, length, reinterpret_cast<const unsigned char*>(key.c_str()), keyLength);

    if (!positions.empty())
    {
        const unsigned char* limit = data + length;
        for (std::vector<int>::const_reverse_iterator it = positions.crbegin(); it != positions.crend(); ++it)
        {
            uint32_t valueLength = 0;
            const unsigned char* p1 = data + (*it) + keyLength;
            const unsigned char* p2 = calcVarint32Ptr(p1, limit, &valueLength);
            if (NULL != p2 && valueLength > 0)
            {
                //MBBuffer
                p2 = calcVarint32Ptr(p2, limit, &valueLength);
                if (NULL != p2 && valueLength > 0)
                {
                    // NSString
                    if (p2 + valueLength < limit)
                    {
                        value.reserve(valueLength);
                        for (uint32_t i = 0; i < valueLength; i++)
                        {
                            value.push_back((reinterpret_cast<const char*>(p2))[i]);
                        }
                        break;
                    }
                }
            }
        }
    }

    return value;
}

MMSettingParser::MMSettingParser(ITunesDb *iTunesDb) : m_iTunesDb(iTunesDb)
{
}

bool MMSettingParser::parse(const std::string& usrNameHash)
{
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
    
    plist_t objectsNode = plist_access_path(node, 1, "$objects");
    if (NULL != objectsNode && PLIST_IS_ARRAY(objectsNode))
    {
        uint32_t objectsNodeSize = plist_array_get_size(objectsNode);
        for (uint32_t idx = 0; idx < objectsNodeSize; ++idx)
        {
            plist_t item = plist_array_get_item(objectsNode, idx);
            if (NULL == item || !PLIST_IS_STRING(item))
            {
                continue;
            }
            
            uint64_t valueLength = 0;
            const char* pValue = plist_get_string_ptr(item, &valueLength);
            if (NULL != pValue && valueLength > 0)
            {
                std::string value;
                value.assign(pValue, valueLength);
                if (startsWith(value, "https://wx.qlogo.cn/mmhead/") || startsWith(value, "http://wx.qlogo.cn/mmhead/"))
                {
                    // Avatar
                    if (endsWith(value, "/0"))
                    {
                        m_portraitHD = value;
                    }
                    else // (endsWith(value, "/132"))
                    {
                        m_portrait = value;
                    }
                }
            }
        }
    }
    
    plist_free(node);
    return true;
}

std::string MMSettingParser::getUsrName() const
{
    return m_usrName;
}

std::string MMSettingParser::getDisplayName() const
{
    return m_displayName;
}
std::string MMSettingParser::getPortrait() const
{
    return m_portrait;
}
std::string MMSettingParser::getPortraitHD() const
{
    return m_portraitHD;
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

SessionsParser::SessionsParser(ITunesDb *iTunesDb, Shell* shell, const std::string& cellDataVersion, bool detailedInfo/* = true*/) : m_iTunesDb(iTunesDb), m_shell(shell), m_cellDataVersion(cellDataVersion), m_detailedInfo(detailedInfo)
{
    if (cellDataVersion.empty())
    {
        m_cellDataVersion = "V";
    }
}

bool SessionsParser::parse(const std::string& userRoot, std::vector<Session>& sessions, const Friends& friends)
{
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
        sessions.push_back(Session());
        Session& session = sessions.back();
        
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

    if (m_detailedInfo)
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

	std::vector<std::string> sessionIds;
    parseMessageDb(dbPath, sessionIds);

	for (typename std::vector<std::string>::const_iterator it = sessionIds.cbegin(); it != sessionIds.cend(); ++it)
	{
		std::vector<Session>::iterator itSession = std::lower_bound(sessions.begin(), sessions.end(), *it, comp);
		if (itSession != sessions.end() && itSession->getHash() == *it)
		{
			itSession->setDbFile(dbPath);
		}
	}

    for (ITunesFilesConstIterator it = dbs.cbegin(); it != dbs.cend(); ++it)
    {
        dbPath = m_iTunesDb->fileIdToRealPath((*it)->fileId);
		sessionIds.clear();
        parseMessageDb(dbPath, sessionIds);

		for (typename std::vector<std::string>::const_iterator itId = sessionIds.cbegin(); itId != sessionIds.cend(); ++itId)
		{
			std::vector<Session>::iterator itSession = std::lower_bound(sessions.begin(), sessions.end(), *itId, comp);
			if (itSession != sessions.end() && itSession->getHash() == *itId)
			{
				itSession->setDbFile(dbPath);
			}
		}
    }

    return true;
}

bool SessionsParser::parseMessageDb(const std::string& mmPath, std::vector<std::string>& sessionIds)
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
        std::smatch sm;
        static std::regex pattern("^Chat_([0-9a-f]{32})$");
        if (std::regex_search(name, sm, pattern))
        {
            sessionIds.push_back(sm[1]);
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

SessionParser::SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, const std::map<std::string, std::string>& templates, const std::map<std::string, std::string>& localeStrings, int options, Downloader& downloader, std::atomic<bool>& cancelled) : m_templates(templates), m_localeStrings(localeStrings), m_options(options), m_myself(myself), m_friends(friends), m_iTunesDb(iTunesDb), m_shell(shell), m_downloader(downloader), m_cancelled(cancelled)
{
}

int SessionParser::parse(const std::string& userBase, const std::string& outputBase, const Session& session, std::string& contents)
{
    int count = 0;
    contents.clear();
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
        if (m_cancelled)
        {
            break;
        }
        
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
            for (std::vector<TemplateValues>::const_iterator it = tvs.cbegin(); it != tvs.cend(); ++it)
            {
                contents.append(buildContentFromTemplateValues(*it));
            }
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return count;
}

std::string SessionParser::getTemplate(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = m_templates.find(key);
    return (it == m_templates.cend()) ? "" : it->second;
}

bool SessionParser::parseRow(MsgRecord& record, const std::string& userBase, const std::string& outputPath, const Session& session, std::vector<TemplateValues>& tvs)
{
    TemplateValues& templateValues = *(tvs.emplace(tvs.end(), "msg"));
    
	std::string msgIdStr = std::to_string(record.msgId);
    std::string assetsDir = combinePath(outputPath, session.getUsrName() + "_files");
	m_shell.makeDirectory(assetsDir);
    
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
    writeFile(combinePath(outputPath, "msg" + std::to_string(record.type) + ".txt"), record.message);
#endif
    if (record.type == 10000 || record.type == 10002)
    {
        templateValues.setName("system");
        std::string sysMsg = record.message;
        removeHtmlTags(sysMsg);
        templateValues["%%MESSAGE%%"] = sysMsg;
    }
    else if (record.type == 34)
    {
        int voicelen = -1;
        std::string vlenstr;
        XmlParser xmlParser(record.message);
        if (xmlParser.parseAttributeValue("/msg/voicemsg", "voicelength", vlenstr) && !vlenstr.empty())
        {
            voicelen = std::stoi(vlenstr);
        }

        // static std::regex voiceLengthPattern("voicelength=\"(\\d+?)\"");
        // std::smatch sm;
        // if (std::regex_search(record.message, sm, voiceLengthPattern))
        // {
        //     voicelen = std::stoi(sm[1]);
        // }
        const ITunesFile* audioSrcFile = m_iTunesDb.findITunesFile(combinePath(userBase, "Audio", session.getHash(), msgIdStr + ".aud"));
        std::string audioSrc;
        if (NULL != audioSrcFile)
        {
            audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
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
            if ((m_options & SPO_IGNORE_AUDIO) == 0)
            {
                silkToPcm(audioSrc, m_pcmData);
                pcmToMp3(m_pcmData, mp3Path);
                updateFileTime(mp3Path, ITunesDb::parseModifiedTime(audioSrcFile->blob));
            }

            templateValues.setName("audio");
            templateValues["%%AUDIOPATH%%"] = session.getUsrName() + "_files/" + msgIdStr + ".mp3";
        }
    }
    else if (record.type == 47)
    {
		std::string url;

        // static std::regex pattern47_1("cdnurl ?= ?\"(.*?)\"");
        // std::smatch sm;
        // if (std::regex_search(record.message, sm, pattern47_1))
        // {
        //     url = sm[1].str();
        // }
        // xml is quicker than regex
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
            
            std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Emoji/" : "Emoji/";
            localfile = emojiPath + localfile + ".gif";
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
        if (senderId.empty())
        {
            XmlParser xmlParser(record.message);
            if (xmlParser.parseAttributeValue("/msg/videomsg", "fromusername", senderId))
            {
            }
        }
        
        std::string vfile = combinePath(userBase, "Video", session.getHash(), msgIdStr);
        parseVideo(outputPath, vfile + ".mp4", session.getUsrName() + "_files/" + msgIdStr + ".mp4", vfile + ".video_thum", session.getUsrName() + "_files/" + msgIdStr + "_thum.jpg", templateValues);
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
        parseImage(outputPath, vfile + ".pic", "", session.getUsrName() + "_files/" + msgIdStr + ".jpg", vfile + ".pic_thum", session.getUsrName() + "_files/" + msgIdStr + "_thumb.jpg", templateValues);
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
        std::map<std::string, std::string> nodes = { {"title", ""}, {"type", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""}, {"recorditem", ""} };
        XmlParser xmlParser(record.message, true);
        if (xmlParser.parseNodesValue("/msg/appmsg/*", nodes))
        {
            std::string appMsgType = nodes["type"];
            if (appMsgType == "2001") templateValues["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
            else if (appMsgType == "2000") templateValues["%%MESSAGE%%"] = getLocaleString("[Transfer]");
            else if (appMsgType == "17") templateValues["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
            else if (appMsgType == "6") templateValues["%%MESSAGE%%"] = getLocaleString("[File]");
            else if (appMsgType == "19")    // Forwarded Msgs
            {
#ifndef NDEBUG
                writeFile(combinePath(outputPath, "msg" + std::to_string(record.type) + "_19.txt"), nodes["recorditem"]);
#endif
                templateValues.setName("msg");
                templateValues["%%MESSAGE%%"] = nodes["title"];
                
                forwardedMsg = nodes["recorditem"];
                forwardedMsgTitle = nodes["title"];
            }
            else
            {
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
            templateValues["%%MESSAGE%%"] = getLocaleString("[Link]");
        }
    }
    else if (record.type == 42)
    {
        std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Portrait" : "Portrait";
        parseCard(outputPath, portraitDir, record.message, templateValues);
    }
    else
    {
        templateValues["%%MESSAGE%%"] = safeHTML(record.message);
    }
    
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Portrait/" : "Portrait/";
    std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Emoji/" : "Emoji/";
    
    std::string localPortrait;
    std::string remotePortrait;
    if (session.isChatroom())
    {
        if (record.des == 0)
        {
            std::string txtsender = m_myself.getDisplayName();
            
            templateValues["%%ALIGNMENT%%"] = "right";
            // templateValues.Add("%%NAME%%", txtsender);
            templateValues["%%NAME%%"] = "";    // Don't show name for self
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
            templateValues["%%NAME%%"] = "";
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

    if (!remotePortrait.empty() && !localPortrait.empty())
    {
        m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
    }

    if (!forwardedMsg.empty())
    {
        // This funtion will change tvs and causes templateValues invalid, so we do it at last
        parseForwardedMsgs(userBase, outputPath, session, record, forwardedMsgTitle, forwardedMsg, tvs);
    }
    return true;
}

void SessionParser::parseVideo(const std::string& sessionPath, const std::string& srcVideo, const std::string& destVideo, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues)
{
    bool hasThumb = requireFile(srcThumb, combinePath(sessionPath, destThumb));
    bool hasVideo = requireFile(srcVideo, combinePath(sessionPath, destVideo));

    if (hasVideo)
    {
        templateValues.setName("video");
        templateValues["%%THUMBPATH%%"] = hasThumb ? destThumb : "";
        templateValues["%%VIDEOPATH%%"] = destVideo;
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = destThumb;
        templateValues["%%MESSAGE%%"] = getLocaleString("(Video Missed)");
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Video]");
    }
}

void SessionParser::parseImage(const std::string& sessionPath, const std::string& src, const std::string& srcPre, const std::string& dest, const std::string& srcThumb, const std::string& destThumb, TemplateValues& templateValues)
{
    bool hasThumb = requireFile(srcThumb, combinePath(sessionPath, destThumb));
    bool hasImage = false;
    if (!srcPre.empty())
    {
        hasImage = requireFile(srcPre, combinePath(sessionPath, dest));
    }
    if (!hasImage)
    {
        hasImage = requireFile(src, combinePath(sessionPath, dest));
    }

    if (hasImage)
    {
        templateValues.setName("image");
        templateValues["%%IMGPATH%%"] = dest;
        templateValues["%%IMGTHUMBPATH%%"] = hasThumb ? destThumb : dest;
    }
    else if (hasThumb)
    {
        templateValues.setName("thumb");
        templateValues["%%IMGTHUMBPATH%%"] = destThumb;
        templateValues["%%MESSAGE%%"] = "";
    }
    else
    {
        templateValues.setName("msg");
        templateValues["%%MESSAGE%%"] = getLocaleString("[Picture]");
    }
}

void SessionParser::parseFile(const std::string& sessionPath, const std::string& src, const std::string& dest, const std::string& fileName, TemplateValues& templateValues)
{
    bool hasFile = requireFile(src, combinePath(sessionPath, dest));

    if (hasFile)
    {
        templateValues.setName("plainshare");
        templateValues["%%SHARINGURL%%"] = dest;
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
    
    std::map<std::string, std::string> attrs = { {"nickname", ""}, {"smallheadimgurl", ""}, {"bigheadimgurl", ""}, {"username", ""} };
    
    XmlParser xmlParser(cardMessage, true);
    if (xmlParser.parseAttributesValue("/msg", attrs) && !attrs["nickname"].empty())
    {
        templateValues.setName("card");
        templateValues["%%CARDNAME%%"] = attrs["nickname"];
        
        std::string portraitUrl = attrs["bigheadimgurl"].empty() ? attrs["smallheadimgurl"] : attrs["bigheadimgurl"];
        if (!attrs["username"].empty() && !portraitUrl.empty())
        {
            templateValues["%%CARDIMGPATH%%"] = portraitDir + "/" + attrs["username"] + ".jpg";
            std::string localfile = combinePath(portraitDir, attrs["username"] + ".jpg");
            m_downloader.addTask(portraitUrl, combinePath(sessionPath, localfile), 0);
        }
        else
        {
            templateValues["%%CARDIMGPATH%%"] = portraitUrl;
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
    }
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
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Portrait/" : "Portrait/";
    std::string localPortrait;
    std::string remotePortrait;
    
    std::string msgIdStr = std::to_string(record.msgId);
    
    tvs.push_back(TemplateValues("notice"));
    TemplateValues& beginTv = tvs.back();
    beginTv["%%MESSAGE%%"] = formatString(getLocaleString("<< %s"), title.c_str());
    beginTv["%%EXTRA_CLS%%"] = "fmsgtag";   // tag for forwarded msg
    
    std::string destDir = combinePath(outputPath, session.getUsrName() + "_files/", msgIdStr);
    bool assertDirExisted = m_shell.existsDirectory(destDir);
    
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
             
            if (it->dataType == "1")
            {
                tv["%%MESSAGE%%"] = replaceAll(replaceAll(replaceAll(it->message, "\r\n", "<br />"), "\r", "<br />"), "\n", "<br />");
            }
            else if (it->dataType == "2")
            {
                if (!assertDirExisted)
                {
                    assertDirExisted = m_shell.makeDirectory(destDir);
                }
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseImage(outputPath, vfile + fileExtName, vfile + fileExtName + "_pre3", session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + ".jpg", vfile + ".record_thumb", session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + "_thumb.jpg", tv);
            }
            else if (it->dataType == "3")
            {
                tv["%%MESSAGE%%"] = it->message;
            }
            else if (it->dataType == "4")
            {
                if (!assertDirExisted)
                {
                    assertDirExisted = m_shell.makeDirectory(destDir);
                }
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseVideo(outputPath, vfile + fileExtName, session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + fileExtName, vfile + ".record_thumb", session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + "_thumb.jpg", tv);
                
            }
            else if (it->dataType == "5")
            {
                if (!assertDirExisted)
                {
                    assertDirExisted = m_shell.makeDirectory(destDir);
                }
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId + ".record_thumb";
                std::string dest = session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + "_thumb.jpg";
                bool hasThumb = requireFile(vfile, combinePath(outputPath, dest));
                
                if (!it->link.empty())
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
                if (!assertDirExisted)
                {
                    assertDirExisted = m_shell.makeDirectory(destDir);
                }
                std::string fileExtName = it->dataFormat.empty() ? "" : ("." + it->dataFormat);
                std::string vfile = userBase + "/OpenData/" + session.getHash() + "/" + msgIdStr + "/" + it->dataId;
                parseFile(outputPath, vfile + fileExtName, session.getUsrName() + "_files/" + msgIdStr + "/" + it->dataId + fileExtName, it->message, tv);
            }
            else if (it->dataType == "16")
            {
                // Card
                std::string portraitDir = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.getUsrName() + "_files/Portrait" : "Portrait";
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
            else
            {
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
                
                if (!remotePortrait.empty() && !localPortrait.empty())
                {
                    m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
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

std::string SessionParser::getLocaleString(const std::string& key) const
{
	// std::string value = key;
	std::map<std::string, std::string>::const_iterator it = m_localeStrings.find(key);
	return it == m_localeStrings.cend() ? key : it->second;
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

std::string SessionParser::buildContentFromTemplateValues(const TemplateValues& values) const
{
    std::string content = getTemplate(values.getName());
    for (TemplateValues::const_iterator it = values.cbegin(); it != values.cend(); ++it)
    {
        if (startsWith(it->first, "%"))
        {
            content = replaceAll(content, it->first, it->second);
        }
    }
    
    std::string::size_type pos = 0;
    while ((pos = content.find("%%", pos)) != std::string::npos)
    {
        std::string::size_type posEnd = content.find("%%", pos + 2);
        if (posEnd == std::string::npos)
        {
            break;
        }
        
        content.erase(pos, posEnd + 2 - pos);
    }
    
    return content;
}
