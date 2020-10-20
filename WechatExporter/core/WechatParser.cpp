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
// #include <libxml/xpathInternals.h>
#include "WechatObjects.h"
#include "RawMessage.h"
#include "OSDef.h"

template<class T>
bool parseMembers(const std::string& xml, T& f)
{
    bool result = false;
    xmlDocPtr doc = NULL;
    xmlXPathContextPtr xpathCtx = NULL;
    xmlXPathObjectPtr xpathObj = NULL;
    xmlNodeSetPtr xpathNodes = NULL;

    doc = xmlParseMemory(xml.c_str(), xml.size());
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
    
    if (msg.parse("10.1.2", value))
    {
        // user.DisplayName = value;
    }
    if (msg.parse("10.2.2.2", value))
    {
        // user.DisplayName = value;
        
        std::ofstream myFile ("/Users/matthew/Documents/reebes/iPhone SE/com.tencent.xin/Documents/LoginInfo2.dat.f1.10.2.2.2", std::ios::out | std::ios::binary);
        myFile.write (value.c_str(), value.size());
        myFile.close();
        
    }
    
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
                        for (int i = 0; i < valueLength; i++)
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
    int rc = sqlite3_open(mmPath.c_str(), &db);
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
#ifndef NDEBUG
        if (userType == 1)
        {
            continue;
        }
#endif
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL == val)
        {
            continue;
        }
        std::string uid = val;
        
        Friend& f = friends.addFriend(uid);
        
        if (f.IsChatroom)
        {
            // f.IsChatroom = true;
        }
        parseRemark(sqlite3_column_blob(stmt, 1), sqlite3_column_bytes(stmt, 1), f);
        parseHeadImage(sqlite3_column_blob(stmt, 3), sqlite3_column_bytes(stmt, 3), f);
        parseChatroom(sqlite3_column_blob(stmt, 2), sqlite3_column_bytes(stmt, 2), f);
        
        // friends[f.getUidHash()] = f;
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
            // f.alias = RawMessage::toUtf8String(value);
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
        // f.dbContactChatRoom = value;
    }
    // 2: 群主
    // 1: 成员uid列表 ;分割

    return true;
}

SessionsParser::SessionsParser(ITunesDb *iTunesDb) : m_iTunesDb(iTunesDb)
{
}

bool SessionsParser::parse(const std::string& userRoot, std::vector<Session>& sessions)
{
    std::string sessionDbPath = m_iTunesDb->findRealPath(combinePath(userRoot, "session", "session.db"));
    
    sqlite3 *db = NULL;
    int rc = sqlite3_open(sessionDbPath.c_str(), &db);
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
        sessions.push_back(Session());
        Session& session = sessions.back();
        
        session.setUsrName(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        session.CreateTime = static_cast<unsigned int>(sqlite3_column_int(stmt, 1));
        const char* extFileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != extFileName) session.ExtFileName = extFileName;
        session.UnreadCount = sqlite3_column_int(stmt, 2);
        
        if (!session.ExtFileName.empty())
        {
            parseCellData(userRoot, session);
        }
        
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    std::vector<std::pair<std::string, std::string>> sessionDbs;
    
    parseMessageDbs(userRoot, sessionDbs);
    
    SessionHashCompare comp;
    std::sort(sessions.begin(), sessions.end(), comp);
    
    std::sort(sessions.begin(), sessions.end(), comp);
    
    for (typename std::vector<std::pair<std::string, std::string>>::const_iterator it = sessionDbs.cbegin(); it != sessionDbs.cend(); ++it)
    {
        std::vector<Session>::iterator itSession = std::lower_bound(sessions.begin(), sessions.end(), it->second, comp);
        if (itSession != sessions.end())
        {
            itSession->dbFile = it->first;
        }
        else
        {
#ifndef NDEBUG
            std::string value = it->second;
            std::cout << value << std::endl;
#endif
        }
    }

    return true;
}

