//
//  ITunesParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "ITunesParser.h"
#include <stdio.h>
#include <dirent.h>
#include <map>
#include <sys/types.h>
#include <sqlite3.h>
#include <algorithm>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "OSDef.h"
#include "Utils.h"

struct __string_less
{
    // _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11
    bool operator()(const std::string& __x, const std::string& __y) const {return __x < __y;}
    bool operator()(const std::pair<std::string, std::string>& __x, const std::string& __y) const {return __x.first < __y;}
    bool operator()(const ITunesFile* __x, const std::string& __y) const {return __x->relativePath < __y;}
    bool operator()(const ITunesFile* __x, const ITunesFile* __y) const {return __x->relativePath < __y->relativePath;}
};

struct PlistDictionary
{
    PlistDictionary(const std::vector<std::string>& tags, const std::vector<std::string>& nodeNames) : m_tags(tags)
    {
        m_flags = 0;
        for (std::vector<std::string>::const_iterator it = nodeNames.cbegin(); it != nodeNames.cend(); ++it)
        {
            m_values[*it] = "";
        }
    }
    
    // std::vector<std::string> m_tag;
    
    void startElementNs(const std::string& localName, const std::string& prefix, const std::string& URI, const std::vector<std::string>& namespaces, const std::map<std::string, std::string>& attrs);
    void endElementNs(const std::string& localName, const std::string& prefix, const std::string& URI);
    void characters(const std::string& ch);
    
    std::string operator[](const std::string& key) const
    {
        std::map<std::string, std::string>::const_iterator it = m_values.find(key);
        return it != m_values.cend() ? it->second : "";
    }

protected:
    const std::string NodeKey = "key";
    
    std::vector<std::string> m_tags;
    std::vector<std::string> m_tagStack;
    std::string m_curKey;
    std::string m_prevKey;
    std::string m_valueBuffer;
    
    std::map<std::string, std::string> m_values;
    
    bool matchesPath() const
    {
        if (m_tagStack.size() != m_tags.size() + 1)
        {
            return false;
        }
        
        for (int idx = 0; idx < m_tags.size(); ++idx)
        {
            if (m_tags[idx] != m_tagStack[idx])
            {
                return false;
            }
        }
        
        return true;
    }
    
    union
    {
        struct
        {
            unsigned int m_nodeMatched : 1;
            unsigned int m_isNodeKey : 1;
            unsigned int m_isReceiveValue : 1;
        };
        unsigned m_flags;
    };
};

ITunesDb::ITunesDb(const std::string& rootPath, const std::string& manifestFileName) : m_rootPath(rootPath), m_manifestFileName(manifestFileName)
{
    std::replace(m_rootPath.begin(), m_rootPath.end(), DIR_SEP_R, DIR_SEP);
    
    if (!endsWith(m_rootPath, DIR_SEP))
    {
        m_rootPath += DIR_SEP;
    }
}

ITunesDb::~ITunesDb()
{
    for (std::vector<ITunesFile *>::iterator it = m_files.begin(); it != m_files.end(); ++it)
    {
        delete *it;
    }
    m_files.clear();
}

bool ITunesDb::load()
{
    return load("", false);
}

bool ITunesDb::load(const std::string& domain)
{
    return load(domain, false);
}

