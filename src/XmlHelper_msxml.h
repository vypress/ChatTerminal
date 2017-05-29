/*
$Id: XmlHelper_msxml.h 24 2010-08-15 22:08:57Z avyatkin $

Declaration of global functions from the xmlhelper namespace, that works with the XML documents

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#pragma once

#include <msxml2.h>

namespace xmlhelper
{
	HRESULT create_DOM_instance(IXMLDOMDocument** ppXMLDoc);
	HRESULT get_xml_item(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, IXMLDOMNode** ppListItem);
	HRESULT get_xml_item_text(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, std::wstring& text);
	HRESULT get_xml_item_text(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, BSTR* pbstrText);
	HRESULT get_xml_attribute(const wchar_t* wszAttrName, IXMLDOMNamedNodeMap* pattributeMap, VARIANT* pvarValue);
}
