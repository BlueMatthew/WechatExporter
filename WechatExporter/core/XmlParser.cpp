//
//  XmlParser.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/11/12.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include "XmlParser.h"

struct NodeValueHandler
{
    std::string& value;
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        // assert ((xpathNodes) && (xpathNodes->nodeNr > 0))
        xmlNode *cur = xpathNodes->nodeTab[0];
        xmlChar* sz = xmlNodeGetContent(cur);
        if (sz != NULL)
        {
            value = reinterpret_cast<char *>(sz);;
            xmlFree(sz);
            return true;
        }
        
        return false;
    }
};

struct NodesValueHandler
{
    std::map<std::string, std::string>& values;
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        // assert ((xpathNodes) && (xpathNodes->nodeNr > 0))
        for (int idx = 0; idx < xpathNodes->nodeNr; ++idx)
        {
            xmlNode *cur = xpathNodes->nodeTab[idx];
            std::map<std::string, std::string>::iterator it = values.find(reinterpret_cast<const char *>(cur->name));
            if (it != values.end())
            {
                xmlChar* sz = xmlNodeGetContent(cur);
                if (sz != NULL)
                {
                    it->second = reinterpret_cast<char *>(sz);;
                    xmlFree(sz);
                }
                else
                {
                    it->second.clear();
                }
            }
        }
        
        return true;
    }
};

struct AttributeHandler
{
    const std::string& name;
    std::string& value;
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        return true;
    }
};

struct AttributesHandler
{
    std::map<std::string, std::string>& attributes;
    
    bool operator() (xmlNodeSetPtr xpathNodes)
    {
        // assert ((xpathNodes) && (xpathNodes->nodeNr > 0))
        xmlNode *cur = xpathNodes->nodeTab[0];
        for (std::map<std::string, std::string>::iterator it = attributes.begin(); it != attributes.end(); ++it)
        {
            xmlChar* attr = xmlGetProp(cur, reinterpret_cast<const xmlChar *>(it->first.c_str()));
            if (NULL != attr)
            {
                it->second = reinterpret_cast<char *>(attr);
                xmlFree(attr);
            }
            else
            {
                it->second.clear();
            }
        }
        
        return true;
    }
};




XmlParser::XmlParser(const std::string& xml, bool noError/* = false*/) : m_doc(NULL), m_xpathCtx(NULL)
{
    int options = XML_PARSE_RECOVER;
    if (noError)
    {
        options |= XML_PARSE_NOERROR;
    }
    
    // xmlSetGenericErrorFunc(NULL, xmlGenericErrorImpl);
    // xmlSetStructuredErrorFunc(NULL, xmlStructuredErrorImpl);
    
    m_doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), NULL, NULL, options);
    if (m_doc != NULL)
    {
        m_xpathCtx = xmlXPathNewContext(m_doc);
    }
}

XmlParser::~XmlParser()
{
    if (m_xpathCtx) { xmlXPathFreeContext(m_xpathCtx); }
    if (m_doc) { xmlFreeDoc(m_doc); }
}

bool XmlParser::parseNodeValue(const std::string& xpath, std::string& value)
{
    NodeValueHandler handler = {value};
    return parseImpl(xpath, handler);
}

bool XmlParser::parseNodesValue(const std::string& xpath, std::map<std::string, std::string>& values)
{
    NodesValueHandler handler = {values};
    return parseImpl(xpath, handler);
}

bool XmlParser::parseAttributeValue(const std::string& xpath, const std::string& attributeName, std::string& value)
{
    AttributeHandler handler = {attributeName, value};
    return parseImpl(xpath, handler);
}

bool XmlParser::parseAttributesValue(const std::string& xpath, std::map<std::string, std::string>& attributes)
{
    AttributesHandler handler = {attributes};
    return parseImpl(xpath, handler);
}
