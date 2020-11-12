//
//  XmlParser.hpp
//  WechatExporter
//
//  Created by Matthew on 2020/11/12.
//  Copyright © 2020 Matthew. All rights reserved.
//

#ifndef XmlParser_h
#define XmlParser_h

#include <cstdio>
#include <string>
#include <map>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

class XmlParser
{
public:
    XmlParser(const std::string& xml, bool noError = false);
    ~XmlParser();
    bool parseNodeValue(const std::string& xpath, std::string& value);
    bool parseNodesValue(const std::string& xpath, std::map<std::string, std::string>& values);  // e.g.: /path1/path2/*
    bool parseAttributeValue(const std::string& xpath, const std::string& attributeName, std::string& value);
    bool parseAttributesValue(const std::string& xpath, std::map<std::string, std::string>& attributes);
    
private:
    template <class TNodeHandler>
    bool parseImpl(const std::string& xpath, TNodeHandler& handler);
    
private:
    xmlDocPtr m_doc;
    xmlXPathContextPtr m_xpathCtx;
};

template <class TNodeHandler>
bool XmlParser::parseImpl(const std::string& xpath, TNodeHandler& handler)
{
    bool result = false;
    if (m_doc == NULL || m_xpathCtx == NULL)
    {
        return false;
    }
    
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST(xpath.c_str()), m_xpathCtx);
    if (xpathObj != NULL)
    {
        xmlNodeSetPtr xpathNodes = xpathObj->nodesetval;
        if ((xpathNodes) && (xpathNodes->nodeNr > 0))
        {
            if (handler(xpathNodes))
            {
                result = true;
            }
        }
        
        xmlXPathFreeObject(xpathObj);
    }

    return result;
}

#endif /* XmlParser_h */
