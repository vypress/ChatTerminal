/**
$Id: ChatTerminal.cpp 36 2011-08-09 07:35:21Z avyatkin $

Implementation of the ChatTerminalApp class

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#ifdef CHATTERM_OS_WINDOWS
#include <process.h>    /* _beginthread, _endthread */
#endif // CHATTERM_OS_WINDOWS

#include <errno.h>

//#include <iosfwd>
#include <iostream>
#include <fstream>

#include <map>
#include <algorithm>

#include "ChatTerminal.h"
#include "ProcessorMsgX.h"

#include "StrResources.h"

#ifdef CHATTERM_OS_WINDOWS
#include <shlobj.h>
#endif // CHATTERM_OS_WINDOWS

using namespace resources;

template <class T> class delete_class_ptr
{
public:
	void operator()(T* p) { delete p; }
};

//Control access to CHANNEL_INFO::SetOfChannels_ and USER_INFO::SetOfUsers_
#ifdef CHATTERM_OS_WINDOWS
CRITICAL_SECTION ContainersMonitor::cs_ = {0};
const wchar_t ChatTerminalApp::wszDefConfUserXmlFile_[] = L"\\chatterm.user.xml";
#else
pthread_mutex_t ContainersMonitor::m_ = {0};//PTHREAD_MUTEX_INITIALIZER;

const wchar_t ChatTerminalApp::wszDefConfUserXmlFile_[] = L"/chatterm.user.xml";
#endif // CHATTERM_OS_WINDOWS

const wchar_t ChatTerminalApp::wszDefConfXmlFile_[] = L"chatterm.xml";

ChatTerminalApp theApp;
#ifndef CHATTERM_OS_WINDOWS
NixHelper NixHlpr;
#endif

ChatTerminalApp::ChatTerminalApp(void) :
#ifdef CHATTERM_OS_WINDOWS
		hCryptProv_(0),
		hTimerEvent_(NULL),
		hTimerThread_(INVALID_HANDLE_VALUE),
#else
		pRSA_(RSA_new()),
		//timer_wait_cond_(PTHREAD_COND_INITIALIZER),
		//timer_mutex_(PTHREAD_MUTEX_INITIALIZER),
		timer_thread_(0),
#endif
		wszInitFile_(0),
		wszConfXmlFile_(0),
		wszConfUserXmlFile_(0),
		fEcho_(false),
		fSigTerm_(false),
		nDropUsersAfterInterval_(5)//5 minutes default dropdown users interval
{
#ifndef CHATTERM_OS_WINDOWS
#ifdef _DEBUG
	CRYPTO_malloc_debug_init();
	CRYPTO_dbg_set_options(V_CRYPTO_MDEBUG_ALL);
	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
#endif // _DEBUG
	pthread_mutexattr_t mattr;
	int err = pthread_mutexattr_init(&mattr);
	err = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_PRIVATE);
	err = pthread_mutex_init(&timer_mutex_, &mattr);

	pthread_condattr_t cattr;
	err = pthread_condattr_init(&cattr);
	err = pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_PRIVATE);
	pthread_cond_init(&timer_wait_cond_, &cattr);
#endif // CHATTERM_OS_WINDOWS
	ContainersMonitor::Initialize();
}

ChatTerminalApp::~ChatTerminalApp(void)
{
#ifdef CHATTERM_OS_WINDOWS
	if(hCryptProv_) CryptReleaseContext(hCryptProv_, 0);
#else
	RSA_free(pRSA_);
	pthread_cond_destroy(&timer_wait_cond_);
	pthread_mutex_destroy(&timer_mutex_);
#endif
	ContainersMonitor::Delete();
}

const wchar_t* ChatTerminalApp::getStrTime(bool fWithSeconds)
{
	static wchar_t wszTime[16]={0};

#ifdef CHATTERM_OS_WINDOWS
	_wstrtime_s(wszTime, _ARRAYSIZE(wszTime));//hh:mm:ss
#else
	time_t t = time(NULL);
	struct tm* local_tm = localtime(&t);
	swprintf(wszTime, _ARRAYSIZE(wszTime), L"%02d:%02d:%02d", local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec);
#endif // CHATTERM_OS_WINDOWS

	if(!fWithSeconds)
		wszTime[5] = 0;//trim seconds

	return wszTime;
}

#ifdef CHATTERM_OS_WINDOWS
bool ChatTerminalApp::testFileExistence(const wchar_t* path, bool fMsg)
{
	int ares = _waccess( path, 04);//read only
	if(0!=ares)
	{
		const size_t buflen = MAX_PATH;
		wchar_t wszPathBuf[buflen] = {0};
		//DWORD WINAPI ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize);
		//BOOL PathUnExpandEnvStrings(LPCTSTR pszPath, LPTSTR pszBuf, UINT cchBuf);
		if(ExpandEnvironmentStrings(path, wszPathBuf, buflen))
		{
			if(0 == _waccess( wszPathBuf, 04)) return true;
		}

		if(!fMsg) return false;

		switch(errno)
		{
		case EACCES:
			std::wcout<<path<<L" - "<<wszAccessDenied<<std::endl;
			break;
		case ENOENT:
			std::wcout<<path<<L" - "<<wszFileNotFound<<std::endl;
			break;
		case EINVAL:
		default:
			std::wcout<<path<<L" - "<<wszFileInvalidParam<<std::endl;
		}
		return false;
	}

	return true;
}

bool ChatTerminalApp::parseCommandLine(int argc, wchar_t* argv[])
{
	for(int i=1; i<argc; i++)
	{
		if((argv[i][0]==L'/'||argv[i][0]==L'-'))
		{
			switch(argv[i][1])
			{
			case L'd'://debug level
			case L'D':
				//if(argv[i][3]=='\0')
				{
					int level = _wtoi(&argv[i][2]);
					ProcessorMsgX::debug_ = (0!=(level&0x01));
					Commands::debug_ = (0!=(level&0x02));
					networkio::Receiver::debug_ = (0!=(level&0x04));
					networkio::Sender::debug_ = (0!=(level&0x08));
				}
				break;

			case L'c'://application settings xml file
			case L'C':
				if(i+1<argc && argv[i+1][0]!=L'/' && argv[i+1][0]!=L'-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszConfXmlFile_ = argv[i];
				}
				break;

			case L'i'://initialization script file file
			case L'I':
				if(i+1<argc && argv[i+1][0]!=L'/' && argv[i+1][0]!=L'-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszInitFile_ = argv[i];
				}
				break;

			case L'u'://user settings xml file
			case L'U':
				if(i+1<argc && argv[i+1][0]!=L'/' && argv[i+1][0]!=L'-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszConfUserXmlFile_ = argv[i];
				}
				break;

			case L'v'://version
			case L'V':
				if(argv[i][2]==L'\0')
				{
					std::wcout<<wszVersionInfo;
					return false;
				}
				break;

			case L'?'://help
				if(argv[i][2]==L'\0')
				{
					std::wcout<<wszUsageText;
					return false;
				}
				break;

			case L'h'://help
			case L'H':
				if(0 ==_wcsicmp(argv[i]+1, L"help"))
				{
					std::wcout<<wszUsageText;
					return false;
				}
			default:
				std::wcout<<wszUnknownParam<<argv[i]<<std::endl;
				std::wcout<<wszUsageText;
				return false;
			}
		}
		else
		{
			std::wcout<<wszUnknownParam<<argv[i]<<std::endl;
			std::wcout<<wszUsageText;
			return false;
		}
	}

	return true;
}
#else
bool ChatTerminalApp::testFileExistence(const char* path, bool fMsg)
{
	int ares = access( path, R_OK);//read only
	if(0!=ares)
	{
		if(!fMsg) return false;

		switch(errno)
		{
		case EACCES:
			std::cout<<path;
			std::wcout<<L" - "<<wszAccessDenied<<std::endl;
			break;
		case ENOENT:
			std::cout<<path;
			std::wcout<<L" - "<<wszFileNotFound<<std::endl;
			break;
		case EINVAL:
		default:
			std::cout<<path;
			std::wcout<<L" - "<<wszFileInvalidParam<<std::endl;
		}
		return false;
	}

	return true;
}

bool ChatTerminalApp::testFileExistence(const wchar_t* path, bool fMsg)
{
	char* szPath = 0;
	NixHlpr.assignCharSz(&szPath, path);
	bool result = testFileExistence(szPath, fMsg);
	delete[] szPath;
	return result;
}

bool ChatTerminalApp::parseCommandLine(int argc, char* argv[])
{
	for(int i=1; i<argc; i++)
	{
		if(argv[i][0]==L'-')
		{
			switch(argv[i][1])
			{
			case 'd'://debug level
			case 'D':
				//if(argv[i][3]=='\0')
				{
					int level = atoi(&argv[i][2]);
					ProcessorMsgX::debug_ = (0!=(level&0x01));
					Commands::debug_ = (0!=(level&0x02));
					networkio::Receiver::debug_ = (0!=(level&0x04));
					networkio::Sender::debug_ = (0!=(level&0x08));
				}
				break;

			case 'c'://application settings xml file
			case 'C':
				if(i+1<argc && argv[i+1][0]!='-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszConfXmlFile_ = argv[i];
				}
				break;

			case 'i'://initialization script file file
			case 'I':
				if(i+1<argc && argv[i+1][0]!='-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszInitFile_ = argv[i];
				}
				break;

			case 'u'://user settings xml file
			case 'U':
				if(i+1<argc && argv[i+1][0]!=L'-')
				{
					++i;
					if(!testFileExistence(argv[i], true)) return false;
					wszConfUserXmlFile_ = argv[i];
				}
				break;

			case '-':
			{
				switch(argv[i][2])
				{
				case 'v'://version
					if(0 ==strcmp(argv[i]+3, "ersion"))
					{
						std::wcout<<wszVersionInfo<<std::endl;
						return false;
					}
					break;

				case 'h'://help
					if(0 ==strcmp(argv[i]+3, "elp"))
					{
						std::wcout<<wszUsageText<<std::endl;
						return false;
					}
					break;
				}
			}
			default:
				std::wcout<<wszUnknownParam<<argv[i]<<std::endl;
				std::wcout<<wszUsageText<<std::endl;
				return false;
			}
		}
		else
		{
			std::wcout<<wszUnknownParam<<argv[i]<<std::endl;
			std::wcout<<wszUsageText<<std::endl;
			return false;
		}
	}

	return true;
}
#endif // CHATTERM_OS_WINDOWS

