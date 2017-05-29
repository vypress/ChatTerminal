/*
$Id: XmlHelper_libxml.cpp 36 2011-08-09 07:35:21Z avyatkin $

Implementation of global functions from the xmlhelper namespace, that works with the XML documents

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include <string>
#include "XmlHelper_libxml.h"
#include "NixHelper.h"

#include <libxml/parser.h>
#include <libxml/xmlschemas.h>

namespace xmlhelper
{
	void genericErrorFunc(void * ctx, const char * msg, ...)
	{
	}

	void structuredErrorFunc(void * userData, xmlErrorPtr error)
	{
	}

	bool is_xml_schema_valid(const xmlDocPtr doc)
	{
		const char wszDefConfSchemaFile[] = "chatterm10.xsd";

		xmlDocPtr schema_doc = xmlReadFile(wszDefConfSchemaFile, NULL, XML_PARSE_NONET|XML_PARSE_NOWARNING);
		if (schema_doc == NULL) {
			/* the schema cannot be loaded or is not well-formed */
			return false;
		}
		xmlSchemaParserCtxtPtr parser_ctxt = xmlSchemaNewDocParserCtxt(schema_doc);
		if (parser_ctxt == NULL) {
			/* unable to create a parser context for the schema */
			xmlFreeDoc(schema_doc);
			return false;
		}

		//it prevents display errors to stdout
		xmlSetGenericErrorFunc(parser_ctxt, genericErrorFunc);
		xmlSetStructuredErrorFunc(parser_ctxt, structuredErrorFunc);

		xmlSchemaPtr schema = xmlSchemaParse(parser_ctxt);
		if (schema == NULL) {
			/* the schema itself is not valid */
			xmlSchemaFreeParserCtxt(parser_ctxt);
			xmlFreeDoc(schema_doc);
			return false;
		}

		xmlSchemaValidCtxtPtr valid_ctxt = xmlSchemaNewValidCtxt(schema);
		if (valid_ctxt == NULL) {
			/* unable to create a validation context for the schema */
			xmlSchemaFree(schema);
			xmlSchemaFreeParserCtxt(parser_ctxt);
			xmlFreeDoc(schema_doc);
			return false; 
		}
		bool is_valid = (xmlSchemaValidateDoc(valid_ctxt, doc) == 0);
		xmlSchemaFreeValidCtxt(valid_ctxt);
		xmlSchemaFree(schema);
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(schema_doc);
		/* force the return value to be non-negative on success */
		return is_valid;
	}

	bool create_Document_instance(xmlDocPtr* ppdoc, const wchar_t* file_path)
	{
		LIBXML_TEST_VERSION
	
		char* szFilePath = 0;
		NixHlpr.assignCharSz(&szFilePath, file_path);

		/* Load XML document */
		xmlDocPtr doc = xmlReadFile(szFilePath, NULL, XML_PARSE_NOBLANKS|XML_PARSE_NOWARNING);
		//xmlDocPtr doc = xmlParseFile(szFilePath);
	
		delete[] szFilePath;
	
		if (doc == NULL)
		{
			return false;
		}
		
		if(!is_xml_schema_valid(doc))
		{
			xmlFreeDoc(doc); 
			return false;
		}

		*ppdoc = doc;
		return true;
	}

	xmlChar* get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name)
	{
		//skip comments
		while(nodePtr && nodePtr->type == XML_COMMENT_NODE) nodePtr = nodePtr->next;
	
		if(nodePtr && nodePtr->type == XML_ELEMENT_NODE)
		{
			if(!xmlStrEqual(elem_name, nodePtr->name)) return false;
	
			xmlChar* xmlText = xmlNodeGetContent(nodePtr);
	
			nodePtr = nodePtr->next;
	
			return xmlText;
		}
	
		return 0;
	}
	
	int get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name, wchar_t** pptext)
	{
		if(0 == pptext) return -1;
	
		xmlChar* xszNodeText = get_xml_item_text(nodePtr, elem_name);
	
		if(0 == xszNodeText) return 0;
	
		int len = xmlUTF8Strlen(xszNodeText);
		wchar_t* wszText = new wchar_t[len+1];
		wszText[len] = 0;

		size_t ret_len = NixHlpr.convUtf8ToWchar(xszNodeText, xmlUTF8Strsize(xszNodeText,len), wszText, len);

		xmlFree(xszNodeText);
	
		if(ret_len>0)
		{
			*pptext = wszText;
			return len+1;
		}
		
		delete[] wszText;
		return 0;
	}
	
	bool get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name, std::wstring& wstrtext)
	{
		wchar_t* ptext = 0;
		bool result = get_xml_item_text(nodePtr, elem_name, &ptext);
		if(result && ptext)
			wstrtext.assign(ptext);
	
		delete[] ptext;
	
		return result;
	}
}
