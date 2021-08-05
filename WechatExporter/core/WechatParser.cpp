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

LoginInfo2Parser::LoginInfo2Parser(ITunesDb *iTunesDb
#if !defined(NDEBUG) || defined(DBG_PERF)
                                   , Logger* logger
#endif
                                   ) : m_iTunesDb(iTunesDb)
#if !defined(NDEBUG) || defined(DBG_PERF)
                                    , m_logger(logger)
#endif
{
}

#if !defined(NDEBUG) || defined(DBG_PERF)
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
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Documents/LoginInfo2.dat not exists.");
#endif
        return false;
    }
    
    return parse(realPath, users);
}

bool LoginInfo2Parser::parse(const std::string& loginInfo2Path, std::vector<Friend>& users)
{
    RawMessage msg;
    if (!msg.mergeFile(loginInfo2Path))
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Failed to parse Documents/LoginInfo2.dat(pb).");
#endif
        return false;
    }
    
    std::string value1;
    if (!msg.parse("1", value1))
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Failed to parse field 1 in Documents/LoginInfo2.dat.");
#endif
        return false;
    }
    
    users.clear();
    int offset = 0;
    std::string::size_type length = value1.size();
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("Length of field 1 in Documents/LoginInfo2.dat = " + std::to_string(length));
#endif
    while (offset < length)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Offset of field 1 in Documents/LoginInfo2.dat = " + std::to_string(offset) + "/" + std::to_string(length));
#endif
        int res = parseUser(value1.c_str() + offset, static_cast<int>(length - offset), users);
        if (res < 0)
        {
            break;
        }
        
        offset += res;
    }
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    for (std::vector<Friend>::iterator it = users.begin(); it != users.end(); ++it)
    {
        m_logger->debug("User from Documents/LoginInfo2.dat:" + it->getUsrName() + "(" + it->getHash() + ") => " + it->getDisplayName());
    }
#endif
    
    parseUserFromFolder(users);
    
    MMSettingInMMappedKVFilter filter;
    ITunesFileVector mmsettings = m_iTunesDb->filter(filter);
    
    std::map<std::string, std::string> mmsettingFiles;  // hash => usrName
    for (ITunesFilesConstIterator it = mmsettings.cbegin(); it != mmsettings.cend(); ++it)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("mmsetting: " + (*it)->relativePath  + " => " + (*it)->fileId);
#endif
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
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Parse user:" + it->getUsrName() + "(" + it->getHash() + ") => " + it->getDisplayName());
#endif
        MMSettingParser mmsettingParser(m_iTunesDb);
        if (mmsettingParser.parse(it->getHash()))
        {
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("Succeeded to parse mmsetting:" + it->getUsrName());
#endif
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
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("Failed to parse mmsetting:" + it->getUsrName());
#endif
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
#if !defined(NDEBUG) || defined(DBG_PERF)
                    MMKVParser parser(m_logger);
#else
                    MMKVParser parser;
#endif
#if !defined(NDEBUG) || defined(DBG_PERF)
                    m_logger->debug("Parse MMKV file: Documents/MMappedKV/mmsetting.archive." + it->getUsrName() + " => " + realPath);
                    m_logger->debug("Parse MMKV file: Documents/MMappedKV/mmsetting.archive." + it->getUsrName() + ".crc => " + realCrcPath);
#endif
                    if (parser.parse(realPath, realCrcPath))
                    {
#if !defined(NDEBUG) || defined(DBG_PERF)
                        m_logger->debug("Succeeded to parse mmkv:" + it->getUsrName());
#endif
                        if (it->isDisplayNameEmpty())
                        {
                            it->setDisplayName(parser.getDisplayName());
                        }
                        it->setPortrait(parser.getPortrait());
                        it->setPortraitHD(parser.getPortraitHD());
                    }
                    else
                    {
#if !defined(NDEBUG) || defined(DBG_PERF)
                        m_logger->debug("Failed to parse MMKV: Documents/MMappedKV/mmsetting.archive." + it->getUsrName());
#endif
                    }
                }
                else
                {
#if !defined(NDEBUG) || defined(DBG_PERF)
                    m_logger->debug("MMKV file not exists: Documents/MMappedKV/mmsetting.archive." + it->getUsrName());
#endif
                }
            }
        }
    }
    
    auto it = users.begin();
    while (it != users.end())
    {
        if (it->getUsrName().empty())
        {
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("Erase: md5=" + it->getHash() + "* dn=" + it->getDisplayName() + "*");
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
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("UsrName from Documents/LoginInfo2.dat = " + value);
#endif
        user.setUsrName(value);
    }
    if (msg.parse("3", value))
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("DisplayName from Documents/LoginInfo2.dat = " + value);
#endif
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
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_error += "LoginInfo2.dat: *" + user.getDisplayName() + "* ";
#endif
    
    return static_cast<int>(userBufferLen + (p - data));
}