int ChatTerminalApp::getPublicKey(unsigned char*& pPubBuffer, unsigned short& cPubLen)
{
	pPubBuffer = 0;
	cPubLen = 0;

#ifdef CHATTERM_OS_WINDOWS
	const DWORD dwKeySpec = AT_KEYEXCHANGE;//AT_SIGNATURE

	HCRYPTKEY hKey = 0;

	if(!CryptGetUserKey(theApp.hCryptProv_, dwKeySpec, &hKey))
	{
		if(!CryptGenKey(theApp.hCryptProv_, dwKeySpec, RSA1024BIT_KEY|CRYPT_EXPORTABLE, &hKey))
		{
			CryptReleaseContext(theApp.hCryptProv_, 0);
			theApp.hCryptProv_ = 0;
			return -1;
		}
	}

	DWORD dwBlobLen = 0;
	LPBYTE pbKeyBlob = 0;

	if(CryptExportKey(hKey, NULL, PUBLICKEYBLOB, 0, NULL, &dwBlobLen))
	{
		pbKeyBlob = new BYTE[dwBlobLen];

		if(!CryptExportKey(hKey, NULL, PUBLICKEYBLOB, 0, pbKeyBlob, &dwBlobLen))
		{
			delete[] pbKeyBlob;
			dwBlobLen = 0;
		}
	}

	pPubBuffer = pbKeyBlob;
	cPubLen = (unsigned short)dwBlobLen;

	CryptDestroyKey(hKey);
	return 0;
#else
	int result = -1;
	BIGNUM* e = BN_new();
	if(BN_set_word(e, 65537))
	{
		char rnd_seed[128];
	
		sprintf(rnd_seed, "ChatTerminal %p seed random numbers generator, time %u %p %p.", this, static_cast<unsigned int>(time(NULL)), e, &rnd_seed);
	
		RAND_seed(rnd_seed, sizeof rnd_seed); /* or OAEP may fail */
	
		//result = RSA_generate_key_ex(pRSA_, 1024, e, NULL);
                pRSA_ =	RSA_generate_key(1024, 65537, NULL, NULL);
		if(pRSA_) result =1;
	}

	BN_free(e);

	if(1 != result) return -1;

	cPubLen = sizeof(PUBKEYDATA);
	pPubBuffer = new unsigned char[cPubLen];

	PUBKEYDATA* pbk_data = reinterpret_cast<PUBKEYDATA*>(pPubBuffer);
	pbk_data->bType = 0x06;
	pbk_data->bVersion = 2;
	const unsigned char reserved[sizeof(pbk_data->reserved)] = {0x00,0x00};
	const unsigned char aiKeyAlg[sizeof(pbk_data->aiKeyAlg)] = {0x00, 0x24, 0x00, 0x00};//CALG_RSA_SIGN
	const unsigned char magic[sizeof(pbk_data->magic)] = {0x52, 0x53, 0x41, 0x31};
	const unsigned char bitlen[sizeof(pbk_data->bitlen)] = {0x00, 0x04, 0x00, 0x00};//1024
	const unsigned char pubexp[sizeof(pbk_data->pubexp)] = {0x01, 0x00, 0x01, 0x00};//65537
	memcpy(pbk_data->reserved, reserved, sizeof(reserved));
	memcpy(pbk_data->aiKeyAlg, aiKeyAlg, sizeof(aiKeyAlg));
	memcpy(pbk_data->magic, magic, sizeof(magic));
	memcpy(pbk_data->bitlen, bitlen, sizeof(bitlen));
	memcpy(pbk_data->pubexp, pubexp, sizeof(pubexp));

	//int len_e = BN_num_bits(pRSA_->e);
	//int len_n = BN_num_bits(pRSA_->n);

	int len_n = BN_bn2bin(pRSA_->n, pbk_data->modulus);
	//reverse bytes for Windows
	for(int i=0; i<len_n/2; ++i)
	{
		unsigned char b = pbk_data->modulus[i];
		pbk_data->modulus[i] = pbk_data->modulus[len_n-i-1];
		pbk_data->modulus[len_n-i-1] = b;
	}
	
	return result==1 ? 0 : -1;
#endif // CHATTERM_OS_WINDOWS
}

void ChatTerminalApp::initMe()
{
	theApp.MyPersonalInfo_.initialize();

	//std::unique_ptr<USER_INFO> me = std::make_unique<USER_INFO>();

	if(0 != getPublicKey(Me_.pub_key, Me_.pub_key_size))
	{
		consoleio::print_line(wszErrKeyPair);
	}

	//loading personal settings from a configuration xml file
	if(0 == wszConfUserXmlFile_)
	{
#ifdef CHATTERM_OS_WINDOWS
		const size_t buflen = MAX_PATH+_ARRAYSIZE(wszDefConfUserXmlFile_);
		wchar_t wszUserDataPath[buflen] = {0};
		HRESULT hr = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, wszUserDataPath);
		if(S_OK == hr)
		{
			wcscat_s(wszUserDataPath, buflen, wszDefConfUserXmlFile_);

			if(testFileExistence(wszUserDataPath, false))
				Me_.loadFromXml(wszUserDataPath);
		}
		else
#else
		char* home_folder = getenv("HOME");
		if(home_folder)
		{
			wchar_t* pwszHomeFolder = 0;
			size_t home_len = NixHlpr.assignWcharSz(&pwszHomeFolder, home_folder);

			size_t buflen = home_len+_ARRAYSIZE(wszDefConfUserXmlFile_);
			wchar_t* pszUserDataPath = new wchar_t[buflen];
			wcscpy(pszUserDataPath, pwszHomeFolder);
			wcscpy(pszUserDataPath+home_len-1,  wszDefConfUserXmlFile_);

			if(testFileExistence(pszUserDataPath, false))
				me->loadFromXml(pszUserDataPath);
			delete[] pszUserDataPath;
			delete[] pwszHomeFolder;
		}
		else
#endif // CHATTERM_OS_WINDOWS
		{
			if(testFileExistence(wszDefConfUserXmlFile_+1, false))
				Me_.loadFromXml(wszDefConfUserXmlFile_+1);//to skip first slash
		}
	}
	else
	{
#ifdef CHATTERM_OS_WINDOWS
		const size_t buflen = MAX_PATH;
		wchar_t wszPathBuf[buflen] = {0};
		//DWORD WINAPI ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize);
		//BOOL PathUnExpandEnvStrings(LPCTSTR pszPath, LPTSTR pszBuf, UINT cchBuf);
		if(ExpandEnvironmentStrings(wszConfUserXmlFile_, wszPathBuf, buflen))
			Me_.loadFromXml(wszPathBuf);
		else
			Me_.loadFromXml(wszConfUserXmlFile_);
#else
		wchar_t* pwszConfUserXmlFile = 0;
		NixHlpr.assignWcharSz(&pwszConfUserXmlFile, wszConfUserXmlFile_);
		theApp.Me_.loadFromXml(pwszConfUserXmlFile);
		delete[] pwszConfUserXmlFile;
#endif // CHATTERM_OS_WINDOWS
	}

#ifdef CHATTERM_OS_WINDOWS
	RPC_STATUS status = RPC_S_OK;
	if(UuidIsNil(&Me_.uuid, &status) || ( RPC_S_OK != status))
		UuidCreate(&Me_.uuid);
#else
	if(uuid_is_null(me->uuid))
		uuid_generate(me->uuid);
#endif // CHATTERM_OS_WINDOWS
	if(USER_INFO::NullNick_ == Me_.getNick())
	{
		if(theApp.MyPersonalInfo_.user_name.length()>0)
			Me_.setNick(theApp.MyPersonalInfo_.user_name.c_str(), theApp.MyPersonalInfo_.user_name.length());
		else
			Me_.setNick(wszDefaultNick, _ARRAYSIZE(wszDefaultNick)-1);
	}

	//std::pair<USER_INFO::ConstIteratorOfUsers, bool> res = USER_INFO::SetOfUsers_.insert(me);

	//theApp.Me_ = **res.first;
}

int ChatTerminalApp::sendChatLine(const wchar_t* line, size_t line_len)
{
	if(0 == line) return -1;

	if(fEcho_)
		consoleio::print_line(L"%ls", line);

	if(line_len < 1) return 0;

	if(*line != L'/')
	{
		ContainersMonitor CONTAINERS_MONITOR;

		int result = 0;
		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();
		if(pacchinfo)
		{
			if(pacchinfo->secured)
				result = Commands_.SecureChannelMsgQ01(pacchinfo->name.c_str(), pacchinfo, line, false);
			else
				result = Commands_.ChannelMsg2A(pacchinfo->name.c_str(), line, false);
		}
		else
			consoleio::print_line(wszYouNotJoinedToChannels);

		return result;
	}

	++line;
	--line_len;

	//remove trailing spaces
	while(iswspace(*(line+line_len-1)) && (line_len > 0)) --line_len;

	if(line_len<1) return 0;

	struct COMMAND_INFO
	{
		const wchar_t* name;
		COMMAND_ID id;
		unsigned int params_count;
	};

	//sorted!!! list of commands
	COMMAND_INFO commands[] = {{L"add", TOPIC_ADD, 1}
								,{L"allchannels", ALL_CHANNELS, 0}
								,{L"allchs", ALL_CHANNELS, 0}
								,{L"allusers", ALL_USERS, 0}
								,{L"beep", BEEP, 1}
								,{L"cc", CHANGE_CHANNEL, 1}
								,{L"channels", CHANNELS, 1}
								,{L"cls", CLS, 0}
								,{L"color", COLOR, 1}
								,{L"echo", ECHO, 1}
								,{L"exit", EXIT, 0}
								,{L"female", FEMALE, 0}
								,{L"flood", FLOOD, 2}
								,{L"help", HELP, 0}
								,{L"here", HERE, 1}
								,{L"info", INFO, 1}
								,{L"join", JOIN, 1}
								,{L"leave", LEAVE, 1}
								,{L"list", LIST, 0}
								,{L"male", MALE, 0}
								,{L"mass", MASS, 1}
								,{L"me", ME, 1}
								,{L"msg", MASS_TO, 2}
								,{L"my", MY_CHANNELS, 0}
								,{L"nick", NICK_NEW, 1}
								,{L"ping", PING, 1}
								,{L"quit", QUIT, 0}
								,{L"sjoin", SJOIN, 2}
								,{L"topic", TOPIC_NEW, 1}
								,{L"wait", WAIT, 1}
								,{L"whoim", WHOIM, 0}
								,{L"users", CHANNEL_USERS, 0}};

	COMMAND_ID cmdId = NULL_COMMAND;
	const wchar_t* wszParams = 0;
	size_t nParamsLen = 0;

	wchar_t* pParamsBuf = 0;
	size_t pos = 0;

	const wchar_t*& line_seek = line;
	size_t& available_len = line_len;

	for(unsigned int i=0; (i<_ARRAYSIZE(commands)) && (NULL_COMMAND == cmdId) ; ++i)
	{
		//compare strings
		while(*(commands[i].name+pos) && available_len && (*(commands[i].name+pos) == *line_seek))
		{
			++pos;
			++line_seek;
			--available_len;
		}

		if(*(commands[i].name+pos))
		{
			//strings are different

			if(pos>0 && i+1<_ARRAYSIZE(commands))
			{
				//because of the array is sorted we can continue search from the current position
				//only if previously compared characters are the same;
				//there is no such string in the array otherwise
				if(0!=wcsncmp(commands[i+1].name, commands[i].name, pos))
					break;
			}

			continue;
		}

		//It doesn't matter if parameters are required
		if(0 == available_len)
		{
			_ASSERTE((0 == *line_seek) || (iswspace(*line_seek)));

			cmdId = commands[i].id;
			break;
		}

		if(!iswspace(*line_seek))
		{
			//wrong command, there are no commands where first characters are the same
			break;//continue;
		}

		cmdId = commands[i].id;

		if(commands[i].params_count > 0)
		{
			while(available_len && iswspace(*line_seek))
			{
				++line_seek;
				--available_len;
			}

			if(available_len>0)
			{
				if(*(line_seek+available_len-1))
				{
					//wszParams must be a null terminated string (without trailing white spaces)
					pParamsBuf = new wchar_t[available_len+1];
					memcpy(pParamsBuf, line_seek, sizeof(wchar_t)*available_len);
					pParamsBuf[available_len] = 0;

					wszParams = pParamsBuf;
				}
				else
					wszParams = line_seek;

				nParamsLen = available_len;
			}
		}
	}

	int result = 0;

	switch(cmdId)
	{
	case NULL_COMMAND:
		consoleio::print_line(wszUnknownCmd);
		break;

	default:
		result = processCommandId(cmdId, wszParams, nParamsLen);
	}

	delete[] pParamsBuf;

	return result;
}

size_t ChatTerminalApp::getSecondParam(const wchar_t* p, const wchar_t*& second, wchar_t*& first_buf)
{
	//first parameter must be enclosed by any characters, for example: |pass"word's|, or "passwd", or '"crazy" user', or "user who was born in 1990"
	wchar_t sep = *p;

	if(0 == *p || (sep==*(p+1)))
	{
		return 0;
	}

	size_t first_len = 0;

	const wchar_t* sep2 = wcschr(++p, sep);
	if(sep2 && *sep2)
	{
		first_len = sep2-p;
		while(iswspace(*++sep2));

		second = sep2;
	}
	else
	{
		return 0;
	}

	first_buf = new wchar_t[first_len+1];
	wcsncpy_s(first_buf, first_len+1, p, first_len);
	first_buf[first_len] = 0;

	return first_len;
}

