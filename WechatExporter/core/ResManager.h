//
//  BaseResConverter.h
//  WechatExporter
//
//  Created by Matthew on 2021/10/26.
//  Copyright © 2021 Matthew. All rights reserved.
//

#ifndef ResManager_h
#define ResManager_h

#include <string>
#include <vector>
#include <map>

#include "Template.h"

class ResManager
{
public:
    
    struct EmojiItem
    {
        std::string m_tag;
        std::string m_fullTag;
        std::string m_title;
        std::string m_fileName;

        EmojiItem()
        {
        }
        
        EmojiItem(const std::string& tag, const std::string& fullTag, const std::string& title, const std::string& fileName) : m_tag(tag), m_fullTag(fullTag), m_title(title), m_fileName(fileName)
        {
        }
        
        inline bool operator<(const EmojiItem &rhs) const
        {
            return m_tag.compare(rhs.m_tag) < 0;
        }
        
        inline bool operator>(const EmojiItem &rhs) const
        {
            return m_tag.compare(rhs.m_tag) > 0;
        }
        
        const std::string& getTag() const
        {
            return m_tag;
        }
        
        const std::string& getTitle() const
        {
            return m_title;
        }
        
        const std::string& getFileName() const
        {
            return m_fileName;
        }
        
        bool equals(const std::string& tag) const
        {
            return m_tag.compare(tag) == 0;
        }
    };
    
    struct EmojiItemCompare
    {
        inline bool operator() ( const EmojiItem& lhs, const std::string& rhs) const
        {
            return lhs.getTag().compare(rhs) < 0;
        }
    };
    
    struct EmojiItemCompareN
    {
        inline bool operator() ( const EmojiItem& lhs, const std::string& rhs) const
        {
            return lhs.getTag().compare(rhs) < 0;
        }
    };
    
    struct EmojiTag
    {
        std::string m_headTag;
        std::string m_tailTag;
        
        std::vector<EmojiItem> m_items;
        
        EmojiTag(const std::string& headTag, const std::string& tailTag) : m_headTag(headTag), m_tailTag(tailTag)
        {
        }
        
        bool hasTailTag() const
        {
            return !m_tailTag.empty();
        }
    };
    
    
public:
    ResManager();
    ~ResManager();
    
    bool initLocaleResource(const std::string& resDir, const std::string& languageCode);
    bool initResources(const std::string& resDir, const std::string& languageCode, const std::string& templateName);
    
    // Default Resource
    std::string getDefaultAvatarPath() const;
    std::string getDefaultAppIconPath() const;

    // Localization
    std::string getLocaleString(const std::string& key) const;
    
    // Template
    std::string getTemplate(const std::string& key) const;
    // const Template& getNewTemplate(const std::string& key) const;
    std::string checkEmptyTemplates() const;
    
    const std::string& buildFromTemplate(const std::string& key, const std::map<std::string, std::string>& values) const;

    // Emoji
    bool hasEmojiTag(const std::string& msg) const;
    std::string convertEmojis(const std::string& msg, const std::string& localRootPath, const std::string& emojiPath, const std::string& emojiUrlPath) const;

    static bool validateResources(const std::string& resDir, std::string& error);
    
protected:
    bool loadLocaleStrings(const std::string& resDir, const std::string& languageCode);
    bool loadTemplates(const std::string& resDir, const std::string& templateName);
    bool loadEmojis(const std::string& resDir);

protected:
    std::string m_resDir;
    std::map<std::string, std::string> m_templates;
    std::map<std::string, Template> m_newTemplates;

    std::map<std::string, std::string> m_localeStrings;
    std::vector<EmojiTag> m_emojiTags;
    
    std::string m_emptyString;
};

#endif /* ResManager_h */