bool LoginInfo2Parser::parseUserFromFolder(std::vector<Friend>& users)
{
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("parseUserFromFolder starts...");
#endif
    UserFolderFilter filter;
    ITunesFileVector folders = m_iTunesDb->filter(filter);
    
    for (ITunesFilesConstIterator it = folders.cbegin(); it != folders.cend(); ++it)
    {
        std::string fileName = filter.parse((*it));
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_error += "User Folder: *" + fileName + "*  ";
#endif
        if (fileName == "00000000000000000000000000000000")
        {
            continue;
        }
        
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("Find User Folder:" + fileName);
#endif
        
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
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("New User Folder:" + fileName);
#endif
            users.emplace(users.end(), "", fileName);
#if !defined(NDEBUG) || defined(DBG_PERF)
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

MMKVParser::MMKVParser(
#if !defined(NDEBUG) || defined(DBG_PERF)
                     Logger *logger
#endif
                       )
#if !defined(NDEBUG) || defined(DBG_PERF)
                    : m_logger(logger)
#endif
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
        if (contents.size() >= 36)
        {
            memcpy(&lastActualSize, &contents[32], 4);
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("MMKV lastActualSize from crc: " + std::to_string(lastActualSize) + " crc file size:" + std::to_string(contents.size()));
#endif
        }
        else
        {
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("MMKV crc file size:" + std::to_string(contents.size()));
#endif
        }
        contents.clear();
    }
    else
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Failed to read MMKV crc file:" + crcPath);
#endif
    }
    
    if (!readFile(path, contents))
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("Failed to read MMKV file:" + path);
#endif
        return false;
    }
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    m_logger->debug("MMKV file size:" + std::to_string(contents.size()));
#endif
    
    uint32_t actualSize = 0;
    if (contents.size() >= 4)
    {
        memcpy(&actualSize, &contents[0], 4);
    }
    if (actualSize <= 0)
    {
        actualSize = lastActualSize;
    }
    
    if (actualSize <= 0)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("MMKV actualSize is less than 0: " + std::to_string(actualSize));