int ChatTerminalApp::processCommandId(COMMAND_ID id, const wchar_t* params, size_t params_len)
{
	//These commands don't need access to CHANNEL_INFO::SetOfChannels_ or/and SetOfUsers containers
	switch(id)
	{
	case CLS:
		consoleio::console_clear_screen();
		return 0;

	case COLOR:
		if(params_len && params && *params)
		{
			long num_color = _wtoi(params);
			if (num_color == 0)//not numeric string
			{
				unsigned char i = 0;
				for( ; i<_ARRAYSIZE(USER_INFO::colors_); i++)
				{
					if(0 == _wcsicmp(USER_INFO::colors_[i], params))
					{
						Me_.color = i;
						break;
					}
				}

				if(i >= _ARRAYSIZE(USER_INFO::colors_))
				{
					if(*params == L'0')
						Me_.color = 0;
					else
						consoleio::print_line(wszIncorrectColor);
				}
			}
			else
			{
				if(num_color<long(_ARRAYSIZE(USER_INFO::colors_)))
					Me_.color = static_cast<unsigned char>(num_color);
				else
					consoleio::print_line(wszIncorrectColor);
			}
		}
		else
			consoleio::print_line(wszNoColor);

		return 0;

	case ECHO:
		if(params_len && params && *params)
		{
			if(0 == _wcsicmp(params,wszOn))
				fEcho_ = true;
			else
				if(0 == _wcsicmp(params,wszOff))
					fEcho_ = false;
		}

		if(fEcho_)
			consoleio::print_line(wszEchoOn);
		else
			consoleio::print_line(wszEchoOff);

		return 0;

	case EXIT:
		return -2;

	case FEMALE:
		theApp.Me_.gender = '1';
		return 0;

	case HELP:
		consoleio::print_line(wszHelp);
		return 0;

	case MALE:
		theApp.Me_.gender = '0';
		return 0;

	case NICK_NEW:
		return Commands_.NickName3(params);

	case QUIT:
		return -2;

	case WAIT:
		{
			int seconds = 0;
			if(params_len && params && *params)
				seconds = _wtoi(params);

			consoleio::console_skip_input(seconds);
		}
		return 0;

		case WHOIM:
		{
			consoleio::print_line(theApp.Me_.color, false,  theApp.Me_.getNick());

			const wchar_t* gender = (theApp.Me_.gender=='1') ? wszWoman : wszMan;
			consoleio::print_line(theApp.Me_.color, false,  wszGender, gender);

#ifdef CHATTERM_OS_WINDOWS
			RPC_WSTR wszMyUuid = 0;
			UuidToString(&theApp.Me_.uuid, &wszMyUuid);
			consoleio::print_line(theApp.Me_.color, false,  wszUuid, wszMyUuid);
			RpcStringFree(&wszMyUuid);
#else
			char szUuid[40]={0};
			uuid_unparse(theApp.Me_.uuid, szUuid);
			wchar_t* wszMyUuid = 0;
			NixHlpr.assignWcharSz(&wszMyUuid, szUuid);
			consoleio::print_line(theApp.Me_.color, false,  wszUuid, wszMyUuid);
			delete[] wszMyUuid;
#endif // CHATTERM_OS_WINDOWS

			consoleio::print_line(theApp.Me_.color, false, wszFullName, theApp.MyPersonalInfo_.full_name.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszUserName, theApp.MyPersonalInfo_.user_name.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszComputerName, theApp.MyPersonalInfo_.computer_name.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszDomainName, theApp.MyPersonalInfo_.domain_name.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszOS, theApp.MyPersonalInfo_.os.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszChatSoftware, theApp.MyPersonalInfo_.chat_software.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszJob, theApp.MyPersonalInfo_.job.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszDepartment, theApp.MyPersonalInfo_.department.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszWorkPhone, theApp.MyPersonalInfo_.phone_work.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszMobilePhone, theApp.MyPersonalInfo_.phone_mob.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszWebAddr, theApp.MyPersonalInfo_.www.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszEmailAddr, theApp.MyPersonalInfo_.email.c_str());
			consoleio::print_line(theApp.Me_.color, false, wszPostAddr, theApp.MyPersonalInfo_.address.c_str());
		}
		return 0;

		default:
			break;
	}

	ContainersMonitor CONTAINERS_MONITOR;

	int result = 0;

	switch(id)
	{
	case ALL_CHANNELS:
		{
			if(CHANNEL_INFO::SetOfChannels_.size()<1)
			{
				consoleio::print_line(wszNoChannels);
				break;
			}

			CHANNEL_INFO::ConstIteratorOfChannels it_ch = CHANNEL_INFO::SetOfChannels_.begin();
			while(it_ch != CHANNEL_INFO::SetOfChannels_.end())
			{
				if(*it_ch)
					consoleio::print_line((*it_ch)->name.c_str());

				++it_ch;
			}
		}
		break;

	case ALL_USERS:
		{
			if(USER_INFO::SetOfUsers_.size()<1)
			{
				consoleio::print_line(wszNoUsersInList);
				break;
			}

			USER_INFO::ConstIteratorOfUsers it = USER_INFO::SetOfUsers_.begin();
			while(it != USER_INFO::SetOfUsers_.end())
			{
				if((*it))
					consoleio::print_line((*it)->getNick());

				++it;
			}
		}
		break;

	case BEEP:
		result = Commands_.BeepH(params);
		break;

	case CHANGE_CHANNEL:
		{
			if(!params_len || !params || !*params)
			{
				consoleio::print_line(wszNoChannel);
				break;
			}

			const CHANNEL_INFO* pcchinfo = CHANNEL_INFO::setActiveChannel(params);

			if(0==pcchinfo)
			{
				consoleio::print_line(wszYouNotJoinedToChannel, params);
				break;
			}

			consoleio::print_line(NULL);//print new channel invitation
		}
		break;

	case CHANNELS:
		result = Commands_.ChannelsN(params);
		break;

	case CHANNEL_USERS:
		{
			const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

			if(!pacchinfo)
			{
				consoleio::print_line(wszYouNotJoinedToChannels);
				break;
			}

			if(pacchinfo->users.size()<1)
			{
				consoleio::print_line(wszNoChannelUsers);
				break;
			}

			CHANNEL_INFO::ConstIteratorOfChannelUsers it = pacchinfo->users.begin();
			while(it != pacchinfo->users.end())
			{
				if(*it)
					consoleio::print_line((*it)->getNick());

				++it;
			}
		}
		break;

	case FLOOD:
		{
			const wchar_t* seconds = 0;
			wchar_t* to = 0;

			if(params_len && params && *params)
				getSecondParam(params, seconds, to);

			int nsecs = 0;
			if(seconds) nsecs = _wtoi(seconds);

			result = Commands_.FloodZ(to, nsecs);

			delete[] to;
		}
		break;

	case HERE:
		{
			bool fSecured = false;
			const wchar_t* channel = params;
			if(!CHANNEL_INFO::getNameWithPrefix(channel, fSecured))
			{
				if(channel)
					consoleio::print_line(wszYouNotJoinedToChannel, channel);
				else
					consoleio::print_line(wszYouNotJoinedToChannels);

				return 0;
			}

			if(fSecured)
				result = Commands_.SecureHereQ8(channel);
			else
				result = Commands_.HereL(channel);
		}
		break;

	case INFO:
		result = Commands_.InfoF(params);
		break;

	case JOIN:
		{
			std::wstring&& channel=std::wstring();

			if(params_len && params && *params)
			{
				std::wstring wstrbuf(CHANNEL_INFO::createNameWithPrefix(params, CHANNEL_INFO::NOT_SECURED));

				channel = wstrbuf.length()>0 ? std::move(wstrbuf) : params;
			}
			else channel = CHANNEL_INFO::wszMainChannel;

			result = Commands_.Join4(channel);
		}
		break;

	case LEAVE:
		{
			bool fSecured = false;
			const wchar_t* channel = params;
			if(!CHANNEL_INFO::getNameWithPrefix(channel, fSecured))
			{
				if(channel)
					consoleio::print_line(wszYouNotJoinedToChannel, channel);
				else
					consoleio::print_line(wszYouNotJoinedToChannels);

				return 0;
			}

			if(fSecured)
				result = Commands_.SecureLeaveQ7(channel);
			else
				result = Commands_.Leave5(channel);
		}
		break;

	case LIST:
		result = Commands_.List0();
		break;

	case MASS:
		result = Commands_.MassTextMsgE(params);
		break;

	case MASS_TO:
		{
			const wchar_t* text = 0;
			wchar_t* to = 0;

			if(params_len && params && *params)
				getSecondParam(params, text, to);

			result = Commands_.MassTextMsgToE(to, text);

			delete[] to;
		}
		break;

	case ME:
		{
			const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

			if(!pacchinfo)
			{
				consoleio::print_line(wszYouNotJoinedToChannels);
				break;
			}

			if(pacchinfo->secured)
				result = Commands_.SecureChannelMsgQ01(pacchinfo->name, pacchinfo, params, true);
			else
				result = Commands_.ChannelMsg2A(pacchinfo->name, params, true);
		}
		break;

	case MY_CHANNELS:
		{
			CHANNEL_INFO::ConstIteratorOfChannels it_ch = CHANNEL_INFO::SetOfChannels_.begin();
			while(it_ch != CHANNEL_INFO::SetOfChannels_.end())
			{
				if(*it_ch && (*it_ch)->joined)
					consoleio::print_line((*it_ch)->name.c_str());

				++it_ch;
			}
		}
		break;

	case PING:
		result = Commands_.PingPongP(params, false);
		break;

	case SJOIN:
		{
			std::wstring&& channel = std::wstring();
			wchar_t* passwd = 0;
			std::unique_ptr<wchar_t[]> buf(nullptr);

			if(params_len && params && *params)
			{
				const wchar_t* channel_param = nullptr;
				size_t passwd_len = getSecondParam(params, channel_param, passwd);
				if(passwd_len<1)
				{
					consoleio::print_line(wszNoPassword);
					break;
				}

				std::wstring wstrbuf(CHANNEL_INFO::createNameWithPrefix(channel, CHANNEL_INFO::SECURED));

				channel = wstrbuf.length() > 0 ? std::move(wstrbuf) : channel_param;
			}

			result = Commands_.SecureJoinQ5(channel, passwd);

			delete[] passwd;
		}
		break;

	case TOPIC_ADD:
		{
			const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

			if(pacchinfo==0 ||pacchinfo->name.length()<1)
			{
				consoleio::print_line(wszYouNotJoinedToChannels);
				break;
			}

			if(!params_len || !params || !*params)
			{
				consoleio::print_line(wszNoAddTopic);
				break;
			}

			size_t old_len = pacchinfo->topic.length();

			size_t buflen = old_len + params_len + wcslen(theApp.Me_.getNick()) + 5;

			wchar_t* pwszTopic = new wchar_t[buflen];

			if( pacchinfo->topic.length())
				swprintf_s(pwszTopic, buflen, L"%ls %ls (%ls)", pacchinfo->topic.c_str(), params, theApp.Me_.getNick());
			else
				swprintf_s(pwszTopic, buflen, L"%ls (%ls)", params, theApp.Me_.getNick());

			if(pacchinfo->secured)
				result = Commands_.SecureNewTopicQ3(pacchinfo->name, pacchinfo, pwszTopic);
			else
				result = Commands_.NewTopicB( pacchinfo->name, pwszTopic);

			delete[] pwszTopic;
		}
		break;

	case TOPIC_NEW:
		{
			const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

			if(pacchinfo==0 || pacchinfo->name.length()<1)
			{
				consoleio::print_line(wszYouNotJoinedToChannels);
				break;
			}

			if(!params_len || !params || !*params)
			{
				if( !pacchinfo->topic.empty())
					consoleio::print_line(pacchinfo->topic.c_str());
				else
					consoleio::print_line(wszNoTopic);
				break;
			}

			size_t buflen = params_len + wcslen(theApp.Me_.getNick())+4;
			wchar_t* pwszTopic = new wchar_t[buflen];
			int topic_len = swprintf_s(pwszTopic, buflen, L"%ls (%ls)", params, theApp.Me_.getNick());

			DBG_UNREFERENCED_LOCAL_VARIABLE(topic_len);
			_ASSERTE(topic_len);

			if(pacchinfo && pacchinfo->secured)
				result = Commands_.SecureNewTopicQ3(pacchinfo->name, pacchinfo, pwszTopic);

			else
				result = Commands_.NewTopicB( pacchinfo->name, pwszTopic);

			delete[] pwszTopic;
		}
		break;

	default:
		consoleio::print_line(wszUnknownCmd);
                break;
	}

	return result;
}

