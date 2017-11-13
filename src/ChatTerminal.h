/**
$Id: ChatTerminal.h 36 2011-08-09 07:35:21Z avyatkin $

Interface and configuration of the ChatTerminalApp class

ChatTerminal - a command line instant chat software that is compatible with Vypress Chat for Windows
Copyright (C) 2011 VyPRESS Research, LLC. All rights reserved.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.

Author: Aleksey Vyatkin

VyPRESS Research, LLC., hereby disclaims all copyright interest in
the application "ChatTerminal" written by Aleksey Vyatkin.

signature of Aleksey Vyatkin, 01 July 2010
Aleksey Vyatkin, President
*/

#pragma once

#include <time.h>

#include <set>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>    // std::lexicographical_compare
#include <cctype>       // std::tolower

#ifndef CHATTERM_OS_WINDOWS
#include <string.h> //memset, memcpy, memcmp
#include "WinNixDefs.h"
#include "NixHelper.h"
#endif // CHATTERM_OS_WINDOWS

#include "NetworkIo.h"
#include "USER_INFO.h"
#include "CHANNEL_INFO.h"
#include "Commands.h"
#include "ConsoleIo.h"
#include "XmlHelper.h"

#ifdef CHATTERM_OS_WINDOWS
#ifdef _DEBUG
#ifdef _CRTDBG_MAP_ALLOC
#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
#endif //_CRTDBG_MAP_ALLOC
#endif //_DEBUG
#endif //CHATTERM_OS_WINDOWS

/**
A ChatTerminalApp class describes an application object that aggregates
information about environment, settings, network, users, and channels.
It contains and maintains globally used objects.
*/
class ChatTerminalApp
{
	// An enumeration of commands types
	enum COMMAND_ID {NULL_COMMAND=0,
					ECHO,

					CLS,
					COLOR,
					EXIT,
					FEMALE,
					HELP,
					MALE,
					NICK_NEW,
					QUIT,
					WAIT,
					WHOIM,

					ALL_CHANNELS,
					ALL_USERS,
					BEEP,
					CHANGE_CHANNEL,
					CHANNELS,
					CHANNEL_USERS,
					FLOOD,
					HERE,
					INFO,
					JOIN,
					LEAVE,
					LIST,
					MASS,
					MASS_TO,
					ME,
					MY_CHANNELS,
					PING,
					SJOIN,
					TOPIC_ADD,
					TOPIC_NEW};

public:
	ChatTerminalApp(void);
	~ChatTerminalApp(void);

	// Main application routine
	int run();

	// Globally used object that creates and sends requests, responses, and notifications
	Commands Commands_;

	// Globally used object that describes information about you
	USER_INFO Me_;

	//Pointer to object that contains user personal information
	PERSONAL_INFO MyPersonalInfo_;

	// Globally used arrays of Sender and Receiver objects, that are used to send and receive messages over the network
	// We do not use std::auto_ptr<> because pointers to Sender are temporary shared
	// between several containers (mapIdIf, mapIdSender, mapIdIfSender) in initNetConfigFromXml()
	// We could to use here a shared pointer or a pointer with reference counting
	// but there are no such safe pointers in the C++ STL
	std::vector< networkio::Sender* > Senders_;
	std::vector< networkio::Receiver* > Receivers_;


	/**
	Copies the time to a buffer
	@fWithSeconds - specifies whether to copy time with or without seconds
	@return - pointer to a static time buffer
	*/
	static const wchar_t* getStrTime(bool fWithSeconds = false);

#ifdef CHATTERM_OS_WINDOWS
	// Globally used "Microsoft Base Cryptographic Provider v1.0" CSP provider descriptor
	HCRYPTPROV hCryptProv_;

	/**
	Standard routine to parse command line parameters
	*/
	bool parseCommandLine(int argc, wchar_t* argv[]);
#else
	//RSA assymetric key pair for messages' digital signature
	RSA* pRSA_;

	bool parseCommandLine(int argc, char* argv[]);
#endif // CHATTERM_OS_WINDOWS

	/**
	Unblocks the application timer from waiting an event
	*/
	void resumeTimer();

private:

