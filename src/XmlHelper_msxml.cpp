/*
$Id: XmlHelper_msxml.cpp 36 2011-08-09 07:35:21Z avyatkin $

Implementation of global functions from the xmlhelper namespace, that works with the XML documents

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include <string>
#include "XmlHelper_msxml.h"

namespace xmlhelper
{
	HRESULT create_DOM_instance(IXMLDOMDocument** ppXMLDoc)
	{
		HRESULT hRes = E_FAIL;
		hRes = CoCreateInstance(CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
		if(FAILED(hRes))
		{
			//hRes = CoCreateInstance(CLSID_DOMDocument50, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
			//if(FAILED(hRes))
			{
				// Trying MSXML 4.0
				hRes = CoCreateInstance(CLSID_DOMDocument40, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
				if(FAILED(hRes))
				{
					hRes = CoCreateInstance(CLSID_DOMDocument30, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
					if (FAILED(hRes))
					{
						hRes = CoCreateInstance(CLSID_DOMDocument26, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
						if (FAILED(hRes))
						{
							//hRes = CoCreateInstance(CLSID_DOMDocument20, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
							//if (FAILED(hRes))
							{
								//hRes = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)ppXMLDoc);
								//if(FAILED(hRes))
								{
									//return hRes;
								}
							}
						}
					}
				}
			}
		}

		if(SUCCEEDED(hRes))
		{
			hRes = (*ppXMLDoc)->put_async(VARIANT_FALSE);
			hRes = (*ppXMLDoc)->put_resolveExternals(VARIANT_TRUE);//VARIANT_FALSE
			hRes = (*ppXMLDoc)->put_validateOnParse(VARIANT_TRUE);//VARIANT_FALSE
			//hRes = (*ppXMLDoc)->put_preserveWhiteSpace(VARIANT_FALSE);
		}

		return hRes;
	}

	bool is_xml_node_name(IXMLDOMNode* node, const wchar_t* name )
	{
		BSTR bstrName = 0;
		//HRESULT hr = node->get_nodeName(&bstrName);
		HRESULT hr = node->get_baseName(&bstrName);
		if(S_OK == hr)
		{
			if(bstrName)
			{
				int res = wcscmp(bstrName, name);
				SysFreeString(bstrName);
				return 0==res;
			}
		}

		return false;
	}

	HRESULT get_xml_item(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, IXMLDOMNode** ppListItem)
	{
		if(0 == ppListItem) return E_FAIL;

		IXMLDOMNode* pListItem = 0;
		HRESULT hr = S_OK;
		DOMNodeType type = NODE_INVALID;

		//skip comments
		do
		{
			if(pListItem)
			{
				++index;//If element with name elem_name was not found them index should be unchanged
				pListItem->Release();
			}

			pListItem = 0;
			hr = pList->get_item(index, &pListItem);
			if((S_OK == hr) && pListItem)
				hr = pListItem->get_nodeType(&type);
		}
		while(S_OK == hr && NODE_COMMENT == type);

		if(S_OK == hr)
		{
			_ASSERTE(NODE_ELEMENT == type);

			if(NODE_ELEMENT == type)
			{
				if(xmlhelper::is_xml_node_name(pListItem, elem_name))
				{
					++index;//If element with name elem_name was not found them index should be unchanged
					*ppListItem = pListItem;
					return hr;
				}
				else
					hr = E_FAIL;
			}
			else
				hr = E_FAIL;

			pListItem->Release();
		}

		return hr;
	}

	HRESULT get_xml_item_text(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, std::wstring& text)
	{
		BSTR bstrText = 0;
		HRESULT hr = get_xml_item_text(pList, index, elem_name, &bstrText);
		if(S_OK==hr && bstrText)
			text = bstrText;

		SysFreeString(bstrText);

		return hr;
	}

	HRESULT get_xml_item_text(IXMLDOMNodeList *pList, long& index, const wchar_t* elem_name, BSTR* pbstrText)
	{
		IXMLDOMNode* pListItem = 0;
		HRESULT hr = get_xml_item(pList, index, elem_name, &pListItem);

		if(S_OK != hr || 0 == pListItem) return hr;

		//hr = pListItem->get_text(pbstrText);

		//another way to get a text without an extension to the World Wide Web Consortium (W3C) DOM.
		//////////////////////
		IXMLDOMNodeList *pChildsList = 0;
		hr = pListItem->get_childNodes(&pChildsList);
		if(S_OK == hr && pChildsList)
		{
			pListItem->Release();
			pListItem = 0;

			DOMNodeType type = NODE_INVALID;
			long index = 0;

			BSTR bstrCollectedText = 0;
			unsigned int cch = 0;
			HRESULT hr1 = S_FALSE;
			//skip comments and collect text from text nodes
			do
			{
				hr1 = pChildsList->get_item(index++, &pListItem);
				if((S_OK == hr) && pListItem)
				{
					hr = pListItem->get_nodeType(&type);

					_ASSERTE(NODE_TEXT == type || NODE_COMMENT == type);

					if(S_OK == hr && NODE_TEXT == type)
					{
						VARIANT value = {0};
						hr = pListItem->get_nodeValue(&value);
						if(S_OK == hr && value.vt == VT_BSTR && value.bstrVal && *value.bstrVal)
						{
							//add a piece of text
							unsigned int text_len = cch;
							cch += SysStringLen(value.bstrVal);
							if(SysReAllocStringLen(&bstrCollectedText, bstrCollectedText, cch))
							{
								wcsncpy_s(bstrCollectedText+text_len, cch+1-text_len, value.bstrVal, SysStringLen(value.bstrVal));
							}
							else
							{
								hr = E_FAIL;
							}
						}

						VariantClear(&value);
					}

					pListItem->Release();
					pListItem = 0;
				}
			}
			while(S_OK == hr1 && (NODE_COMMENT == type || NODE_TEXT == type));

			if(S_OK == hr1)
			{
				_ASSERTE(NODE_TEXT == type || NODE_COMMENT == type);

				if(NODE_COMMENT != type && NODE_TEXT != type)
					hr = E_FAIL;
			}

			pChildsList->Release();

			if(0 == bstrCollectedText) return E_FAIL;

			if(S_OK == hr)
				*pbstrText = bstrCollectedText;
			else
				SysFreeString(bstrCollectedText);
		}

		////////////////////////

		//pListItem->Release();

		return hr;
	}

	HRESULT get_xml_attribute(const wchar_t* wszAttrName, IXMLDOMNamedNodeMap* pattributeMap, VARIANT* pvarValue)
	{
		BSTR bstrAttrName = SysAllocString(wszAttrName);

		IXMLDOMNode* namedItem = 0;
		HRESULT hr = pattributeMap->getNamedItem(bstrAttrName, &namedItem);
		if((S_OK == hr) && namedItem)
		{
			hr = namedItem->get_nodeValue(pvarValue);
			namedItem->Release();
		}

		if(bstrAttrName) SysFreeString(bstrAttrName);

		return hr;
	}
}