int ChatTerminalApp::processStreamInput(std::wistream& ins)
{
	std::wstring strLine;
	while(std::getline(ins,strLine))
	{
		const wchar_t* line = strLine.c_str();
		size_t len = strLine.length();

		while(iswspace(*line))
		{
			++line;
			--len;
		}

		if(len<1) continue;//empty line
		if(*line == L'#') continue;//comment

		int result = sendChatLine(line, len);

		if(0 != result)
			return result;
	}

	return 0;
}

void ChatTerminalApp::leaveNetwork(void)
{
	ContainersMonitor CONTAINERS_MONITOR;

	CHANNEL_INFO::NameComparator comparator(CHANNEL_INFO::wszMainChannel);

	//send Leave to all joined channels
	CHANNEL_INFO::ConstIteratorOfChannels it_ch = CHANNEL_INFO::SetOfChannels_.begin();
	CHANNEL_INFO::ConstIteratorOfChannels it_ch_end = CHANNEL_INFO::SetOfChannels_.end();
	CHANNEL_INFO::ConstIteratorOfChannels it_ch_main = it_ch_end;
	while(it_ch != it_ch_end)
	{
		if(*it_ch && (*it_ch)->joined && (*it_ch)->name.length()>0)
		{
			if((it_ch_main == it_ch_end) && comparator(*it_ch))
			{
				it_ch_main = it_ch;
			}
			else
				if(it_ch_main != it_ch)//#Main channel should be the latest
				{
					if((*it_ch)->secured)
						Commands_.SecureLeaveQ7(*it_ch);
					else
						Commands_.Leave5(*it_ch);
				}
		}

		++it_ch;
	}

	//#Main channel should be the latest
	if((it_ch_main != it_ch_end) && (*it_ch_main)->joined)
	{
		Commands_.Leave5(*it_ch_main);
	}

	//free channels objects
	std::for_each(CHANNEL_INFO::SetOfChannels_.begin(), CHANNEL_INFO::SetOfChannels_.end(), delete_class_ptr<CHANNEL_INFO>());

	CHANNEL_INFO::setActiveChannel((const wchar_t*)NULL);
	CHANNEL_INFO::SetOfChannels_.clear();
	USER_INFO::SetOfUsers_.clear();
}

bool ChatTerminalApp::initConfig()
{
	//loading network settings from a configuration xml file
	if(0 == wszConfXmlFile_)
	{
		if(testFileExistence(wszDefConfXmlFile_, false))
		{
			if(initConfigFromXml(wszDefConfXmlFile_))
				return true;
		}
	}
	else
	{
#ifdef CHATTERM_OS_WINDOWS
		const size_t buflen = MAX_PATH;
		wchar_t wszPathBuf[buflen] = {0};
		//DWORD WINAPI ExpandEnvironmentStrings(LPCTSTR lpSrc, LPTSTR lpDst, DWORD nSize);
		//BOOL PathUnExpandEnvStrings(LPCTSTR pszPath, LPTSTR pszBuf, UINT cchBuf);
		if(ExpandEnvironmentStrings(wszConfXmlFile_, wszPathBuf, buflen))
			if(initConfigFromXml(wszPathBuf))
				return true;
		else
			if(initConfigFromXml(wszConfXmlFile_))
				return true;
#else
		wchar_t* pwszConfXmlFile = 0;
		NixHlpr.assignWcharSz(&pwszConfXmlFile, wszConfXmlFile_);
		bool result = initConfigFromXml(pwszConfXmlFile);
		delete[] pwszConfXmlFile;

		if(result)
			return true;
#endif // CHATTERM_OS_WINDOWS
	}

	//initialize default network configuration
	if(!initDefaultNetConfig())
	{
		consoleio::print_line(wszErrNoNetIfs);
		return false;
	}

	return true;
}

bool ChatTerminalApp::initDefaultNetConfig()
{
	networkio::Interface* pi = 0;
	networkio::Sender* ps = 0;
	networkio::Receiver* pr = 0;
	networkio::DESTADDR_INFO* pd = 0;

	if(!networkio::get_default_netconfig(pi, ps, pr, pd))
		return false;

	Interfaces_.push_back(pi);
	Senders_.push_back(ps);
	Receivers_.push_back(pr);
	Commands_.Destinations_.push_back(pd);

	return true;
}

#ifdef CHATTERM_OS_WINDOWS
bool ChatTerminalApp::initConfigFromXml(const wchar_t* file_path)
{
	IXMLDOMDocument* pXMLDoc = 0;
	HRESULT hr = xmlhelper::create_DOM_instance(&pXMLDoc);
	if(S_OK != hr)
	{
		if(pXMLDoc) pXMLDoc->Release();
		return false;
	}

	VARIANT xmlSource ={0};
	VariantInit(&xmlSource);
	xmlSource.vt = VT_BSTR;
	xmlSource.bstrVal = SysAllocString(file_path);

	VARIANT_BOOL bIssuccessful = {0};
	hr = pXMLDoc->load(xmlSource, &bIssuccessful);
	VariantClear(&xmlSource);

	if(S_OK!=hr || bIssuccessful!=VARIANT_TRUE)
	{
		consoleio::print_line(wszErrLoadXmlFile, file_path);

		IXMLDOMParseError *pIParseError = NULL;
		hr = pXMLDoc->get_parseError(&pIParseError);

		if(pIParseError)
		{
			BSTR reasonString = 0;
			hr = pIParseError->get_reason(&reasonString);
			if(SUCCEEDED(hr))
			{
				consoleio::print_line(reasonString);
				SysFreeString(reasonString);
			}

			pIParseError->Release();
		}

		if(pXMLDoc) pXMLDoc->Release();
		return false;
	}

	IXMLDOMDocument2* pXMLDoc2 = 0;
	if(S_OK == pXMLDoc->QueryInterface(IID_IXMLDOMDocument2, (void**)&pXMLDoc2))
	{
		VARIANT value = {0};
		VariantInit(&value);
		value.vt = VT_BSTR;
		value.bstrVal = SysAllocString(L"XPath");
		BSTR bstrProp = SysAllocString(L"SelectionLanguage");
		hr = pXMLDoc2->setProperty(bstrProp, value);

		SysReAllocString(&bstrProp, L"SelectionNamespaces");
		SysReAllocString(&value.bstrVal, L"xmlns:n='http://www.vypress.com/chatterm10'");

		hr = pXMLDoc2->setProperty (bstrProp, value);

		SysFreeString(bstrProp);
		VariantClear(&value);

		pXMLDoc2->Release();
	}

	bool result = initNetConfigFromXml(pXMLDoc);

	if(pXMLDoc)
	{
		const wchar_t wszTagUsersList[] = L"/n:chatterminal/n:users_list";
		const wchar_t wszTagCmdLine[] = L"/n:chatterminal/n:command_line";
		/*
		<users_list drop_after="3"></users_list>
		<command_line remember="100"></command_line>
		*/

		IXMLDOMNode* pNode = 0;
		BSTR bstrNodeName = SysAllocString(wszTagUsersList);// /chatterminal/users_list
		hr = pXMLDoc->selectSingleNode(bstrNodeName, &pNode);
		if(S_OK == hr)
		{
			IXMLDOMNamedNodeMap* attributeMap = 0;
			hr = pNode->get_attributes(&attributeMap);
			if(S_OK == hr)
			{
				VARIANT varAttrValue = {0};

				hr = xmlhelper::get_xml_attribute(L"drop_after", attributeMap, &varAttrValue);
				if(S_OK == hr)
				{
					nDropUsersAfterInterval_ = static_cast<unsigned int>(_wtoi(varAttrValue.bstrVal));

					VariantClear(&varAttrValue);
				}
			}
			pNode->Release();
		}

		if(SysReAllocString(&bstrNodeName, wszTagCmdLine))// /chatterminal/command_line
		{
			hr = pXMLDoc->selectSingleNode(bstrNodeName, &pNode);
			if(S_OK == hr)
			{
				IXMLDOMNamedNodeMap* attributeMap = 0;
				hr = pNode->get_attributes(&attributeMap);
				if(S_OK == hr)
				{
					VARIANT varAttrValue = {0};

					hr = xmlhelper::get_xml_attribute(L"remember", attributeMap, &varAttrValue);
					if(S_OK == hr)
					{
						consoleio::nCommandLinesStackSize = static_cast<unsigned int>(_wtoi(varAttrValue.bstrVal));

						VariantClear(&varAttrValue);
					}

					hr = xmlhelper::get_xml_attribute(L"echo", attributeMap, &varAttrValue);
					if(S_OK == hr)
					{
						if(0 == _wcsicmp(varAttrValue.bstrVal, L"on"))
							fEcho_ = true;

						VariantClear(&varAttrValue);
					}
				}

				pNode->Release();
			}
		}

		if(bstrNodeName) SysFreeString(bstrNodeName);

		pXMLDoc->Release();
	}

	return result;
}

