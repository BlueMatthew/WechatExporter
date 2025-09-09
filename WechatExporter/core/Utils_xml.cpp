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

	// Check if the data looks like valid XML before attempting to parse
	// Skip if empty or doesn't start with '<' (likely binary data)
	if (xml.empty() || xml[0] != '<') {
		return false;
	}
	
	// Additional check: look for common XML patterns
	// If the string contains too many non-printable characters, it's likely binary
	int nonPrintableCount = 0;
	for (size_t i = 0; i < std::min(xml.size(), size_t(100)); ++i) {
		if (xml[i] < 32 && xml[i] != '\t' && xml[i] != '\n' && xml[i] != '\r') {
			nonPrintableCount++;
		}
	}
	// If more than 10% of the first 100 characters are non-printable, likely binary
	if (nonPrintableCount > 10) {
		return false;
	}

	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodeSetPtr xpathNodes = NULL;

	doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
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

	// Check if the data looks like valid XML before attempting to parse
	// Skip if empty or doesn't start with '<' (likely binary data)
	if (xml.empty() || xml[0] != '<') {
		return false;
	}
	
	// Additional check: look for common XML patterns
	// If the string contains too many non-printable characters, it's likely binary
	int nonPrintableCount = 0;
	for (size_t i = 0; i < std::min(xml.size(), size_t(100)); ++i) {
		if (xml[i] < 32 && xml[i] != '\t' && xml[i] != '\n' && xml[i] != '\r') {
			nonPrintableCount++;
		}
	}
	// If more than 10% of the first 100 characters are non-printable, likely binary
	if (nonPrintableCount > 10) {
		return false;
	}

	xmlDocPtr doc = NULL;
	xmlXPathContextPtr xpathCtx = NULL;
	xmlXPathObjectPtr xpathObj = NULL;
	xmlNodeSetPtr xpathNodes = NULL;

	doc = xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), NULL, NULL, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
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