bool ITunesDb::load(const std::string& domain, bool onlyFile)
{
    std::string dbPath = combinePath(m_rootPath, m_manifestFileName);
    
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(dbPath, &db);
    if (rc != SQLITE_OK)
    {
        // printf("Open database failed!");
        sqlite3_close(db);
        return false;
    }
    
    std::string sql = "SELECT fileID,relativePath,flags,file FROM Files";
    if (domain.size() > 0 && onlyFile)
    {
        sql += " WHERE domain=? AND flags=1";
    }
    else if (domain.size() > 0)
    {
        sql += " WHERE domain=?";
    }
    else if (onlyFile)
    {
        sql += " WHERE flags=1";
    }

    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    
    if (domain.size() > 0)
    {
        rc = sqlite3_bind_text(stmt, 1, domain.c_str(), (int)(domain.size()), NULL);
        if (rc != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
    }
    
    m_files.reserve(2048);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ITunesFile *file = new ITunesFile();
        
        const char *fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != fileId)
        {
            file->fileId = fileId;
        }
        const char *relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (NULL != relativePath)
        {
            file->relativePath = relativePath;
        }
        int flags = sqlite3_column_int(stmt, 2);
        if (flags == 1)
        {
            // Files
            int blobBytes = sqlite3_column_bytes(stmt, 3);
            const unsigned char *blob = reinterpret_cast<const unsigned char*>(sqlite3_column_blob(stmt, 3));
            if (blobBytes > 0 && NULL != blob)
            {
                std::vector<unsigned char> blobVector(blob, blob + blobBytes);
                file->blob.swap(blobVector);
            }
        }
        
        m_files.push_back(file);
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::sort(m_files.begin(), m_files.end(), __string_less());
    
    return true;
}

std::string ITunesDb::findFileId(const std::string& relativePath) const
{
	std::string formatedPath = relativePath;
	std::replace(formatedPath.begin(), formatedPath.end(), '\\', '/');

    typename std::vector<ITunesFile *>::const_iterator it = std::lower_bound(m_files.begin(), m_files.end(), formatedPath, __string_less());
    
    if (it == m_files.end() || (*it)->relativePath != formatedPath)
    {
        return std::string();
    }
    return (*it)->fileId;
}

std::string ITunesDb::fileIdToRealPath(const std::string& fileId) const
{
    if (!fileId.empty())
    {
        return combinePath(m_rootPath, fileId.substr(0, 2), fileId);
    }
    
    return std::string();
}

std::string ITunesDb::findRealPath(const std::string& relativePath) const
{
    std::string fieldId = findFileId(relativePath);
    return fileIdToRealPath(fieldId);
}

ManifestParser::ManifestParser(const std::string& manifestPath, const std::string& xml) : m_manifestPath(manifestPath), m_xml(xml)
{
    
}

std::vector<BackupManifest> ManifestParser::parse()
{
    std::vector<BackupManifest> manifests;
    
    struct dirent *entry;
    DIR *dir = opendir(m_manifestPath.c_str());
    if (dir == NULL)
    {
        return manifests;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strlen(entry->d_name) != 40)
        {
            continue;
        }
        std::string subDir = entry->d_name;
        
        BackupManifest manifest = parse(entry->d_name);
        if (manifest.isValid())
        {
            manifests.push_back(manifest);
        }
    }
    closedir(dir);

    return manifests;
}

BackupManifest ManifestParser::parse(const std::string& backupId)
{
    xmlSAXHandler saxHander;
    memset(&saxHander, 0, sizeof(xmlSAXHandler));

    saxHander.initialized = XML_SAX2_MAGIC;
    saxHander.startElementNs = startElementNs;
    saxHander.startElement = startElement;
    saxHander.endElementNs = endElementNs;
    saxHander.characters = characters;

    std::string path = combinePath(m_manifestPath, backupId);
    std::string fileName = combinePath(path, m_xml);
    
    const std::string NodePlist = "plist";
    const std::string NodeDict = "dict";
    
    const std::string NodeValueString = "string";
    const std::string NodeValueDate = "date";
    const std::string ValueLastBackupDate = "Last Backup Date";
    const std::string ValueDisplayName = "Display Name";
    const std::string ValueDeviceName = "Device Name";
    
    std::vector<std::string> tags;
    std::vector<std::string> keys;
    
    tags.push_back(NodePlist);
    tags.push_back(NodeDict);
    
    keys.push_back(ValueLastBackupDate);
    keys.push_back(ValueDisplayName);
    keys.push_back(ValueDeviceName);
    
    PlistDictionary plistDict(tags, keys);
    int res = xmlSAXUserParseFile(&saxHander, &plistDict, fileName.c_str());
    if (res == 0)
    {
    }
   
    xmlCleanupParser();
    
    BackupManifest manifest(path, plistDict[ValueDeviceName], plistDict[ValueDisplayName], plistDict[ValueLastBackupDate]);

    return manifest;
}

