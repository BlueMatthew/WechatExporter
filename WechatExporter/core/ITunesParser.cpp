//
//  ITunesParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "ITunesParser.h"
#include <stdio.h>
#include <map>
#include <sys/types.h>
#include <sqlite3.h>
#include <algorithm>
#include <plist/plist.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#include "OSDef.h"
#include "Utils.h"

inline std::string getPlistStringValue(plist_t node)
{
    std::string value;
    
    if (NULL != node)
    {
        uint64_t length = 0;
        const char* ptr = plist_get_string_ptr(node, &length);
        if (length > 0)
        {
            value.assign(ptr, length);
        }
    }
    
    return value;
}

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
    
    static void startElement(void * ctx, const xmlChar * fullName, const xmlChar ** attrs);
    static void startElementNs(void * ctx, const xmlChar * localName, const xmlChar * prefix, const xmlChar * URI, int nb_namespaces, const xmlChar ** namespaces, int nb_attributes, int nb_defaulted, const xmlChar ** attrs);
    static void endElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI);
    static void characters(void* ctx, const xmlChar * ch, int len);
    
    static std::string toString(const xmlChar* ch);
    static std::string toString(const xmlChar* ch, int len);
    
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

inline std::string PlistDictionary::toString(const xmlChar* ch)
{
    const char *p = reinterpret_cast<const char *>(ch);
    return std::string(p, p + xmlStrlen(ch));
}

inline std::string PlistDictionary::toString(const xmlChar* ch, int len)
{
    const char *p = reinterpret_cast<const char *>(ch);
    return std::string(p, p + len);
}

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

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: start.....%s\r\n", getCurrentTimestamp().c_str());
#endif

    sqlite3_exec(db, "PRAGMA mmap_size=2097152;", NULL, NULL, NULL); // 8M:8388608  2M 2097152
    sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
    
    std::string sql = "SELECT fileID,relativePath,flags,file FROM Files";
    if (domain.size() > 0)
    {
        sql += " WHERE domain=?";
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
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: %s sql=%s, domain=%s\r\n", getCurrentTimestamp().c_str(), sql.c_str(), domain.c_str());
#endif
    
    bool hasFilter = (bool)m_loadingFilter;
    
    m_files.reserve(2048);
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int flags = sqlite3_column_int(stmt, 2);
        if (onlyFile && flags == 2)
        {
            // Putting flags=1 into sql causes sqlite3 to use index of flags instead of domain and don't know why...
            // So filter the directory with the code
            continue;
        }
        
        const char *relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (hasFilter && !m_loadingFilter(relativePath, flags))
        {
            continue;
        }
        
        ITunesFile *file = new ITunesFile();
        const char *fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (NULL != fileId)
        {
            file->fileId = fileId;
        }
        
        if (NULL != relativePath)
        {
            file->relativePath = relativePath;
        }
        file->flags = static_cast<unsigned int>(flags);
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

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: end.....%s, size=%lu\r\n", getCurrentTimestamp().c_str(), m_files.size());
#endif
    
    std::sort(m_files.begin(), m_files.end(), __string_less());
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: after sort.....%s\r\n", getCurrentTimestamp().c_str());
#endif
    return true;
}

unsigned int ITunesDb::parseModifiedTime(const std::vector<unsigned char>& data)
{
    uint64_t val = 0;
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
    if (NULL != node)
    {
        plist_t lastModified = plist_access_path(node, 3, "$objects", 1, "LastModified");
        if (NULL != lastModified)
        {
            plist_get_uint_val(lastModified, &val);
        }
        
        plist_free(node);
    }

    return static_cast<unsigned int>(val);
}

std::string ITunesDb::findFileId(const std::string& relativePath) const
{
    const ITunesFile* file = findITunesFile(relativePath);
    
    if (NULL == file)
    {
        return std::string();
    }
    return file->fileId;
}

const ITunesFile* ITunesDb::findITunesFile(const std::string& relativePath) const
{
    std::string formatedPath = relativePath;
    std::replace(formatedPath.begin(), formatedPath.end(), '\\', '/');

    typename std::vector<ITunesFile *>::iterator it = std::lower_bound(m_files.begin(), m_files.end(), formatedPath, __string_less());
    
    if (it == m_files.end() || (*it)->relativePath != formatedPath)
    {
        return NULL;
    }
    return *it;
}

std::string ITunesDb::fileIdToRealPath(const std::string& fileId) const
{
    if (!fileId.empty())
    {
        return combinePath(m_rootPath, fileId.substr(0, 2), fileId);
    }
    
    return std::string();
}

std::string ITunesDb::getRealPath(const ITunesFile& file) const
{
    return fileIdToRealPath(file.fileId);
}

std::string ITunesDb::findRealPath(const std::string& relativePath) const
{
    std::string fieldId = findFileId(relativePath);
    return fileIdToRealPath(fieldId);
}

ManifestParser::ManifestParser(const std::string& manifestPath, const Shell* shell) : m_manifestPath(manifestPath), m_shell(shell)
{
}

std::string ManifestParser::getLastError() const
{
	return m_lastError;
}

bool ManifestParser::parse(std::vector<BackupManifest>& manifests) const
{
    bool res = parseDirectory(m_manifestPath, manifests);
    if (res)
    {
        return res;
    }
    
    std::string path = normalizePath(m_manifestPath);
    if (endsWith(path, normalizePath("/MobileSync")) || endsWith(path, normalizePath("/MobileSync/")))
    {
        path = combinePath(path, "Backup");
        res = parseDirectory(path, manifests);
    }
    
    return res;
}

