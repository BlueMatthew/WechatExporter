//
//  ITunesParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "ITunesParser.h"
#include <sqlite3.h>
#include <algorithm>

#include "OSDef.h"
#include "Utils.h"

struct __string_less
{
    // _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11
    bool operator()(const std::string& __x, const std::string& __y) const {return __x < __y;}
    bool operator()(const std::pair<std::string, std::string>& __x, const std::string& __y) const {return __x.first < __y;}
};

ITunesDb::ITunesDb(const std::string& rootPath, const std::string& manifestFileName) : m_rootPath(rootPath), m_manifestFileName(manifestFileName)
{
    std::replace(m_rootPath.begin(), m_rootPath.end(), DIR_SEP_R, DIR_SEP);
    
    if (!endsWith(m_rootPath, DIR_SEP))
    {
        m_rootPath += DIR_SEP;
    }
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
    std::string dbPath = m_rootPath + m_manifestFileName;
    
    sqlite3 *db = NULL;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK)
    {
        // printf("Open database failed!");
        sqlite3_close(db);
        return false;
    }
    
    std::string sql;
    if (domain.size() > 0 && onlyFile)
    {
        sql = "SELECT fileID,relativePath FROM Files WHERE domain=? AND flags=1";
    }
    else if (domain.size() > 0)
    {
        sql = "SELECT fileID,relativePath FROM Files WHERE domain=?";
    }
    else if (onlyFile)
    {
        sql = "SELECT fileID,relativePath FROM Files WHERE flags=1";
    }
    else
    {
        sql = "SELECT fileID,relativePath FROM Files";
    }

    sqlite3_stmt* stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql.c_str(), (int)(sql.size()), &stmt, NULL);
    if (rc != SQLITE_OK)
    {
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
                            
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
#ifndef NDEBUG
        std::string fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
#endif
        m_files.push_back(std::make_pair(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))), std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)))));
        
    }
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::sort(m_files.begin(), m_files.end());
    
    return true;
}

std::string ITunesDb::findFileId(const std::string& relativePath) const
{
    std::string fileId;
    
    typename std::vector<std::pair<std::string, std::string>>::const_iterator it = std::lower_bound(m_files.begin(), m_files.end(), relativePath, __string_less());
    
    if (it == m_files.end() || it->first != relativePath)
    {
        return std::string();
    }
    return it->second;
}

std::string ITunesDb::fieldIdToRealPath(const std::string& fieldId) const
{
    if (!fieldId.empty())
    {
        return combinePath(m_rootPath, fieldId.substr(0, 2), fieldId);
    }
    
    return std::string();
}

std::string ITunesDb::findRealPath(const std::string& relativePath) const
{
    std::string fieldId = findFileId(relativePath);
    return fieldIdToRealPath(fieldId);
}

ManifestParser::ManifestParser(std::string m_manifestPath, std::string m_xml)
{
    
}

std::vector<BackupManifest> ManifestParser::parse()
{
    std::vector<BackupManifest> manifests;
    
    std::string aa = "<key>DeviceName</key>";
    
    return manifests;
}
