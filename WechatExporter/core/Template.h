//
//  Template.h
//  WechatExporter
//
//  Created by Matthew on 2022/4/21.
//  Copyright © 2022 Matthew. All rights reserved.
//

#ifndef Template_h
#define Template_h

#include <string>
#include <vector>
#include <map>

struct TEMPLATE_TAG
{
    std::string tag;
    size_t pos;
    size_t length;
    
    TEMPLATE_TAG(const std::string& t, size_t p, size_t l) : tag(t), pos(p), length(l) {}
};

class Template
{
public:
    Template();
    Template(const std::string& tmpl);
    
    Template(const Template& rhs);
    
    Template& operator=(const Template& rhs);
    
    std::string build(const std::vector<std::pair<std::string, std::string>>& values) const;
    
    const std::string& build(const std::map<std::string, std::string>& values) const;
    
protected:
    std::string m_template;
    mutable std::string m_result;
    std::vector<TEMPLATE_TAG> m_tags;
};

#endif /* Template_h */
