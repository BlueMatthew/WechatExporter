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

#include <sstream>
#include <iomanip>
#include <ctime>
#include "Shell.h"
#include "Utils.h"

#ifndef ITunesParser_h
#define ITunesParser_h

struct ITunesFile
{
    std::string fileId;
    std::string relativePath;
    unsigned int modifiedTime;
    std::vector<unsigned char> blob;
    
    ITunesFile()
    {
    }
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

    BackupManifest(std::string path, std::string deviceName, std::string displayName, std::string backupTime) : m_path(path), m_deviceName(deviceName), m_displayName(displayName)
    {
		if (!backupTime.empty())
		{
			m_backupTime = utcToLocal(backupTime);
		}
    }

    bool isValid() const
    {
        return !m_displayName.empty() && !m_backupTime.empty() && !m_deviceName.empty();
    }
    
    std::string toString() const
    {
        return m_displayName + " " + m_backupTime + " (" + m_path + ")";
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
    
    const ITunesFile* findITunesFile(const std::string& relativePath) const;
    std::string findFileId(const std::string& relativePath) const;
    std::string fileIdToRealPath(const std::string& fileId) const;
    std::string findRealPath(const std::string& relativePath) const;
    template<class TFilter>
    ITunesFileVector filter(TFilter f) const;
    
    std::string getRealPath(const ITunesFile& file) const;
    
    static unsigned int parseModifiedTime(const std::vector<unsigned char>& data);
    
protected:
    mutable std::vector<ITunesFile *> m_files;
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
	const Shell* m_shell;
	
    
public:
    ManifestParser(const std::string& m_manifestPath, const std::string& m_xml, const Shell* shell);
    std::vector<BackupManifest> parse() const;

protected:
    std::vector<BackupManifest> parseDirectory(const std::string& path) const;
    BackupManifest parse(const std::string& backupPath, const std::string& backupId) const;
};

#endif /* ITunesParser_h */
