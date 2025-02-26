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
#include <queue>
#include <set>
#include <sys/types.h>
#include <sqlite3.h>
#include <algorithm>
#include <plist/plist.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

#ifndef NDEBUG
#include <assert.h>
#endif
#include <sys/stat.h>
#if defined(_WIN32)
// #define S_IFMT          0170000         /* [XSI] type of file mask */
// #define S_IFDIR         0040000         /* [XSI] directory */

#include <shlwapi.h>
#include <atlstr.h>

#else
#include <unistd.h>
#include <dirent.h>

#endif

#include "MbdbReader.h"
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

inline std::string getPlistStringValue(plist_t node, const char* key)
{
    std::string value;
    
    if (NULL != node)
    {
        plist_t subNode = plist_dict_get_item(node, key);
        if (NULL != subNode)
        {
            uint64_t length = 0;
            const char* ptr = plist_get_string_ptr(subNode, &length);
            if (length > 0)
            {
                value.assign(ptr, length);
            }
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

class SqliteITunesFileEnumerator : public ITunesDb::ITunesFileEnumerator
{
public:
    SqliteITunesFileEnumerator(const std::string& dbPath, const std::vector<std::string>& domains, bool onlyFile) : m_db(NULL), m_stmt(NULL), m_onlyFile(onlyFile)
    {
#ifndef NDEBUG
        if (!existsFile(dbPath))
        {
            return;
        }
#endif
        int rc = openSqlite3Database(dbPath, &m_db);
        if (rc != SQLITE_OK)
        {
            // printf("Open database failed!");
            closeDb();
            return;
        }

        sqlite3_exec(m_db, "PRAGMA mmap_size=268435456;", NULL, NULL, NULL); // 256M:268435456  2M 2097152
        sqlite3_exec(m_db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
        
        std::string sql = "SELECT fileID,domain,relativePath,flags,file FROM Files";
        if (domains.size() > 0)
        {
            sql += " WHERE ";
            // domain=?";
            std::vector<std::string> conditions(domains.size(), "domain=?");
                
            sql += join(conditions, " OR ");
        }

        rc = sqlite3_prepare_v2(m_db, sql.c_str(), (int)(sql.size()), &m_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::string error = sqlite3_errmsg(m_db);
            closeDb();
            return;
        }
        
        int idx = 1;
        for (std::vector<std::string>::const_iterator it = domains.cbegin(); it != domains.cend(); ++it, ++idx)
        {
            rc = sqlite3_bind_text(m_stmt, idx, (*it).c_str(), (int)((*it).size()), NULL);
            if (rc != SQLITE_OK)
            {
                finalizeStmt();
                closeDb();
                return;
            }
        }
    }
    
    virtual bool isInvalid() const
    {
        return NULL == m_db || NULL == m_stmt;
    }
    
    virtual bool nextFile(ITunesFile& file)
    {
        while (sqlite3_step(m_stmt) == SQLITE_ROW)
        {
            int flags = sqlite3_column_int(m_stmt, 3);
            if (m_onlyFile && flags != 1)
            {
                // Putting flags=1 into sql causes sqlite3 to use index of flags instead of domain and don't know why...
                // So filter the directory with the code
                continue;
            }
            
            const char *relativePath = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, 2));
            if (NULL != relativePath)
            {
                file.relativePath = relativePath;
            }
            else
            {
                file.relativePath.clear();
            }
            const char *domain = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, 1));
            if (NULL != domain)
            {
                file.domain = domain;
            }
            else
            {
                file.domain.clear();
            }
            const char *fileId = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, 0));
            if (NULL != fileId)
            {
                file.fileId = fileId;
            }
            else
            {
                file.fileId.clear();
            }

            file.flags = static_cast<unsigned int>(flags);
            // Files
            const unsigned char *blob = reinterpret_cast<const unsigned char*>(sqlite3_column_blob(m_stmt, 4));
            int blobBytes = sqlite3_column_bytes(m_stmt, 4);
            file.blob.clear();
            file.size = 0;
            file.modifiedTime = 0;
            if (blobBytes > 0 && NULL != blob)
            {
                std::vector<unsigned char> blobVector(blob, blob + blobBytes);
                file.blob.insert(file.blob.end(), blob, blob + blobBytes);
            }
            file.blobParsed = false;

			return true;
            // break;
        }

        return false;
    }
    
    virtual ~SqliteITunesFileEnumerator()
    {
        finalizeStmt();
        closeDb();
    }
    
