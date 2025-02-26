//
//  Template.cpp
//  WechatExporter
//
//  Created by Matthew on 2022/4/21.
//  Copyright © 2022 Matthew. All rights reserved.
//

#include "Template.h"

#define TEMPLATE_TAG "%%"
#define TEMPLATE_TAG_LENGTH 2


Template::Template()
{
}

Template::Template(const std::string& tmpl) : m_template(tmpl)
{
    size_t pos = 0;
    size_t posEnd = 0;
    while (1)
    {
        pos = m_template.find(TEMPLATE_TAG, pos);
        if (pos == std::string::npos)
        {
            break;
        }
        posEnd = m_template.find(TEMPLATE_TAG, pos + TEMPLATE_TAG_LENGTH);
        if (posEnd == std::string::npos)
        {
            break;
        }
        
        size_t length = posEnd - pos + TEMPLATE_TAG_LENGTH;
        std::string tag = m_template.substr(pos, length);
        
        m_tags.emplace_back(tag, pos, length);
        
        pos = posEnd + TEMPLATE_TAG_LENGTH;
    }
}

Template::Template(const Template& rhs) : m_template(rhs.m_template), m_tags(rhs.m_tags)
{
}

Template& Template::operator=(const Template& rhs)
{
    m_template = rhs.m_template;
    m_result = rhs.m_result;
    m_tags = rhs.m_tags;
    
    return *this;
}

const std::string& Template::build(const std::map<std::string, std::string>& values) const
{
    m_result.assign(m_template);
    
    for (auto it = m_tags.crbegin(); it != m_tags.crend(); ++it)
    {
        auto itVal = values.find(it->tag);
        if (itVal == values.cend())
        {
            m_result.erase(m_result.begin() + it->pos, m_result.begin() + it->pos + it->length);
        }
        else
        {
            m_result.replace(it->pos, it->length, itVal->second);
        }
    }

    return m_result;
}
