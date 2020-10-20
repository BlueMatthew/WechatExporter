//
//  ITunesParser.h
//  WechatExporter
//
//  Created by Matthew on 2020/9/29.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include <vector>
#include <utility>

// #include <sstream>
// #include <iomanip>
#include "Utils.h"

#ifndef ITunesParser_h
#define ITunesParser_h

using StringPairVector = std::vector<std::pair<std::string, std::string>>;
using SPVectorIterator = typename StringPairVector::iterator;
using SPVectorConstIterator = typename StringPairVector::const_iterator;

class BackupManifest
{
protected:
    std::string m_path;
    std::string m_deviceName;
    std::string m_displayName;
    unsigned int m_backupTime;
    
public:
    BackupManifest(std::string path, std::string deviceName, std::string displayName, unsigned int backupTime) : m_path(path), m_deviceName(deviceName), m_displayName(displayName), m_backupTime(backupTime)
    {
    }

    std::string toString() const
    {
        return m_displayName + " " + fromUnixTime(m_backupTime);
    }
};

class ITunesDb
{
private:
    std::string m_rootPath;
    std::string m_manifestFileName;
public:
    ITunesDb(const std::string& rootPath, const std::string& manifestFileName);
    
    bool load();
    bool load(const std::string& domain);
    bool load(const std::string& domain, bool onlyFile);
    
    std::string findFileId(const std::string& relativePath) const;
    std::string fieldIdToRealPath(const std::string& fieldId) const;
    std::string findRealPath(const std::string& relativePath) const;
    template<class TFilter>
    StringPairVector filter(TFilter f) const;

protected:
    StringPairVector m_files;
};

template<class TFilter>
StringPairVector ITunesDb::filter(TFilter f) const
{
    StringPairVector files;
    std::pair<SPVectorConstIterator, SPVectorConstIterator> range = std::equal_range(m_files.cbegin(), m_files.cend(), f, f);
    if (range.first != range.second)
    {
        for (SPVectorConstIterator it = range.first; it != range.second; ++it)
        {
            std::string val = it->first;
            
            if (f == *it)
            {
                files.push_back(*it);
            }
        }
    }
    // {
        // files.insert(files.end(), range.first, range.second);
    // }
    
    
    return files;
}

class ManifestParser
{
protected:
    std::string m_manifestPath;
    std::string m_xml;
    
public:
    ManifestParser(std::string m_manifestPath, std::string m_xml);
    std::vector<BackupManifest> parse();
    
};

#endif /* ITunesParser_h */