#endif
        return false;
    }

    actualSize += 4;
    if (contents.size() < actualSize)
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("MMKV contents size < actualSize: " + std::to_string(contents.size()) + "/" + std::to_string(actualSize));
#endif
    }
    
    MMKVReader reader(&contents[0], actualSize);
    reader.seek(8);
    
    while (!reader.isAtEnd())
    {
#if !defined(NDEBUG) || defined(DBG_PERF)
        m_logger->debug("MMKV offset: " + std::to_string(reader.getPos()) + "/" + std::to_string(actualSize));
#endif
        // 
        const auto k = reader.readKey();
        if (k.empty())
        {
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("MMKV exception: empty key");
#endif
            break;
        }
        
        if (k == "86")
        {
            m_usrName = reader.readStringValue();
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("MMKV usrName: " + m_usrName);
#endif
        }
        else if (k == "87")
        {
            m_name = reader.readStringValue();
        }
        else if (k == "88")
        {
            m_displayName = reader.readStringValue();
#if !defined(NDEBUG) || defined(DBG_PERF)
            m_logger->debug("MMKV displayName: " + m_displayName);
#endif
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

#ifndef NDEBUG
void FriendsParser::setOutputPath(const std::string& outputPath)
{
    m_outputPath = outputPath;
}
#endif

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
        if (!Friend::isInvalidPortrait(value))
        {
            f.setPortrait(value);
        }
    }
    if (msg.parse("3", value))
    {
        if (!Friend::isInvalidPortrait(value))
        {
            f.setPortraitHD(value);
        }
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

SessionsParser::SessionsParser(ITunesDb *iTunesDb, ITunesDb *iTunesDbShare, const std::string& cellDataVersion, bool detailedInfo/* = true*/) : m_iTunesDb(iTunesDb), m_iTunesDbShare(iTunesDbShare), m_cellDataVersion(cellDataVersion), m_detailedInfo(detailedInfo)
{
    if (cellDataVersion.empty())
    {
        m_cellDataVersion = "V";
    }
}

bool SessionsParser::parse(const Friend& user, const Friends& friends, std::vector<Session>& sessions)
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
        if (NULL != extFileName)
        {
            session.setExtFileName(extFileName);
        }
        else
        {
            // Guess ext file name
            // /session/data/c3/2488b928e0bf604ec1cb02b53f18a7
            std::string relativePath = combinePath(userRoot, "session/data", session.getHash().substr(0, 2), session.getHash().substr(2));
            const ITunesFile* file = m_iTunesDb->findITunesFile(relativePath);
            if (NULL != file)
            {
                session.setExtFileName(file->relativePath.substr(userRoot.size())); // file->relativePath is formatted
            }
            
        }
        session.setUnreadCount(sqlite3_column_int(stmt, 2));
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    // if (m_detailedInfo)
    {
        parseMessageDbs(user, userRoot, sessions);
    }
    
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); )
    {
        if (!it->isExtFileNameEmpty())
        {
            parseCellData(userRoot, *it);
        }
        if (!it->isUsrNameEmpty() && it->isSubscription())
        {
            it = sessions.erase(it);
            continue;
        }

        const Friend* f = friends.getFriend(it->getHash());
        if (NULL != f)
        {
            if (!it->isChatroom())
            {
                it->update(*f);
            }
        }
        ++it;
    }
    
    // Check displayName and avatar
    std::string shareUserRoot = "share/" + usrNameHash;
    parseSessionsInGroupApp(shareUserRoot, sessions);

    int sessionId = 1;
    for (std::vector<Session>::iterator it = sessions.begin(); it != sessions.end(); ++it)
    {
        if (it->isUsrNameEmpty())
        {
            it->setEmptyUsrName("wxid_unknwn_" + std::to_string(sessionId++));
        }
    }

#ifndef NDEBUG
    // Invalidate db path
    int cnt = 0;
    for (std::vector<Session>::const_iterator it = sessions.cbegin(); it != sessions.cend(); ++it)
    {
        if (it->isDbFileEmpty())
        {
            cnt++;
        }
    }
    if (cnt > 0)
    {
        // assert(false);
    }
#endif

    return true;
}

bool SessionsParser::parseMessageDbs(const Friend& user, const std::string& userRoot, std::vector<Session>& sessions)
{
	SessionHashCompare comp;
	std::sort(sessions.begin(), sessions.end(), comp);

    MessageDbFilter filter(userRoot);
    ITunesFileVector dbs = m_iTunesDb->filter(filter);
    const ITunesFile* file = m_iTunesDb->findITunesFile(combinePath(userRoot, "DB", "MM.sqlite"));
    if (NULL != file)
    {
        dbs.push_back(const_cast<ITunesFile *>(file));
    }

    std::vector<Session> deletedSessions;
    std::vector<std::pair<std::string, int>> sessionIds;
    for (ITunesFilesConstIterator it = dbs.cbegin(); it != dbs.cend(); ++it)
    {
        std::string mmPath = m_iTunesDb->getRealPath(*it);
        parseMessageDb(user, mmPath, sessions, deletedSessions);
    }
    
    /*
    // Append deletedSessions at last as parseMessageDb needs SORTED sessions
    for (std::vector<Session>::iterator it = deletedSessions.begin(); it != deletedSessions.end(); ++it)
    {
        // /session/data/c3/2488b928e0bf604ec1cb02b53f18a7
        std::string relativePath = combinePath(userRoot, "/session/data/", it->getHash().substr(0, 2), it->getHash().substr(2));
        const ITunesFile* file = m_iTunesDb->findITunesFile(relativePath);
        if (NULL != file)
        {
            it->setExtFileName(file->relativePath); // it->relativePath is formatted
        }
        
        sessions.push_back(*it);
    }
     */

    return true;
}