bool SessionsParser::parseCellData(const std::string& userRoot, Session session)
{
    std::string fileName = session.ExtFileName;
    if (startsWith(fileName, DIR_SEP) || startsWith(fileName, DIR_SEP_R))
    {
        fileName = fileName.substr(1);
    }
    fileName = m_iTunesDb->findRealPath(combinePath(userRoot, fileName));
    // SessionCellDataFilter filter(fileName);
    // StringPairVector files = m_iTunesDb->filter(filter);
    
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
    if (msg.parse("1.1.4", value))
    {
        session.DisplayName = value;
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

    return true;
}

bool SessionsParser::parseMessageDbs(const std::string& userRoot, std::vector<std::pair<std::string, std::string>>& sessions)
{
    MessageDbFilter filter(userRoot);
    StringPairVector dbs = m_iTunesDb->filter(filter);
    
    std::string dbPath = m_iTunesDb->findRealPath(combinePath(userRoot, "DB", "MM.sqlite"));
    parseMessageDb(dbPath, sessions);
    for (SPVectorConstIterator it = dbs.cbegin(); it != dbs.cend(); ++it)
    {
        dbPath = m_iTunesDb->fieldIdToRealPath(it->first);
        parseMessageDb(dbPath, sessions);
    }
    
    return true;
}

bool SessionsParser::parseMessageDb(const std::string& mmPath, std::vector<std::pair<std::string, std::string>>& sessions)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open(mmPath.c_str(), &db);
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
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        
        std::smatch sm;
        static std::regex pattern("^Chat_([0-9a-f]{32})$");
        if (std::regex_search(name, sm, pattern))
        {
            sessions.push_back(std::make_pair(mmPath, sm[1]));
        }
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return true;
}

SessionParser::SessionParser(Friend& myself, Friends& friends, const ITunesDb& iTunesDb, const Shell& shell, Logger& logger, std::map<std::string, std::string>& templates, DownloadPool& dlPool) : m_myself(myself), m_friends(friends), m_iTunesDb(iTunesDb), m_shell(shell), m_logger(logger),  m_templates(templates), m_downloadPool(dlPool)
{
}

