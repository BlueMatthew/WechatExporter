//
//  Utils.cpp
//  WechatExporter
//
//  Created by Matthew on 2020/9/30.
//  Copyright © 2020 Matthew. All rights reserved.
//

#include <string>
#include "Utils.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

bool getXmlNodeValue(const std::string& xml, const std::string& xpath, std::string& value)
{
	bool result = false;
	value.clear();

	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodeSetPtr xpathNodes = NULL;

	doc = xmlParseMemory(xml.c_str(), static_cast<int>(xml.size()));
	if (doc == NULL) { goto end; }

	xpathCtx = xmlXPathNewContext(doc);
	if (xpathCtx == NULL) { goto end; }

	xpathObj = xmlXPathEvalExpression(BAD_CAST(xpath.c_str()), xpathCtx);
	if (xpathObj == NULL) { goto end; }

	xpathNodes = xpathObj->nodesetval;
	if ((xpathNodes) && (xpathNodes->nodeNr > 0))
	{
		xmlNode *cur = xpathNodes->nodeTab[0];
		xmlChar* sz = xmlNodeGetContent(cur);
		if (sz != NULL)
		{
			value = reinterpret_cast<char *>(sz);;
			xmlFree(sz);
		}
		result = true;
	}

end:
	if (xpathObj) { xmlXPathFreeObject(xpathObj); }
	if (xpathCtx) { xmlXPathFreeContext(xpathCtx); }
	if (doc) { xmlFreeDoc(doc); }

	return result;
}

bool getXmlNodeAttributeValue(const std::string& xml, const std::string& xpath, const std::string& attributeName, std::string& value)
{
	bool result = false;
	value.clear();

	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodeSetPtr xpathNodes = NULL;

	doc = xmlParseMemory(xml.c_str(), static_cast<int>(xml.size()));
	if (doc == NULL) { goto end; }

	xpathCtx = xmlXPathNewContext(doc);
	if (xpathCtx == NULL) { goto end; }

	xpathObj = xmlXPathEvalExpression(BAD_CAST(xpath.c_str()), xpathCtx);
	if (xpathObj == NULL) { goto end; }

	xpathNodes = xpathObj->nodesetval;
	if ((xpathNodes) && (xpathNodes->nodeNr > 0))
	{
		xmlNode *cur = xpathNodes->nodeTab[0];
		xmlChar* attr = xmlGetProp(cur, reinterpret_cast<const xmlChar *>(attributeName.c_str()));
		if (NULL != attr)
		{
			value = reinterpret_cast<char *>(attr);
			xmlFree(attr);
		}
		
		result = true;
	}

end:
	if (xpathObj) { xmlXPathFreeObject(xpathObj); }
	if (xpathCtx) { xmlXPathFreeContext(xpathCtx); }
	if (doc) { xmlFreeDoc(doc); }

	return result;
}