private:
    void closeDb()
    {
        if (NULL != m_db)
        {
            sqlite3_close(m_db);
            m_db = NULL;
        }
    }
    
    void finalizeStmt()
    {
        if (NULL != m_stmt)
        {
            sqlite3_finalize(m_stmt);
            m_stmt = NULL;
        }
    }
private:
    sqlite3*        m_db;
    sqlite3_stmt*   m_stmt;
    
    bool m_onlyFile;
};

class MbdbITunesFileEnumerator : public ITunesDb::ITunesFileEnumerator
{
public:
    MbdbITunesFileEnumerator(const std::string& dbPath, const std::vector<std::string>& domains, bool onlyFile) : m_valid(false), m_domains(domains), m_onlyFile(onlyFile)
    {
        std::memset(m_fixedData, 0, 40);
        if (!m_reader.open(dbPath))
        {
            return;
        }
        
        m_valid = true;
    }
    
    virtual bool isInvalid() const
    {
        return !m_valid;
    }
    
    virtual bool nextFile(ITunesFile& file)
    {
        std::string domainInFile;
        std::string path;
        std::string linkTarget;
        std::string dataHash;
        std::string alwaysNull;
        unsigned short fileMode = 0;
        bool isDir = false;
        bool skipped = false;
        
        // bool hasFilter = (bool)m_loadingFilter;

        while (m_reader.hasMoreData())
        {
            if (!m_reader.read(domainInFile))
            {
                break;
            }
            
            skipped = false;
            if (!existsDomain(domainInFile))
            {
                skipped = true;
            }
            
            if (skipped)
            {
                // will skip it
                m_reader.skipString();    // path
                m_reader.skipString();    // linkTarget
                m_reader.skipString();    // dataHash
                m_reader.skipString();    // alwaysNull;
                
                m_reader.read(m_fixedData, 40);
                int propertyCount = m_fixedData[39];

                for (int j = 0; j < propertyCount; ++j)
                {
                    m_reader.skipString();
                    m_reader.skipString();
                }
            }
            else
            {
                m_reader.read(path);
                m_reader.read(linkTarget);
                m_reader.readD(dataHash);
                m_reader.readD(alwaysNull);
                
                m_reader.read(m_fixedData, 40);

                fileMode = (m_fixedData[0] << 8) | m_fixedData[1];
                
                isDir = S_ISDIR(fileMode);
                
                // unsigned char flags = fixedData[38];
                
                if (m_onlyFile && isDir)
                {
                    skipped = true;
                }
                
                unsigned int aTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 18)));
                unsigned int bTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 22)));
                // unsigned int cTime = bigEndianToNative(*((uint32_t *)(m_fixedData + 26)));
                
                file.size = bigEndianToNative(*((int64_t *)(m_fixedData + 30)));
                
                int propertyCount = m_fixedData[39];
                
                for (int j = 0; j < propertyCount; ++j)
                {
                    if (skipped)
                    {
                        m_reader.skipString(); // name
                        m_reader.skipString(); // value
                    }
                    else
                    {
                        std::string name;
                        std::string value;
                        
                        m_reader.read(name);
                        m_reader.read(value);
                    }
                }
                
                if (!skipped)
                {
                    file.relativePath = path;
                    file.domain = domainInFile;
                    file.fileId = sha1(domainInFile + "-" + path);
                    file.flags = isDir ? 2 : 1;
                    file.modifiedTime = aTime != 0 ? aTime : bTime;
                    file.blobParsed = true;
                    // file.size =
                    
                    return true;
                }
                
            }
        }
        
        return false;
    }
    
    virtual ~MbdbITunesFileEnumerator()
    {
    }
    