int SessionParser::parse(const std::string& userBase, const std::string& path, const Session& session, Friend& f)
{
    int count = 0;
    sqlite3 *db = NULL;
    int rc = sqlite3_open(session.dbFile.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT CreateTime,Message,Des,Type,MesLocalID FROM Chat_" + session.Hash + " ORDER BY CreateTime";

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
        values.clear();
        templateKey = "";
        
        record.createTime = sqlite3_column_int(stmt, 0);
        record.message = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.des = sqlite3_column_int(stmt, 2);
        record.type = sqlite3_column_int(stmt, 3);
        record.msgid = sqlite3_column_int(stmt, 4);

        /*
        if (parseRow(record, userBase, path, displayName, id, myself, table, f, templateKey, values))
        {
            count++;
            std::string ts = getTemplate(templateKey);
            for (std::map<std::string, std::string>::const_iterator it = values.cbegin(); it != values.cend(); ++it)
            {
                // ts = ts.combinePath(entry.Key, entry.Value);
            }
            
            ofstream myfile ("example.txt");
            if (myfile.is_open())
            {
              myfile << "This is a line.\n";
              myfile << "This is another line.\n";
              myfile.close();
            }
             
        }
         */
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return true;
}


std::string SessionParser::getTemplate(std::string key) const
{
    std::map<std::string, std::string>::const_iterator it = m_templates.find(key);
    return (it == m_templates.cend()) ? "" : it->second;
}

bool SessionParser::parseRow(Record& record, const std::string& userBase, const std::string& path, const Session& session, std::string& templateKey, std::map<std::string, std::string>& templateValues)
{
    templateValues.clear();
    templateKey = "msg";
    
    std::string assetsdir = combinePath(path, session.UsrName + "_files");
    
    if (session.isChatroom())
    {
        if (record.des == 0)
        {
            std::string txtsender = m_myself.DisplayName();
            
            templateValues["%%ALIGNMENT%%"] = "right";
            // templateValues.Add("%%NAME%%", txtsender);
            templateValues["%%NAME%%"] = "";
            templateValues["%%AVATAR%%"] = "Portrait/" + m_myself.FindPortrait();
        }
        else
        {
            templateValues["%%ALIGNMENT%%"] = "left";
            
            std::string::size_type enter = record.message.find(":\n");
            if (enter != std::string::npos && enter + 2 < record.message.size())
            {
                std::string senderid = record.message.substr(0, enter);
                std::string txtsender = senderid;
                
                record.message = record.message.substr(enter + 2);
                txtsender = session.getMemberName(md5(senderid));
                
                Friend *f = m_friends.getFriendByUid(senderid);
                if (txtsender.empty() && NULL != f)
                {
                    txtsender = f->DisplayName();
                }
            
                templateValues["%%NAME%%"] = txtsender;
                templateValues["%%AVATAR%%"] = (NULL != f) ? ("Portrait/" + f->FindPortrait()) : "Portrait/DefaultProfileHead@2x.png";
            }
            else
            {
                templateValues["%%NAME%%"] = "";
                templateValues["%%AVATAR%%"] = "";
            }
        }
    }
    else
    {
        if (record.des == 0)
        {
            templateValues["%%ALIGNMENT%%"] = "right";
            templateValues["%%NAME%%"] = "";
            templateValues["%%AVATAR%%"] = "Portrait/" + m_myself.FindPortrait();
        }
        else
        {
            Friend *f = m_friends.getFriend(session.Hash);
            templateValues["%%ALIGNMENT%%"] = "left";
            templateValues["%%NAME%%"] = session.DisplayName;
            templateValues["%%AVATAR%%"] = "Portrait/" + f->FindPortrait();
        }
        /*
        else
        {
            templateValues["%%ALIGNMENT%%"] = "left";
            templateValues["%%NAME%%"] = displayname;
            templateValues["%%AVATAR%%"] = "Portrait/DefaultProfileHead@2x.png";
        }
         */
    }
    
    if (record.type == 10000 || record.type == 10002)
    {
        templateKey = "system";
        templateValues["%%MESSAGE%%"] = record.message;
    }
    else if (record.type == 34)
    {
        int voicelen = -1;
        
        static std::regex voiceLengthPattern("voicelength=\"(\\d+?)\"");
        std::smatch sm;
        
        if (std::regex_search(record.message, sm, voiceLengthPattern))
        {
            voicelen = std::stoi(sm[1]);
        }
        std::string audiosrc = m_iTunesDb.findFileId(combinePath(userBase, "Audio", session.Hash, std::to_string(record.msgid) + ".aud"));
        if (audiosrc.empty())
        {
            templateKey = "msg";
            templateValues["%%MESSAGE%%"] = voicelen == -1 ? "[语音]" : "[语音 " + getDisplayTime(voicelen) + "]";
        }
        else
        {
            m_shell.convertSilk(audiosrc, combinePath(assetsdir, std::to_string(record.msgid) + ".mp3"));
            templateKey = "audio";
            templateValues["%%AUDIOPATH%%"] = session.UsrName + "_files/" + std::to_string(record.msgid) + ".mp3";
        }
    }
    else if (record.type == 47)
    {
        static std::regex cdnUrlPattern("cdnurl ?= ?\"(.+?)\"");
        std::smatch sm;
        if (std::regex_search(record.message, sm, cdnUrlPattern))
        {
            std::string localfile = removeCdata(sm[1].str());
            std::smatch sm2;
            static std::regex localFilePattern("\/(\w+?)\/\w*$");
            if (std::regex_search(record.message, sm, cdnUrlPattern))
            {
                localfile = sm2[1];
            }
            else
            {
                static int uniqueFileName = 1000000000;
                localfile = std::to_string(uniqueFileName++);
            }
            
            m_downloadPool.addTask(sm[1], localfile + ".gif");
            // message = "<img src=\"Emoji/" + localfile + ".gif\" style=\"max-width:100px;max-height:60px\" />";
            templateKey = "emoji";
            // message = "[表情]";
            templateValues["%%EMOJIPATH%%"] = "Emoji/" + localfile + ".gif";
        }
        else
        {
            templateKey = "msg";
            // message = "[表情]";
            templateValues["%%MESSAGE%%"] = "[表情]";
        }
    }
    else if (record.type == 62 || record.type == 43)
    {
        /*
        var hasthum = requireResource(combinePath(userBase, "Video", table, record.msgid + ".video_thum"), combinePath(assetsdir, record.msgid + "_thum.jpg"));
        var hasvid = requireResource(combinePath(userBase, "Video", table, record.msgid + ".mp4"), combinePath(assetsdir, record.msgid + ".mp4"));

        if (hasvid)
        {
            templateKey = "video";
            templateValues["%%THUMBPATH%%"] = hasthum ? (id + "_files/" + msgid + "_thum.jpg") : "";
            templateValues["%%VIDEOPATH%%"] = hasthum ? (id + "_files/" + msgid + ".mp4") : "";
        }
        else if (hasthum)
        {
            templateKey = "thumb";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (id + "_files/" + msgid + "_thum.jpg") : "";
            templateValues["%%MESSAGE%%"] = "（视频丢失）";
        }
        else
        {
            templateKey = "msg";
            templateValues["%%MESSAGE%%"] = "[视频]";
        }
        */
    }
    else if (record.type == 50)
    {
        templateKey = "msg";
        templateValues["%%MESSAGE%%"] = "[视频/语音通话]";
        // message = "[视频/语音通话]";
    }
    else if (record.type == 64)
    {
        /*
        var serializer = new DataContractJsonSerializer(typeof(Message64));
        var stream = new MemoryStream(Encoding.UTF8.GetBytes(message));
        Message64 msg64 = (Message64)serializer.ReadObject(stream);

        templateKey = "notice";
        templateValues["%%MESSAGE%%"] = msg64.msgContent;
         */
        
    }
    else if (record.type == 3)
    {
        /*
        var hasthum = requireResource(combinePath(userBase, "Img", table, msgid + ".pic_thum"), combinePath(assetsdir, msgid + "_thum.jpg"));
        var haspic = requireResource(combinePath(userBase, "Img", table, msgid + ".pic"), combinePath(assetsdir, msgid + ".jpg"));
        
        if (haspic)
        {
            templateKey = "image";
            templateValues["%%IMGPATH%%"] = id + "_files/" + msgid + ".jpg";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (id + "_files/" + msgid + "_thum.jpg") : (id + "_files/" + msgid + ".jpg");
        }
        else if (hasthum)
        {
            templateKey = "thumb";
            templateValues["%%IMGTHUMBPATH%%"] = hasthum ? (id + "_files/" + msgid + "_thum.jpg") : "";
            templateValues["%%MESSAGE%%"] = "";
        }
        else
        {
            templateKey = "msg";
            templateValues["%%MESSAGE%%"] = "[图片]";
        }
         */
    }
    else if (record.type == 48)
    {
        /*
        var match1 = Regex.Match(message, @"x ?= ?""(.+?)""");
        var match2 = Regex.Match(message, @"y ?= ?""(.+?)""");
        var match3 = Regex.Match(message, @"label ?= ?""(.+?)""");
        if (match1.Success && match2.Success && match3.Success) message = "[位置 (" + RemoveCdata(match2.Groups[1].Value) + "," + RemoveCdata(match1.Groups[1].Value) + ") " + RemoveCdata(match3.Groups[1].Value) + "]";
        else message = "[位置]";

         */
        templateKey = "msg";
        templateValues["%%MESSAGE%%"] = record.message;
    }
    else if (record.type == 49)
    {
        /*
        if (message.Contains("<type>2001<")) templateValues.Add("%%MESSAGE%%", "[红包]");
        else if (message.Contains("<type>2000<")) templateValues.Add("%%MESSAGE%%", "[转账]");
        else if (message.Contains("<type>17<")) templateValues.Add("%%MESSAGE%%", "[实时位置共享]");
        else if (message.Contains("<type>6<")) templateValues.Add("%%MESSAGE%%", "[文件]");
        else
        {
            var match1 = Regex.Match(message, @"<title>(.+?)<\/title>");
            var match2 = Regex.Match(message, @"<des>(.*?)<\/des>");
            var match3 = Regex.Match(message, @"<url>(.+?)<\/url>");
            var match4 = Regex.Match(message, @"<thumburl>(.+?)<\/thumburl>");
            if (match1.Success && match3.Success)
            {
                templateKey = "share";

                templateValues["%%SHARINGIMGPATH%%"] = "";
                templateValues["%%SHARINGURL%%"] = RemoveCdata(match3.Groups[1].Value);
                templateValues["%%SHARINGTITLE%%"] = RemoveCdata(match1.Groups[1].Value);
                templateValues["%%MESSAGE%%"] = "";

                if (match4.Success)
                {
                    templateValues["%%SHARINGIMGPATH%%"] = RemoveCdata(match4.Groups[1].Value);
                    // message += "<img src=\"" + RemoveCdata(match4.Groups[1].Value) + "\" style=\"float:left;max-width:100px;max-height:60px\" />";
                }
                // message += "<a href=\"" + RemoveCdata(match3.Groups[1].Value) + "\"><b>" + RemoveCdata(match1.Groups[1].Value) + "</b></a>";
                if (match2.Success)
                {
                    // message += "<br />" + RemoveCdata(match2.Groups[1].Value);
                    templateValues["%%MESSAGE%%"] = RemoveCdata(match2.Groups[1].Value);
                }
            }
            else
            {
                templateValues["%%MESSAGE%%"] = "[链接]";
            }
        }
         */
    }
    else if (record.type == 42)
    {
        /*
        var match1 = Regex.Match(message, "nickname ?= ?\"(.+?)\"");
        var match2 = Regex.Match(message, "smallheadimgurl ?= ?\"(.+?)\"");
        if (match1.Success)
        {
            message = "";
            if (match2.Success) message += "<img src=\"" + RemoveCdata(match2.Groups[1].Value) + "\" style=\"float:left;max-width:100px;max-height:60px\" />";
            message += "[名片] " + removeCdata(match1.Groups[1].Value);

            templateKey = "card";
            templateValues["%%CARDIMGPATH%%"] = (match2.Success) ? removeCdata(match2.Groups[1].Value) : "";
            templateValues["%%CARDNAME%%"] = removeCdata(match1.Groups[1].Value);
        }
        else
        {
            templateValues["%%MESSAGE%%"] = "[名片]";
        }
         */
    }
    else
    {
        templateValues["%%MESSAGE%%"] = safeHTML(record.message);
    }

    // templateValues.Add("%%TIME%%", fromUnixTime(record.createTime).ToLocalTime().ToString().combinePath(" ", "&nbsp;"));
    

    return true;
    // ts = ts.combinePath(@"%%TIME%%", FromUnixTime(unixtime).ToLocalTime().ToString().combinePath(" ", "&nbsp;"));
    // ts = ts.combinePath(@"%%MESSAGE%%", message);
    // ts += @"<td width=""100"" align=""center"">" + FromUnixTime(unixtime).ToLocalTime().ToString().combinePath(" ","<br />") + "</td>";
    // ts += @"<td>" + message + @"</td></tr>";
    // sw.WriteLine(ts);
    
}

bool SessionParser::requireResource(std::string vpath, std::string dest)
{
    std::string srcPath = m_iTunesDb.findFileId(vpath);
    if (!srcPath.empty())
    {
        return m_shell.copyFile(srcPath, dest, true);
    }
    
    return false;
}

std::string SessionParser::getDisplayTime(int ms) const
{
    if (ms < 1000) return "1\"";
    return std::to_string(std::round((double)ms)) + "\"";
}


bool UserParser::parse(Friend& myself)
{
    bool succ = false;
    /*
    myself.UsrName = uid;
    myself.NickName = "我";
    
    // friend = new Friend() { UsrName = uid, NickName = "我", alias = null, PortraitRequired=true };
    
    try
    {
        // m_shell
        
        // var pr = new BinaryPlistReader();
        std::string mmsetting = combinePath(m_backupPath, m_iTunesDb->findFileId(combinePath(userBase, "mmsetting.archive")));
        if (existsFile(mmsetting))
        {
            std::string xmlPath = std::tmpnam();
            if (m_shell->convertPlist(mmsetting, xmlPath))
            //using (var sw = new FileStream(mmsetting, FileMode.Open))
            {
                xmlDoc *doc = NULL;
                
                if (doc = xmlReadFile(xmlPath.c_str(), NULL, 0)) != NULL)
                {
                    xmlNode *root_element = NULL;
                    
                    root_element = xmlDocGetRootElement(doc);
                    // print_element_names(root_element);
                    
                    xmlFreeDoc(doc);       // free document
                    
                }
                
                xmlCleanupParser();    // Free globals
                
                var dd = pr.ReadObject(sw);
                var objs = dd["$objects"] as object[];
                var dict = GetCFUID(objs[1] as Dictionary<object, object>);
                if (dict.ContainsKey("UsrName") && dict.ContainsKey("NickName"))
                {
                    friend.UsrName = objs[dict["UsrName"]] as string;
                    friend.NickName = objs[dict["NickName"]] as string;
                    succ = true;
                }
                if (dict.ContainsKey("AliasName"))
                {
                    friend.alias = objs[dict["AliasName"]] as string;
                }
                for (int i = 0; i < objs.Length; i++)
                {
                    if (objs[i].GetType() != typeof(string)) continue;
                    string obj = (objs[i] as string);
                    
                    if (obj.StartsWith("http://wx.qlogo.cn/mmhead/") || obj.StartsWith("https://wx.qlogo.cn/mmhead/"))
                    {
                        if (obj.EndsWith("/0")) friend.PortraitHD = obj;
                        else if (obj.EndsWith("/132")) friend.Portrait = obj;
                    }
                }
            }
        }
        else
        {
            // Find it from MMappedKV
            mmsetting = FindMMSettingFromMMappedKV(uid);
            if (mmsetting != null && (mmsetting = GetBackupFilePath(mmsetting)) != null)
            {
                byte[] data = null;
                try
                {
                    data = File.ReadAllBytes(mmsetting);
                }
                catch (Exception) { }

                if (data != null)
                {
                    byte[] nameKey = { 56, 56 };
                    friend.NickName = GetStringFromMMSetting2(data, nameKey);
                    friend.alias = friend.NickName;

                    byte[] headImgUrl = Encoding.UTF8.GetBytes("headimgurl");
                    friend.Portrait = GetStringFromMMSetting(data, headImgUrl);

                    byte[] headHDImgUrl = Encoding.UTF8.GetBytes("headhdimgurl");
                    friend.PortraitHD = GetStringFromMMSetting(data, headHDImgUrl);

                    succ = true;
                }
            }
        }

    }
    catch (Exception ex)
    {
        logger.Debug(ex.ToString());
    }
*/
    return succ;
}

bool WechatParser::parse()
{
    /*
    Directory.CreateDirectory(saveBase);
    logger.AddLog("分析文件夹结构");
    WeChatInterface wechat = new WeChatInterface(backupPath, files92, logger);
    wechat.BuildFilesDictionary();
    logger.AddLog("查找UID");
    var UIDs = wechat.FindUIDs();
    logger.AddLog("找到" + UIDs.Count + "个账号的消息记录");
    var uidList = new List<WeChatInterface.DisplayItem>();
    foreach (var uid in UIDs)
    {
#ifndef NDEBUG
        if (!uid.Equals("ed93c38987566a06ce6430aa8bb5a1ef"))
        {
            // continue;
        }
#endif
        var userBase = Path.Combine("Documents", uid);
        logger.AddLog("开始处理UID: " + uid);
        logger.AddLog("读取账号信息");
        if (wechat.GetUserBasics(uid, userBase, out Friend myself))
        {
            logger.AddLog("微信号：" + myself.ID() + " 昵称：" + myself.DisplayName());
        }
        else
        {
            // logger.AddLog("没有找到本人信息，用默认值替代，可以手动替换正确的头像文件：" + Path.Combine("res", "DefaultProfileHead@2x-Me.png").ToString());
        }
        var userSaveBase = Path.Combine(saveBase, myself.ID());
        Directory.CreateDirectory(userSaveBase);
        logger.AddLog("正在打开数据库");
        var emojidown = new HashSet<DownloadTask>();
        var chatList = new List<WeChatInterface.DisplayItem>();
        Dictionary<string, Friend> friends = null;
        int friendcount = 0;

        Dictionary<string, Session> sessions = null;
        if (wechat.OpenSessions(userBase, out SQLiteConnection sessionConnection))
        {
            wechat.GetSessionDict(userBase, sessionConnection, out sessions);
            sessionConnection.Close();
            if (sessions == null)
            {
                sessions = new Dictionary<string, Session>();
            }
        }

        List<string> dbs = wechat.GetMMSqlites(userBase);
        foreach (string db in dbs)
        {
            if (!wechat.OpenMMSqlite(userBase, db, out SQLiteConnection conn))
            {
                logger.AddLog("打开MM.sqlite失败，跳过");
                continue;
            }

            if (db.Equals("MM.sqlite"))
            {
                if (wechat.OpenWCDBContact(userBase, out SQLiteConnection wcdb))
                    logger.AddLog("存在WCDB，与旧版好友列表合并使用");
                logger.AddLog("读取好友列表");
                if (!wechat.GetFriendsDict(conn, wcdb, myself, out friends, out friendcount))
                {
                    logger.AddLog("读取好友列表失败，跳过");
                    continue;
                }
                logger.AddLog("找到" + friendcount + "个好友/聊天室");
            }
            
            logger.AddLog("查找对话");
            wechat.GetChatSessions(conn, out List<string> chats);
            logger.AddLog("找到" + chats.Count + "个对话");
            
            foreach (var chat in chats)
            {
                var hash = chat;
                string displayname = chat, id = displayname;
                Friend friend = null;
                if (friends.ContainsKey(hash))
                {
                    friend = friends[hash];
                    displayname = friend.DisplayName();
                    logger.AddLog("处理与" + displayname + "的对话");
                    id = friend.ID();
                }
                else logger.AddLog("未找到好友信息，用默认名字代替");

                if (displayname.EndsWith("@chatroom"))
                {
                    if (sessions.ContainsKey(displayname))
                    {
                        Session session = sessions[displayname];
                        if (session.DisplayName != null && session.DisplayName.Length != 0)
                        {
                            displayname = session.DisplayName;
                        }
                    }
                }
    #if DEBUG
                if (!"23069688360@chatroom".Equals(id))
                {
                    continue;
                }
    #endif
                long lastMsgTime = 0;
                if (wechat.SaveHtmlRecord(conn, userBase, userSaveBase, displayname, id, myself, chat, friend, friends, out int count, out HashSet<DownloadTask> _emojidown, out lastMsgTime))
                {
                    logger.AddLog("成功处理" + count + "条");
                    chatList.Add(new WeChatInterface.DisplayItem() { pic = "Portrait/" + (friend != null ? friend.FindPortrait() : "DefaultProfileHead@2x.png"), text = displayname, link = id + ".html", lastMessageTime = lastMsgTime });
                }
                else logger.AddLog("失败");
                emojidown.UnionWith(_emojidown);
                
            }
            conn.Close();
        }

        if (outputHtml)
        {
            // 最后一条消息的时间倒叙
            chatList.Sort((x, y) => { return y.lastMessageTime.CompareTo(x.lastMessageTime); });
            wechat.MakeListHTML(chatList, Path.Combine(userSaveBase, "聊天记录.html"));
        }
        var portraitdir = Path.Combine(userSaveBase, "Portrait");
        Directory.CreateDirectory(portraitdir);
        var downlist = new HashSet<DownloadTask>();
        foreach (var item in friends)
        {
            var tfriend = item.Value;
            // Console.WriteLine(tfriend.ID());
#ifndef NDEBUG
            if (!"25926707592@chatroom".Equals(tfriend.ID()))
            {
                // continue;
            }
#endif
            if (!tfriend.PortraitRequired) continue;
            if (tfriend.Portrait != null && tfriend.Portrait != "") downlist.Add(new DownloadTask() { url = tfriend.Portrait, filename = tfriend.ID() + ".jpg" });
            //if (tfriend.PortraitHD != null && tfriend.PortraitHD != "") downlist.Add(new DownloadTask() { url = tfriend.PortraitHD, filename = tfriend.ID() + "_hd.jpg" });
        }
        var downloader = new Downloader(6);
        if (downlist.Count > 0)
        {
            logger.AddLog("下载" + downlist.Count + "个头像");
            foreach (var item in downlist)
            {
                downloader.AddTask(item.url, Path.Combine(portraitdir, item.filename));
            }
            try
            {
                File.Copy(Path.Combine("res", "DefaultProfileHead@2x.png"), Path.Combine(portraitdir, "DefaultProfileHead@2x.png"), true);
            }
            catch (Exception) { }
        }
        var emojidir = Path.Combine(userSaveBase, "Emoji");
        Directory.CreateDirectory(emojidir);
        if (emojidown != null && emojidown.Count > 0)
        {
            logger.AddLog("下载" + emojidown.Count + "个表情");
            foreach (var item in emojidown)
            {
                downloader.AddTask(item.url, Path.Combine(emojidir, item.filename));
            }
        }
        string displayName = myself.DisplayName();
        if (displayName == "我" && myself.alias != null && myself.alias.Length != 0)
        {
            displayName = myself.alias;
        }
        uidList.Add(new WeChatInterface.DisplayItem() { pic = myself.ID() + "/Portrait/" + myself.FindPortrait(), text = displayName, link = myself.ID() + "/聊天记录.html" });
        downloader.StartDownload();
        System.Threading.Thread.Sleep(16);
        downloader.WaitToEnd();
        logger.AddLog("完成当前账号");
    }
    if (outputHtml) wechat.MakeListHTML(uidList, indexPath);
    logger.AddLog("任务结束");

    wechat = null;
     */
    return true;
}