bool ChatTerminalApp::initNetConfigFromXml(IXMLDOMDocument* pXMLDoc)
{
	const wchar_t wszTagNetwork[] = L"/n:chatterminal/n:network";
	const wchar_t wszTagInterfaces[] = L"interfaces";
	const wchar_t wszTagInterface[] = L"interface";
	const wchar_t wszTagBinding[] = L"binding";
	const wchar_t wszTagSenders[] = L"senders";
	const wchar_t wszTagSender[] = L"sender";
	const wchar_t wszTagReceivers[] = L"receivers";
	const wchar_t wszTagReceiver[] = L"receiver";
	const wchar_t wszTagBroadcastDestinations[] = L"broadcast_destinations";
	const wchar_t wszTagDestination[] = L"destination";
	const wchar_t wszTagFloodProtection[] = L"flood_protection";
	/*
	<network>
	<interfaces>
	<interface id="i1" address="192.168.1.2" ipver="4"></interface>
	<interface id="i2" address="192.168.10.2" ipver="4"></interface>
	<interface id="i3" name="Подключение по локальной сети10" ipver="4"></interface>
	</interfaces>
	<binding>
	  <senders>
		<sender id="s1" interface="i1" bindport="0" ttl="1"></sender>
		<sender id="s2" interface="i2" bindport="0" ttl="1"></sender>
		<sender id="s3" interface="i1" bindport="8168" ttl="255"></sender>
	  </senders>
	<receivers>
	  <receiver interface="i1" bindport="8167" mcastgroups="227.0.0.1 227.0.0.2" sender="s1"></receiver>
	  <receiver interface="i2" bindport="8167" sender="s1" ></receiver>
	  <receiver interface="i1" bindport="8168" mcastgroups="227.0.0.2" sender="s2" ></receiver>
	  <receiver interface="i2" bindport="8168" mcastgroups="227.0.0.1" sender="s2" ></receiver>
	</receivers>
	<broadcast_destinations>
	  <destination address="227.0.0.2" port="8167" sender="s1" ></destination>
	  <destination address="227.0.0.2" port="8167" sender="s2" ></destination>
	  <destination address="192.168.10.255" port="8167" sender="s2"></destination>
	</broadcast_destinations>
	</binding>
	<flood_protection disabled="false" max_rate="30" ignore_time="30"></flood_protection>
	</network>
	*/

	if(!pXMLDoc) return false;

	IXMLDOMNode* pNetworkNode = 0;
	BSTR bstrNetworkInfo = SysAllocString(wszTagNetwork);// /chatterminal/network
	HRESULT hr = pXMLDoc->selectSingleNode(bstrNetworkInfo, &pNetworkNode);
	if(S_OK != hr)
	{
		SysFreeString(bstrNetworkInfo);
		return false;
	}

	IXMLDOMNodeList *childsNetworkList = 0;
	hr = pNetworkNode->get_childNodes(&childsNetworkList);

	long index = 0;

	long listLength = 0;
	if(childsNetworkList) hr = childsNetworkList->get_length(&listLength);

	if(S_OK == hr && listLength >= 3) //interfaces and binding
	{
		std::map< std::wstring, networkio::Interface*> mapIdIf;//Temporary map Interface string Id -> Interface object pointer
		std::map< std::wstring, networkio::Sender*> mapIdSender;//Temporary map Sender string Id -> Sender object pointer
		std::map< std::wstring, networkio::Sender*> mapIdIfSender;//Temporary map Interface string Id -> Sender object pointer - it uses by receivers for selecting a default sender

		IXMLDOMNode* pIfItem = 0;
		hr = xmlhelper::get_xml_item(childsNetworkList, index, wszTagInterfaces, &pIfItem);
		if(S_OK == hr)
		{
			IXMLDOMNodeList *childsIfList = 0;
			hr = pIfItem->get_childNodes(&childsIfList);
			if(S_OK == hr && childsIfList)
			{
				listLength = 0;
				hr = childsIfList->get_length(&listLength);

				long ifindex = 0;
				IXMLDOMNode* pInterfaceItem = 0;

				while(S_OK == hr && listLength>0) //interfaces
				{
					hr = xmlhelper::get_xml_item(childsIfList, ifindex, wszTagInterface, &pInterfaceItem);
					if(S_OK == hr)
					{
						IXMLDOMNamedNodeMap* attributeMap = 0;
						hr = pInterfaceItem->get_attributes(&attributeMap);
						if(S_OK == hr)
						{
							VARIANT varIdValue = {0};
							VARIANT varAddressValue = {0};
							VARIANT varNameValue = {0};

							HRESULT hr1 = xmlhelper::get_xml_attribute(L"id", attributeMap, &varIdValue);
							HRESULT hr2 = xmlhelper::get_xml_attribute(L"address", attributeMap, &varAddressValue);
							HRESULT hr3 = xmlhelper::get_xml_attribute(L"name", attributeMap, &varNameValue);

							if(S_OK==hr1 && ( S_OK==hr2 || S_OK==hr3 ))
							{
								VARIANT varIpVerValue = {0};
								int family = AF_INET;
								HRESULT hr4 = xmlhelper::get_xml_attribute(L"ipver", attributeMap, &varIpVerValue);
								if((S_OK==hr4) && (0==wcscmp(varIpVerValue.bstrVal, L"6"))) family = AF_INET6;

								networkio::Interface* pIf = 0;
								if((S_OK==hr3) && varNameValue.bstrVal && *varNameValue.bstrVal)
								{
									networkio::get_interface_by_adapter_name(varNameValue.bstrVal, varAddressValue.bstrVal, family, pIf);
								}
								else
									if((S_OK==hr2) && varAddressValue.bstrVal && *varAddressValue.bstrVal)
									{
										pIf = new networkio::Interface(varAddressValue.bstrVal, family);
									}

								if(pIf)
								{
									Interfaces_.push_back(pIf);
									mapIdIf[varIdValue.bstrVal] = pIf;
								}

								VariantClear(&varIpVerValue);
							}

							VariantClear(&varNameValue);
							VariantClear(&varAddressValue);
							VariantClear(&varIdValue);
						}
					}

					if(pInterfaceItem)
					{
						pInterfaceItem->Release();
						pInterfaceItem = 0;
					}
				}

				childsIfList->Release();
			}
			pIfItem->Release();
		}

		//////////////////////////////////////////////////////////
		IXMLDOMNode* pBindingItem = 0;
		hr = xmlhelper::get_xml_item(childsNetworkList, index, wszTagBinding, &pBindingItem);//binding

		if(S_OK == hr)
		{
			IXMLDOMNodeList *childsBindingList = 0;
			hr = pBindingItem->get_childNodes(&childsBindingList);

			if(S_OK == hr && childsBindingList)
			{
				listLength = 0;
				hr = childsBindingList->get_length(&listLength);

				long bindex = 0;
				if(S_OK == hr && listLength >= 3)
				{
					IXMLDOMNode* childsSenders = 0;//senders
					hr = xmlhelper::get_xml_item(childsBindingList, bindex, wszTagSenders, &childsSenders);//senders
					if(S_OK == hr)
					{
						IXMLDOMNodeList *childsSendersList = 0;
						hr = childsSenders->get_childNodes(&childsSendersList);

						if(S_OK == hr && childsSendersList)
						{
							listLength = 0;
							hr = childsSendersList->get_length(&listLength);

							long sindex = 0;
							IXMLDOMNode* pSenderItem = 0;

							while(S_OK == hr && listLength>0) //sender
							{
								hr = xmlhelper::get_xml_item(childsSendersList, sindex, wszTagSender, &pSenderItem);
								if(S_OK == hr)
								{
									IXMLDOMNamedNodeMap* attributeMap = 0;
									hr = pSenderItem->get_attributes(&attributeMap);
									if(S_OK == hr)
									{
										VARIANT varIdValue = {0};
										VARIANT varIfValue = {0};
										VARIANT varPortValue = {0};
										VARIANT varTtlValue = {0};

										HRESULT hr1 = xmlhelper::get_xml_attribute(L"interface", attributeMap, &varIfValue);
										HRESULT hr2 = xmlhelper::get_xml_attribute(L"id", attributeMap, &varIdValue);
										HRESULT hr3 = xmlhelper::get_xml_attribute(L"bindport", attributeMap, &varPortValue);
										HRESULT hr4 = xmlhelper::get_xml_attribute(L"ttl", attributeMap, &varTtlValue);

										if(S_OK==hr1 && S_OK==hr2)
										{
											std::map< std::wstring, networkio::Interface*>::iterator it = mapIdIf.find(varIfValue.bstrVal);
											//Interface* pif = mapIdIf[varIfValue.bstrVal];

											if(it != mapIdIf.end())
											{
												networkio::Interface* pif = it->second;

												networkio::Sender* s = new networkio::Sender();

												unsigned short port = 0;
												if(S_OK==hr3 && varPortValue.bstrVal && *varPortValue.bstrVal)
													port = (unsigned short)_wtoi(varPortValue.bstrVal);

												DWORD dwTTL = DWORD(-1);
												if(S_OK==hr4 && varTtlValue.bstrVal && *varTtlValue.bstrVal)
													dwTTL = (DWORD)_wtoi(varTtlValue.bstrVal);

												if(0!=s->bindToInterface(pif, port, dwTTL))
												{
													wchar_t* wszAddress = pif->getStringAddress();
													consoleio::print_line(wszErrNotBindSender, wszAddress);
													delete[] wszAddress;
												}

												mapIdSender[varIdValue.bstrVal] = s;
												mapIdIfSender[varIfValue.bstrVal] = s;

												Senders_.push_back(s);
											}
										}

										VariantClear(&varTtlValue);
										VariantClear(&varPortValue);
										VariantClear(&varIfValue);
										VariantClear(&varIdValue);

										if(attributeMap) attributeMap->Release();
									}
								}

								if(pSenderItem)
								{
									pSenderItem->Release();
									pSenderItem = 0;
								}
							}

							childsSendersList->Release();
						}
					}

					if(childsSenders) childsSenders->Release();

					IXMLDOMNode* childsReceivers = 0;//receivers
					hr = xmlhelper::get_xml_item(childsBindingList, bindex, wszTagReceivers, &childsReceivers);
					if(S_OK == hr)
					{
						IXMLDOMNodeList *childsReceiversList = 0;
						hr = childsReceivers->get_childNodes(&childsReceiversList);

						if(S_OK == hr && childsReceiversList)
						{
							listLength = 0;
							hr = childsReceiversList->get_length(&listLength);

							long rindex = 0;
							IXMLDOMNode* pReceiverItem = 0;

							while(S_OK == hr && listLength>0) //receiver
							{
								hr = xmlhelper::get_xml_item(childsReceiversList, rindex, wszTagReceiver, &pReceiverItem);
								if(S_OK == hr)
								{
									IXMLDOMNamedNodeMap* attributeMap = 0;
									hr = pReceiverItem->get_attributes(&attributeMap);
									if(S_OK == hr)
									{
										VARIANT varIfValue = {0};
										VARIANT varPortValue = {0};
										VARIANT varMcastValue = {0};

										HRESULT hr1 = xmlhelper::get_xml_attribute(L"interface", attributeMap, &varIfValue);
										HRESULT hr2 = xmlhelper::get_xml_attribute(L"bindport", attributeMap, &varPortValue);
										HRESULT hr3 = xmlhelper::get_xml_attribute(L"mcastgroups", attributeMap, &varMcastValue);
										DBG_UNREFERENCED_LOCAL_VARIABLE(hr3);

										if(S_OK==hr1 && S_OK==hr2) //hr3 is not necessary for broadcast
										{
											VARIANT varSenderValue = {0};
											networkio::Sender* s = 0;
											HRESULT hr4 = xmlhelper::get_xml_attribute(L"sender", attributeMap, &varSenderValue);
											if(S_OK==hr4)
											{
												std::map< std::wstring, networkio::Sender*>::iterator it = mapIdSender.find(varSenderValue.bstrVal);
												if(it != mapIdSender.end())
													s = it->second;
											}
											else
											{
												std::map< std::wstring, networkio::Sender*>::iterator it = mapIdIfSender.find(varSenderValue.bstrVal);
												if(it != mapIdIfSender.end())
													s = it->second;
											}

											if(s)
											{
												//Interface* pif = mapIdIf[varIfValue.bstrVal];
												std::map< std::wstring, networkio::Interface*>::iterator it = mapIdIf.find(varIfValue.bstrVal);

												if(it != mapIdIf.end())
												{
													networkio::Interface* pif = it->second;

													networkio::Receiver* r = new networkio::Receiver(s);

													unsigned short port = (unsigned short)_wtoi(varPortValue.bstrVal);
													if(0 == r->bindToInterface(pif, port, varMcastValue.bstrVal))
													{
														Receivers_.push_back(r);
													}
													else
													{
														delete r;

														wchar_t* wszAddress = pif->getStringAddress(port);
														consoleio::print_line(wszErrNotBindRcvr, wszAddress);
														delete[] wszAddress;
													}
												}
											}
											else
												consoleio::print_line(wszErrReceiverXml, varIfValue.bstrVal, varPortValue.bstrVal, varMcastValue.bstrVal, varSenderValue.bstrVal);

											VariantClear(&varSenderValue);
										}

										VariantClear(&varMcastValue);
										VariantClear(&varPortValue);
										VariantClear(&varIfValue);
										if(attributeMap) attributeMap->Release();
									}
								}

								if(pReceiverItem)
								{
									pReceiverItem->Release();
									pReceiverItem = 0;
								}
							}
							childsReceiversList->Release();
						}
					}

					if(childsReceivers) childsReceivers->Release();

					IXMLDOMNode* childsDestinations = 0;//destinations
					hr = xmlhelper::get_xml_item(childsBindingList, bindex, wszTagBroadcastDestinations, &childsDestinations);
					if(S_OK == hr)
					{
						IXMLDOMNodeList *childsDestinationsList = 0;
						hr = childsDestinations->get_childNodes(&childsDestinationsList);

						if(S_OK == hr && childsDestinationsList)
						{
							listLength = 0;
							hr = childsDestinationsList->get_length(&listLength);

							long dindex = 0;
							IXMLDOMNode* pDestItem = 0;

							while(S_OK == hr && listLength>0) //destination
							{
								hr = xmlhelper::get_xml_item(childsDestinationsList, dindex, wszTagDestination, &pDestItem);
								if(S_OK == hr)
								{
									IXMLDOMNamedNodeMap* attributeMap = 0;
									hr = pDestItem->get_attributes(&attributeMap);
									if(S_OK == hr)
									{
										VARIANT varAddrValue = {0};
										VARIANT varPortValue = {0};
										VARIANT varSenderValue = {0};

										HRESULT hr1 = xmlhelper::get_xml_attribute(L"address", attributeMap, &varAddrValue);
										HRESULT hr2 = xmlhelper::get_xml_attribute(L"port", attributeMap, &varPortValue);
										HRESULT hr3 = xmlhelper::get_xml_attribute(L"sender", attributeMap, &varSenderValue);

										if(S_OK==hr1 && S_OK==hr2 && S_OK==hr3)
										{
											//Sender* s = mapIdSender[varSenderValue.bstrVal];
											networkio::Sender* s = 0;

											std::map< std::wstring, networkio::Sender*>::const_iterator it = mapIdSender.find(varSenderValue.bstrVal);
											if(it != mapIdSender.end())
												s = it->second;

											if(s)
											{
												networkio::DESTADDR_INFO* d = new networkio::DESTADDR_INFO(s);

												unsigned short port = (unsigned short)_wtoi(varPortValue.bstrVal);
												if(0 == d->bindToAddress(varAddrValue.bstrVal, port))
												{
													Commands_.Destinations_.push_back(d);
												}
												else
												{
													delete d;
													consoleio::print_line(wszErrNotBindDestAddr, varAddrValue.bstrVal);
												}
											}
											else
												consoleio::print_line(wszErrDestinationXml, varAddrValue.bstrVal, varPortValue.bstrVal, varSenderValue.bstrVal);
										}

										VariantClear(&varSenderValue);
										VariantClear(&varPortValue);
										VariantClear(&varAddrValue);

										if(attributeMap) attributeMap->Release();
									}
								}

								if(pDestItem)
								{
									pDestItem->Release();
									pDestItem = 0;
								}
							}
							childsDestinationsList->Release();
						}
					}
					if(childsDestinations) childsDestinations->Release();
				}
				childsBindingList->Release();
			}
			pBindingItem->Release();
		}

		IXMLDOMNode* pFloodItem = 0;//flood_protection
		hr = xmlhelper::get_xml_item(childsNetworkList, index, wszTagFloodProtection, &pFloodItem);
		if(S_OK == hr)
		{
			IXMLDOMNamedNodeMap* attributeMap = 0;
			hr = pFloodItem->get_attributes(&attributeMap);
			if(S_OK == hr)
			{
				VARIANT varMaxRateValue = {0};
				VARIANT varIgnoreTimeValue = {0};

				HRESULT hr1 = xmlhelper::get_xml_attribute(L"max_rate", attributeMap, &varMaxRateValue);
				if(S_OK == hr1)
					networkio::Receiver::nFloodRate_ = static_cast<unsigned int>(_wtoi(varMaxRateValue.bstrVal));

				HRESULT hr2 = xmlhelper::get_xml_attribute(L"ignore_time", attributeMap, &varIgnoreTimeValue);
				if(S_OK == hr2)
					networkio::Receiver::nFloodProtectionTimeInterval_ = _wtoi(varIgnoreTimeValue.bstrVal);

				VariantClear(&varMaxRateValue);
				VariantClear(&varIgnoreTimeValue);
			}

			pFloodItem->Release();
		}
	}

	if(childsNetworkList) childsNetworkList->Release();

	if(pNetworkNode) pNetworkNode->Release();
	if(bstrNetworkInfo) SysFreeString(bstrNetworkInfo);

	return true;
}

