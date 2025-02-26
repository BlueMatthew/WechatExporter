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
            value = reinterpret_cast<char *>(sz);
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
                    it->second = reinterpret_cast<char *>(sz);
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
        // assert ((xpathNodes) && (xpathNodes->nodeNr > 0))
        xmlNode *cur = xpathNodes->nodeTab[0];

        xmlChar* attr = xmlGetProp(cur, reinterpret_cast<const xmlChar *>(name.c_str()));
        if (NULL != attr)
        {
            value = reinterpret_cast<char *>(attr);
            xmlFree(attr);
            return true;
        }
        
        return false;
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

bool XmlParser::getChildNodeContent(xmlNodePtr node, const std::string& childName, std::string& value)
{
    bool found = false;
    xmlNodePtr childNode = xmlFirstElementChild(node);
    while (NULL != childNode)
    {
        if (xmlStrcmp(childNode->name, BAD_CAST(childName.c_str())) == 0)
        {
            value = getNodeInnerText(childNode);
            found = true;
            break;
        }
        
        childNode = childNode->next;
    }

    return found;
}

xmlNodePtr XmlParser::getChildNode(xmlNodePtr node, const std::string& childName)
{
    xmlNodePtr result = NULL;
    xmlNodePtr childNode = xmlFirstElementChild(node);
    while (NULL != childNode)
    {
        if (xmlStrcmp(childNode->name, BAD_CAST(childName.c_str())) == 0)
        {
            result = childNode;
            break;
        }
        
        childNode = childNode->next;
    }

    return result;
}

xmlNodePtr XmlParser::getNextNodeSibling(xmlNodePtr node)
{
    return xmlNextElementSibling(node);
}

bool XmlParser::getNodeAttributeValue(xmlNodePtr node, const std::string& attributeName, std::string& value)
{
    xmlChar* attr = xmlGetProp(node, BAD_CAST(attributeName.c_str()));
    if (NULL != attr)
    {
        value = reinterpret_cast<char *>(attr);
        xmlFree(attr);
        return true;
    }
    return false;
}

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

xmlXPathObjectPtr XmlParser::evalXPathOnNode(xmlNodePtr node, const std::string& xpath)
{
    return xmlXPathNodeEval(node, BAD_CAST(xpath.c_str()), m_xpathCtx);
}

bool XmlParser::parseNodeValue(const std::string& xpath, std::string& value) const
{
    NodeValueHandler handler = {value};
    return parseWithHandler(xpath, handler);
}

bool XmlParser::parseNodesValue(const std::string& xpath, std::map<std::string, std::string>& values) const
{
    NodesValueHandler handler = {values};
    return parseWithHandler(xpath, handler);
}

bool XmlParser::parseChildNodesValue(xmlNodePtr parentNode, const std::string& xpath, std::map<std::string, std::string>& values) const
{
    XPathEnumerator enumerator(*this, parentNode, xpath);
    if (enumerator.isInvalid())
    {
        return false;
    }
    
    while (enumerator.hasNext())
    {
        xmlNodePtr cur = enumerator.nextNode();
        
        std::string name = reinterpret_cast<const char*>(cur->name);
        std::map<std::string, std::string>::iterator it = values.find(name);
        if (it != values.end())
        {
            it->second = getNodeInnerText(cur);
        }
    }
    
    return true;
}

bool XmlParser::parseAttributeValue(const std::string& xpath, const std::string& attributeName, std::string& value) const
{
    AttributeHandler handler = {attributeName, value};
    return parseWithHandler(xpath, handler);
}

bool XmlParser::parseAttributesValue(const std::string& xpath, std::map<std::string, std::string>& attributes) const
{
    AttributesHandler handler = {attributes};
    return parseWithHandler(xpath, handler);
}
