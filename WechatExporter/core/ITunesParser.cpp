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

#include <sys/stat.h>
#if defined(_WIN32)
// #define S_IFMT          0170000         /* [XSI] type of file mask */
// #define S_IFDIR         0040000         /* [XSI] directory */
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)     /* directory */

#else
#include <unistd.h>
#endif

#include "MbdbReader.h"
#include "OSDef.h"
#include "Utils.h"
#include "FileSystem.h"

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

ITunesDb::ITunesDb(const std::string& rootPath, const std::string& manifestFileName) : m_isMbdb(false), m_rootPath(rootPath), m_manifestFileName(manifestFileName)
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
    m_version.clear();
    BackupManifest manifest;
    if (ManifestParser::parseInfoPlist(m_rootPath, manifest))
    {
        m_version = manifest.getITunesVersion();
        m_iOSVersion = manifest.getIOSVersion();
    }
    
    std::string dbPath = combinePath(m_rootPath, "Manifest.mbdb");
    if (existsFile(dbPath))
    {
        m_isMbdb = true;
        return loadMbdb(domain, onlyFile);
    }
    
    m_isMbdb = false;
    dbPath = combinePath(m_rootPath, "Manifest.db");
    
    sqlite3 *db = NULL;
    int rc = openSqlite3ReadOnly(dbPath, &db);
    if (rc != SQLITE_OK)
    {
        // printf("Open database failed!");
        sqlite3_close(db);
        return false;
    }

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: start.....%s\r\n", getTimestampString(false, true).c_str());
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
    printf("PERF: %s sql=%s, domain=%s\r\n", getTimestampString(false, true).c_str(), sql.c_str(), domain.c_str());
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
    printf("PERF: end.....%s, size=%lu\r\n", getTimestampString(false, true).c_str(), m_files.size());
#endif
    
    std::sort(m_files.begin(), m_files.end(), __string_less());
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: after sort.....%s\r\n", getTimestampString(false, true).c_str());
#endif
    return true;
}

