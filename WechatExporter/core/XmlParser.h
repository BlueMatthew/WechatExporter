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
    
    static std::string getNodeInnerText(xmlNodePtr node);
    static std::string getNodeInnerXml(xmlNodePtr node);
    static bool getChildNodeContent(xmlNodePtr node, const std::string& childName, std::string& value);
    static bool getNodeAttributeValue(xmlNodePtr node, const std::string& attributeName, std::string& value);
    
public:
    template <class TNodeHandler>
    bool parseWithHandler(const std::string& xpath, TNodeHandler& handler);
    xmlXPathObjectPtr evalXPathOnNode(xmlNodePtr node, const std::string& xpath);
    
private:
    xmlDocPtr m_doc;
    xmlXPathContextPtr m_xpathCtx;
};

inline std::string XmlParser::getNodeInnerText(xmlNodePtr node)
{
    const char* content = reinterpret_cast<const char*>(XML_GET_CONTENT(node->children));
    return NULL == content ? "" : std::string(content);
}

inline std::string XmlParser::getNodeInnerXml(xmlNodePtr node)
{
    xmlBufferPtr buffer = xmlBufferCreate();
#ifndef NDEBUG
    int size = xmlNodeDump(buffer, node->doc, node, 0, 1);
#else
    int size = xmlNodeDump(buffer, node->doc, node, 0, 0);  // no format for release
#endif
    
    // const char* content = reinterpret_cast<const char*>(XML_GET_CONTENT(node->children));
    std::string xml;
    if (size > 0 && NULL != buffer->content)
    {
        xml.assign(reinterpret_cast<const char*>(buffer->content), size);
    }
    xmlBufferFree(buffer);
    return xml;
}

template <class TNodeHandler>
bool XmlParser::parseWithHandler(const std::string& xpath, TNodeHandler& handler)
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