	/**
	Initializes application configuration, starts receive threads and timer thread
	*/
	void initialize();

	/**
	Frees application resources, stops receive threads and timer thread
	*/
	void finalize();

	/**
	Initializes application configuration
	@return - true if successful, false otherwise
	*/
	bool initConfig();

	/**
	Initializes default network configuration
	@return - true if successful, false otherwise
	*/
	bool initDefaultNetConfig();

	/**
	Loads application configuration from specified XML file
	@file_path - path to xml file
	@return - true if successful, false otherwise
	*/
	bool initConfigFromXml(const wchar_t* file_path);

	/**
	Loads network configuration from specified XML document object
	@pXMLDoc - pointer to the XML object
	@return - true if successful, false otherwise
	*/
#ifdef CHATTERM_OS_WINDOWS
	bool initNetConfigFromXml(IXMLDOMDocument* pXMLDoc);
#else
	bool initNetConfigFromXml(xmlNodePtr network_nodePtr);
#endif // CHATTERM_OS_WINDOWS

	/**
	Initializes Me_ object
	*/
	void initMe();

	/**
	Creates asymmetric 1024 bits key pair for signing messages
	@pPubBuffer - reference to returned buffer that contains a private key
	@cPubLen - reference to a variable that receives the size of the pPubBuffer buffer
	@return - 0 if successful, -1 otherwise
	*/
	int getPublicKey(unsigned char*& pPubBuffer, unsigned short& cPubLen);

	/**
	Processes typed text, sends a command or a message to a channel
	@line - typed text
	@line_len - typed text length
	@return - 0 if successful, negative value otherwise
	*/
	int sendChatLine(const wchar_t* line, size_t line_len);

	/**
	Tests if a file exists, used by parseCommandLine()
	@path - to the file
	@return - true if the file exists, false otherwise
	*/

	bool testFileExistence(const wchar_t* path, bool fMsg);
#ifndef CHATTERM_OS_WINDOWS
	bool testFileExistence(const char* path, bool fMsg);
#endif // CHATTERM_OS_WINDOWS

	/**
	Processes command parameters string,
	and determines a first parameter that
	must be enclosed with any symbols (for example ' a single quote)
	@p - parameters string to be processed
	@second - reference to a variable that receives position to a second parameter
	@first_buf - reference to a variable that receive a pointer to a created buffer
	             which contains first parameter;
	             Returned buffer must be freed by delete[] operator
	@return - length of first parameter
	*/
	size_t getSecondParam(const wchar_t* p, const wchar_t*& second, wchar_t*& first_buf);

	/**
	Processes command by provided ID and parameters string
	@id - command ID
	@params - parameters string
	@params_len - length of parameters string
	@return - 0 if successful, negative value otherwise
	*/
	int processCommandId(COMMAND_ID id, const wchar_t* params, size_t params_len);

	/**
	Processes an input stream from the initialization file or the redirected input stream
	@ins - reference to the stream
	@return - 0 if successful, negative value otherwise
	*/
	int processStreamInput(std::wistream& ins);

	/**
	Executes all necessary commands to leave all channels and chat networks;
	Clears all users and channels
	*/
	void leaveNetwork(void);

#ifdef CHATTERM_OS_WINDOWS
	// Event to stop the application timer
	HANDLE hTimerEvent_;

	// Descriptor of the thread which maintains the application timer
	HANDLE hTimerThread_;

	/**
	A routine that begins execution of a timer thread
	@pParam - parameter passed to a new thread or NULL; It is a pointer to the global ChatTerminalApp object
	@return - 0
	*/
	static unsigned int __stdcall timerProc(void* pParam);

	/**
	An application-defined function used with the SetConsoleCtrlHandler function.
	A console process uses this function to handle control signals received by the process.
	When the signal is received, the system creates a new thread in the process to execute the function.

	@dwCtrlType - type of control signal received by the handler
	@return - if the function handles the control signal, it should return TRUE.
	          If it returns FALSE, the next handler function in the list of handlers for this process is used.
	*/
	static BOOL WINAPI HandlerRoutine( DWORD dwCtrlType);
        