BOOL WINAPI ChatTerminalApp::HandlerRoutine( DWORD dwCtrlType)
{
	switch(dwCtrlType)
	{
	case CTRL_C_EVENT:
		break;

	case CTRL_BREAK_EVENT:
		break;

	case CTRL_CLOSE_EVENT:
		break;

	case CTRL_LOGOFF_EVENT:
		break;

	case CTRL_SHUTDOWN_EVENT:
		break;
	}

	//Nothing works normally here
	theApp.fSigTerm_ = true;
	theApp.finalize();
	consoleio::console_finalize();

	ExitProcess(1);
	//return TRUE;
}

void ChatTerminalApp::resumeTimer()
{
	SetEvent(hTimerEvent_);
}

unsigned int __stdcall ChatTerminalApp::timerProc(void* pParam)
{
	UNREFERENCED_PARAMETER(pParam);

	while(theApp.hTimerEvent_ != INVALID_HANDLE_VALUE/*theApp.nDropUsersAfterInterval_*/)
	{
		{//scope for ContainersMonitor CONTAINERS_MONITOR;
			ContainersMonitor CONTAINERS_MONITOR;

			theApp.signalAlarm(0);
		}

		theApp.Commands_.sendDelayedMsgs();

		if(theApp.Commands_.delayedMsgs_.size()>0)
			WaitForSingleObject(theApp.hTimerEvent_, 50);
		else
		{
			if(theApp.nDropUsersAfterInterval_)
			{
				const DWORD dwNextTimerInterval = 60 * theApp.nDropUsersAfterInterval_/(maxUserPings_+1);//seconds
				WaitForSingleObject(theApp.hTimerEvent_, dwNextTimerInterval*1000);
			}
			else
				WaitForSingleObject(theApp.hTimerEvent_, INFINITE);
		}
	}

	//sends all retain messages
	theApp.Commands_.sendDelayedMsgs();

	return 0;
}
#else
bool ChatTerminalApp::initConfigFromXml(const wchar_t* file_path)
{
	const xmlChar* xszTagNetwork = BAD_CAST"/n:chatterminal/n:network";
	const xmlChar* xszTagUsersList = BAD_CAST"/n:chatterminal/n:users_list";
	const xmlChar* xszTagCmdLine = BAD_CAST"/n:chatterminal/n:command_line";

	xmlDocPtr doc = 0;
	if(!xmlhelper::create_Document_instance(&doc, file_path))
	{
		consoleio::print_line(wszErrLoadXmlFile, file_path);
		return false;
	}

	if (doc == NULL)
	{
		consoleio::print_line(wszErrLoadXmlFile, file_path);
		return false;
	}

	/* Init libxml */
	//xmlInitParser();

	/* Create xpath evaluation context */
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL)
	{
		xmlFreeDoc(doc); 
		return false;
	}
	
	/* Register namespaces from list (if any) */
	/* do register namespace */
	if(xmlXPathRegisterNs(xpathCtx, BAD_CAST "n", BAD_CAST "http://www.vypress.com/chatterm10") != 0)
	{
		xmlXPathFreeContext(xpathCtx);
		xmlFreeDoc(doc);
		return false;
	}

	if(!xpathCtx) return false;

	bool network_result = false;

	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xszTagNetwork, xpathCtx);
	if(xpathObj)
	{
		if(xpathObj->nodesetval)
		{
			xmlNodePtr network_nodePtr = xpathObj->nodesetval->nodeTab[0];

			network_result = initNetConfigFromXml(network_nodePtr);
		}

		xmlXPathFreeObject(xpathObj);
	}
	
	/*
	<users_list drop_after="3"></users_list>
	<command_line remember="100"></command_line>
	*/

	xpathObj = xmlXPathEvalExpression(xszTagUsersList, xpathCtx);
	if(xpathObj)
	{
		if(xpathObj->nodesetval)
		{
			xmlNodePtr users_list_nodePtr = xpathObj->nodesetval->nodeTab[0];
	
			if(users_list_nodePtr && users_list_nodePtr->type == XML_ELEMENT_NODE)
			{
				xmlChar* xszDropAfter = xmlGetProp(users_list_nodePtr, BAD_CAST"drop_after");
				nDropUsersAfterInterval_ = static_cast<unsigned int>(atoi(reinterpret_cast<const char*>(xszDropAfter)));
				xmlFree(xszDropAfter);
			}
		}

		xmlXPathFreeObject(xpathObj);
	}


	xpathObj = xmlXPathEvalExpression(xszTagCmdLine, xpathCtx);
	if(xpathObj)
	{
		if(xpathObj->nodesetval)
		{
			xmlNodePtr cmd_line_nodePtr = xpathObj->nodesetval->nodeTab[0];
	
			if(cmd_line_nodePtr && cmd_line_nodePtr->type == XML_ELEMENT_NODE)
			{
				xmlChar* xszRemember = xmlGetProp(cmd_line_nodePtr, BAD_CAST"remember");
				consoleio::nCommandLinesStackSize = static_cast<unsigned int>(atoi(reinterpret_cast<const char*>(xszRemember)));
				xmlFree(xszRemember);

				xmlChar* xszEcho = xmlGetProp(cmd_line_nodePtr, BAD_CAST"echo");
				if(xmlStrEqual(xszEcho, BAD_CAST"on"))
					fEcho_ = true;
				xmlFree(xszEcho);

			}
		}

		xmlXPathFreeObject(xpathObj);
	}

	xmlXPathFreeContext(xpathCtx); 
	xmlFreeDoc(doc); 
	return network_result;
}

