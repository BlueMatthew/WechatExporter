//
//  BaseResConverter.cpp
//  WechatExporter
//
//  Created by Matthew on 2021/10/27.
//  Copyright © 2021 Matthew. All rights reserved.
//

#include "ResManager.h"
#include <algorithm>
#include <json/json.h>
#include "FileSystem.h"
#include "Utils.h"

#define TEMPLATE_NAMES {"frame", "msg", "video", "notice", "system", "audio", "image", "card", "emoji", "plainshare", "share", "thumb", "listframe", "listitem", "scripts", "filter", "refermsg", "channels", "wxemoji", "transfer"}

#define TEMPLATE_FORMATS {"template", "template_txt"}

ResManager::ResManager()
{
}

ResManager::~ResManager()
{
}

bool ResManager::initLocaleResource(const std::string& resDir, const std::string& languageCode)
{
    m_resDir = resDir;
    
    bool res = true;
    if (!loadLocaleStrings(resDir, languageCode))
    {
        res = false;
    }

    return res;
}

bool ResManager::initResources(const std::string& resDir, const std::string& languageCode, const std::string& templateName)
{
    m_resDir = resDir;
    
    bool res = true;
    if (!loadLocaleStrings(resDir, languageCode))
    {
        res = false;
    }
    if (!loadTemplates(resDir, templateName))
    {
        res = false;
    }
    if (!loadEmojis(resDir))
    {
        res = false;
    }

    return res;
}

std::string ResManager::getDefaultAvatarPath() const
{
    return combinePath(m_resDir, "res", "DefaultAvatar.png");
}

bool ResManager::loadLocaleStrings(const std::string& resDir, const std::string& languageCode)
{
    m_localeStrings.clear();
    
    std::string path = combinePath(resDir, "res", languageCode + ".txt");
    if (!existsFile(path))
    {
        return false;
    }
    std::string contents = readFile(path);
    if (contents.empty())
    {
        return false;
    }
    
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    Json::Value value;
    if (reader->parse(contents.c_str(), contents.c_str() + contents.size(), &value, NULL))
    {
        int sz = value.size();
        for (int idx = 0; idx < sz; ++idx)
        {
            std::string k = value[idx]["key"].asString();
            std::string v = value[idx]["value"].asString();
            if (m_localeStrings.find(k) != m_localeStrings.cend())
            {
                // return false;
            }
            m_localeStrings[k] = v;
        }
    }

    return true;
}

bool ResManager::loadTemplates(const std::string& resDir, const std::string& templateName)
{
    m_templates.clear();
    
    const char* names[] = TEMPLATE_NAMES;
    
    std::string preTag = "%%ML:";
    std::string::size_type preTagLen = preTag.size();
    std::string postTag = "%%";
    std::string::size_type postTagLen = postTag.size();
    
    bool res = true;
    for (int idx = 0; idx < sizeof(names) / sizeof(const char*); idx++)
    {
        std::string name = names[idx];
        std::string path = combinePath(resDir, "res", templateName, name + ".html");
        if (!existsFile(path))
        {
            res = false;
            continue;
        }
        std::string contents = readFile(path);
        
        // Localization
        std::string::size_type tailPos = 0;
        std::string::size_type pos = std::string::npos;
        while ((pos = contents.find(preTag, tailPos)) != std::string::npos)
        {
            std::string::size_type tailPos = contents.find(postTag, pos + preTagLen);
            if (tailPos == std::string::npos)
            {
                break;
            }
            
            std::string str = contents.substr(pos + preTagLen, tailPos - pos - preTagLen);
            std::string localizedStr = getLocaleString(str);
            
            contents.replace(pos, tailPos + postTagLen - pos, localizedStr);
            
            tailPos += postTagLen + localizedStr.size() - str.size() - preTagLen - postTagLen;
        }
        
        m_templates[name] = contents;
        
        m_newTemplates[name] = Template(contents);
        
        
#ifndef NDEBUG
        writeFile("/Users/matthew/Documents/WechatHistory/test/templates/" + name + ".html", contents);
#endif
    }

    return res;
}

