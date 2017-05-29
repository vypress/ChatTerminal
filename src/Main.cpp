/*
$Id: Main.cpp 36 2011-08-09 07:35:21Z avyatkin $

Main entry point

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#ifdef CHATTERM_OS_WINDOWS
	#include <crtdbg.h>
#endif // CHATTERM_OS_WINDOWS

//#include <iosfwd>
#include <iostream>

#include "ChatTerminal.h"
#include "StrResources.h"

// HACK Test todo comment.
// UNDONE Test todo comment.
// TODO: Add a command to display current application settings
// TODO: Add references to error, output, and debug streams parameters for the Commands and for the ProcessorMsgX classes
// TODO: Add a reference to a message handler parameter for the ProcessorMsgX class

#ifdef CHATTERM_OS_WINDOWS
int wmain(int argc,wchar_t*argv[],wchar_t*envp[])
{
	UNREFERENCED_PARAMETER(envp);

	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(353);

	//For normal working with Russian and other languages
	setlocale( LC_ALL, ".ACP" );

	if(!theApp.parseCommandLine(argc, argv)) return 0;

	//Initialize Winsock
	WSADATA WSAData = {0};
	if (WSAStartup (MAKEWORD(2,2), &WSAData) != 0)
	{
		std::wcout<< resources::wszErrWsa <<std::hex<<WSAGetLastError();
		return -1;
	}

	if(0 != consoleio::console_initialize())
	{
		WSACleanup();
		return -1;
	}

	CoInitialize(NULL);

	int run_result = theApp.run();

	CoUninitialize();

	consoleio::console_finalize();

	WSACleanup();

	return run_result;
}
#else
int main(int argc, char* argv[])
{
	//For normal working with Russian and other languages
	setlocale( LC_ALL, "" );

	if(!theApp.parseCommandLine(argc, argv)) return 0;

	if(0 != consoleio::console_initialize())
		return -1;

	int run_result = theApp.run();

	consoleio::console_finalize();

	return run_result;
}
#endif // CHATTERM_OS_WINDOWS