bool ChatTerminalApp::initNetConfigFromXml(xmlNodePtr network_nodePtr)
{
	const xmlChar* xszTagInterfaces = BAD_CAST"interfaces";
	const xmlChar* xszTagInterface = BAD_CAST"interface";
	const xmlChar* xszTagBinding = BAD_CAST"binding";
	const xmlChar* xszTagSenders = BAD_CAST"senders";
	const xmlChar* xszTagSender = BAD_CAST"sender";
	const xmlChar* xszTagReceivers = BAD_CAST"receivers";
	const xmlChar* xszTagReceiver = BAD_CAST"receiver";
	const xmlChar* xszTagBroadcastDestinations = BAD_CAST"broadcast_destinations";
	const xmlChar* xszTagDestination = BAD_CAST"destination";
	const xmlChar* xszTagFloodProtection = BAD_CAST"flood_protection";
	/*
	<network>
	<interfaces>
	<interface id="i1" address="192.168.1.2" ipver="4"></interface>
	<interface id="i2" address="192.168.10.2" ipver="4"></interface>
	<interface id="i3" name="Local Area10" ipver="4"></interface>
	</interfaces>
	<binding>
	  <senders>
		<sender id="s1" interface="i1" bindport="0" ttl="1"></sender>
		<sender id="s2" interface="i2" bindport="0" ttl="1"></sender>
		<sender id="s3" interface="i1" bindport="8168" ttl="255"></sender>
	  </senders>
	<receivers>
	  <receiver interface="i1" bindport="8167" mcastgroups="227.0.0.1 227.0.0.2" sender="s1"></receiver>
	  <receiver interface="i2" bindport="8167" sender="s1" ></receiver>
	  <receiver interface="i1" bindport="8168" mcastgroups="227.0.0.2" sender="s2" ></receiver>
	  <receiver interface="i2" bindport="8168" mcastgroups="227.0.0.1" sender="s2" ></receiver>
	</receivers>
	<broadcast_destinations>
	  <destination address="227.0.0.2" port="8167" sender="s1" ></destination>
	  <destination address="227.0.0.2" port="8167" sender="s2" ></destination>
	  <destination address="192.168.10.255" port="8167" sender="s2"></destination>
	</broadcast_destinations>
	</binding>
	<flood_protection disabled="false" max_rate="30" ignore_time="30"></flood_protection>
	</network>
	*/

	if(!network_nodePtr || network_nodePtr->type != XML_ELEMENT_NODE)
	{
		return false;
	}

	xmlNodePtr childrenPtr = network_nodePtr->children;
	if(!childrenPtr)
	{
		return false;
	}

	//skip comments
	while(childrenPtr && childrenPtr->type == XML_COMMENT_NODE) childrenPtr = childrenPtr->next;

	//interfaces
	if(0==childrenPtr || !xmlStrEqual(xszTagInterfaces, childrenPtr->name))
	{
		return false;
	}

	xmlNodePtr interfacePtr = childrenPtr->children;

	std::map< std::string, networkio::Interface*> mapIdIf;//Temporary map Interface string Id -> Interface object pointer
	std::map< std::string, networkio::Sender*> mapIdSender;//Temporary map Sender string Id -> Sender object pointer
	std::map< std::string, networkio::Sender*> mapIdIfSender;//Temporary map Interface string Id -> Sender object pointer - it uses by receivers for selecting a default sender

	//interfaces
	while(interfacePtr)
	{
		//skip comments
		while(interfacePtr && interfacePtr->type == XML_COMMENT_NODE) interfacePtr = interfacePtr->next;
	
		if(0==interfacePtr) break;

		if(!xmlStrEqual(xszTagInterface, interfacePtr->name))
		{
			return false;
		}

		xmlChar* xszIdValue = xmlGetProp(interfacePtr, BAD_CAST"id");
		xmlChar* xszAddressValue = xmlGetProp(interfacePtr, BAD_CAST"address");
		xmlChar* xszNameValue = xmlGetProp(interfacePtr, BAD_CAST"name");

		if(xszIdValue && ( xszAddressValue || xszNameValue ))
		{
			int len = xmlUTF8Strlen(xszAddressValue);
			if(len<0) len=0;
			wchar_t* wszAddressValue = new wchar_t[len+1];
			if(xszAddressValue)
				NixHlpr.convUtf8ToWchar(xszAddressValue, xmlUTF8Strsize(xszAddressValue,len), wszAddressValue, len);

			wszAddressValue[len] = 0;

			int family = AF_INET;
			xmlChar* xszIpVerValue = xmlGetProp(interfacePtr, BAD_CAST"ipver");
			if(xszIpVerValue && xmlStrEqual(xszIpVerValue, BAD_CAST"6")) family = AF_INET6;

			networkio::Interface* pIf = 0;
			if(xszNameValue)
			{
				int len = xmlUTF8Strlen(xszNameValue);
				if(len<0) len=0;
				wchar_t* wszNameValue = new wchar_t[len+1];
				wszNameValue[len] = 0;
		
				size_t ret_len = NixHlpr.convUtf8ToWchar(xszNameValue, xmlUTF8Strsize(xszNameValue,len), wszNameValue, len);
                                DBG_UNREFERENCED_LOCAL_VARIABLE(ret_len);

				networkio::get_interface_by_adapter_name(wszNameValue, wszAddressValue, family, pIf);

				delete[] wszNameValue;
			}
			else
				if(xszAddressValue)
				{
					pIf = new networkio::Interface(wszAddressValue, family);
				}

			delete[] wszAddressValue;

			if(pIf)
			{
				Interfaces_.push_back(pIf);
				mapIdIf[reinterpret_cast<const char*>(xszIdValue)] = pIf;
			}

			xmlFree(xszIpVerValue);
		}

		xmlFree(xszNameValue);
		xmlFree(xszAddressValue);
		xmlFree(xszIdValue);

		interfacePtr = interfacePtr->next;
	}

	childrenPtr = childrenPtr->next;//binding

	//skip comments
	while(childrenPtr && childrenPtr->type == XML_COMMENT_NODE) childrenPtr = childrenPtr->next;

	//binding
	if(0==childrenPtr || !xmlStrEqual(xszTagBinding, childrenPtr->name))
	{
		return false;
	}

	xmlNodePtr sendersPtr = childrenPtr->children;//senders

	//skip comments
	while(sendersPtr && sendersPtr->type == XML_COMMENT_NODE) sendersPtr = sendersPtr->next;

	//senders
	if(0==sendersPtr || !xmlStrEqual(xszTagSenders, sendersPtr->name))
	{
		return false;
	}

	xmlNodePtr senderPtr = sendersPtr->children;//sender

	//sender
	while(senderPtr)
	{
		//skip comments
		while(senderPtr && senderPtr->type == XML_COMMENT_NODE) senderPtr = senderPtr->next;

		if(0==senderPtr) break;	

		if(!xmlStrEqual(xszTagSender, senderPtr->name))
		{
			return false;
		}

		xmlChar* xszIdValue = xmlGetProp(senderPtr, BAD_CAST"id");
		xmlChar* xszIfValue = xmlGetProp(senderPtr, BAD_CAST"interface");
		xmlChar* xszPortValue = xmlGetProp(senderPtr, BAD_CAST"bindport");
		xmlChar* xszTtlValue = xmlGetProp(senderPtr, BAD_CAST"ttl");

		if(xszIdValue && xszIfValue)
		{
			std::map< std::string, networkio::Interface*>::iterator it = mapIdIf.find(reinterpret_cast<const char*>(xszIfValue));
			//Interface* pif = mapIdIf[reinterpret_cast<const char*>(xszIfValue)];

			if(it != mapIdIf.end())
			{
				networkio::Interface* pif = it->second;

				networkio::Sender* s = new networkio::Sender();

				unsigned short port = 0;
				if(xszPortValue)
					port = static_cast<unsigned short>(atoi(reinterpret_cast<const char*>(xszPortValue)));

				DWORD dwTTL = DWORD(-1);
				if(xszTtlValue)
					dwTTL = static_cast<DWORD>(atoi(reinterpret_cast<const char*>(xszTtlValue)));

				if(0!=s->bindToInterface(pif, port, dwTTL))
				{
					wchar_t* wszAddress = pif->getStringAddress();
					consoleio::print_line(wszErrNotBindSender, wszAddress);
					delete[] wszAddress;
				}

				mapIdSender[reinterpret_cast<const char*>(xszIdValue)] = s;
				mapIdIfSender[reinterpret_cast<const char*>(xszIfValue)] = s;

				Senders_.push_back(s);
			}
		}

		xmlFree(xszTtlValue);
		xmlFree(xszPortValue);
		xmlFree(xszIfValue);
		xmlFree(xszIdValue);

		senderPtr = senderPtr->next;
	}


	xmlNodePtr receiversPtr = sendersPtr->next;//receivers
	//skip comments
	while(receiversPtr && receiversPtr->type == XML_COMMENT_NODE) receiversPtr = sendersPtr->next;

	if(0==receiversPtr || !xmlStrEqual(xszTagReceivers, receiversPtr->name))
	{
		return false;
	}

	xmlNodePtr receiverPtr = receiversPtr->children;//receiver

	//receiver
	while(receiverPtr)
	{
		//skip comments
		while(receiverPtr && receiverPtr->type == XML_COMMENT_NODE) receiverPtr = receiverPtr->next;
	
		if(0==receiverPtr) continue;

		if(!xmlStrEqual(xszTagReceiver, receiverPtr->name))
		{
			return false;
		}

		xmlChar* xszIfValue = xmlGetProp(receiverPtr, BAD_CAST"interface");
		xmlChar* xszPortValue = xmlGetProp(receiverPtr, BAD_CAST"bindport");
		xmlChar* xszMcastValue = xmlGetProp(receiverPtr, BAD_CAST"mcastgroups");

		if(xszIfValue && xszPortValue) //xszMcastValue is not necessary for broadcast
		{
			xmlChar* xszSenderValue = xmlGetProp(receiverPtr, BAD_CAST"sender");
			networkio::Sender* s = 0;
			if(xszSenderValue)
			{
				std::map< std::string, networkio::Sender*>::iterator it = mapIdSender.find(reinterpret_cast<const char*>(xszSenderValue));
				if(it != mapIdSender.end())
					s = it->second;
			}
			else
			{
				std::map< std::string, networkio::Sender*>::iterator it = mapIdIfSender.find(reinterpret_cast<const char*>(xszSenderValue));
				if(it != mapIdIfSender.end())
					s = it->second;
			}

			if(s)
			{
				//Interface* pif = mapIdIf[reinterpret_cast<const char*>(xszIfValue)];
				std::map< std::string, networkio::Interface*>::iterator it = mapIdIf.find(reinterpret_cast<const char*>(xszIfValue));

				if(it != mapIdIf.end())
				{
					networkio::Interface* pif = it->second;

					networkio::Receiver* r = new networkio::Receiver(s);

					unsigned short port = static_cast<unsigned short>(atoi(reinterpret_cast<const char*>(xszPortValue)));

					int len = xmlUTF8Strlen(xszMcastValue);
					if(len<0) len=0;
					wchar_t* wszMcastValue = new wchar_t[len+1];
					wszMcastValue[len] = 0;
			
					size_t ret_len = NixHlpr.convUtf8ToWchar(xszMcastValue, xmlUTF8Strsize(xszMcastValue,len), wszMcastValue, len);
                                        DBG_UNREFERENCED_LOCAL_VARIABLE(ret_len);

					if(0 == r->bindToInterface(pif, port, wszMcastValue))
					{
						Receivers_.push_back(r);
					}
					else
					{
						delete r;

						wchar_t* wszAddress = pif->getStringAddress(port);
						consoleio::print_line(wszErrNotBindRcvr, wszAddress);
						delete[] wszAddress;
					}

					delete[] wszMcastValue;
				}
			}
			else
			{
				consoleio::print_line(wszErrReceiverXmlUtf8, xszIfValue, xszPortValue, xszMcastValue, xszSenderValue);
			}

			xmlFree(xszSenderValue);
		}

		xmlFree(xszMcastValue);
		xmlFree(xszPortValue);
		xmlFree(xszIfValue);

		receiverPtr = receiverPtr->next;
	}


	xmlNodePtr destinationsPtr = receiversPtr->next;//broadcast_destinations
	//skip comments
	while(destinationsPtr && destinationsPtr->type == XML_COMMENT_NODE) destinationsPtr = sendersPtr->next;

	if(0==destinationsPtr || !xmlStrEqual(xszTagBroadcastDestinations, destinationsPtr->name))
		return false;

	xmlNodePtr destinationPtr = destinationsPtr->children;//destination

	//destination
	while(destinationPtr)
	{
		//skip comments
		while(destinationPtr && destinationPtr->type == XML_COMMENT_NODE) destinationPtr = destinationPtr->next;
	
		if(0==destinationPtr) break;

		if(!xmlStrEqual(xszTagDestination, destinationPtr->name))
			return false;

		xmlChar* xszAddrValue = xmlGetProp(destinationPtr, BAD_CAST"address");
		xmlChar* xszPortValue = xmlGetProp(destinationPtr, BAD_CAST"port");
		xmlChar* xszSenderValue = xmlGetProp(destinationPtr, BAD_CAST"sender");

		if(xszAddrValue && xszPortValue && xszSenderValue)
		{
			//Sender* s = mapIdSender[varSenderValue.bstrVal];
			networkio::Sender* s = 0;

			std::map< std::string, networkio::Sender*>::const_iterator it = mapIdSender.find(reinterpret_cast<const char*>(xszSenderValue));
			if(it != mapIdSender.end())
				s = it->second;

			if(s)
			{
				networkio::DESTADDR_INFO* d = new networkio::DESTADDR_INFO(s);

				int len = xmlUTF8Strlen(xszAddrValue);
				if(len<0) len=0;
				wchar_t* wszAddressValue = new wchar_t[len+1];
				wszAddressValue[len] = 0;
				size_t ret_len = NixHlpr.convUtf8ToWchar(xszAddrValue, xmlUTF8Strsize(xszAddrValue,len), wszAddressValue, len);
                                DBG_UNREFERENCED_LOCAL_VARIABLE(ret_len);

				unsigned short port = static_cast<unsigned short>(atoi(reinterpret_cast<const char*>(xszPortValue)));
				if(0 == d->bindToAddress(wszAddressValue, port))
				{
					Commands_.Destinations_.push_back(d);
				}
				else
				{
					delete d;
					consoleio::print_line(wszErrNotBindDestAddr, wszAddressValue);
				}

				delete[] wszAddressValue;
			}
			else
				consoleio::print_line(wszErrDestinationXmlUtf8, xszAddrValue, xszPortValue, xszSenderValue);
		}

		xmlFree(xszSenderValue);
		xmlFree(xszPortValue);
		xmlFree(xszAddrValue);

		destinationPtr = destinationPtr->next;
	}

	childrenPtr = childrenPtr->next;

	//skip comments
	while(childrenPtr && childrenPtr->type == XML_COMMENT_NODE) childrenPtr = childrenPtr->next;

	//interfaces and binding
	if(0==childrenPtr || !xmlStrEqual(xszTagFloodProtection, childrenPtr->name))
	{
		return false;
	}

	xmlNodePtr floodPtr = childrenPtr;//flood_protection

	if(floodPtr)
	{
		xmlChar* xszMaxRateValue = xmlGetProp(floodPtr, BAD_CAST"max_rate");
		xmlChar* xszIgnoreTimeValue = xmlGetProp(floodPtr, BAD_CAST"ignore_time");

		if(xszMaxRateValue)
			networkio::Receiver::nFloodRate_ = static_cast<unsigned int>(atoi(reinterpret_cast<const char*>(xszMaxRateValue)));

		if(xszIgnoreTimeValue)
			networkio::Receiver::nFloodProtectionTimeInterval_ = atoi(reinterpret_cast<const char*>(reinterpret_cast<const char*>(xszIgnoreTimeValue)));

		xmlFree(xszMaxRateValue);
		xmlFree(xszIgnoreTimeValue);
	}

	return true;
}