void ManifestParser::startElement(void * ctx, const xmlChar * fullName, const xmlChar ** attrs)
{
    // NSLog(@"%@", [NSString stringWithUTF8String:localName]);
    std::string fullNameStr = toString(fullName);
}

void ManifestParser::startElementNs(void * ctx, const xmlChar * localName, const xmlChar * prefix, const xmlChar * URI, int nb_namespaces, const xmlChar ** namespaces, int nb_attributes, int nb_defaulted, const xmlChar ** attrs)
{
    // NSLog(@"%@", [NSString stringWithUTF8String:localName]);
    
    std::vector<std::string> ns;
    std::map<std::string, std::string> as;
    
    int idx;
    ns.reserve(nb_namespaces);
    for (idx = 0; idx < nb_namespaces; ++idx)
    {
        ns.push_back(toString(namespaces[idx]));
    }
    // as.reserve(nb_attributes + nb_defaulted);
    for (idx = 0; idx < nb_namespaces; ++idx)
    {
        // ns(toString(attrs[idx]));
    }
    
    // xmlParserCtxtPtr ctxt = reinterpret_cast<xmlParserCtxtPtr>(ctx);
    // __ManifestUserData* userData = reinterpret_cast<__ManifestUserData*>(ctxt->userData);
    PlistDictionary* userData = reinterpret_cast<PlistDictionary*>(ctx);
    if (userData)
    {
        userData->startElementNs(toString(localName), toString(prefix), toString(URI), ns, as);
    }
}

void ManifestParser::endElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI)
{
    // xmlParserCtxtPtr ctxt = reinterpret_cast<xmlParserCtxtPtr>(ctx);
    PlistDictionary* userData = reinterpret_cast<PlistDictionary*>(ctx);
    if (userData)
    {
        userData->endElementNs(toString(localname), toString(prefix), toString(URI));
    }
}

void ManifestParser::characters(void* ctx, const xmlChar* ch, int len)
{
    // xmlParserCtxtPtr ctxt = reinterpret_cast<xmlParserCtxtPtr>(ctx);
    PlistDictionary* userData = reinterpret_cast<PlistDictionary*>(ctx);
    if (userData)
    {
        userData->characters(toString(ch, len));
    }
}

void PlistDictionary::startElementNs(const std::string& localName, const std::string& prefix, const std::string& URI, const std::vector<std::string>& namespaces, const std::map<std::string, std::string>& attrs)
{
    m_tagStack.push_back(localName);
    
    m_curKey = "";
    m_valueBuffer.clear();

    m_nodeMatched = matchesPath();
    m_isNodeKey = m_nodeMatched && m_tagStack.back() == NodeKey;
    m_isReceiveValue = m_nodeMatched && !m_prevKey.empty() && m_values.find(m_prevKey) != m_values.cend();
}

void PlistDictionary::endElementNs(const std::string& localName, const std::string& prefix, const std::string& URI)
{
    if (m_nodeMatched)
    {
        if (m_isNodeKey)
        {
            m_prevKey = m_valueBuffer;
        }
        else
        {
            if (!m_valueBuffer.empty())
            {
                std::map<std::string, std::string>::iterator it = m_values.find(m_prevKey);
                if (it != m_values.cend())
                {
                    it->second = m_valueBuffer;
                }
            }
        }
    }

    m_tagStack.pop_back();
    // m_prevKey = m_curKey;
}

void PlistDictionary::characters(const std::string& ch)
{
    if (m_flags)
    {
        m_valueBuffer.append(ch);
    }
}
