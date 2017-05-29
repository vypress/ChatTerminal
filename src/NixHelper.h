/**
$Id: NixHelper.h 36 2011-08-09 07:35:21Z avyatkin $

Interface, configuration, and implementation of NixHelper class

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#pragma once

#include <iconv.h>
#include <wctype.h> // towlower

/**
A NixHelper class contains helper methods for conversions
This class is used only in *nix environment
*/
class NixHelper
{
#ifdef _LIBICONV_VERSION//libicon header file
typedef const char* inbufTypePtr;
#else//glibc header file
typedef char* inbufTypePtr;
#endif

public:
	NixHelper()
		: icdToWchar_(iconv_open("WCHAR_T", "UTF-8")),
		icdToUtf8_(iconv_open("UTF-8", "WCHAR_T")),
		icdToUtf16_(iconv_open("UTF-16LE", "WCHAR_T"))
	{
	}

	~NixHelper()
	{
		iconv_close(icdToUtf16_);
		iconv_close(icdToUtf8_);
		iconv_close(icdToWchar_);
	}

	size_t convUtf8ToWchar(unsigned char* from, size_t fromlen, wchar_t* to, size_t tolen)
	{
		size_t inbytesleft = fromlen;
		size_t outbytesleft = tolen*sizeof(wchar_t);
	
		inbufTypePtr inbuf = reinterpret_cast<inbufTypePtr>(from);
		char* outbuf = reinterpret_cast<char*>(to);
	
		/*size_t ret_len = */iconv(icdToWchar_, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		return tolen-outbytesleft;
	}

	size_t convWcharToUtf8(wchar_t* from, size_t fromlen, unsigned char* to, size_t tolen)
	{
		size_t inbytesleft = fromlen*sizeof(wchar_t);
		size_t outbytesleft = tolen;
	
		inbufTypePtr inbuf = reinterpret_cast<inbufTypePtr>(from);
		char* outbuf = reinterpret_cast<char*>(to);
	
		/*size_t ret_len = */iconv(icdToUtf8_, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		return tolen-outbytesleft;
	}

	size_t convWcharToUtf16(wchar_t* from, size_t fromlen, unsigned char* to, size_t tolen)
	{
		size_t inbytesleft = fromlen*sizeof(wchar_t);
		size_t outbytesleft = tolen;
	
		inbufTypePtr inbuf = reinterpret_cast<inbufTypePtr>(from);
		char* outbuf = reinterpret_cast<char*>(to);
	
		/*size_t ret_len = */iconv(icdToUtf16_, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		return tolen-outbytesleft;
	}

	void assignWstr(std::wstring& wstr, const char* pszStr)
	{
		wchar_t* pwszBuf = 0;
		size_t ret = assignWcharSz(&pwszBuf, pszStr);
		if(ret>0)
			wstr = pwszBuf;
	
		delete[] pwszBuf;
	}
	
	size_t assignWcharSz(wchar_t** ppwsz, const char* pcsz)
	{
		size_t ret = mbstowcs(NULL, pcsz, 0);
		if(ret<1 || (size_t(-1) == ret)) return 0;

		wchar_t* pwsz = new wchar_t[ret+1];
		ret = mbstowcs(pwsz, pcsz, ret+1);

		if(ret>0 && (size_t(-1) != ret))
		{
			*ppwsz = pwsz;
			return ret+1;
		}
	
		delete[] pwsz;
		return 0;
	}

	size_t assignCharSz(char** ppsz, const wchar_t* pcwsz)
	{
		const wchar_t* src = pcwsz;
		size_t ret = wcsrtombs (NULL, &pcwsz, 0, NULL);
		if(ret<1 || (size_t(-1) == ret)) return 0;
	
		src = pcwsz;
		char* psz = new char[ret+1];
		ret = wcsrtombs (psz, &pcwsz, ret+1, NULL);
		psz[ret] = 0;
	
		if(ret>0 && (size_t(-1) != ret))
		{
			*ppsz = psz;
			return ret+1;
		}
		
		delete[] psz;
		return 0;
	}

	//Mac OS X doesn't have wcscasecmp
        int wcsicmp(const wchar_t* s1, const wchar_t* s2)
        {
            wchar_t w1=0, w2=0;
            while(*s1 && *s2)
            {
                w1 = towlower(*s1++);
                w2 = towlower(*s2++);
                if(w1>w2) return 1;
                if(w1<w2) return -1;
            }

            if(*s1) return 1;
            if(*s2) return -1;

            return 0;
        }
	
	//PowerPC processors are big endian
	bool isBigEndian()
	{
	    const unsigned char puch[]={0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf};
	    const size_t* const pdw = reinterpret_cast<const size_t* const>(puch);
	    return 0!=(*pdw & 0xFFUL);
	}

private:

	iconv_t icdToWchar_;
	iconv_t icdToUtf8_;
	iconv_t icdToUtf16_;
};

//Global object
extern NixHelper NixHlpr;