bool ResManager::loadEmojis(const std::string& resDir)
{
    m_emojiTags.clear();

    std::string path = combinePath(resDir, "res", "emoji", "emoji.json");
    if (!existsFile(path))
    {
        return false;
    }
    
    std::string contents = readFile(path);
    if (contents.empty())
    {
        return false;
    }

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader>reader(builder.newCharReader());

    Json::Value value;
    if (reader->parse(contents.c_str(), contents.c_str() + contents.size(), &value, NULL))
    {
        if (value.isArray())
        {
            for (Json::Value::const_iterator it = value.begin(); it != value.end(); ++it)
            {
                if (!it->isObject()) continue;
                
                Json::Value preTagValue = it->get("preTag", Json::Value::null);
                std::string preTag = preTagValue.isNull() ? "" : preTagValue.asCString();
                
                if (preTag.empty()) continue;

                Json::Value postTagValue = it->get("postTag", Json::Value::null);
                std::string postTag = postTagValue.isNull() ? "" : postTagValue.asCString();
                
                std::vector<EmojiTag>::iterator itEmojiTag = m_emojiTags.emplace(m_emojiTags.end(), preTag, postTag);
                
                Json::Value keysValue = it->get("keys", Json::Value::null);
                if (keysValue.isArray())
                {
                    for (Json::Value::const_iterator itKey = keysValue.begin(); itKey != keysValue.end(); ++itKey)
                    {
                        if (!itKey->isObject()) continue;
                        
                        Json::Value keyValue = itKey->get("key", Json::Value::null);
                        std::string key = keyValue.isNull() ? "" : keyValue.asCString();
                        
                        Json::Value fileValue = itKey->get("file", Json::Value::null);
                        std::string file = fileValue.isNull() ? "" : fileValue.asCString();
                        
                        if (key.empty() || file.empty()) continue;
                        
                        std::string fullTag = preTag + key + postTag;
                        
                        itEmojiTag->m_items.emplace(itEmojiTag->m_items.end(), key, fullTag, fullTag, file);
                    }
                    
                    std::sort(itEmojiTag->m_items.begin(), itEmojiTag->m_items.end());
                }
            }
        }
        int sz = value.size();
        for (int idx = 0; idx < sz; ++idx)
        {
            std::string k = value[idx]["key"].asString();
            std::string v = value[idx]["value"].asString();
            if (m_localeStrings.find(k) != m_localeStrings.cend())
            {
                // return false;
            }
            m_localeStrings[k] = v;
        }
    }

    return true;
}

std::string ResManager::getTemplate(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = m_templates.find(key);
    return (it == m_templates.cend()) ? "" : it->second;
}

const std::string& ResManager::buildFromTemplate(const std::string& key, const std::map<std::string, std::string>& values) const
{
    auto it = m_newTemplates.find(key);
    if (it == m_newTemplates.cend())
    {
        return m_emptyString;
    }
    
    return it->second.build(values);
}

std::string ResManager::checkEmptyTemplates() const
{
    std::vector<std::string> keys;
    for (std::map<std::string, std::string>::const_iterator it = m_templates.cbegin(); it != m_templates.cend(); ++it)
    {
        if (it->second.empty())
        {
            keys.push_back(it->second);
        }
    }
    
    return join(keys, ",");
}

std::string ResManager::getLocaleString(const std::string& key) const
{
    // std::string value = key;
    std::map<std::string, std::string>::const_iterator it = m_localeStrings.find(key);
    return it == m_localeStrings.cend() ? key : it->second;
}

// Emoji
bool ResManager::hasEmojiTag(const std::string& msg) const
{
    if (msg.empty())
    {
        return false;
    }
    
    bool existed = false;
    EmojiItemCompare comp;
    for (std::vector<EmojiTag>::const_iterator itTag = m_emojiTags.cbegin(); itTag != m_emojiTags.cend(); ++itTag)
    {
        std::string::size_type pos = 0;
        std::string::size_type tailPos = 0;
        while ((pos = msg.find(itTag->m_headTag, pos)) != std::string::npos)
        {
            if (itTag->hasTailTag())
            {
                tailPos = msg.find(itTag->m_tailTag, pos + itTag->m_headTag.size());
                if (tailPos == std::string::npos)
                {
                    break;
                }
                
                std::string tag = msg.substr(pos + itTag->m_headTag.size(), tailPos - pos - itTag->m_headTag.size());
                if (!tag.empty())
                {
                    std::vector<EmojiItem>::const_iterator it = std::lower_bound(itTag->m_items.cbegin(), itTag->m_items.cend(), tag, comp);
                    if (it != itTag->m_items.cend() && it->equals(tag))
                    {
                        existed = true;
                        break;
                    }
                }
                
                pos = tailPos + itTag->m_tailTag.size();
            }
            else
            {
                pos += itTag->m_headTag.size();
                for (std::vector<EmojiItem>::const_iterator it = itTag->m_items.cbegin(); it != itTag->m_items.cend(); ++it)
                {
                    if (msg.compare(pos, it->getTag().size(), it->getTag()) == 0)
                    {
                        existed = true;
                        break;
                    }
                }
                
                if (existed) break;
            }
        }
        
        if (existed) break;
    }
   
    return existed;
}