private:
    bool existsDomain(const std::string& domain) const
    {
        if (m_domains.empty())
        {
            return true;
        }
        
        for (std::vector<std::string>::const_iterator it = m_domains.cbegin(); it != m_domains.cend(); ++it)
        {
            if (domain == *it)
            {
                return true;
            }
        }
        return false;
    }

private:
    MbdbReader                  m_reader;
    bool                        m_valid;
    std::vector<std::string>    m_domains;
    bool                        m_onlyFile;
    unsigned char               m_fixedData[40];
};

ITunesDb::ITunesDb(const std::string& rootPath, const std::string& manifestFileName) : m_isMbdb(false), m_rootPath(rootPath), m_manifestFileName(manifestFileName)
{
    std::replace(m_rootPath.begin(), m_rootPath.end(), ALT_DIR_SEP, DIR_SEP);
    
    if (!endsWith(m_rootPath, DIR_SEP))
    {
        m_rootPath += DIR_SEP;
    }
    
    m_version.clear();
    BackupItem manifest;
    if (ManifestParser::parseInfoPlist(m_rootPath, manifest, false))
    {
        m_version = manifest.getITunesVersion();
        m_iOSVersion = manifest.getIOSVersion();
    }
    
    std::string dbPath = combinePath(m_rootPath, "Manifest.mbdb");
    if (existsFile(dbPath))
    {
        m_isMbdb = true;
        
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
    std::vector<std::string> domains;
    if (!domain.empty())
    {
        domains.push_back(domain);
    }
    
#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: start.....%s\r\n", getTimestampString(false, true).c_str());
#endif
    
    std::unique_ptr<ITunesFileEnumerator> enumerator(buildEnumerator(domains, onlyFile));
    if (enumerator->isInvalid())
    {
        return false;
    }

    bool hasFilter = (bool)m_loadingFilter;
    
    ITunesFile file;
    m_files.reserve(2048);
    while (enumerator->nextFile(file))
    {
        if (hasFilter && !m_loadingFilter(file.relativePath.c_str(), file.flags))
        {
            continue;
        }
        m_files.push_back(new ITunesFile(file));
    }

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: end.....%s, size=%lu\r\n", getTimestampString(false, true).c_str(), m_files.size());
#endif
    
    std::sort(m_files.begin(), m_files.end(), __string_less());

#if !defined(NDEBUG) || defined(DBG_PERF)
    printf("PERF: after sort.....%s\r\n", getTimestampString(false, true).c_str());
#endif
    return true;
}

ITunesDb::ITunesFileEnumerator* ITunesDb::buildEnumerator(const std::vector<std::string>& domains, bool onlyFile) const
{
    std::string dbPath = combinePath(m_rootPath, m_isMbdb ? "Manifest.mbdb" : "Manifest.db");
    ITunesFileEnumerator* enumerator = m_isMbdb ? (ITunesFileEnumerator*)(new MbdbITunesFileEnumerator(dbPath, domains, onlyFile)) : (ITunesFileEnumerator*)(new SqliteITunesFileEnumerator(dbPath, domains, onlyFile));
    return enumerator;
}

ITunesDb::ITunesFileEnumerator* ITunesDb::buildEnumerator(const std::string& dbPath, const std::vector<std::string>& domains, bool onlyFile) const
{
    ITunesFileEnumerator* enumerator = m_isMbdb ? (ITunesFileEnumerator*)(new MbdbITunesFileEnumerator(dbPath, domains, onlyFile)) : (ITunesFileEnumerator*)(new SqliteITunesFileEnumerator(dbPath, domains, onlyFile));
    return enumerator;
}

bool ITunesDb::copy(const std::string& destPath, const std::string& backupId, std::vector<std::string>& domains, std::function<bool(const ITunesDb*, const ITunesFile*)>& func) const
{
    std::string destBackupPath = backupId.empty() ? combinePath(destPath, "Backup") : combinePath(destPath, "Backup", backupId);
    if (!existsDirectory(destBackupPath))
    {
        makeDirectory(destBackupPath);
    }
    
    // Copy control files
    const char* files[] = {"Info.plist", (m_isMbdb ? "Manifest.mbdb" : "Manifest.db"), "Manifest.plist", "Status.plist"};
    for (int idx = 0; idx < sizeof(files) / sizeof(const char *); ++idx)
    {
        ::copyFile(combinePath(m_rootPath, files[idx]), combinePath(destBackupPath, files[idx]));
    }
    
    std::unique_ptr<ITunesFileEnumerator> enumerator;
    if (m_isMbdb)
    {
        enumerator.reset(buildEnumerator(combinePath(destBackupPath, "Manifest.mbdb"), domains, false));
    }
    else
    {
        std::string dbPath = combinePath(destBackupPath, "Manifest.db");
        
        sqlite3 *db = NULL;
        int rc = openSqlite3Database(dbPath, &db, false);
        if (rc != SQLITE_OK)
        {
            // printf("Open database failed!");
            sqlite3_close(db);
            return false;
        }

        sqlite3_exec(db, "PRAGMA mmap_size=268435456;", NULL, NULL, NULL); // 256M:268435456  2M 2097152
        sqlite3_exec(db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
        
        std::string sql = "DELETE FROM Files";
        
        if (domains.size() > 0)
        {
            sql += " WHERE ";
            // domain=?";
            std::vector<std::string> conditions(domains.size(), "domain<>?");
                
            sql += join(conditions, " AND ");
        }

        sqlite3_stmt* stmt = NULL;
        rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::string error = sqlite3_errmsg(db);
            sqlite3_close(db);
            return false;
        }

        int idx = 1;
        for (std::vector<std::string>::const_iterator it = domains.cbegin(); it != domains.cend(); ++it, ++idx)
        {
            rc = sqlite3_bind_text(stmt, idx, (*it).c_str(), (int)((*it).size()), NULL);
            if (rc != SQLITE_OK)
            {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }
        }
        
    #if !defined(NDEBUG)
    #ifdef __APPLE__
        if (__builtin_available(macOS 10.12, *))
        {
    #endif
            char *expandedSql = sqlite3_expanded_sql(stmt);
            printf("PERF: %s sql=%s\r\n", getTimestampString(false, true).c_str(), sqlite3_expanded_sql(stmt));
    #ifdef __APPLE__
        }
    #endif
    #endif
        
        if ((rc = sqlite3_step(stmt)) != SQLITE_DONE)
        {
    #ifndef NDEBUG
            const char *errMsg = sqlite3_errmsg(db);
            m_lastError = std::string(errMsg);
    #endif
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        
        sqlite3_finalize(stmt);
        
        sqlite3_exec(db, "VACUUM;", NULL, NULL, NULL);
		sqlite3_close(db);
        
        enumerator.reset(buildEnumerator(dbPath, std::vector<std::string>(), false));
    }
    
    if (enumerator->isInvalid())
    {
        return false;
    }
    
    std::string subPath;
    std::string prefix;
    std::string srcFilePath;
    std::set<std::string> subFolders;
    ITunesFile file;
    while (enumerator->nextFile(file))
    {
        if (file.fileId.empty())
        {
            continue;
        }
        
		prefix = file.fileId.substr(0, 2);
        
        srcFilePath = combinePath(m_rootPath, prefix, file.fileId);
        if (!existsFile(srcFilePath))
        {
#ifndef NDEBUG
            // assert(!"Source file not exists.");
#endif
            continue;
        }

        subPath = combinePath(destBackupPath, prefix);
        if (subFolders.find(prefix) == subFolders.cend())
        {
            if (!existsDirectory(subPath))
            {
                makeDirectory(subPath);
            }
            subFolders.insert(prefix);
        }
        bool ret = ::copyFile(srcFilePath, combinePath(subPath, file.fileId));
#ifndef NDEBUG
        if (!ret)
        {
            std::string msg = "Failed to copy file" + combinePath(subPath, file.fileId);
            assert(!msg.c_str());
        }
#endif
        
        if (func)
        {
            if (!func(this, &file))
            {
                break;
            }
        }
    }

    return true;
}

unsigned int ITunesDb::parseModifiedTime(const std::vector<unsigned char>& data)
{
    if (data.empty())
    {
        return 0;
    }
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

bool ITunesDb::parseFileInfo(const ITunesFile* file)
{
    if (NULL == file || file->blob.empty())
    {
        return false;
    }
    
    if (file->blobParsed)
    {
        return true;
    }
    
    file->blobParsed = true;
    
    uint64_t val = 0;
    plist_t node = NULL;
    plist_from_memory(reinterpret_cast<const char *>(&file->blob[0]), static_cast<uint32_t>(file->blob.size()), &node);
    if (NULL != node)
    {
        plist_t lastModifiedNode = plist_access_path(node, 3, "$objects", 1, "LastModified");
        if (NULL != lastModifiedNode)
        {
            plist_type pt = plist_get_node_type(lastModifiedNode);
            plist_get_uint_val(lastModifiedNode, &val);
            file->modifiedTime = (unsigned int)val;
        }
        
        plist_t sizeNode = plist_access_path(node, 3, "$objects", 1, "Size");
        if (NULL != sizeNode)
        {
            plist_type pt = plist_get_node_type(sizeNode);
            val = 0;
            plist_get_uint_val(sizeNode, &val);
            file->size = val;
        }
        
        plist_free(node);
        return true;
    }
    
    return false;
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

std::string ITunesDb::getRealPath(const ITunesFile* file) const
{
    return fileIdToRealPath(file->fileId);
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
                if (file->modifiedTime != 0)
                {
                    updateFileTime(destFullPath, static_cast<time_t>(file->modifiedTime));
                }
                else if (!file->blob.empty())
                {
                    updateFileTime(destFullPath, ITunesDb::parseModifiedTime(file->blob));
                }
            }
            return result;
        }
    }
    
    return false;
}

DecodedWechatITunesDb::DecodedWechatITunesDb(const std::string& rootPath, const std::string& manifestFileName) : ITunesDb(rootPath, manifestFileName)
{
    
}

DecodedWechatITunesDb::~DecodedWechatITunesDb()
{
    
}

bool DecodedWechatITunesDb::load(const std::string& domain, bool onlyFile)
{
    return loadFiles(m_rootPath, onlyFile);
}

bool DecodedWechatITunesDb::loadFiles(const std::string& root, bool onlyFile)
{
#ifdef _WIN32
	std::queue<CString> directories;
	directories.push("");
	
	TCHAR szRoot[MAX_PATH] = { 0 };
	_tcscpy(szRoot, CW2T(CA2W(root.c_str(), CP_UTF8)));
	PathAddBackslash(szRoot);
	
	TCHAR szPath[MAX_PATH] = { 0 };
	TCHAR szRelativePath[MAX_PATH] = { 0 };
	ULARGE_INTEGER ull;

	while (!directories.empty())
	{
		const CString& dirName = directories.front();

		PathCombine(szPath, szRoot, dirName);
		PathAddBackslash(szPath);
		PathAppend(szPath, TEXT("*.*"));

		WIN32_FIND_DATA FindFileData;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		hFind = FindFirstFile((LPTSTR)szPath, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		do
		{
			if (_tcscmp(FindFileData.cFileName, TEXT(".")) == 0 || _tcscmp(FindFileData.cFileName, TEXT("..")) == 0)
			{
				continue;
			}
			bool isDir = ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
			
			PathCombine(szRelativePath, dirName, FindFileData.cFileName);
			
			if (!onlyFile || !isDir)
			{
				CString relativePath = szRelativePath;
				relativePath.Replace(DIR_SEP, ALT_DIR_SEP);

				CW2A pszU8(CT2W(szRelativePath), CP_UTF8);

				ITunesFile *file = new ITunesFile();
				file->relativePath = (LPCSTR)CW2A(CT2W(relativePath), CP_UTF8);;
				file->fileId = (LPCSTR)pszU8;
				file->flags = isDir ? 2 : 1;

				ull.LowPart = FindFileData.ftLastWriteTime.dwLowDateTime;
				ull.HighPart = FindFileData.ftLastWriteTime.dwHighDateTime;

				file->modifiedTime = static_cast<unsigned int>(ull.QuadPart / 10000000ULL - 11644473600ULL);
				
				m_files.push_back(file);
			}

			if (isDir)
			{
				PathAddBackslash(szRelativePath);
				directories.emplace(szRelativePath);
			}

		} while (::FindNextFile(hFind, &FindFileData));
		FindClose(hFind);

		directories.pop();
	}
    
#else
	std::queue<std::string> directories;
	directories.push("");
    struct stat statbuf;
    
    while (!directories.empty())
    {
        const std::string& dirName = directories.front();
        std::string path = combinePath(root, dirName);
        
        struct dirent *entry = NULL;
        DIR *dir = opendir(path.c_str());
        if (dir == NULL)
        {
            return false;
        }

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }
            
            if (filterFile(dirName, entry->d_name))
            {
                continue;
            }
            
            bool isDir = false;
            
            std::string relativePath = dirName + entry->d_name;

            lstat(combinePath(path, entry->d_name).c_str(), &statbuf);
            isDir = S_ISDIR(statbuf.st_mode);
            
            if (!onlyFile || !isDir)
            {
				std::string fileId = relativePath;

                ITunesFile *file = new ITunesFile();
                file->relativePath = relativePath;
                file->fileId = fileId;
                file->flags = isDir ? 2 : 1;
                file->modifiedTime = static_cast<unsigned int>(statbuf.st_mtimespec.tv_sec);
                
                m_files.push_back(file);
            }
            if (isDir)
            {
                directories.push(endsWith(relativePath, "/") ? relativePath : (relativePath + "/"));
            }
        }
        closedir(dir);
        directories.pop();
    }
    
#endif
    
    std::sort(m_files.begin(), m_files.end(), __string_less());
    
    return true;
}