void ChatTerminalApp::signalFinalize(int signo)
{
	theApp.fSigTerm_ = true;
}

void ChatTerminalApp::resumeTimer()
{	
	pthread_mutex_lock(&timer_mutex_);

	pthread_cond_signal(&timer_wait_cond_);

	pthread_mutex_unlock(&timer_mutex_);
}

void* ChatTerminalApp::timerProc(void* pParam)
{
	UNREFERENCED_PARAMETER(pParam);

	pthread_mutex_lock(&theApp.timer_mutex_);

	timespec ts = {0};
	//clock_gettime(CLOCK_REALTIME, &ts); //clock_gettime is not implemented in Mac OS X

	while(theApp.timer_thread_/*theApp.theApp.nDropUsersAfterInterval_*/)
	{
		if(theApp.Commands_.delayedMsgs_.size()>0)
		{
			struct timeval now = {0};
			gettimeofday(&now, NULL);
			now.tv_usec += 500000;
			if(now.tv_usec >= 1000000)
			{
				now.tv_usec -= 1000000;
				now.tv_sec += 1;
			}
			
			ts.tv_sec = now.tv_sec;
			ts.tv_nsec = now.tv_usec*1000; //  usec to nsec
		}
		else
		{
			if(theApp.nDropUsersAfterInterval_)
			{
				const DWORD dwNextTimerInterval = 60 * theApp.nDropUsersAfterInterval_/(maxUserPings_+1);//seconds

				ts.tv_sec = time(NULL) + dwNextTimerInterval;
			}
			else
				ts.tv_sec = 0;
		}

		int ret_code = EINVAL;
		if(ts.tv_sec)
		{
#ifdef _DEBUG
			consoleio::print_line(L"TimerProc pthread_cond_timedwait %d:%d", ts.tv_sec, ts.tv_nsec);
#endif
			ret_code = pthread_cond_timedwait(&theApp.timer_wait_cond_, &theApp.timer_mutex_, &ts);
		}
		else
		{
#ifdef _DEBUG
			consoleio::print_line(L"TimerProc pthread_cond_wait %d:%d", ts.tv_sec, ts.tv_nsec);
#endif
			ret_code = pthread_cond_wait(&theApp.timer_wait_cond_, &theApp.timer_mutex_);
		}

		if(0 == ret_code || ETIMEDOUT == ret_code)
		{
			{//scope for ContainersMonitor CONTAINERS_MONITOR;
				ContainersMonitor CONTAINERS_MONITOR;

				signalAlarm(0);
			}
			
			theApp.Commands_.sendDelayedMsgs();
			
		}
		else
			break;
	}

	//sends all retain messages
	theApp.Commands_.sendDelayedMsgs();
	
	pthread_mutex_unlock(&theApp.timer_mutex_);

	return 0;
}
#endif // CHATTERM_OS_WINDOWS

void ChatTerminalApp::signalAlarm(int signo)
{
	UNREFERENCED_PARAMETER(signo);

	const DWORD dwNextTimerInterval = 60 * theApp.nDropUsersAfterInterval_/(maxUserPings_+1);//seconds

	time_t t={0};
	time(&t);

	USER_INFO::ConstIteratorOfUsers it = USER_INFO::SetOfUsers_.begin();
	//USER_INFO::ConstIteratorOfUsers end = USER_INFO::SetOfUsers_.end();
	while(theApp.nDropUsersAfterInterval_ && (it != USER_INFO::SetOfUsers_.end()))
	{
		if(!*it)
		{
			//it = USER_INFO::SetOfUsers_.erase(it);
			it = USER_INFO::removeUserFromList(it);
			continue; // to avoid it++
		}

		const std::unique_ptr<USER_INFO>& pinfo = *it;

		//if(pinfo.get() != &theApp.Me_)
		{
			if(pinfo->flood < 1)
			{
				double diff = difftime(t, pinfo->last_activity);
				if(dwNextTimerInterval < diff)
				{
#ifdef _DEBUG
					if(Commands::debug_)
						consoleio::print_line(L"Test user %ls, pings - %u", pinfo->getNick(), pinfo->pings);
#endif
					if( difftime(t, pinfo->last_ping) > 60 * theApp.nDropUsersAfterInterval_*2)
						pinfo->pings = 0;//all pings are expired, start again

					if(pinfo->pings < maxUserPings_)
					{
						theApp.Commands_.PingPongP(pinfo.get(), false);
						time(&pinfo->last_ping);
					}
					else
					{
						wchar_t* wszFromAddr = networkio::sockaddr_to_string(pinfo->naddr_info.psaddr_, sizeof(sockaddr_in6));
						consoleio::print_line( pinfo->color, false, wszUserDropped, getStrTime(true), pinfo->getNick(), wszFromAddr);
						delete[] wszFromAddr;

						//delete *it;
						//it = USER_INFO::SetOfUsers_.erase(it);
						it = USER_INFO::removeUserFromList(it);
						continue; // to avoid it++
					}
				}
			}
		}

		++it;
	}
}

void ChatTerminalApp::initialize()
{
	initConfig();

	//assign the first receiver as my
	if(Receivers_.size())
	{
		networkio::NETADDR_INFO::assign_from_receiver(Me_.naddr_info, Receivers_.front());
	}

	//start receivers' threads
	std::for_each(Receivers_.begin(), Receivers_.end(), std::mem_fun<int, networkio::Receiver>( &networkio::Receiver::start ));

	//Start ping-pong and delayed messages timer thread
	//if(nDropUsersAfterInterval_ > 0)
	{
#ifdef CHATTERM_OS_WINDOWS
		hTimerEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);

		hTimerThread_ = (HANDLE)_beginthreadex(NULL, 0, timerProc, this, 0, NULL);

		SetThreadPriority(hTimerThread_, THREAD_PRIORITY_BELOW_NORMAL);
#else
		//signal(SIGALRM, signalAlarm);
		//const DWORD dwNextTimerInterval = 60 * nDropUsersAfterInterval_/(maxUserPings_+1);//seconds
		//alarm(dwNextTimerInterval);

		int errcode = pthread_create(&timer_thread_, NULL, timerProc, this);
		DBG_UNREFERENCED_LOCAL_VARIABLE(errcode);
		assert(0==errcode);
#endif // CHATTERM_OS_WINDOWS
	}
}

void ChatTerminalApp::finalize()
{
	nDropUsersAfterInterval_ = 0;//Set stop flag
		
#ifdef CHATTERM_OS_WINDOWS
	if(INVALID_HANDLE_VALUE != hTimerThread_)
	{
		HANDLE hCopyTimerEvent = hTimerEvent_;
		hTimerEvent_ = INVALID_HANDLE_VALUE;//to break a loop in the timer thread

		SetEvent(hCopyTimerEvent);

		WaitForSingleObject( hTimerThread_, INFINITE );
		CloseHandle(hTimerThread_);
		hTimerThread_ = INVALID_HANDLE_VALUE;

		CloseHandle(hCopyTimerEvent);
		//hTimerEvent_ = INVALID_HANDLE_VALUE;
	}
#else
	//alarm(0);

	if(timer_thread_)
	{
		pthread_t copy_timer_thread = timer_thread_;
		timer_thread_ = 0; //to break a loop in the timer thread

		resumeTimer();
				
		int *status = 0; // holds return code
		pthread_join(copy_timer_thread, reinterpret_cast<void**>(&status));
		
		assert(0==status);
	}
#endif // CHATTERM_OS_WINDOWS

	leaveNetwork();

	//stop receivers' threads
	//std::for_each(Receivers_.begin(), Receivers_.end(), std::mem_fun<int, networkio::Receiver>( &networkio::Receiver::stop ));
	std::for_each(Receivers_.begin(), Receivers_.end(), delete_class_ptr<networkio::Receiver>());
	Receivers_.clear();

	std::for_each(Commands_.Destinations_.begin(), Commands_.Destinations_.end(), delete_class_ptr<networkio::DESTADDR_INFO>());
	Commands_.Destinations_.clear();

	std::for_each(Senders_.begin(), Senders_.end(), delete_class_ptr<networkio::Sender>());
	Senders_.clear();

	std::for_each(Interfaces_.begin(), Interfaces_.end(), delete_class_ptr<networkio::Interface>());
	Interfaces_.clear();
}

int ChatTerminalApp::run()
{
#ifdef CHATTERM_OS_WINDOWS
	if(!hCryptProv_ && !CryptAcquireContext(&hCryptProv_, 0, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		consoleio::print_line(wszErrCsp, MS_DEF_PROV);
		return -1;
	}
#endif // CHATTERM_OS_WINDOWS

	initMe();

	consoleio::print_line(wszWelcome, theApp.Me_.getNick());

	initialize();

#ifdef CHATTERM_OS_WINDOWS
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
#else
	//signal(SIGABRT, signalFinalize);
	signal(SIGINT, signalFinalize);
	signal(SIGKILL, signalFinalize);
	signal(SIGTERM, signalFinalize);
	signal(SIGQUIT, signalFinalize);
	signal(SIGHUP, signalFinalize);
#endif // CHATTERM_OS_WINDOWS

	bool bContinue = true;

	//run the initial script
	if(wszInitFile_)
	{
		std::wifstream ifs(wszInitFile_);
		if (!ifs.bad())
		{
#ifdef CHATTERM_OS_WINDOWS
			ifs.imbue(std::locale(".ACP"));
#endif // CHATTERM_OS_WINDOWS
			bContinue = (0 == processStreamInput(ifs));
		}
		else
		{
			consoleio::print_line(wszErrInitScriptFile, wszInitFile_, errno);
		}
	}

	if(bContinue)
	{
		if(consoleio::fConsoleIn)
		{
			int result = 0;
			std::wstring strLine;
			while((result = consoleio::console_read_input(strLine))>=0)
			{
				if(fSigTerm_) break;

				if(networkio::Receiver::fAddressListChange_)
				{
					consoleio::print_line(wszLocalAddrsChanged);
					consoleio::print_line(wszLeavingNet);

#ifdef CHATTERM_OS_WINDOWS
					Sleep(5000);//Sleep 5 secs to wait until the network configuration changed completely
#else
					sleep(5);
#endif // CHATTERM_OS_WINDOWS
					//reinitialize network configuration
					finalize();

					consoleio::print_line(wszInitializingNet);

					networkio::Receiver::fAddressListChange_ = false;

					initialize();
					consoleio::print_line(wszReinitializedNet);
				}

				if(result > 0)
				{
					if(0 != sendChatLine(strLine.c_str(), strLine.length())) break;
					strLine.clear();
				}
			}
		}
		else
		{
			processStreamInput(std::wcin);
		}
	}

	finalize();

	return 0;
}