bool ITunesDb::loadMbdb(const std::string& domain, bool onlyFile)
{
    MbdbReader reader;
    if (!reader.open(combinePath(m_rootPath, "Manifest.mbdb")))
    {
        return false;
    }
    
    unsigned char mdbxBuffer[26];           // buffer for .mbdx record
    unsigned char fixedData[40] = { 0 };    // buffer for the fixed part of .mbdb record
    // SHA1CryptoServiceProvider hasher = new SHA1CryptoServiceProvider();

    // System.DateTime unixEpoch = new System.DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc);

    std::string domainInFile;
    std::string path;
    std::string linkTarget;
    std::string dataHash;
    std::string alwaysNull;
    unsigned short fileMode = 0;
    bool isDir = false;
    unsigned int mtime = 0;
    bool skipped = false;
    
    bool hasFilter = (bool)m_loadingFilter;

    while (reader.hasMoreData())
    {
        if (!reader.read(domainInFile))
        {
            break;
        }
        
        skipped = false;
        if (!domain.empty() && domain != domainInFile)
        {
            skipped = true;
        }
        
        if (skipped)
        {
            // will skip it
            reader.skipString();    // path
            reader.skipString();    // linkTarget
            reader.skipString();    // dataHash
            reader.skipString();    // alwaysNull;
            
            reader.read(fixedData, 40);
            int propertyCount = fixedData[39];

            for (int j = 0; j < propertyCount; ++j)
            {
                reader.skipString();
                reader.skipString();
            }
        }
        else
        {
            reader.read(path);
            reader.read(linkTarget);
            reader.readD(dataHash);
            reader.readD(alwaysNull);
            
            reader.read(fixedData, 40);

            fileMode = (fixedData[0] << 8) | fixedData[1];
            
            isDir = S_ISDIR(fileMode);
            
            // unsigned char flags = fixedData[38];
            
            if (onlyFile && isDir)
            {
                skipped = true;
            }
            
            if (hasFilter && !m_loadingFilter(path.c_str(), (isDir ? 2 : 1)))
            {
                skipped = true;
            }
            
            unsigned int aTime = GetBigEndianInteger(fixedData, 18);
            unsigned int bTime = GetBigEndianInteger(fixedData, 22);
            unsigned int cTime = GetBigEndianInteger(fixedData, 26);
            
            int propertyCount = fixedData[39];
            
            // rec.Properties = new MBFileRecord.Property[rec.PropertyCount];
            for (int j = 0; j < propertyCount; ++j)
            {
                if (skipped)
                {
                    reader.skipString(); // name
                    reader.skipString(); // value
                }
                else
                {
                    std::string name;
                    std::string value;
                    
                    reader.read(name);
                    reader.read(value);
                }
                
            }
            /*
            StringBuilder fileName = new StringBuilder();
            byte[] fb = hasher.ComputeHash(ASCIIEncoding.UTF8.GetBytes(rec.Domain + "-" + rec.Path));
            for (int k = 0; k < fb.Length; k++)
            {
                fileName.Append(fb[k].ToString("x2"));
            }

            rec.key = fileName.ToString();
             */
            
            if (!skipped)
            {
                ITunesFile *file = new ITunesFile();
                file->relativePath = path;
                file->fileId = sha1(domain + "-" + path);
                file->flags = isDir ? 2 : 1;
                file->modifiedTime = aTime != 0 ? aTime : bTime;
                
                m_files.push_back(file);
            }
            
        }
        
        
    }
    
    std::sort(m_files.begin(), m_files.end(), __string_less());

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
        return m_isMbdb ? combinePath(m_rootPath, fileId) : combinePath(m_rootPath, fileId.substr(0, 2), fileId);
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

bool ITunesDb::copyFile(const std::string& vpath, const std::string& dest, bool overwrite/* = false*/) const
{
    std::string destPath = normalizePath(dest);
    if (!overwrite && existsFile(destPath))
    {
        return true;
    }
    
    const ITunesFile* file = findITunesFile(vpath);
    if (NULL != file)
    {
        std::string srcPath = getRealPath(*file);
        if (!srcPath.empty())
        {
            normalizePath(srcPath);
            bool result = ::copyFile(srcPath, destPath, true);
            if (result)
            {
                updateFileTime(dest, ITunesDb::parseModifiedTime(file->blob));
            }
            return result;
        }
    }
    
    return false;
}

bool ITunesDb::copyFile(const std::string& vpath, const std::string& destPath, const std::string& destFileName, bool overwrite/* = false*/) const
{
    std::string destFullPath = normalizePath(combinePath(destPath, destFileName));
    if (!overwrite && existsFile(destFullPath))
    {
        return true;
    }
    
    const ITunesFile* file = findITunesFile(vpath);
    if (NULL != file)
    {
        std::string srcPath = getRealPath(*file);
        if (!srcPath.empty())
        {
            normalizePath(srcPath);
            if (!existsDirectory(destPath))
            {
                makeDirectory(destPath);
            }
            bool result = ::copyFile(srcPath, destFullPath, true);
            if (result)
            {
                updateFileTime(destFullPath, ITunesDb::parseModifiedTime(file->blob));
            }
            return result;
        }
    }
    
    return false;
}

ManifestParser::ManifestParser(const std::string& manifestPath) : m_manifestPath(manifestPath)
{
}

std::string ManifestParser::getLastError() const
{
	return m_lastError;
}

bool ManifestParser::parse(std::vector<BackupManifest>& manifests) const
{
    bool res = false;
    
    std::string path = normalizePath(m_manifestPath);
    if (endsWith(path, normalizePath("/MobileSync")) || endsWith(path, normalizePath("/MobileSync/")))
    {
        path = combinePath(path, "Backup");
        res = parseDirectory(path, manifests);
    }
    else if (isValidBackupItem(path))
    {
        BackupManifest manifest;
        if (parse(path, manifest) && manifest.isValid())
        {
            res = true;
            manifests.push_back(manifest);
        }
    }
    else
    {
        // Assume the directory is ../../Backup/../
        res = parseDirectory(path, manifests);
    }
    
    return res;
}

bool ManifestParser::parseDirectory(const std::string& path, std::vector<BackupManifest>& manifests) const
{
    std::vector<std::string> subDirectories;
    if (!listSubDirectories(path, subDirectories))
    {
#ifndef NDEBUG
#endif
		m_lastError += "Failed to list subfolder in:" + path + "\r\n";
        return false;
    }
    
    bool res = false;
    for (std::vector<std::string>::const_iterator it = subDirectories.cbegin(); it != subDirectories.cend(); ++it)
    {
        std::string backupPath = combinePath(path, *it);
		if (!isValidBackupItem(backupPath))
		{
			continue;
		}
        
        BackupManifest manifest;
        if (parse(backupPath, manifest) && manifest.isValid())
        {
            res = true;
            manifests.push_back(manifest);
        }
    }

	if (!res)
	{
		m_lastError += "No valid backup id found in:" + path + "\r\n";
	}

    return res;
}

bool ManifestParser::isValidBackupItem(const std::string& path) const
{
    std::string fileName = combinePath(path, "Info.plist");
    if (!existsFile(fileName))
    {
        m_lastError += "Info.plist not found\r\n";
        return false;
    }

    fileName = combinePath(path, "Manifest.plist");
    if (!existsFile(fileName))
    {
        m_lastError += "Manifest.plist not found\r\n";
        return false;
    }

    // < iOS 10: Manifest.mbdb
    // >= iOS 10: Manifest.db
    if (!existsFile(combinePath(path, "Manifest.db")) && !existsFile(combinePath(path, "Manifest.mbdb")))
    {
        m_lastError += "Manifest.db/Manifest.mbdb not found\r\n";
        return false;
    }
    
    return true;
}

bool ManifestParser::parse(const std::string& path, BackupManifest& manifest) const
{
    //Info.plist is a xml file
    if (!parseInfoPlist(path, manifest))
    {
		m_lastError += "Failed to parse xml: Info.plist\r\n";
        return false;
    }
    
    std::string fileName = combinePath(path, "Manifest.plist");
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
            
            if (manifest.getIOSVersion().empty())
            {
                plist_t iOSVersionNode = plist_access_path(node, 2, "Lockdown", "ProductVersion");
                if (NULL != iOSVersionNode)
                {
                    uint64_t length = 0;
                    const char* ptr = plist_get_string_ptr(iOSVersionNode, &length);
                    if (ptr != NULL && length > 0)
                    {
                        manifest.setIOSVersion(std::string(ptr));
                    }
                }
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

bool ManifestParser::parseInfoPlist(const std::string& backupIdPath, BackupManifest& manifest)
{
    //Info.plist is a xml file
    xmlSAXHandler saxHander;
    memset(&saxHander, 0, sizeof(xmlSAXHandler));

    saxHander.initialized = XML_SAX2_MAGIC;
    saxHander.startElementNs = PlistDictionary::startElementNs;
    saxHander.startElement = PlistDictionary::startElement;
    saxHander.endElementNs = PlistDictionary::endElementNs;
    saxHander.characters = PlistDictionary::characters;

    std::string fileName = combinePath(backupIdPath, "Info.plist");

    const std::string NodePlist = "plist";
    const std::string NodeDict = "dict";
    
    const std::string NodeValueString = "string";
    const std::string NodeValueDate = "date";
    const std::string ValueLastBackupDate = "Last Backup Date";
    const std::string ValueDisplayName = "Display Name";
    const std::string ValueDeviceName = "Device Name";
    const std::string ValueITunesVersion = "iTunes Version";
    const std::string ValueMacOSVersion = "macOS Version";
    const std::string ValueProductVersion = "Product Version";
    
    std::vector<std::string> tags = {NodePlist, NodeDict};
    std::vector<std::string> keys = {ValueLastBackupDate, ValueDisplayName, ValueDeviceName, ValueITunesVersion, ValueMacOSVersion, ValueProductVersion};
    
    PlistDictionary plistDict(tags, keys);
    int res = xmlSAXUserParseFile(&saxHander, &plistDict, fileName.c_str());
    xmlCleanupParser();
    if (res != 0)
    {
        // m_lastError += "Failed to parse xml: Info.plist\r\n";
        return false;
    }
    
    manifest.setPath(backupIdPath);
    manifest.setDeviceName(plistDict[ValueDeviceName]);
    manifest.setDisplayName(plistDict[ValueDisplayName]);
    manifest.setITunesVersion(plistDict[ValueITunesVersion]);
    manifest.setMacOSVersion(plistDict[ValueMacOSVersion]);    // Embeded iTunes
    manifest.setIOSVersion(plistDict[ValueProductVersion]);    // Embeded iTunes
    std::string localDate = utcToLocal(plistDict[ValueLastBackupDate]);
    manifest.setBackupTime(localDate);
    
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