std::string DecodedWechatITunesDb::fileIdToRealPath(const std::string& fileId) const
{
    if (!fileId.empty())
    {
        return combinePath(m_rootPath, fileId);
    }
    
    return std::string();
}

bool DecodedWechatITunesDb::filterFile(const std::string& relativeDir, const std::string& fileName)
{
    return (relativeDir.empty() && fileName.compare("Shared") == 0);
}

DecodedSharedWechatITunesDb::DecodedSharedWechatITunesDb(const std::string& rootPath, const std::string& manifestFileName) : DecodedWechatITunesDb(rootPath, manifestFileName)
{
    m_rootPath = combinePath(m_rootPath, "Shared", "group.com.tencent.xin");
}

DecodedSharedWechatITunesDb::~DecodedSharedWechatITunesDb()
{
}

bool DecodedSharedWechatITunesDb::load(const std::string& domain, bool onlyFile)
{
    return loadFiles(m_rootPath, onlyFile);
}

bool DecodedSharedWechatITunesDb::filterFile(const std::string& relativeDir, const std::string& fileName)
{
    return false;
}

ManifestParser::ManifestParser(const std::string& manifestPath, bool incudingApps) : m_manifestPath(manifestPath), m_incudingApps(incudingApps)
{
}

std::string ManifestParser::getLastError() const
{
	return m_lastError;
}