bool SessionsParser::parseMessageDb(const Friend& user, const std::string& mmPath, std::vector<Session>& sessions, std::vector<Session>& deletedSessions)
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
    
    SessionHashCompare comp;
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
                
                std::vector<Session>::iterator it = std::lower_bound(sessions.begin(), sessions.end(), chatId, comp);
                if (it != sessions.end() && it->getHash() == chatId)
                {
                    it->setDbFile(mmPath);
                    it->setRecordCount(recordCount);
                }
                else
                {
                    uint32_t lastCreateTime = 0;
                    std::string sql3 = "SELECT MAX(CreateTime) AS lct FROM " + name;
                    sqlite3_stmt* stmt3 = NULL;
                    rc = sqlite3_prepare_v2(db, sql3.c_str(), (int)(sql3.size()), &stmt3, NULL);
                    if (rc == SQLITE_OK)
                    {
                        if (sqlite3_step(stmt3) == SQLITE_ROW)
                        {
                            lastCreateTime = sqlite3_column_int(stmt3, 0);
                        }
                        sqlite3_finalize(stmt3);
                    }

                    it = deletedSessions.emplace(deletedSessions.cend(), "", chatId, &user);
                    it->setDbFile(mmPath);
                    it->setLastMessageTime(lastCreateTime);
                    it->setRecordCount(recordCount);
                }
            }
            
            // sessionIds.push_back(std::make_pair<>(chatId, recordCount));
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
	int value2 = 0;
    if (msg.parse("1.1.1", value))
    {
        if (session.isUsrNameEmpty() && (session.isHashEmpty() || md5(value) == session.getHash()))
        {
            session.setUsrName(value);
        }
    }
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
        if (startsWith(value, "http://") || startsWith(value, "https://"))
        {
            session.setPortrait(value);
        }
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
        if (session.getRecordCount() == 0)
        {
            session.setRecordCount(value2);
        }
    }
    
    if (session.isDisplayNameEmpty())
    {
        SessionCellDataFilter filter(cellDataPath, m_cellDataVersion);
        ITunesFileVector items = m_iTunesDb->filter(filter);

        unsigned int lastModifiedTime = 0;
        for (ITunesFilesConstIterator it = items.cbegin(); it != items.cend(); ++it)
        {
            fileName = m_iTunesDb->getRealPath(*it);
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
        if (!it->isUsrNameEmpty())
        {
            sessionMap.insert(sessionMap.cend(), std::make_pair<>(it->getUsrName(), &(*it)));
        }
    }
    
    SessionHashCompare comp;
    std::sort(sessions.begin(), sessions.end(), comp);
    
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
                                Session* session = NULL;
                                std::map<std::string, Session*>::iterator it = sessionMap.find(value2);
                                if (it != sessionMap.end())
                                {
                                    session = it->second;
                                }
                                else
                                {
                                    std::string uidHash = md5(value2);
                                    std::vector<Session>::iterator it = std::lower_bound(sessions.begin(), sessions.end(), uidHash, comp);
                                    if (it != sessions.end() && it->getHash() == uidHash)
                                    {
                                        session = &(*it);
                                        if (it->isUsrNameEmpty())
                                        {
                                            it->setUsrName(value2);
                                            sessionMap.insert(sessionMap.cend(), std::make_pair<>(value2, session));
                                        }
                                    }
                                }
                                
                                if (NULL != session && session->isDisplayNameEmpty())
                                {
                                    std::string name;
                                    if (msg2.parse("2", value2))
                                    {
                                        name = value2;
                                    }
                                    if (!msg2.parse("3", value2))
                                    {
                                        value2 = "";
                                    }
                                    session->setDisplayName(value2.empty() ? name : value2);
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
                                Session* session = NULL;
                                std::map<std::string, Session*>::iterator it = sessionMap.find(value2);
                                if (it != sessionMap.end())
                                {
                                    session = it->second;
                                }
                                else
                                {
                                    std::string uidHash = md5(value2);
                                    std::vector<Session>::iterator it = std::lower_bound(sessions.begin(), sessions.end(), uidHash, comp);
                                    if (it != sessions.end() && it->getHash() == uidHash)
                                    {
                                        session = &(*it);
                                        if (it->isUsrNameEmpty())
                                        {
                                            it->setUsrName(value2);
                                            sessionMap.insert(sessionMap.cend(), std::make_pair<>(value2, session));
                                        }
                                    }
                                }
                                
                                if (NULL != session && session->isDisplayNameEmpty())
                                {
                                    std::string name;
                                    if (msg2.parse("2", value2))
                                    {
                                        name = value2;
                                    }
                                    if (!msg2.parse("3", value2))
                                    {
                                        value2 = "";
                                    }
                                    session->setDisplayName(value2.empty() ? name : value2);
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

SessionParser::SessionParser(int options) : m_options(options)
{
}

SessionParser::MessageEnumerator* SessionParser::buildMsgEnumerator(const Session& session)
{
    return new MessageEnumerator(session, m_options);
}

struct MSG_ENUMERATOR_CONTEXT
{
    sqlite3* db;
    sqlite3_stmt* stmt;
    
    MSG_ENUMERATOR_CONTEXT(sqlite3* d, sqlite3_stmt* s) : db(d), stmt(s)
    {
        
    }
    
    ~MSG_ENUMERATOR_CONTEXT()
    {
        if (NULL != stmt) sqlite3_finalize(stmt);
        if (NULL != db) sqlite3_close(db);
    }
};

SessionParser::MessageEnumerator::MessageEnumerator(const Session& session, int options)
{
    MSG_ENUMERATOR_CONTEXT* context = new MSG_ENUMERATOR_CONTEXT(NULL, NULL);
    m_context = context;
    
    int rc = openSqlite3ReadOnly(session.getDbFile(), &(context->db));
    if (rc != SQLITE_OK)
    {
        sqlite3_close(context->db);
        context->db = NULL;
        return;
    }
    
    std::string sql = "SELECT CreateTime,Message,Des,Type,MesLocalID FROM Chat_" + session.getHash() + " ORDER BY CreateTime";
    if ((options & SPO_DESC) == SPO_DESC)
    {
        sql += " DESC";
    }
    
    rc = sqlite3_prepare_v2(context->db, sql.c_str(), (int)(sql.size()), &(context->stmt), NULL);
    if (rc != SQLITE_OK)
    {
        sqlite3_close(context->db);
        context->db = NULL;
        return;
    }
}

SessionParser::MessageEnumerator::~MessageEnumerator()
{
    if (NULL != m_context)
    {
        MSG_ENUMERATOR_CONTEXT* context = reinterpret_cast<MSG_ENUMERATOR_CONTEXT *>(m_context);
        if (NULL != context)
        {
            delete context;
        }
        m_context = NULL;
    }
}

bool SessionParser::MessageEnumerator::isInvalid() const
{
    if (NULL != m_context)
    {
        const MSG_ENUMERATOR_CONTEXT* context = reinterpret_cast<const MSG_ENUMERATOR_CONTEXT *>(m_context);
        if (NULL != context)
        {
            return NULL != context->db && NULL != context->stmt;
        }
    }
    
    return false;
}

bool SessionParser::MessageEnumerator::nextMessage(WXMSG& msg)
{
    if (NULL == m_context)
    {
        return NULL;
    }
        
    MSG_ENUMERATOR_CONTEXT* context = reinterpret_cast<MSG_ENUMERATOR_CONTEXT *>(m_context);
    if (NULL == context || NULL == context->db || NULL == context->stmt)
    {
        return false;
    }
    
    if (sqlite3_step(context->stmt) == SQLITE_ROW)
    {
        msg.createTime = sqlite3_column_int(context->stmt, 0);
        const unsigned char* pMessage = sqlite3_column_text(context->stmt, 1);
        if (pMessage != NULL)
        {
            msg.content = reinterpret_cast<const char*>(pMessage);
        }
        else
        {
            msg.content.clear();
        }
        msg.des = sqlite3_column_int(context->stmt, 2);
        msg.type = sqlite3_column_int(context->stmt, 3);
        msg.msgId = std::to_string(sqlite3_column_int(context->stmt, 4));
        
        return true;
    }
    
    return false;
}