bool ManifestParser::parseDirectory(const std::string& path, std::vector<BackupManifest>& manifests) const
{
    std::vector<std::string> subDirectories;
    if (!m_shell->listSubDirectories(path, subDirectories))
    {
#ifndef NDEBUG
#endif
		m_lastError += "Failed to list subfolder in:" + path + "\r\n";
        return false;
    }
    
    bool res = false;
    for (std::vector<std::string>::const_iterator it = subDirectories.cbegin(); it != subDirectories.cend(); ++it)
    {
		if (!isValidBackupId(path, *it))
		{
			continue;
		}
        
        BackupManifest manifest;
        if (parse(path, *it, manifest) && manifest.isValid())
        {
            res = true;
            manifests.push_back(manifest);
        }
    }

	if (!res)
	{
		m_lastError += "No valid backup id found in:" + path + "\r\n";
		// res = false;
	}

    return res;
}

bool ManifestParser::isValidBackupId(const std::string& backupPath, const std::string& backupId) const
{
	std::string path = combinePath(backupPath, backupId);
	std::string fileName = combinePath(path, "Info.plist");

	// if (it->size() != 40)
	// {
	//	return false;
	// }

	if (!m_shell->existsFile(fileName))
	{
		m_lastError += "Info.plist not found\r\n";
		return false;
	}

	fileName = combinePath(path, "Manifest.plist");
	if (!m_shell->existsFile(fileName))
	{
		m_lastError += "Manifest.plist not found\r\n";
		return false;
	}

	fileName = combinePath(path, "Manifest.db");
	if (!m_shell->existsFile(fileName))
	{
		m_lastError += "Manifest.db not found\r\n";
		return false;
	}
	return true;
}

bool ManifestParser::parse(const std::string& backupPath, const std::string& backupId, BackupManifest& manifest) const
{
    //Info.plist is a xml file
    xmlSAXHandler saxHander;
    memset(&saxHander, 0, sizeof(xmlSAXHandler));

    saxHander.initialized = XML_SAX2_MAGIC;
    saxHander.startElementNs = PlistDictionary::startElementNs;
    saxHander.startElement = PlistDictionary::startElement;
    saxHander.endElementNs = PlistDictionary::endElementNs;
    saxHander.characters = PlistDictionary::characters;

    std::string path = combinePath(backupPath, backupId);
    std::string fileName = combinePath(path, "Info.plist");

    const std::string NodePlist = "plist";
    const std::string NodeDict = "dict";
    
    const std::string NodeValueString = "string";
    const std::string NodeValueDate = "date";
    const std::string ValueLastBackupDate = "Last Backup Date";
    const std::string ValueDisplayName = "Display Name";
    const std::string ValueDeviceName = "Device Name";
    const std::string ValueITunesVersion = "iTunes Version";
    const std::string ValueMacOSVersion = "macOS Version";
    
    std::vector<std::string> tags;
    std::vector<std::string> keys;
    
    tags.push_back(NodePlist);
    tags.push_back(NodeDict);
    
    keys.push_back(ValueLastBackupDate);
    keys.push_back(ValueDisplayName);
    keys.push_back(ValueDeviceName);
    keys.push_back(ValueITunesVersion);
    keys.push_back(ValueMacOSVersion);
    
    PlistDictionary plistDict(tags, keys);
    int res = xmlSAXUserParseFile(&saxHander, &plistDict, fileName.c_str());
    xmlCleanupParser();
    if (res != 0)
    {
		m_lastError += "Failed to parse xml: Info.plist\r\n";
        return false;
    }
    
    manifest.setPath(path);
    manifest.setDeviceName(plistDict[ValueDeviceName]);
    manifest.setDisplayName(plistDict[ValueDisplayName]);
    manifest.setITunesVersion(plistDict[ValueITunesVersion]);
    manifest.setMacOSVersion(plistDict[ValueMacOSVersion]);    // Embeded iTunes
    std::string localDate = utcToLocal(plistDict[ValueLastBackupDate]);
    manifest.setBackupTime(localDate);
    
    fileName = combinePath(path, "Manifest.plist");
    std::vector<unsigned char> data;
    if (readFile(fileName, data))
    {
        plist_t node = NULL;
        plist_from_memory(reinterpret_cast<const char *>(&data[0]), static_cast<uint32_t>(data.size()), &node);
        if (NULL != node)
        {
            plist_t isEncryptedNode = plist_access_path(node, 1, "IsEncrypted");
            if (NULL != isEncryptedNode)
            {
                uint8_t val = 0;
                plist_get_bool_val(isEncryptedNode, &val);
                manifest.setEncrypted(val != 0);
            }
            
            plist_free(node);
        }
    }
	else
	{
		m_lastError = "Failed to read Manifest.plist\r\n";
		return false;
	}

    return true;
}

void PlistDictionary::startElement(void * ctx, const xmlChar * fullName, const xmlChar ** attrs)
{
    // NSLog(@"%@", [NSString stringWithUTF8String:localName]);
    std::string fullNameStr = toString(fullName);
}

void PlistDictionary::startElementNs(void * ctx, const xmlChar * localName, const xmlChar * prefix, const xmlChar * URI, int nb_namespaces, const xmlChar ** namespaces, int nb_attributes, int nb_defaulted, const xmlChar ** attrs)
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

void PlistDictionary::endElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI)
{
    // xmlParserCtxtPtr ctxt = reinterpret_cast<xmlParserCtxtPtr>(ctx);
    PlistDictionary* userData = reinterpret_cast<PlistDictionary*>(ctx);
    if (userData)
    {
        userData->endElementNs(toString(localname), toString(prefix), toString(URI));
    }
}

void PlistDictionary::characters(void* ctx, const xmlChar* ch, int len)
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
                    it->second.erase(it->second.find_last_not_of(" \n\r\t") + 1);
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
