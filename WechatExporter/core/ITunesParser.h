//
//  ITunesParser.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <vector>
#include <map>

// #include <sstream>
// #include <iomanip>
#include <libxml/parser.h>
#include "Utils.h"

#ifndef ITunesParser_h
#define ITunesParser_h

struct ITunesFile
{
    std::string fileId;
    std::string relativePath;
    // unsigned int modifiedTime;
    std::vector<unsigned char> blob;
    
    ITunesFile()
    {
    }
    
    // unsigned int getModifiedTime() const;
};

using ITunesFileVector = std::vector<ITunesFile *>;
using ITunesFilesIterator = typename ITunesFileVector::iterator;
using ITunesFilesConstIterator = typename ITunesFileVector::const_iterator;
using ITunesFileRange = std::pair<ITunesFilesConstIterator, ITunesFilesConstIterator>;

class BackupManifest
{
protected:
    std::string m_path;
    std::string m_deviceName;
    std::string m_displayName;
    std::string m_backupTime;
    
public:
    BackupManifest()
    {
    }
    
    bool operator==(const BackupManifest& rhs) const
    {
        if (this == &rhs)
        {
            return true;
        }
        
        return m_path == rhs.m_path;
    }
    
    BackupManifest(std::string path, std::string deviceName, std::string displayName, std::string backupTime) : m_path(path), m_deviceName(deviceName), m_displayName(displayName), m_backupTime(backupTime)
    {
    }

    bool isValid() const
    {
        return !m_displayName.empty() && !m_backupTime.empty() && !m_deviceName.empty();
    }
    
    std::string toString() const
    {
        return m_displayName + " " + m_backupTime + "(" + m_path + ")";
    }
    
    std::string getPath() const
    {
        return m_path;
    }
};

class ITunesDb
{
public:
    ITunesDb(const std::string& rootPath, const std::string& manifestFileName);
    ~ITunesDb();
    
    bool load();
    bool load(const std::string& domain);
    bool load(const std::string& domain, bool onlyFile);
    
    std::string findFileId(const std::string& relativePath) const;
    std::string fileIdToRealPath(const std::string& fileId) const;
    std::string findRealPath(const std::string& relativePath) const;
    template<class TFilter>
    ITunesFileVector filter(TFilter f) const;

protected:
    std::vector<ITunesFile *> m_files;
    std::string m_rootPath;
    std::string m_manifestFileName;
};

template<class TFilter>
ITunesFileVector ITunesDb::filter(TFilter f) const
{
    ITunesFileVector files;
    ITunesFileRange range = std::equal_range(m_files.cbegin(), m_files.cend(), f, f);
    if (range.first != range.second)
    {
        for (ITunesFilesConstIterator it = range.first; it != range.second; ++it)
        {
            if (f == *it)
            {
                files.push_back(*it);
            }
        }
    }
    
    return files;
}

class ManifestParser
{
protected:
    std::string m_manifestPath;
    std::string m_xml;
    
public:
    ManifestParser(const std::string& m_manifestPath, const std::string& m_xml);
    std::vector<BackupManifest> parse();
    BackupManifest parse(const std::string& backupId);
    
    static void startElement(void * ctx, const xmlChar * fullName, const xmlChar ** attrs);
    static void startElementNs(void * ctx, const xmlChar * localName, const xmlChar * prefix, const xmlChar * URI, int nb_namespaces, const xmlChar ** namespaces, int nb_attributes, int nb_defaulted, const xmlChar ** attrs);
    static void endElementNs(void* ctx, const xmlChar* localname, const xmlChar* prefix, const xmlChar* URI);
    static void characters(void* ctx, const xmlChar * ch, int len);

    static std::string toString(const xmlChar* ch);
    static std::string toString(const xmlChar* ch, int len);
};

inline std::string ManifestParser::toString(const xmlChar* ch)
{
    const char *p = reinterpret_cast<const char *>(ch);
    return std::string(p, p + xmlStrlen(ch));
}
inline std::string ManifestParser::toString(const xmlChar* ch, int len)
{
    const char *p = reinterpret_cast<const char *>(ch);
    return std::string(p, p + len);
}

#endif /* ITunesParser_h */
