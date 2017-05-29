/*
$Id: XmlHelper_libxml.h 36 2011-08-09 07:35:21Z avyatkin $

Declaration of global functions from the xmlhelper namespace, that works with the XML documents

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#pragma once

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

namespace xmlhelper
{
	/**
	Validates an XML document schema
	*/
	bool is_xml_schema_valid(const xmlDocPtr doc);

	bool create_Document_instance(xmlDocPtr* ppdoc, const wchar_t* file_path);

	xmlChar* get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name);

	int get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name, wchar_t** pptext);

	bool get_xml_item_text(xmlNodePtr& nodePtr, const xmlChar* elem_name, std::wstring& wstrtext);
}