        // Path to the initialization commands file
	const wchar_t* wszInitFile_;
        
	// Path to the application configuration file
	const wchar_t* wszConfXmlFile_;

	// Path to the configuration file of Me_ object (Your user information);
	// By default this file is located in the %USERPROFILE%\Application Data\ folder
	const wchar_t* wszConfUserXmlFile_;
#else
	// Event to stop the application timer
	pthread_cond_t timer_wait_cond_;

	// Mutex to control access to event (timer_wait_cond_) of the application timer
	pthread_mutex_t timer_mutex_;

	// Descriptor of the thread which maintains the application timer
	pthread_t timer_thread_;

	/**
	A routine that begins execution of a timer thread
	@pParam - parameter passed to a new thread or NULL; It is a pointer to the global ChatTerminalApp object
	@return - 0
	*/
	static void* timerProc(void* pParam);

	/**
	A standard UNIX signals handler that executes void finalize(); 
	*/
	static void signalFinalize(int signo);
        
        // Path to the initialization commands file
	const char* wszInitFile_;
        
	// Path to the application configuration file
	const char* wszConfXmlFile_;

	// Path to the configuration file of Me_ object (Your user information);
	// By default this file is located in the %USERPROFILE%\Application Data\ folder
	const char* wszConfUserXmlFile_;
#endif // CHATTERM_OS_WINDOWS

	static void signalAlarm(int signo);
        
	// If true then all entered commands and messages are echoed to the output stream
	bool fEcho_;

	// Became true after any terminate UNIX signal was received or by HandlerRoutine()
	bool fSigTerm_;

	// Time interval during which unreachable users are dropped
	unsigned int nDropUsersAfterInterval_;

	// Maximum attempts to reach a user
	static const int maxUserPings_ = 3;

	// Array of available network interfaces
	// We do not use std::auto_ptr<> because pointers to Sender are temporary shared
	// between several containers (mapIdIf, mapIdSender, mapIdIfSender) in initNetConfigFromXml()
	// We could to use here a shared pointer or a pointer with reference counting
	// but there are no such safe pointers in the C++ STL
	std::vector< networkio::Interface* > Interfaces_;

	//Default path to the application configuration file
	static const wchar_t wszDefConfXmlFile_[];

	//Default path to the user configuration file
	static const wchar_t wszDefConfUserXmlFile_[];
};

//Globally available ChatTerminalApp object description
extern ChatTerminalApp theApp;

//Controls access to SetOfChannels_ and SetOfUsers_ objects
class ContainersMonitor
{
public:
	explicit ContainersMonitor()
	{
#ifdef CHATTERM_OS_WINDOWS
		EnterCriticalSection(&cs_);
#else
		pthread_mutex_lock( &m_ );
#endif// CHATTERM_OS_WINDOWS
	}

	~ContainersMonitor()
	{
#ifdef CHATTERM_OS_WINDOWS
		LeaveCriticalSection(&cs_);
#else
		pthread_mutex_unlock( &m_ );
#endif// CHATTERM_OS_WINDOWS
	}

private:
	ContainersMonitor(const ContainersMonitor&);
	ContainersMonitor& operator=(const ContainersMonitor&) = delete;
	ContainersMonitor* operator&() = delete;

	static void Initialize()
	{
#ifdef CHATTERM_OS_WINDOWS
		InitializeCriticalSection(&cs_);
#else
		pthread_mutexattr_t attr;
		int err = pthread_mutexattr_init(&attr);
		err = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
		err = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		err = pthread_mutex_init(&m_, &attr);
#endif// CHATTERM_OS_WINDOWS
	}

	static void Delete()
	{
#ifdef CHATTERM_OS_WINDOWS
		DeleteCriticalSection(&cs_);
#else
		pthread_mutex_destroy(&m_);
#endif// CHATTERM_OS_WINDOWS
	}

#ifdef CHATTERM_OS_WINDOWS
	static CRITICAL_SECTION cs_;
#else
	static pthread_mutex_t m_;
#endif// CHATTERM_OS_WINDOWS

	friend class ChatTerminalApp;
};
