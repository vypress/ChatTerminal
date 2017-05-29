/*
$Id: WinNixDefs.h 36 2011-08-09 07:35:21Z avyatkin $

Definitions for compatibility between Windows and *nix code

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#ifndef CHATTERM_OS_WINDOWS

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/objects.h>
#include <openssl/md5.h>
#include <openssl/rc4.h> // for CHANNEL_INFO.h

#include <uuid/uuid.h>// for USER_INFO.h
#include <signal.h>
#include <assert.h>

typedef int BOOL;
typedef int INT;
typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef struct _SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME;

/*
Base provider public key BLOBs have the following format.
PUBLICKEYSTRUC publickeystruc;
RSAPUBKEY rsapubkey;
BYTE modulus[rsapubkey.bitlen/8];

typedef struct _PUBLICKEYSTRUC {
  BYTE bType;
  BYTE bVersion;
  WORD reserved;
  ALG_ID aiKeyAlg;
} BLOBHEADER,
 PUBLICKEYSTRUC;

The RSAPUBKEY structure contains information specific to the particular public key contained in the key
BLOB.
 typedef struct _RSAPUBKEY {
   DWORD magic;
   DWORD bitlen;
   DWORD pubexp;
 } RSAPUBKEY;
*/
struct PUBKEYDATA
{
	unsigned char bType;
	unsigned char bVersion;
	unsigned char reserved[2/*sizeof(WORD)*/];
	unsigned char aiKeyAlg[4/*sizeof(DWORD)*/];
	unsigned char magic[4/*sizeof(DWORD)*/];
	unsigned char bitlen[4/*sizeof(DWORD)*/];
	unsigned char pubexp[4/*sizeof(DWORD)*/];
	unsigned char modulus[128];
};

#define MAX_PATH PATH_MAX
#define _ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))
#define _ASSERTE(val) assert(val)

#define DBG_UNREFERENCED_LOCAL_VARIABLE(a) (a)
#define UNREFERENCED_PARAMETER(a) (a)

#define FOREGROUND_BLUE	1
#define FOREGROUND_GREEN	2
#define FOREGROUND_RED	4
#define FOREGROUND_INTENSITY	8

/*
* min()/max() macros that also do
* strict type-checking.. See the
* "unnecessary" pointer comparison.
*/
/*
#define __min(x,y) ({\
const typeof(x) _x = (x);\
const typeof(y) _y = (y);\
(void) (&_x == &_y);\
_x < _y ? _x : _y; })

#define __max(x,y) ({\
const typeof(x) _x = (x);\
const typeof(y) _y = (y);\
(void) (&_x == &_y);\
_x > _y ? _x : _y; })
*/
#define __max(a,b) std::max(a,b)
#define __min(a,b) std::min(a,b)

//#define _wcsicmp(s1,s2) wcscasecmp(s1,s2)
//Mac OS X doesn't have wcscasecmp
#define _wcsicmp(s1,s2) NixHlpr.wcsicmp(s1,s2)

#define wcsncpy_s(dest, dest_size, src, n) wcsncpy(dest, src, dest_size)
#define wcscpy_s(dest, dest_size, src) wcscpy(dest, src)
#define wcsnlen_s(str, len) wcslen(str)
#define _wtoi(val) wcstol(val, NULL, 10)
#define swprintf_s swprintf
#define sprintf_s snprintf

#endif // CHATTERM_OS_WINDOWS
