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
            f.Members[uidHash] = std::make_pair(uid, displayName);
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
        user.NickName = value;
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
        Friend& f = friends.addFriend(uid);

        parseRemark(sqlite3_column_blob(stmt, 1), sqlite3_column_bytes(stmt, 1), f);
        parseHeadImage(sqlite3_column_blob(stmt, 3), sqlite3_column_bytes(stmt, 3), f);
        parseChatroom(sqlite3_column_blob(stmt, 2), sqlite3_column_bytes(stmt, 2), f);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return true;
}

bool FriendsParser::parseRemark(const void *data, int length, Friend& f)
{
    // if (f.alias.empty())
    {
        RawMessage msg;
        if (!msg.merge(reinterpret_cast<const char *>(data), length))
        {
            return false;
        }
        
        std::string value;
        if (msg.parse("1", value))
        {
            f.NickName = value;
        }
        /*
        if (msg.parse("6", value))
        {
        }
        */
    }
    
    return true;
}

bool FriendsParser::parseHeadImage(const void *data, int length, Friend& f)
{
    RawMessage msg;
    if (!msg.merge(reinterpret_cast<const char *>(data), length))
    {
        return false;
    }
    
    std::string value;
    if (msg.parse("2", value))
    {
        f.Portrait = value;
    }
    if (msg.parse("3", value))
    {
        f.PortraitHD = value;
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

SessionsParser::SessionsParser(ITunesDb *iTunesDb, Shell* shell) : m_iTunesDb(iTunesDb), m_shell(shell)
{
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
        session.CreateTime = static_cast<unsigned int>(sqlite3_column_int(stmt, 1));
        const char* extFileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != extFileName) session.ExtFileName = extFileName;
        session.UnreadCount = sqlite3_column_int(stmt, 2);
        
        if (session.UsrName == "sun_yunfeng")
        {
            int aa = 1;
        }
        
        if (!session.ExtFileName.empty())
        {
            parseCellData(userRoot, session);
        }
        const Friend* f = friends.getFriend(session.Hash);
        if (NULL != f)
        {
            if (!session.isChatroom())
            {
                session.copyInfoFromFriend(*f);
            }
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    parseMessageDbs(userRoot, sessions);
    
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
		if (itSession != sessions.end() && itSession->Hash == *it)
		{
			itSession->dbFile = dbPath;
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
			if (itSession != sessions.end() && itSession->Hash == *itId)
			{
				itSession->dbFile = dbPath;
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
	std::string fileName = session.ExtFileName;
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
        session.DisplayName = value;
    }
    if (msg.parse("1.1.4", value))
    {
        if (session.DisplayName.empty())
        {
            session.DisplayName = value;
        }
    }
	if (msg.parse("1.1.14", value))
	{
		session.Portrait = value;
	}
	if (msg.parse("1.5", value))
	{
		parseMembers(value, session);
	}
	if (msg.parse("2.7", value2))
	{
		session.LastMessageTime = static_cast<unsigned int>(value2);
	}
    
    if (session.DisplayName.empty())
    {
        SessionCellDataFilter filter(cellDataPath);
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
            if (session.DisplayName.empty() || (!displayName.empty() && modifiedTime > lastModifiedTime))
            {
                session.DisplayName = displayName;
                lastModifiedTime = modifiedTime;
            }
        }
    }

	return true;
}

SessionParser::SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, const std::map<std::string, std::string>& templates, const std::map<std::string, std::string>& localeStrings, int options, Downloader& downloader, std::atomic<bool>& cancelled) : m_templates(templates), m_localeStrings(localeStrings), m_options(options), m_myself(myself), m_friends(friends), m_iTunesDb(iTunesDb), m_shell(shell), m_downloader(downloader), m_cancelled(cancelled)
{
}

int SessionParser::parse(const std::string& userBase, const std::string& outputBase, const Session& session, std::string& contents) const
{
    int count = 0;
    contents.clear();
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(session.dbFile, &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT CreateTime,Message,Des,Type,MesLocalID FROM Chat_" + session.Hash + " ORDER BY CreateTime";
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
    
    std::map<std::string, std::string> values;
    std::string templateKey;
    Record record;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (m_cancelled)
        {
            break;
        }
        
        values.clear();
        templateKey = "";
        
        record.createTime = sqlite3_column_int(stmt, 0);
        const unsigned char* pMessage = sqlite3_column_text(stmt, 1);
        
        record.message = pMessage != NULL ? reinterpret_cast<const char*>(pMessage) : "";
        record.des = sqlite3_column_int(stmt, 2);
        record.type = sqlite3_column_int(stmt, 3);
        record.msgid = sqlite3_column_int(stmt, 4);
        if (pMessage == NULL)
        {
            int aa = 0;
        }
        if (parseRow(record, userBase, outputBase, session, templateKey, values))
        {
            count++;
            std::string ts = getTemplate(templateKey);
            for (std::map<std::string, std::string>::const_iterator it = values.cbegin(); it != values.cend(); ++it)
            {
                ts = replace_all(ts, it->first, it->second);
            }
            
			contents.append(ts);
#ifndef NDEBUG
            static std::map<std::string, int> types;
            std::string key = templateKey + "_" + values["%%ALIGNMENT%%"];
            if (types.find(key) == types.end())
            {
                types[key] = record.type;
                writeFile(combinePath(outputBase, "type_" + key + ".html"), ts);
            }
#endif
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

bool SessionParser::parseRow(Record& record, const std::string& userBase, const std::string& outputPath, const Session& session, std::string& templateKey, std::map<std::string, std::string>& templateValues) const
{
    templateValues.clear();
    templateKey = "msg";
    
	std::string msgIdStr = std::to_string(record.msgid);
    std::string assetsDir = combinePath(outputPath, session.UsrName + "_files");
	m_shell.makeDirectory(assetsDir);
    
    templateValues["%%MSGID%%"] = std::to_string(record.msgid);
	templateValues["%%NAME%%"] = "";
	templateValues["%%TIME%%"] = fromUnixTime(record.createTime);
	templateValues["%%MESSAGE%%"] = "";

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
        templateKey = "system";
        templateValues["%%MESSAGE%%"] = record.message;
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
        const ITunesFile* audioSrcFile = m_iTunesDb.findITunesFile(combinePath(userBase, "Audio", session.Hash, msgIdStr + ".aud"));
        std::string audioSrc;
        if (NULL != audioSrcFile)
        {
            audioSrc = m_iTunesDb.getRealPath(*audioSrcFile);
        }
        if (audioSrc.empty())
        {
            templateKey = "msg";
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

            templateKey = "audio";
            templateValues["%%AUDIOPATH%%"] = session.UsrName + "_files/" + msgIdStr + ".mp3";
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
        
		if (!url.empty())
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
            
            std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.UsrName + "_files/Emoji/" : "Emoji/";
            localfile = emojiPath + localfile + ".gif";
            m_downloader.addTask(url, combinePath(outputPath, localfile), record.createTime);
            templateKey = "emoji";
            templateValues["%%EMOJIPATH%%"] = localfile;
        }
        else
        {
            templateKey = "msg";
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
        bool hasthum = requireFile(combinePath(userBase, "Video", session.Hash, msgIdStr + ".video_thum"), combinePath(assetsDir, msgIdStr + "_thum.jpg"));
        bool hasvid = requireFile(combinePath(userBase, "Video", session.Hash, msgIdStr + ".mp4"), combinePath(assetsDir, msgIdStr + ".mp4"));

		std::string msgFile;
		if (hasthum || hasvid)
		{
			msgFile = session.UsrName + "_files/";
			msgFile += msgIdStr;
		}
        if (hasvid)
        {
            templateKey = "video";
            templateValues["%%THUMBPATH%%"] = hasthum ? (msgFile + "_thum.jpg") : "";
            templateValues["%%VIDEOPATH%%"] = hasthum ? (msgFile + ".mp4") : "";
        }
        else if (hasthum)
        {
            templateKey = "thumb";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (msgFile + "_thum.jpg") : "";
            templateValues["%%MESSAGE%%"] = getLocaleString("(Video Missed)");
        }
        else
        {
            templateKey = "msg";
            templateValues["%%MESSAGE%%"] = getLocaleString("[Video]");
        }
    }
    else if (record.type == 50)
    {
        templateKey = "msg";
        templateValues["%%MESSAGE%%"] = getLocaleString("[Video/Audio Call]");
    }
    else if (record.type == 64)
    {
		templateKey = "notice";

		Json::Reader reader;
		Json::Value root;
		if (reader.parse(record.message, root))
		{
			templateValues["%%MESSAGE%%"] = root["msgContent"].asString();
		}
    }
    else if (record.type == 3)
    {
		std::string vfile = combinePath(userBase, "Img", session.Hash, msgIdStr);

        bool hasthum = requireFile(vfile + ".pic_thum", combinePath(assetsDir, msgIdStr + "_thum.jpg"));
        bool haspic = requireFile(vfile + ".pic", combinePath(assetsDir, msgIdStr + ".jpg"));
        
		std::string msgFile;
		if (hasthum || haspic)
		{
			msgFile = session.UsrName + "_files/";
			msgFile += msgIdStr;
		}

        if (haspic)
        {
            templateKey = "image";
            templateValues["%%IMGPATH%%"] = msgFile + ".jpg";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (msgFile + "_thum.jpg") : (msgFile + ".jpg");
        }
        else if (hasthum)
        {
            templateKey = "thumb";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (msgFile + "_thum.jpg") : "";
            templateValues["%%MESSAGE%%"] = "";
        }
        else
        {
            templateKey = "msg";
            templateValues["%%MESSAGE%%"] = getLocaleString("[Picture]");
        }
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
		templateKey = "msg";
    }
    else if (record.type == 49)
    {
        if (record.message.find("<type>2001<") != std::string::npos) templateValues["%%MESSAGE%%"] = getLocaleString("[Red Packet]");
        else if (record.message.find("<type>2000<") != std::string::npos) templateValues["%%MESSAGE%%"] = getLocaleString("[Transfer]");
        else if (record.message.find("<type>17<") != std::string::npos) templateValues["%%MESSAGE%%"] = getLocaleString("[Real-time Location]");
        else if (record.message.find("<type>6<") != std::string::npos) templateValues["%%MESSAGE%%"] = getLocaleString("[File]");
        else
        {
            std::map<std::string, std::string> nodes = { {"title", ""}, {"des", ""}, {"url", ""}, {"thumburl", ""} };
            
#ifndef NDEBUG
            static std::regex pattern49_1("<title>(.+?)<\\/title>");
            static std::regex pattern49_2("<des>(.*?)<\\/des>");
            static std::regex pattern49_3("<url>(.+?)<\\/url>");
            static std::regex pattern49_4("<thumburl>(.+?)<\\/thumburl>");
            
            std::smatch sm1;
            std::smatch sm2;
            std::smatch sm3;
            std::smatch sm4;
            bool match1 = std::regex_search(record.message, sm1, pattern49_1);
            bool match2 = std::regex_search(record.message, sm2, pattern49_2);
            bool match3 = std::regex_search(record.message, sm3, pattern49_3);
            bool match4 = std::regex_search(record.message, sm4, pattern49_4);
#endif
            XmlParser xmlParser(record.message, true);
            if (xmlParser .parseNodesValue("/msg/appmsg/*", nodes))
            {
#ifndef NDEBUG
                if (nodes["title"] != sm1[1].str() || nodes["des"] != sm2[1].str() || nodes["url"] != sm3[1].str() || nodes["thumburl"] != sm4[1].str())
                {
                    int aa = 1;
                }
#endif
                if (!nodes["title"].empty() && !nodes["url"].empty())
                {
                    templateKey = "share";

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
            else
            {
                templateValues["%%MESSAGE%%"] = getLocaleString("[Link]");
            }
        }
    }
    else if (record.type == 42)
    {
		// static std::regex pattern42_1("nickname ?= ?\"(.+?)\"");
		// static std::regex pattern42_2("smallheadimgurl ?= ?\"(.+?)\"");
        
        std::map<std::string, std::string> attrs = { {"nickname", ""}, {"smallheadimgurl", ""}, {"username", ""} };
        
        XmlParser xmlParser(record.message, true);
        if (xmlParser.parseAttributesValue("/msg", attrs) && !attrs["nickname"].empty())
        {
            templateKey = "card";
            templateValues["%%CARDNAME%%"] = attrs["nickname"];
            
            if (!attrs["username"].empty() && !attrs["smallheadimgurl"].empty())
            {
                templateValues["%%CARDIMGPATH%%"] = "Portrait/" + attrs["username"] + ".jpg";
                std::string localfile = combinePath("Portrait", attrs["username"] + ".jpg");
                m_downloader.addTask(attrs["smallheadimgurl"], combinePath(outputPath, localfile), 0);
            }
            else
            {
                templateValues["%%CARDIMGPATH%%"] = attrs["smallheadimgurl"];
            }
        }
        else
        {
            templateValues["%%MESSAGE%%"] = getLocaleString("[Contact Card]");
        }
    }
    else
    {
        templateValues["%%MESSAGE%%"] = safeHTML(record.message);
    }
    
    std::string portraitPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.UsrName + "_files/Portrait/" : "Portrait/";
    std::string emojiPath = ((m_options & SPO_ICON_IN_SESSION) == SPO_ICON_IN_SESSION) ? session.UsrName + "_files/Emoji/" : "Emoji/";
    
    std::string localPortrait;
    std::string remotePortrait;
    if (session.isChatroom())
    {
        if (record.des == 0)
        {
            std::string txtsender = m_myself.DisplayName();
            
            templateValues["%%ALIGNMENT%%"] = "right";
            // templateValues.Add("%%NAME%%", txtsender);
            templateValues["%%NAME%%"] = "";    // Don't show name for self
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            templateValues["%%AVATAR%%"] = localPortrait;
            remotePortrait = m_myself.getPortraitUrl();
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
                    txtsender = f->DisplayName();
                }
                templateValues["%%NAME%%"] = txtsender.empty() ? senderId : txtsender;
                localPortrait = portraitPath + ((NULL != f) ? f->getLocalPortrait() : "DefaultProfileHead@2x.png");
                remotePortrait = (NULL != f) ? f->getPortraitUrl() : "";
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
        if (record.des == 0 || session.UsrName == m_myself.getUsrName())
        {
            templateValues["%%ALIGNMENT%%"] = "right";
            templateValues["%%NAME%%"] = "";
            localPortrait = portraitPath + m_myself.getLocalPortrait();
            remotePortrait = m_myself.getPortraitUrl();
            templateValues["%%AVATAR%%"] = localPortrait;
        }
        else
        {
            templateValues["%%ALIGNMENT%%"] = "left";

            const Friend *f = m_friends.getFriend(session.Hash);
            if (NULL == f)
            {
                templateValues["%%NAME%%"] = session.DisplayName;
                localPortrait = portraitPath + (session.Portrait.empty() ? "DefaultProfileHead@2x.png" : session.getLocalPortrait());
                remotePortrait = session.Portrait;
                templateValues["%%AVATAR%%"] = localPortrait;
            }
            else
            {
                templateValues["%%NAME%%"] = f->DisplayName();
                localPortrait = portraitPath + f->getLocalPortrait();
                remotePortrait = f->getPortraitUrl();
                templateValues["%%AVATAR%%"] = localPortrait;
            }
        }
    }

    if (!localPortrait.empty() && !remotePortrait.empty())
    {
        m_downloader.addTask(remotePortrait, combinePath(outputPath, localPortrait), record.createTime);
    }

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
    if (m_shell.existsFile(dest))
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
            bool result = m_shell.copyFile(srcPath, dest, true);
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