bool ManifestParser::parse(std::vector<BackupItem>& manifests) const
{
    bool res = false;
    
    std::string path = normalizePath(m_manifestPath);
    if (endsWith(path, normalizePath("/MobileSync")) || endsWith(path, normalizePath("/MobileSync/")) || isValidMobileSync(path))
    {
        path = combinePath(path, "Backup");
        res = parseDirectory(path, manifests);
    }
    else if (isValidBackupItem(path))
    {
        BackupItem manifest;
        if (parse(path, manifest) && manifest.isValid())
        {
            manifests.push_back(manifest);
            res = true;
        }
    }
    else
    {
        // Assume the directory is ../../Backup/../
        res = parseDirectory(path, manifests);
    }
    
    return res;
}

bool ManifestParser::parseDirectory(const std::string& path, std::vector<BackupItem>& manifests) const
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
        
        BackupItem manifest;
        manifest.setBackupId(*it);
        if (parse(backupPath, manifest) && manifest.isValid())
        {
            manifests.push_back(manifest);
            res = true;
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

bool ManifestParser::isValidMobileSync(const std::string& path) const
{
    std::string backupPath = combinePath(path, "Backup");
    if (!existsDirectory(backupPath))
    {
        m_lastError += "Backup folder not found\r\n";
        return false;
    }

    return true;
}

bool ManifestParser::parse(const std::string& path, BackupItem& manifest) const
{
    //Info.plist is a xml file
    if (!parseInfoPlist(path, manifest, m_incudingApps))
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
                manifest.setIOSVersion(getPlistStringValue(iOSVersionNode));
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

bool ManifestParser::parseInfoPlist(const std::string& backupIdPath, BackupItem& manifest, bool includingApps)
{
    std::string fileName = combinePath(backupIdPath, "Info.plist");
    std::string contents = readFile(fileName);
    plist_t node = NULL;
    plist_from_memory(contents.c_str(), static_cast<uint32_t>(contents.size()), &node);
    if (NULL == node)
    {
        return false;
    }
    
    manifest.setPath(backupIdPath);
    
    const char* ptr = NULL;
    uint64_t length = 0;
    std::string val;

    const char* ValueLastBackupDate = "Last Backup Date";
    const char* ValueDisplayName = "Display Name";
    const char* ValueDeviceName = "Device Name";
    const char* ValueITunesVersion = "iTunes Version";
    const char* ValueMacOSVersion = "macOS Version";
    const char* ValueProductVersion = "Product Version";
    const char* ValueInstalledApps = "Installed Applications";
    const char* ValueUniqueIdentifier = "Unique Identifier";
    const char* ValueTargetIdentifier = "Target Identifier";
    
    plist_t subNode = NULL;
    
    manifest.setDeviceName(getPlistStringValue(node, ValueDeviceName));
    manifest.setDisplayName(getPlistStringValue(node, ValueDisplayName));
    
    subNode = plist_dict_get_item(node, ValueLastBackupDate);
    if (NULL != subNode)
    {
        int32_t sec = 0, usec = 0;
        plist_get_date_val(subNode, &sec, &usec);
        manifest.setBackupTime(fromUnixTime(sec + 978278400, false));
    }
    
    manifest.setITunesVersion(getPlistStringValue(node, ValueITunesVersion));
    manifest.setIOSVersion(getPlistStringValue(node, ValueProductVersion));
    manifest.setMacOSVersion(getPlistStringValue(node, ValueMacOSVersion));
    
    std::string uniqueId = getPlistStringValue(node, ValueUniqueIdentifier);
    std::string targetId = getPlistStringValue(node, ValueTargetIdentifier);
    std::string uniqueIdUpper = toUpper(uniqueId);
    if (toUpper(manifest.getBackupId()) != uniqueIdUpper)
    {
        if (toUpper(targetId) != uniqueIdUpper)
        {
            manifest.setBackupId(targetId);
        }
        else
        {
            manifest.setBackupId(toLower(uniqueId));
        }
    }
    
    if (includingApps)
    {
        subNode = plist_dict_get_item(node, ValueInstalledApps);
        if (NULL != subNode && PLIST_IS_ARRAY(subNode))
        {
            uint32_t arraySize = plist_array_get_size(subNode);
            plist_t itemNode = NULL;
            for (uint32_t idx = 0; idx < arraySize; ++idx)
            {
                itemNode = plist_array_get_item(subNode, idx);
                if (itemNode == NULL)
                {
                    continue;
                }
                std::string bundleId = getPlistStringValue(itemNode);
                if (!bundleId.empty())
                {
                    plist_t appNode = plist_access_path(node, 2, "Applications", bundleId.c_str());
                    
                    if (NULL != appNode)
                    {
                        plist_t appSubNode = NULL;
                        
                        appSubNode = plist_dict_get_item(appNode, "iTunesMetadata");
                        if (NULL != appSubNode)
                        {
                            plist_type ptype = plist_get_node_type(appSubNode);
                            ptr = plist_get_data_ptr(appSubNode, &length);
                            if (ptr != NULL && length > 0)
                            {
                                std::string metadata(ptr, length);
                                BackupItem::AppInfo appInfo;
                                appInfo.bundleId = bundleId;
                                parseITunesMetadata(metadata, appInfo);
                                manifest.addApp(appInfo);
                            }
                            
                        }
                        
                    }
                }
                
            }
        
        }
    }
    
    plist_free(node);

    return true;
}

bool ManifestParser::parseITunesMetadata(const std::string& metadata, BackupItem::AppInfo& appInfo)
{
    plist_t node = NULL;
    plist_from_memory(metadata.c_str(), static_cast<uint32_t>(metadata.size()), &node);
    if (NULL == node)
    {
        return false;
    }
    
    /*
    plist_dict_iter it = NULL;
    char *key = NULL;
    plist_t plistVal = NULL;
    
    plist_dict_new_iter(node, &it);

    while (1)
    {
        plist_dict_next_item(node, it, &key, &plistVal);
        
        if (NULL == key)
        {
            break;
        }
        std::string keyString = key;
        int aa = 0;
    }
    */

    appInfo.name = getPlistStringValue(node, "itemName");
    appInfo.bundleShortVersion = getPlistStringValue(node, "bundleShortVersionString");
    appInfo.bundleVersion = getPlistStringValue(node, "bundleVersion");
    
    plist_free(node);
    
    return true;
}

DecodedManifestParser::DecodedManifestParser(const std::string& manifestPath, bool includingApps) : ManifestParser(manifestPath, includingApps)
{
}

bool DecodedManifestParser::parse(std::vector<BackupItem>& manifests) const
{
    bool res = false;
    
    std::string path = normalizePath(m_manifestPath);
    if (isValidBackupItem(path))
    {
        BackupItem manifest;
        if (parse(path, manifest) && manifest.isValid())
        {
            manifests.push_back(manifest);
            res = true;
        }
    }
    
    return res;
}

bool DecodedManifestParser::isValidBackupItem(const std::string& path) const
{
    std::string fileName = combinePath(path, "Documents", "LoginInfo2.dat");
    if (!existsFile(fileName))
    {
        m_lastError += "LoginInfo2.dat not found:" + fileName + "\r\n";
        return false;
    }

    return true;
}

bool DecodedManifestParser::parse(const std::string& path, BackupItem& manifest) const
{
    std::string fileName = combinePath(path, "Documents", "LoginInfo2.dat");
    if (!existsFile(fileName))
    {
        m_lastError += "LoginInfo2.dat not found\r\n";
        return false;
    }
    
	manifest.setPath(path);
    
#ifdef _WIN32
	struct _stat statbuf;
	CA2W wpath(path.c_str(), CP_UTF8);
	_wstat((LPCWSTR)wpath, &statbuf);
	std::time_t ts = statbuf.st_mtime;
#else
    struct stat statbuf;
    lstat(fileName.c_str(), &statbuf);
    std::time_t ts = statbuf.st_mtimespec.tv_sec;
    
#endif
	std::tm * ptm = std::localtime(&ts);

    char buffer[32];
	std::strftime(buffer, 32, "%Y-%m-%d %H:%M", ptm);
    
    manifest.setDeviceName("localhost");
    manifest.setDisplayName("Wechat Backup");
    
    manifest.setBackupTime(buffer);

    return true;
}
