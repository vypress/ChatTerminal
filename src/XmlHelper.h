/*
$Id: XmlHelper.h 22 2010-08-15 21:49:13Z avyatkin $

Declaration of global functions from the xmlhelper namespace, that works with the XML documents

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#pragma once

#ifdef CHATTERM_OS_WINDOWS
#include "XmlHelper_msxml.h"
#else
#include "XmlHelper_libxml.h"
#endif // CHATTERM_OS_WINDOWS