std::string ResManager::convertEmojis(const std::string& msg, const std::string& localRootPath, const std::string& emojiPath, const std::string& emojiUrlPath) const
{
    struct FindEmojiItem
    {
        std::string::size_type  pos;
        std::string::size_type  length;
        const EmojiItem*        emojiItem;
        
        FindEmojiItem(std::string::size_type p, std::string::size_type l, const EmojiItem* ei) : pos(p), length(l), emojiItem(ei)
        {
        }
        inline bool operator<(const FindEmojiItem &rhs) const
        {
            return pos < rhs.pos;
        }
        
        inline bool operator>(const FindEmojiItem &rhs) const
        {
            return pos > rhs.pos;
        }
    };
    
    // greatter to less
    struct FindEmojiItemCompare
    {
        inline bool operator() ( const FindEmojiItem& lhs, const FindEmojiItem& rhs) const
        {
            return lhs.pos > rhs.pos;
        }
    };

    std::string emojiTemplate = getTemplate("wxemoji");
    if (emojiTemplate.empty())
    {
        return msg;
    }
    
    EmojiItemCompare comp;
    std::vector<FindEmojiItem> findEmojiItems;
    bool matched = false;
    for (std::vector<EmojiTag>::const_iterator itTag = m_emojiTags.cbegin(); itTag != m_emojiTags.cend(); ++itTag)
    {
        std::string::size_type pos = 0;
        std::string::size_type tailPos = 0;
        while ((pos = msg.find(itTag->m_headTag, pos)) != std::string::npos)
        {
            if (itTag->hasTailTag())
            {
                tailPos = msg.find(itTag->m_tailTag, pos + itTag->m_headTag.size());
                if (tailPos == std::string::npos)
                {
                    // break while loop
                    break;
                }
                
                matched = false;
                std::string tag = msg.substr(pos + itTag->m_headTag.size(), tailPos - pos - itTag->m_headTag.size());
                if (!tag.empty())
                {
                    std::vector<EmojiItem>::const_iterator it = std::lower_bound(itTag->m_items.cbegin(), itTag->m_items.cend(), tag, comp);
                    if (it != itTag->m_items.cend() && it->equals(tag))
                    {
                        findEmojiItems.emplace(findEmojiItems.end(), pos, tailPos + itTag->m_tailTag.size() - pos, &(*it));
                        matched = true;
                    }
                }
                matched ? (pos += itTag->m_headTag.size()) : (pos = tailPos + itTag->m_tailTag.size());
            }
            else
            {
                std::string::size_type oldPos = pos;
                pos += itTag->m_headTag.size();
                for (std::vector<EmojiItem>::const_iterator it = itTag->m_items.cbegin(); it != itTag->m_items.cend(); ++it)
                {
                    if (msg.compare(pos, it->getTag().size(), it->getTag()) == 0)
                    {
                        findEmojiItems.emplace(findEmojiItems.end(), oldPos, itTag->m_headTag.size() + it->getTag().size(), &(*it));
                        pos += it->getTag().size();
                        break;
                    }
                }
            }
        }
        
    }
        
    if (findEmojiItems.empty())
    {
        return msg;
    }
        
    std::sort(findEmojiItems.begin(), findEmojiItems.end(), FindEmojiItemCompare());

    std::string newMsg = msg;
    std::string srcEmojiPath = combinePath(m_resDir, "res", "emoji", "images");
    std::string destEmojiPath = combinePath(localRootPath, emojiPath, "Emoji", "wx");
    bool destEmojiPathExisted = existsDirectory(destEmojiPath);
    for (std::vector<FindEmojiItem>::const_iterator it = findEmojiItems.cbegin(); it != findEmojiItems.cend(); ++it)
    {
        std::string emojiStr = emojiTemplate;

        replaceAll(emojiStr, "%%EMOJI_PATH%%", emojiUrlPath + "/Emoji/wx/" + it->emojiItem->m_fileName + ".png");
        replaceAll(emojiStr, "%%EMOJI_TITLE%%", it->emojiItem->getTitle());
        replaceAll(emojiStr, "%%EMOJI_RAW%%", it->emojiItem->m_fullTag);
        
        newMsg.replace(it->pos, it->length, emojiStr);
        
        std::string destFileName = combinePath(destEmojiPath, it->emojiItem->m_fileName + ".png");
        if (!existsFile(destFileName))
        {
            if (!destEmojiPathExisted)
            {
                destEmojiPathExisted = makeDirectory(destEmojiPath);
            }
            
            copyFile(combinePath(srcEmojiPath, it->emojiItem->m_fileName + ".png"), destFileName);
        }
    }

    return newMsg;
}

bool ResManager::validateResources(const std::string& resDir, std::string& error)
{
    const char* names[] = TEMPLATE_NAMES;
    const char* formats[] = TEMPLATE_FORMATS;
    
    bool res = true;
    for (int idx1 = 0; idx1 < sizeof(formats) / sizeof(const char*); idx1++)
    {
        std::string format = formats[idx1];
        for (int idx2 = 0; idx2 < sizeof(names) / sizeof(const char*); idx2++)
        {
            std::string name = names[idx2];
            std::string path = combinePath(resDir, "res", format, name + ".html");
            if (!existsFile(path))
            {
                error = "Templagte file doesn't exist: res";
                error += DIR_SEP_STR + format + DIR_SEP_STR + name + ".html";
                res = false;
                break;
            }
            
            if (getFileSize(path) == 0)
            {
                error = "Templagte file is empty: res";
                error += DIR_SEP_STR + format + DIR_SEP_STR + name + ".html";
                res = false;
                break;
            }
        }
    }
    
    return res;
}
