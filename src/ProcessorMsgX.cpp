
/*
$Id: ProcessorMsgX.cpp 36 2011-08-09 07:35:21Z avyatkin $

Implementation of the ProcessorMsgX class

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include "ChatTerminal.h"
#include "ProcessorMsgX.h"
#include "StrResources.h"

#ifndef CHATTERM_OS_WINDOWS
#include <cursesw.h> //beep()
#endif // CHATTERM_OS_WINDOWS

using namespace resources;

bool ProcessorMsgX::debug_ = false;

ProcessorMsgX::ProcessorMsgX()
#ifndef CHATTERM_OS_WINDOWS
			: fBe_(NixHlpr.isBigEndian())
#endif
{
};

ProcessorMsgX::~ProcessorMsgX(void)
{
};

int ProcessorMsgX::randomSleep()
{
	static int rt = (srand(static_cast<unsigned int>(time(NULL))), 0);
	
	rt = rand();
	if(rt>500) rt%=500;
	
#ifdef CHATTERM_OS_WINDOWS
	//Sleep(rt);
#else
	//struct timespec ts = {0, rt*1000000};
	//nanosleep(&ts, NULL);
#endif

#ifdef _DEBUG
	consoleio::print_line(L"Generated sleep time is %d", rt);
#endif

	return rt;
}

unsigned char ProcessorMsgX::dwordToByteColor(DWORD dwColor)
{
	static int t = (srand(static_cast<unsigned int>(time(NULL))), 0);
	DBG_UNREFERENCED_LOCAL_VARIABLE(t);

	unsigned char r = static_cast<unsigned char>(dwColor&(0x000000FF));
	unsigned char g = static_cast<unsigned char>((dwColor&(0x0000FF00))>>8);
	unsigned char b = static_cast<unsigned char>((dwColor&(0x00FF0000))>>16);

	//0xAA + 0x55 = 0xFF

	unsigned char byteColor = 0;
	unsigned char byteIntensity = 0;
	if(r>0xAA-1 || g>0xAA-1 || b>0xAA-1)
	{
		byteColor|=FOREGROUND_INTENSITY;
		byteIntensity = 0x55 + 1;
	}

	if(r>0x55-1+byteIntensity) byteColor|=FOREGROUND_RED;
	if(g>0x55-1+byteIntensity) byteColor|=FOREGROUND_GREEN;
	if(b>0x55-1+byteIntensity) byteColor|=FOREGROUND_BLUE;

	return byteColor;
}

const wchar_t* ProcessorMsgX::getMode(char status)
{
	static wchar_t mode[] = L"Unknown";

	switch(status)
	{
	case '0': return wszStatusNormal;
	case '1': return wszStatusDnd;
	case '2': return wszStatusAway;
	case '3': return wszStatusOff;
	}

	mode[0] = status;
	mode[1] = 0;
	return mode;

	//return wszStatusUnknown;
}

size_t ProcessorMsgX::convertLineBreaks(wchar_t* wszStr)
{
	const wchar_t lbreaks[] = { L'\x000D', //CR Carriage return \r
					//L'\x000A', //LF Line feed \n
					L'\x000C', //FF Form feed (new page) \f
					L'\x0028', //LS UNICODE line Separator
					L'\x2029', //PS UNICODE Paragraph Separator
					};

	int converted = 0;
	wchar_t* pos = 0;
	wchar_t* seek = wszStr;

	while(*seek)
	{
		if(wcschr(lbreaks, *seek))
		{
			if(*seek == L'\r' && *(seek+1) == L'\n')
			{
				if(0==pos) pos = seek;
				++seek;
			}

			*seek = L'\n';

			++converted;
		}

		if(pos)
			*pos++ = *seek;

		++seek;
	}

	if(pos)
	{
		*pos = 0;
		return pos - wszStr;
	}

	return seek - wszStr;
}

#ifdef CHATTERM_OS_WINDOWS
bool ProcessorMsgX::decryptData(unsigned char* data, int size, const CHANNEL_INFO* pcchinfo)
{
	DWORD dwLen = size;
	return TRUE == CryptDecrypt(pcchinfo->hKey, NULL, TRUE, 0, data, &dwLen);
}

bool ProcessorMsgX::verifySignature(const char* pMessage, size_t cMessageLen, const unsigned char* pSignatute, size_t cSignatureLen, std::unique_ptr<unsigned char[]> const &ptrPubKey, size_t cPubLen)
{
	return verifySignature(pMessage, cMessageLen, pSignatute, cSignatureLen, ptrPubKey.get(), cPubLen);
}

bool ProcessorMsgX::verifySignature(const char* pMessage, size_t cMessageLen, const unsigned char* pSignatute, size_t cSignatureLen, const unsigned char* pPubKey, size_t cPubLen)
{
	HCRYPTKEY hKey = NULL;
	HCRYPTHASH hHash = 0;
	bool bResult = false;

	try
	{
		if(!CryptImportKey(theApp.hCryptProv_, pPubKey, static_cast<DWORD>(cPubLen), NULL, 0, &hKey))
		{
			throw -1;
		}

		// Acquire a hash object handle.
		if(!CryptCreateHash(theApp.hCryptProv_, CALG_MD5, 0, 0, &hHash))
		{
			throw -1;
		}

		if(!CryptHashData(hHash, (const unsigned char*)pMessage, static_cast<DWORD>(cMessageLen), 0))
		{
			throw -1;
		}

		bResult = (TRUE == CryptVerifySignature(hHash, pSignatute, static_cast<DWORD>(cSignatureLen), hKey, 0, 0));
	}
	catch(int)
	{
	}

	if(hHash) CryptDestroyHash(hHash);
	if(hKey) CryptDestroyKey(hKey);

	return bResult;
}
#else
bool ProcessorMsgX::decryptData(unsigned char* data, int size, const CHANNEL_INFO* pcchinfo)
{
	//We use salt value of 11 NULL bytes for compatibility with Windows
	RC4_KEY key = {0};

	unsigned char key_data[16] = {0};
	//set 40 bits key
	memcpy(key_data, pcchinfo->passwd_hash, 5);
	RC4_set_key(&key, 16, key_data);

	unsigned char* outdata = new unsigned char[size];
	RC4(&key, size, data, outdata);

	memcpy(data, outdata, size);
	delete[] outdata;
	return true;
}

bool ProcessorMsgX::verifySignature(const char* pMessage, size_t cMessageLen, const unsigned char* pSignatute, size_t cSignatureLen, const unsigned char* pPubKey, size_t cPubLen)
{
	if(cPubLen != sizeof(PUBKEYDATA)) return false;

	RSA* r = RSA_new();
	if (NULL == r) return false;
/*
	PUBKEYDATA* pbk_data = new PUBKEYDATA;
	pbk_data->bType = 0x06;
	pbk_data->bVersion = 2;
	pbk_data->reserved = 0;
	pbk_data->aiKeyAlg = 0x00002400;
	pbk_data->magic = 0x31415352;
	pbk_data->bitlen = 1024;
	pbk_data->pubexp = 65537;
	unsigned char modulus[128];
*/
	//CALG_RSA_KEYX	0x0000a400
	//CALG_RSA_SIGN	0x00002400

	const PUBKEYDATA* pbk_data = reinterpret_cast<const PUBKEYDATA*>(pPubKey);
	if(pbk_data->bType != 0x06) return false;
	if(pbk_data->bVersion != 2) return false;
	
	const unsigned char aiKeyAlgSig[sizeof(pbk_data->aiKeyAlg)] = {0x00, 0x24, 0x00, 0x00};//CALG_RSA_SIGN
	const unsigned char aiKeyAlgKeyEx[sizeof(pbk_data->aiKeyAlg)] = {0x00, 0xa4, 0x00, 0x00};//CALG_RSA_KEYX
	const unsigned char magic[sizeof(pbk_data->magic)] = {0x52, 0x53, 0x41, 0x31};
	
	if(0!=memcmp(pbk_data->aiKeyAlg, aiKeyAlgSig, sizeof(pbk_data->aiKeyAlg))
	    && 0!=memcmp(pbk_data->aiKeyAlg, aiKeyAlgKeyEx, sizeof(pbk_data->aiKeyAlg))) return false;
	if(0!=memcmp(pbk_data->magic, magic, sizeof(pbk_data->magic))) return false;

	unsigned long pubexp = 0;//65537;
	int bitlen = 0;//1024;
#ifndef CHATTERM_OS_WINDOWS
	if(fBe_)
	{
	    unsigned char* pbytes = (unsigned char*)&pubexp;
	    for(size_t j=0; j<__min(sizeof(pubexp), sizeof(pbk_data->pubexp)); ++j)
		pbytes[sizeof(pubexp)-j-1] = pbk_data->pubexp[j];

    	    pbytes = (unsigned char*)&bitlen;
	    for(size_t j=0; j<__min(sizeof(bitlen), sizeof(pbk_data->bitlen)); ++j)
		pbytes[sizeof(bitlen)-j-1] = pbk_data->bitlen[j];
	}
	else
#endif // CHATTERM_OS_WINDOWS
	{
	    memcpy(&pubexp, pbk_data->pubexp, __min(sizeof(pubexp), sizeof(pbk_data->pubexp)));
	    memcpy(&bitlen, pbk_data->bitlen, __min(sizeof(bitlen), sizeof(pbk_data->bitlen)));
	}
	
	r->e = BN_new();
	BN_set_word(r->e, pubexp);

	int len_n = bitlen/8;

	unsigned char* modulus = new unsigned char[len_n];
	//reverse bytes for Windows
	for(int i=0; i<len_n; ++i)
	{
		modulus[i] = pbk_data->modulus[len_n-i-1];
	}

	r->n = BN_bin2bn(modulus, bitlen/8 ,NULL);

	delete[] modulus;
	//int len_e = BN_num_bytes(r->e);
	//int len_iqmp = BN_num_bytes(r->n);

	unsigned int siglen = RSA_size(r);
	if(cSignatureLen < siglen)
	{
		RSA_free(r);
		return false;
	}

	unsigned char md[MD5_DIGEST_LENGTH] = {0};
	MD5(reinterpret_cast<const unsigned char*>(pMessage), cMessageLen, md);

	unsigned char* signature = new unsigned char[siglen];

	//reverse bytes for Windows Crypto API
	for(unsigned int i=0; i<siglen; ++i)
	{
		signature[i] = pSignatute[siglen-i-1];
	}

	int result = RSA_verify(NID_md5, md, MD5_DIGEST_LENGTH, signature, siglen, r);

	delete[] signature;

	RSA_free(r);

	return (1 == result);
}
#endif // CHATTERM_OS_WINDOWS

bool ProcessorMsgX::checkMessage(const wchar_t* to, const wchar_t* from, const wchar_t* channel, std::shared_ptr<USER_INFO>& ptrUserInfo, std::shared_ptr<CHANNEL_INFO>& ptrChInfo)
{
	//To update USER_INFO::last_activity
	if(nullptr == from || !USER_INFO::isUserInList(from, ptrUserInfo))
	{
		if(debug_) consoleio::print_line( wszDbgNotInList);
		return false;
	}

	if(nullptr != to && !theApp.ptrMe_->cmpNick(to))
	{
		if(debug_) consoleio::print_line(wszDbgYouNotRecipient);
		return false;
	}

	if(channel)
	{
		if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
		{
			if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
			return false;
		}

		if (!ptrChInfo) return false;

		if(ptrUserInfo && !ptrChInfo->isMember(ptrUserInfo.get()))
		{
			if(debug_) consoleio::print_line( wszDbgSenderNotInChannel);
			return false;
		}
	}

	return true;
}

bool ProcessorMsgX::process(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr, const char* packet_id)
{
	switch(*pmsg)
	{
	case '0'://List
		return processList0(pmsg, msglen, pcrcvr);
	case '1'://List reply
		return processReplyList1(pmsg, msglen, pcrcvr);
	case '2'://ChannelMsg
		return processChannelMsg2A(pmsg, msglen, false);
	case '3'://NickName
		return processNickName3(pmsg, msglen);
	case '4'://Join
		return processJoin4(pmsg, msglen, pcrcvr);
	case '5'://Leave
		return processLeave5(pmsg, msglen);
	case '7'://MassTextMsgConfirm
		return processMassTextMsgConfirm7(pmsg, msglen);
	case 'A'://ChannelMeMsg
		return processChannelMsg2A(pmsg, msglen, true);
	case 'B'://NewTopic
		return processNewTopicB(pmsg, msglen, pcrcvr);
	case 'C'://Topic
		return processTopicC(pmsg, msglen);
	case 'D'://NewStatus
		return processNewStatusD(pmsg, msglen);
	case 'E'://MassTextMsg
		return processMassTextMsgE(pmsg, msglen, packet_id);
	case 'F'://Info
		return processInfoF(pmsg, msglen);
	case 'G'://Info reply
		return processReplyInfoG(pmsg, msglen);
	case 'H'://Beep
		return processBeepH(pmsg, msglen);
	case 'K'://Here reply
		return processReplyHereK(pmsg, msglen, pcrcvr);
	case 'L'://Here
		return processHereL(pmsg, msglen, pcrcvr);
	case 'M'://WndState
		return processWndStateM(pmsg, msglen);
	case 'N'://Channels
		return processChannelsN(pmsg, msglen);
	case 'O'://Channels reply
		return processReplyChannelsO(pmsg, msglen, pcrcvr);
	case 'P'://PingPong
		return processPingPongP(pmsg, msglen);
	case 'U'://ChangeNickName
		return processChangeNickNameU(pmsg, msglen);
	case 'Z'://Flood
		return processFloodZ(pmsg, msglen);

	case 'Q':
		{
			if(msglen<2) break;
			switch(*(pmsg+1))
			{
			case '0'://SecureChannelMsg
				return processSecureChannelMsgQ01(pmsg+1, msglen-1, false);
			case '1'://SecureChannelMeMsg
				return processSecureChannelMsgQ01(pmsg+1, msglen-1, true);
			case '2'://SecureTopic
				return processSecureTopicMsgQ23(pmsg+1, msglen-1, pcrcvr, false);
			case '3'://SecureNewTopic
				return processSecureTopicMsgQ23(pmsg+1, msglen-1, pcrcvr, true);
			case '4'://SecureHere reply
				return processReplySecureHereQ4(pmsg+1, msglen-1);
			case '5'://SecureJoin
				return processSecureJoinQ5(pmsg+1, msglen-1);
			case '6'://SecureJoin reply
				return processReplySecureJoinQ6(pmsg+1, msglen-1, pcrcvr);
			case '7'://SecureLeave
				return processSecureLeaveQ7(pmsg+1, msglen-1);
			case '8'://SecureHere
				return processSecureHereQ8(pmsg+1, msglen-1);
			}
		}
		break;
	}

	return false;
}

size_t ProcessorMsgX::parseMessageFields(const char* pMessage, size_t len, MSG_FIELD* pFields, int nFields)
{
	size_t processed = 0;
	const char* seek = pMessage;
	for(int i=0; i<nFields && processed<len; i++)
	{
		switch(pFields[i].type)
		{
			case SIGNATURE_FIELD:
				{
					size_t data_len = pFields[i].size;
					if(data_len<1)
					{
						data_len = len-(seek-pMessage);
						pFields[i].size = data_len;
					}

					unsigned char* pbytes = new unsigned char[pFields[i].size];
					memcpy(pbytes, seek, pFields[i].size);
					seek+=pFields[i].size;

					pFields[i].delete_ = true;
					pFields[i].data.bytes_ = pbytes;
					processed+=data_len;
				}
				break;

			case SIGNATURE_LEN_FIELD:
			case LEN_OF_STRING_FIELD:
			case NUMBER_FIELD:
#ifndef CHATTERM_OS_WINDOWS
				if(fBe_)
				{
					size_t data_len = pFields[i].size;
					unsigned char* pbytes = new unsigned char[pFields[i].size];

					for(size_t j=0; j<pFields[i].size; ++j)
					pbytes[pFields[i].size-j-1] = seek[j];
					
					seek+=pFields[i].size;
					pFields[i].delete_ = true;
					pFields[i].data.bytes_ = pbytes;
					processed+=data_len;

					break;
				}
#endif	
			case BYTES_FIELD:
				{
					size_t data_len = pFields[i].size;
					unsigned char* pbytes = new unsigned char[pFields[i].size];
					memcpy(pbytes, seek, pFields[i].size);
					seek+=pFields[i].size;

					pFields[i].delete_ = true;
					pFields[i].data.bytes_ = pbytes;
					processed+=data_len;
				}
				break;

			case STRING_FIELD:
				{
					size_t max_data_len = len-(seek-pMessage);
					if(pFields[i].size>0 && pFields[i].size<max_data_len) max_data_len = pFields[i].size;

					int data_len = 0;
					const char* field_data = seek;
					while(max_data_len && *seek)
					{
						++data_len;
						++seek;
						--max_data_len;
					}

					if(max_data_len)
					{
						++data_len;
						++seek;
					}

					wchar_t* wszData = new wchar_t[data_len+1];//Some STRING_FIELD fields may not have a terminating null
					memset(wszData, 0x00, (data_len+1)*sizeof(wchar_t));
#ifdef CHATTERM_OS_WINDOWS
					pFields[i].size = static_cast<size_t>(sizeof(wchar_t)*MultiByteToWideChar(CP_UTF8, 0, field_data, data_len, wszData, data_len+1));
#else
					pFields[i].size = NixHlpr.convUtf8ToWchar((unsigned char*)field_data, data_len, wszData, data_len+1);
#endif // CHATTERM_OS_WINDOWS
					pFields[i].delete_ = true;
					pFields[i].data.wsz_ = wszData;

					processed+=data_len;
				}
				break;

			case CHAR_FIELD:
				{
					size_t data_len = 1;
					pFields[i].size = 1;
					pFields[i].data.ch_ = *seek++;
					processed+=data_len;
				}
				break;

			default:
				break;
		}
	}

	return processed;
}

bool ProcessorMsgX::processList0(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'0' From h00 CodePage h00*/
	MSG_FIELD fields0[3] = {{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{BYTES_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fields0, _ARRAYSIZE(fields0));
	if(result<1) return false;

	wchar_t* from = fields0[0].data.wsz_;
	
	if(debug_)
	{
		//swap background and foreground colors
		consoleio::print_line_selected(L"List");

		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgCP, fields0[1].data.ch_);
	}
	
	if (theApp.ptrMe_->cmpNick(from))
	{
		networkio::NETADDR_INFO::assign_from_receiver(theApp.ptrMe_->naddr_info, pcrcvr);
		theApp.Commands_.ReplyList1(theApp.ptrMe_.get(), randomSleep());
		return true;
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	USER_INFO::isUserInList(from, ptrUserInfo);//It may return false if a user is blocked

	if(ptrUserInfo)
	{
		networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);
	}
	else
	{
		//Create a new user with provided IP address
		ptrUserInfo = std::make_shared<USER_INFO>();
		ptrUserInfo->setNick(from);

		networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);

		USER_INFO::SetOfUsers_.insert(ptrUserInfo);

		//avoid destruction
		fields0[0].data.wsz_ = 0;
		fields0[0].size = 0;
		fields0[0].type = VOID_FIELD;
	}

	//Vypress Chat can send this message to Here message instead of ReplyHere
	//so we should to add the user to the Main channel
	//CHANNEL_INFO::addChannelMember(CHANNEL_INFO::wszMainChannel, pUserInfo, CHANNEL_INFO::NOT_SECURED);

	theApp.Commands_.ReplyList1(ptrUserInfo.get(), randomSleep());

	return true;
}

bool ProcessorMsgX::processReplyList1(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	const DWORD ver21 = 0x00020001;
	DWORD ver = 0x00000000;
	DWORD dwColor = 0xff000000;
#ifdef CHATTERM_OS_WINDOWS
	UUID Uuid = {0};
#else
	uuid_t Uuid = {0};
#endif // CHATTERM_OS_WINDOWS
	DWORD dwLicenseID = 0;

	WORD cPubLen = 0;
	unsigned char byteIcon = 0;

	//'1' To h00 From h00 Status WndActive
	//Ver 1.9 h00 Version Gender UUID h00 LicenseID CodePage
	//Color h00
	//Ver 1.9.5 LastMsgBoardMessage TotalMsgBoardMessages MsgBoardMessageIDs h00
	//PubKeySize PubKey
	//Ver 2.0 Icon
	//Ver 2.1 Signature SignatureSize

	MSG_FIELD fields1[16] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{CHAR_FIELD,1,0,false}
		,{BYTES_FIELD,1,0,false},{NUMBER_FIELD,sizeof(ver),wszEmptyString,false},{CHAR_FIELD,1,0,false}
		,{BYTES_FIELD,sizeof(Uuid),wszEmptyString,false},{BYTES_FIELD,1,0,false}
		,{NUMBER_FIELD,sizeof(dwLicenseID),wszEmptyString,false},{CHAR_FIELD,1,0,false}
		,{NUMBER_FIELD,sizeof(dwColor),wszEmptyString,false},{BYTES_FIELD,1,0,false}
		,{BYTES_FIELD,sizeof(SYSTEMTIME)+sizeof(WORD),wszEmptyString,false}, {STRING_FIELD,0,0,false}//No Message Board messages
		,{NUMBER_FIELD, sizeof(cPubLen),wszEmptyString,false}};

	size_t result = parseMessageFields(seek, len, fields1, _ARRAYSIZE(fields1));
	if(result<1) return false;

	wchar_t* const& to = fields1[0].data.wsz_;
	wchar_t*& from = fields1[1].data.wsz_;
	const char& status = fields1[2].data.ch_;
	const char& wnd_state = fields1[3].data.ch_;
	ver = *(DWORD*)fields1[5].data.bytes_;
	const char& gender = fields1[6].data.ch_;
	memcpy(&Uuid, fields1[7].data.bytes_, __min(fields1[7].size, sizeof(Uuid)));
	dwLicenseID = *(DWORD*)fields1[9].data.bytes_;
	const char& codepage = fields1[10].data.ch_;
	dwColor = *(DWORD*)fields1[11].data.bytes_;
	cPubLen = *(WORD*)fields1[15].data.bytes_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplyList");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgStatus, status);
		consoleio::print_line(wszDbgWndState, wnd_state);

		consoleio::print_line(wszDbgVer, ver);
		consoleio::print_line(wszDbgGender, gender);

#ifdef CHATTERM_OS_WINDOWS
		RPC_WSTR wszUserUuid = 0;
		UuidToString(&Uuid, &wszUserUuid);
		consoleio::print_line( wszUuid, wszUserUuid);
		RpcStringFree(&wszUserUuid);
#else
		char szUserUuid[40]={0};
		uuid_unparse(Uuid, szUserUuid);
		wchar_t* wszUserUuid = 0;
		NixHlpr.assignWcharSz(&wszUserUuid, szUserUuid);
		consoleio::print_line( wszUuid, wszUserUuid);
		delete[] wszUserUuid;
#endif // CHATTERM_OS_WINDOWS

		consoleio::print_line(wszDbgLicenseId, dwLicenseID);
		consoleio::print_line(wszDbgCP, codepage);
		consoleio::print_line(wszDbgColor, dwColor);
	}

	//to prevent deleting fields2[0].data.bytes_ when going out of scope
	MSG_FIELD fields2[2] = {{BYTES_FIELD, cPubLen,0,false},{BYTES_FIELD, sizeof(byteIcon),0,false}};

	unsigned char* pub_key = 0;
	bool bSignature = false;
	if(ver >= ver21)
	{
		seek+= result;
		len-= result;

		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;

		if((sig_result==sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);

			const unsigned char* Signature = (unsigned char*)(seek + len);

			result = parseMessageFields(seek, len, fields2, _ARRAYSIZE(fields2));
			if(result<1) return false;

			pub_key = fields2[0].data.bytes_;
			byteIcon = fields2[1].data.bytes_[0];

			size_t cMessageLen = msglen - sig_len - sizeof(WORD);

			bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, pub_key, cPubLen);
		}

		if(debug_)
		{
			consoleio::print_line(wszDbgIcon, byteIcon);

			if(bSignature)
				consoleio::print_line(wszDbgSigValid);
			else
				consoleio::print_line(wszDbgSigNotValid);
		}
	}

	if((ver >= ver21) && !bSignature) return true;

	//It doesn't matter who is this message for

	//USER_INFO::ConstIteratorOfUsers it = find_user_by_uuid((const UUID*)fields1[7].data.bytes_);

	if (theApp.ptrMe_->cmpNick(from)) return true;

	std::shared_ptr<USER_INFO> ptrUserInfo;
	USER_INFO::isUserInList(from, ptrUserInfo);//It may return false if a user is blocked

	if(ptrUserInfo)
	{
		ptrUserInfo->freeFields();
	}
	else
	{
		ptrUserInfo = std::make_shared<USER_INFO>();
		USER_INFO::SetOfUsers_.insert(ptrUserInfo);
	}

	ptrUserInfo->setNick(from);

	ptrUserInfo->status = status;
	ptrUserInfo->wnd_state = wnd_state;
	ptrUserInfo->ver = ver;
	ptrUserInfo->gender = gender;
#ifdef CHATTERM_OS_WINDOWS
	ptrUserInfo->uuid = Uuid;
#else
	memcpy(&pUserInfo->uuid, &Uuid, sizeof(Uuid));
#endif // CHATTERM_OS_WINDOWS
	ptrUserInfo->license_id = dwLicenseID;
	ptrUserInfo->codepage = codepage;
	ptrUserInfo->color = dwordToByteColor(dwColor);
	ptrUserInfo->pub_key_size = cPubLen;
	ptrUserInfo->pub_key = std::unique_ptr<unsigned char[]>(pub_key);
	ptrUserInfo->icon = byteIcon;

	networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);

	//avoid destruction from
	fields1[1].data.wsz_ = 0;
	fields1[1].size = 0;
	fields1[1].type = VOID_FIELD;

	//avoid destruction pub_key
	fields2[0].data.bytes_ = 0;
	fields2[0].size = 0;
	fields2[0].type = VOID_FIELD;

	//Vypress Chat can send this reply to Here message instead of ReplyHere
	//so we should to add the user to the Main channel
	CHANNEL_INFO::addChannelMember(CHANNEL_INFO::wszMainChannel, ptrUserInfo.get(), CHANNEL_INFO::NOT_SECURED);

	return true;
}

bool ProcessorMsgX::processChannelMsg2A(const char* pmsg, size_t msglen, bool fMe)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'2' Channel h00 From h00 MessageText h00 Signature*/
	/*'A' Channel h00 From h00 MessageText h00 Signature*/
	MSG_FIELD fields[4] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{SIGNATURE_FIELD,0,0,false}};

	if(parseMessageFields(seek, len, fields, _ARRAYSIZE(fields))<1) return false;

	wchar_t* const& channel = fields[0].data.wsz_;
	wchar_t* const& from = fields[1].data.wsz_;

	convertLineBreaks(fields[2].data.wsz_);
	wchar_t* const& message = fields[2].data.wsz_;

	if(debug_)
	{
		if(fMe)
			consoleio::print_line_selected(L"ChannelMeMsg");
		else
			consoleio::print_line_selected(L"ChannelMsg");

		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgMessage, message);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(nullptr, from, channel, ptrUserInfo, ptrChInfo))
		return true;

	size_t cMessageLen = msglen-fields[3].size;
	bool bSignature = verifySignature(pmsg, cMessageLen, fields[3].data.bytes_, fields[3].size, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(bSignature)
	{
		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

		if(pacchinfo && (0 == wcscmp(pacchinfo->name.c_str(), channel)))
		{
			if(fMe)
				consoleio::print_line(ptrUserInfo->color, false, wszInChannelMeMsg, theApp.getStrTime(), from, message);
			else
				consoleio::print_line(ptrUserInfo->color, false, wszInChannelMsg, theApp.getStrTime(), from, message);
		}
		else
		{
			if(fMe)
				consoleio::print_line(ptrUserInfo->color, false, wszNotInChannelMeMsg, channel, theApp.getStrTime(), from, message);
			else
				consoleio::print_line(ptrUserInfo->color, false, wszNotInChannelMsg, channel, theApp.getStrTime(), from, message);
		}
	}

	return true;
}

bool ProcessorMsgX::processNickName3(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'3' From h00 NewNick h00 Gender Signature*/
	MSG_FIELD fields[4] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{SIGNATURE_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	wchar_t*& nick = fields[1].data.wsz_;
	const char& gender = fields[2].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"NickName");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgNewNick, nick);
		consoleio::print_line(wszDbgGender, gender);
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	USER_INFO::isUserInList(from, pUserInfo);//It may return false if a user is blocked

	if(!pUserInfo)
	{
		if(debug_) consoleio::print_line( wszDbgNotInList);
		return true;
	}

	size_t cMessageLen = msglen-fields[3].size;
	bool bSignature = verifySignature(pmsg, cMessageLen, fields[3].data.bytes_, fields[3].size, pUserInfo->pub_key, pUserInfo->pub_key_size);

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(bSignature)
	{
		if(pUserInfo->gender == '1')
			consoleio::print_line(pUserInfo->color, false, wszHerNickChanged, pUserInfo->getNick(), nick);
		else
			consoleio::print_line(pUserInfo->color, false, wszHisNickChanged, pUserInfo->getNick(), nick);

		pUserInfo->setNick(nick);
		pUserInfo->gender = gender;

		//avoid destruction
		fields[1].data.wsz_ = 0;
		fields[1].size = 0;
		fields[1].type = VOID_FIELD;
	}

	return true;
}

bool ProcessorMsgX::processJoin4(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	/*Notification about joining a UA to a chat network and #Main channel ("JoinMain")
	          '4' From h00 Channel h00 Status Gender
	Ver 1.9   h00 Version UUID h00 CodePage
	          Color h00
	Ver 1.9.5 LastMsgBoardMessage TotalMsgBoardMessages MsgBoardMessageIDs h00
	          PubKeySize PubKey
	Ver 2.0   Icon
	Ver 2.1   Signature SignatureSize

	Notification about joining a UA to a channel ("Join")
	'4' From h00 Channel h00 Status Gender*/

	const char* seek = pmsg+1;
	size_t len = msglen-1;

	MSG_FIELD fields1[4] = {{STRING_FIELD,0,0,false}
			,{STRING_FIELD,0,0,false}
			,{CHAR_FIELD,1,0,false},{CHAR_FIELD,1,0,false}};

	size_t result = parseMessageFields(seek, len, fields1, _ARRAYSIZE(fields1));
	if(result<1) return false;

	wchar_t* from = fields1[0].data.wsz_;//real pointer is needed here
	wchar_t* const& channel = fields1[1].data.wsz_;
	const char& status = fields1[2].data.ch_;
	const char& gender = fields1[3].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Join");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgStatus, status);
		consoleio::print_line(wszDbgGender, gender);
	}

	const DWORD ver20 = 0x00020000;
	const DWORD ver21 = 0x00020001;
	DWORD ver = 0x00000000;
	DWORD dwColor = 0xff000000;
	unsigned char* pub_key = 0;
#ifdef CHATTERM_OS_WINDOWS
	UUID Uuid = {0};
#else
	uuid_t Uuid = {0};
#endif // CHATTERM_OS_WINDOWS
	char codepage = '1';

	WORD cPubLen = 0;
	unsigned char byteIcon = 0;
	bool bSignature = false;

	//to prevent deleting fields3[0].data.bytes_ when going out of scope
	MSG_FIELD fields3[2] = {{BYTES_FIELD, cPubLen,0,false},{BYTES_FIELD, sizeof(byteIcon),0,false}};

	const int min_info_len = 1+sizeof(ver)+sizeof(Uuid)+1+1+sizeof(dwColor)+1;
	if(len>result+min_info_len-1)
	{
		//ver > 1.9
		ver = 0x00010009;
		seek+= result;
		len-= result;

		MSG_FIELD fields2[10] = {{BYTES_FIELD,1,0,false}
					,{NUMBER_FIELD,sizeof(ver),wszEmptyString,false}
					,{BYTES_FIELD,sizeof(Uuid),wszEmptyString,false}
					,{CHAR_FIELD,1,0,false}
					,{CHAR_FIELD,1,0,false},{NUMBER_FIELD,sizeof(dwColor),wszEmptyString,false},{BYTES_FIELD,1,0,false}
					,{BYTES_FIELD,sizeof(SYSTEMTIME)+sizeof(WORD),wszEmptyString,false}, {STRING_FIELD,0,0,false}//No Message Board messages
					,{NUMBER_FIELD, sizeof(cPubLen),wszEmptyString,false}};

		result = parseMessageFields(seek, len, fields2, _ARRAYSIZE(fields2));
		if(result>0)
		{
			seek+= result;
			len-= result;

			ver = *(DWORD*)fields2[1].data.bytes_;
			memcpy(&Uuid, fields2[2].data.bytes_, __min(fields2[2].size, sizeof(Uuid)));
			codepage = fields2[3].data.ch_;

			dwColor = *(DWORD*)fields2[5].data.bytes_;
			cPubLen = *(WORD*)fields2[9].data.bytes_;

			if(debug_)
			{
				consoleio::print_line( wszDbgVer, ver);

#ifdef CHATTERM_OS_WINDOWS
				RPC_WSTR wszUserUuid = 0;
				UuidToString(&Uuid, &wszUserUuid);
				consoleio::print_line( wszUuid, wszUserUuid);
				RpcStringFree(&wszUserUuid);
#else
				char szUserUuid[40]={0};
				uuid_unparse(Uuid, szUserUuid);
				wchar_t* wszUserUuid = 0;
				NixHlpr.assignWcharSz(&wszUserUuid, szUserUuid);
				consoleio::print_line( wszUuid, wszUserUuid);
				delete[] wszUserUuid;
#endif // CHATTERM_OS_WINDOWS

				consoleio::print_line(wszDbgCP, fields2[4].data.ch_);
				consoleio::print_line(wszDbgColor, dwColor);
			}

			if(ver >= ver21)
			{
				const char* psig_len = seek + len - sizeof(WORD);
				//WORD sig_len = *((WORD*)psig_len);
				MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
				size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
				WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;

				if((sig_result==sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
				{
					len -= sig_len+sizeof(WORD);
					const unsigned char* Signature = (const unsigned char*)(seek + len);

					fields3[0].size = cPubLen;
					result = parseMessageFields(seek, len, fields3, _ARRAYSIZE(fields3));
					if(result>0)
					{
						pub_key = fields3[0].data.bytes_;
						byteIcon= fields3[1].data.bytes_[0];

						size_t cMessageLen = msglen - sig_len - sizeof(WORD);

						bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, pub_key, cPubLen);
					}
				}

				if(debug_)
				{
					consoleio::print_line(wszDbgIcon, byteIcon);

					if(bSignature)
						consoleio::print_line(wszDbgSigValid);
					else
						consoleio::print_line(wszDbgSigNotValid);
				}
			}
		}
	}

	if((ver >= ver21) && !bSignature) return true;

	//It doesn't matter what channel user entered to
	//if(0 != _wcsicmp(channel, wszMainChannel))
	//	return true;

	const CHANNEL_INFO* pcchinfo = nullptr;

	if(theApp.ptrMe_->cmpNick(from))
	{
		//You have joined to a new channel
		theApp.Commands_.HereL(channel);

		pcchinfo = CHANNEL_INFO::setActiveChannel(channel);

		if (pcchinfo && pcchinfo->joined)
			consoleio::print_line(theApp.ptrMe_->color, false, wszJoinedToChannel, theApp.getStrTime(), from, channel);

		return true;
	}

	//USER_INFO::ConstIteratorOfUsers it = find_user_by_uuid((const UUID*)fields1[7].data.bytes_);
	std::shared_ptr<USER_INFO> ptrUserInfo;
	USER_INFO::isUserInList(from, ptrUserInfo);//It may return false if a user is blocked

	if(!ptrUserInfo)
	{
		ptrUserInfo = std::make_shared<USER_INFO>();
		USER_INFO::SetOfUsers_.insert(ptrUserInfo);
	}

	if(ver)
	{
		ptrUserInfo->ver = ver;
#ifdef CHATTERM_OS_WINDOWS
		ptrUserInfo->uuid = Uuid;
#else
		memcpy(&pUserInfo->uuid, &Uuid, sizeof(Uuid));
#endif // CHATTERM_OS_WINDOWS
		ptrUserInfo->codepage = codepage;
		ptrUserInfo->color = dwordToByteColor(dwColor);

		if((ver >= ver21) && pub_key && cPubLen)
		{
			ptrUserInfo->pub_key = std::unique_ptr<unsigned char[]>(pub_key);
			ptrUserInfo->pub_key_size = cPubLen;

			//avoid destruction
			fields3[0].data.bytes_ = 0;
			fields3[0].size = 0;
			fields3[0].type = VOID_FIELD;
		}

		if(ver>=ver20)
			ptrUserInfo->icon = byteIcon;
	}

	ptrUserInfo->setNick(from);

	//avoid destruction from
	fields1[0].data.wsz_ = 0;
	fields1[0].size = 0;
	fields1[0].type = VOID_FIELD;

	networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);

	ptrUserInfo->status = status;
	ptrUserInfo->gender = gender;

	pcchinfo = CHANNEL_INFO::addChannelMember(channel, ptrUserInfo.get(), CHANNEL_INFO::NOT_SECURED);

	if(!pcchinfo->topic.empty())
	{
		theApp.Commands_.ReplyTopicC(channel, pcchinfo->topic.c_str(), ptrUserInfo.get(), randomSleep());
	}

	if(pcchinfo && pcchinfo->joined)
		consoleio::print_line(ptrUserInfo->color, false, wszJoinedToChannel, theApp.getStrTime(), from, channel);

	return true;
}

bool ProcessorMsgX::processLeave5(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'5' From h00 Channel h00 Gender Signature*/
	MSG_FIELD fields[4] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{SIGNATURE_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	wchar_t* const& channel = fields[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Leave", from);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgGender, fields[2].data.ch_);
	}

	size_t cMessageLen = result + 1 - fields[3].size;

	bool bSignature = false;

	std::shared_ptr<USER_INFO> ptrUserInfo;
	if(USER_INFO::isUserInList(from, ptrUserInfo))
		bSignature = verifySignature(pmsg, cMessageLen, fields[3].data.bytes_, fields[3].size, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);
	else
	{
		if (debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if (!bSignature) return true;

	if (0 == _wcsicmp(channel, CHANNEL_INFO::wszMainChannel))
	{
		//We are unable to use here erase with std::remove_if or std::copy_if with std::set containers because of type of CHANNEL_INFO::SetOfChannels_.begin() is const_iterator
		std::set< std::shared_ptr<CHANNEL_INFO>, CHANNEL_INFO::Less >::iterator it = CHANNEL_INFO::SetOfChannels_.begin();
		while(it != CHANNEL_INFO::SetOfChannels_.end())
		{
			if((*it)->removeMember(ptrUserInfo.get()))
				consoleio::print_line(ptrUserInfo->color, false, wszLeftChannel, theApp.getStrTime(), from, (*it)->name.c_str());

			if((*it)->users.size() < 1)
				it = CHANNEL_INFO::SetOfChannels_.erase(it);
			else
				++it;
		}

		if (ptrUserInfo != theApp.ptrMe_)
			USER_INFO::removeUserFromList(from);
	}
	else
	{
		std::shared_ptr<CHANNEL_INFO> ptrChInfo;
		if (CHANNEL_INFO::isMyChannel(channel, ptrChInfo) && ptrChInfo)
		{
			if (ptrChInfo->removeMember(ptrUserInfo.get()))
			{
				consoleio::print_line(ptrUserInfo->color, false, wszLeftChannel, theApp.getStrTime(), from, ptrChInfo->name.c_str());

				if(ptrChInfo->users.size() < 1)
					CHANNEL_INFO::SetOfChannels_.erase(ptrChInfo);
			}
		}
	}

	return true;
}

bool ProcessorMsgX::processMassTextMsgConfirm7(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'7' Status To h00 From h00 Gender CurrentAA h00
		DatagramID h00
	*/
	MSG_FIELD fields[7] = {{CHAR_FIELD,1,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{STRING_FIELD,0,0,false},{BYTES_FIELD, 1,0,false}
							,{STRING_FIELD,0,0,false}};

	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	const char& status = fields[0].data.ch_;
	wchar_t* const& from = fields[2].data.wsz_;
	wchar_t* const& to = fields[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"MassTextMsgConfirm");
		consoleio::print_line(wszDbgStatus, status);
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgGender, fields[3].data.ch_);
		consoleio::print_line(wszDbgCurrentAA, fields[4].data.wsz_);
		consoleio::print_line(wszDbgDatagramID, fields[6].data.wsz_);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	consoleio::print_line(ptrUserInfo->color, false, wszReceivedMassMsg, from, getMode(status));

	return true;
}

bool ProcessorMsgX::processNewTopicB(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'B' Channel h00 Topic ' (From) ' h00 Signature*/
	MSG_FIELD fields[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{SIGNATURE_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& channel = fields[0].data.wsz_;
	wchar_t* const& topic = fields[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"NewTopic");
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgTopic, topic);

		USER_INFO::ConstIteratorOfUsers it = USER_INFO::findUsersByReceiver(USER_INFO::SetOfUsers_.begin(), pcrcvr);
		if(it == USER_INFO::SetOfUsers_.end())
			consoleio::print_line(wszDbgNotInList);

		while(it != USER_INFO::SetOfUsers_.end())
		{
			consoleio::print_line(wszDbgFromByIp, (*it)->getNick());
			it = USER_INFO::findUsersByReceiver(++it, pcrcvr);
		}
	}

	std::wstring strFrom;
	const wchar_t* sk1 = wcsrchr(topic, L'(');
	if(sk1)
	{
		if(sk1>topic && *(sk1-1)==L' ')
		{
			const wchar_t* sk2 = wcsrchr(sk1, L')');
			if(sk2)
			{
				//*(sk1-1) = 0;
				//*sk2 = 0;
				strFrom.assign(sk1+1, sk2-sk1-1);
				//from = sk1+1;
			}
		}
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(NULL, strFrom.c_str(), channel, ptrUserInfo, ptrChInfo))
		return true;

	size_t cMessageLen = msglen-fields[2].size;
	bool bSignature = verifySignature(pmsg, cMessageLen, fields[2].data.bytes_, fields[2].size, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(!bSignature) return true;

	ptrChInfo->topic=topic;

	const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

	if(pacchinfo == ptrChInfo.get())
	{
		consoleio::print_line(ptrUserInfo->color, false, wszNewTopic, theApp.getStrTime(), strFrom.c_str());
		consoleio::print_line(ptrUserInfo->color, false, wszTopic, topic);
	}
	else
	{
		consoleio::print_line(ptrUserInfo->color, false, wszChannelNewTopic, channel, theApp.getStrTime(), strFrom.c_str());
		consoleio::print_line(ptrUserInfo->color, false, wszChannelTopic, channel, topic);
	}

	//avoid destruction
	fields[1].data.wsz_ = 0;
	fields[1].size = 0;
	fields[1].type = VOID_FIELD;

	return true;
}

bool ProcessorMsgX::processTopicC(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'C' To h00 Channel h00 Topic h00*/
	MSG_FIELD fields[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& to = fields[0].data.wsz_;
	wchar_t* const& channel = fields[1].data.wsz_;
	wchar_t* const& topic = fields[2].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Topic");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgTopic, topic);
	}

	//It doesn't matter whom this message is for
	//if(0 != _wcsicmp(fields[0].data._ws, theApp.ptrMe_->getNick()))
	//{
	//	if(debug_) consoleio::print_line(wszDbgYouNotRecipient);
	//	return true;
	//}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
	{
		if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
		return true;
	}

	if(ptrChInfo->topic.empty())
	{
		ptrChInfo->topic = topic;

		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

		if(pacchinfo == ptrChInfo.get())
			consoleio::print_line(wszTopic, topic);
		else
			consoleio::print_line(wszChannelTopic, channel, topic);

		//avoid destruction
		fields[2].data.wsz_ = 0;
		fields[2].size = 0;
		fields[2].type = VOID_FIELD;
	}

	return true;
}

bool ProcessorMsgX::processNewStatusD(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	const char* psig_len = seek + len - sizeof(WORD);
	//WORD sig_len = *((WORD*)psig_len);
	MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
	size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
	WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
		
	/*'D' From h00 Status Gender CurrentAA h00 Signature SignatureSize*/
	MSG_FIELD fields[4] = {{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{CHAR_FIELD,1,0,false},{STRING_FIELD,0,0,false}};

	if(parseMessageFields(seek, len - sig_len-sizeof(WORD), fields, _ARRAYSIZE(fields))<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	const char& status = fields[1].data.ch_;
	wchar_t* const& aa = fields[3].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"NewStatus");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgStatus, status);
		consoleio::print_line(wszDbgGender, fields[2].data.ch_);
		consoleio::print_line(wszDbgCurrentAA, aa);
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	if(!USER_INFO::isUserInList(from, pUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	bool bSignature = false;
	if((sig_result==sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
	{
		unsigned char* Signature = (unsigned char*)(seek + len - sizeof(WORD) - sig_len);
		size_t cMessageLen = msglen - sig_len - sizeof(WORD);

		bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, pUserInfo->pub_key, pUserInfo->pub_key_size);
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(!bSignature) return true;

	if(pUserInfo->gender == '1')
		consoleio::print_line(pUserInfo->color, false, wszHerStatus, theApp.getStrTime(), from, getMode(status), aa);
	else
		consoleio::print_line(pUserInfo->color, false, wszHisStatus, theApp.getStrTime(), from, getMode(status), aa);

	return true;
}

bool ProcessorMsgX::processMassTextMsgE(const char* pmsg, size_t msglen, const char* packet_id)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'E' From h00 To h00 MessageText h00*/
	MSG_FIELD fields[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	wchar_t* const& to = fields[1].data.wsz_;
	convertLineBreaks(fields[2].data.wsz_);
	wchar_t* const& message = fields[2].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"MassTextMsg");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgMessage, message);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, 0, ptrUserInfo, ptrChInfo))
		return true;

	consoleio::print_line(ptrUserInfo->color, false, wszMassMsg, theApp.getStrTime(), from, message);

	wchar_t wszDatagramId[10]={0};
#ifdef CHATTERM_OS_WINDOWS
	MultiByteToWideChar(CP_UTF8, 0, packet_id, -1, wszDatagramId, _ARRAYSIZE(wszDatagramId));
#else
	NixHlpr.convUtf8ToWchar((unsigned char*)packet_id, 10, wszDatagramId, _ARRAYSIZE(wszDatagramId));
#endif // CHATTERM_OS_WINDOWS

	theApp.Commands_.ReplyConfirmMassTextMsg7(wszDatagramId, ptrUserInfo.get(), randomSleep());

	return true;
}

bool ProcessorMsgX::processInfoF(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'F' To h00 From h00*/
	MSG_FIELD fieldsF[2] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsF, _ARRAYSIZE(fieldsF));
	if(result<1) return false;

	const wchar_t* to = fieldsF[0].data.wsz_;
	const wchar_t* from = fieldsF[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Info");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgTo, to);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	theApp.Commands_.ReplyInfoG(ptrUserInfo.get());

	return true;
}

bool ProcessorMsgX::processReplyInfoG(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'G' To h00 From h00 Computer name h00 User name h00
	IP addresses h00 ListOfChannels '#' h00 CurrentAA h00 Domain name h00
	OS h00 Chat software h00
	FullName h00 Job h00 Department h00 Work phone h00
	Mobile phone h00 www h00 e-mail h00 address h00*/
	MSG_FIELD fieldsG[18] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}
							,{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}
							,{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}
							,{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}
							,{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}
							,{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};

	size_t result = parseMessageFields(seek, len, fieldsG, _ARRAYSIZE(fieldsG));
	if(result<1) return false;

	const wchar_t* to = fieldsG[0].data.wsz_;
	const wchar_t* from = fieldsG[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplyInfo");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgTo, to);

		consoleio::print_line(wszComputerName, fieldsG[2].data.wsz_);
		consoleio::print_line(wszUserName, fieldsG[3].data.wsz_);
		consoleio::print_line(wszDbgIpAddrs, fieldsG[4].data.wsz_);
		consoleio::print_line(wszDbgListOfChannels, fieldsG[5].data.wsz_);
		consoleio::print_line(wszDbgCurrentAA, fieldsG[6].data.wsz_);
		consoleio::print_line(wszDomainName, fieldsG[7].data.wsz_);
		consoleio::print_line(wszOS, fieldsG[8].data.wsz_);
		consoleio::print_line(wszChatSoftware, fieldsG[9].data.wsz_);
		consoleio::print_line(wszFullName, fieldsG[10].data.wsz_);
		consoleio::print_line(wszJob, fieldsG[11].data.wsz_);
		consoleio::print_line(wszDepartment, fieldsG[12].data.wsz_);
		consoleio::print_line(wszWorkPhone, fieldsG[13].data.wsz_);
		consoleio::print_line(wszMobilePhone, fieldsG[14].data.wsz_);
		consoleio::print_line(wszWebAddr, fieldsG[15].data.wsz_);
		consoleio::print_line(wszEmailAddr, fieldsG[16].data.wsz_);
		consoleio::print_line(wszPostAddr, fieldsG[17].data.wsz_);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	if(0 < ptrUserInfo->infos--)
	{
		consoleio::print_line(ptrUserInfo->color, false, wszInfoAbout, theApp.getStrTime(), from);

		consoleio::print_line(ptrUserInfo->color, false, wszFullName, fieldsG[10].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszUserName, fieldsG[3].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszComputerName, fieldsG[2].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszDomainName, fieldsG[7].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszOS, fieldsG[8].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszChatSoftware, fieldsG[9].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszJob, fieldsG[11].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszDepartment, fieldsG[12].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszWorkPhone, fieldsG[13].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszMobilePhone, fieldsG[14].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszWebAddr, fieldsG[15].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszEmailAddr, fieldsG[16].data.wsz_);
		consoleio::print_line(ptrUserInfo->color, false, wszPostAddr, fieldsG[17].data.wsz_);
	}

	return true;
}

bool ProcessorMsgX::processBeepH(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'H' '0' To h00 From h00*/
	MSG_FIELD fieldsH[3] = {{CHAR_FIELD,1,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsH, _ARRAYSIZE(fieldsH));
	if(result<1) return false;

	wchar_t* const& to = fieldsH[1].data.wsz_;
	wchar_t* const& from = fieldsH[2].data.wsz_;
	const char& param = fieldsH[0].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Beep");
		consoleio::print_line(wszDbgParam, param);
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgFrom, from);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	switch(param)
	{
	case '0':
#ifdef CHATTERM_OS_WINDOWS
		if(Beep( 750, 300 ))
#else
		//char beep = '\007';
		//std::wcout<<beep;
		if(OK == beep()) // - curses beep
#endif //CHATTERM_OS_WINDOWS
			theApp.Commands_.ReplyConfirmBeepH(ptrUserInfo.get());
		break;

	case '1':
		if(0 < ptrUserInfo->beeps--)
			consoleio::print_line(ptrUserInfo->color, false, wszBeepConfirmed, theApp.getStrTime(), from);
		break;
	}

	return true;
}

bool ProcessorMsgX::processReplyHereK(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'K' To h00 Channel h00 From h00 RemoteActive*/
	MSG_FIELD fields[4] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& to = fields[0].data.wsz_;
	wchar_t* const& channel = fields[1].data.wsz_;
	wchar_t*& from = fields[2].data.wsz_;
	const char& channel_state = fields[3].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplyHere");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChState, channel_state);
	}

	//It doesn't matter whom this message is for
	//if(0 != _wcsicmp(to, theApp.ptrMe_->getNick()))
	//{
	//	if(debug_) consoleio::print_line(wszDbgYouNotRecipient);
	//	return true;
	//}

	if (theApp.ptrMe_->cmpNick(from)) return true;
	std::shared_ptr<USER_INFO> ptrUserInfo;
	USER_INFO::isUserInList(from, ptrUserInfo);//It may return false if a user is blocked

	if (!ptrUserInfo)
	{
		//Reply to Here message may arrive earlier that List reply at first join to #Main time

		//Create a new user with provided IP address
		ptrUserInfo = std::make_shared<USER_INFO>();
		ptrUserInfo->setNick(from);

		networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);

		USER_INFO::SetOfUsers_.insert(ptrUserInfo);

		//avoid destruction from
		fields[2].data.wsz_ = 0;
		fields[2].size = 0;
		fields[2].type = VOID_FIELD;
	}

	CHANNEL_INFO::addChannelMember(channel, ptrUserInfo.get(), CHANNEL_INFO::NOT_SECURED);
	return true;
}

bool ProcessorMsgX::processHereL(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'L' From h00 Channel h00*/
	MSG_FIELD fieldsL[2] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsL, _ARRAYSIZE(fieldsL));
	if(result<1) return false;

	wchar_t* const& channel = fieldsL[1].data.wsz_;
	wchar_t*& from = fieldsL[0].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Here");
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgFrom, from);
	}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
	{
		if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
		return true;
	}

	//is_user_in_channel - is not necessary

	if (theApp.ptrMe_->cmpNick(from)) return true;

	std::shared_ptr<USER_INFO> ptrUserInfo;
	USER_INFO::isUserInList(from, ptrUserInfo);//It may return false if a user is blocked

	if (!ptrUserInfo)
	{
		//Create a new user with provided IP address
		ptrUserInfo = std::make_shared<USER_INFO>();
		ptrUserInfo->setNick(from);

		networkio::NETADDR_INFO::assign_from_receiver(ptrUserInfo->naddr_info, pcrcvr);

		USER_INFO::SetOfUsers_.insert(ptrUserInfo);

		//avoid destruction from
		fieldsL[0].data.wsz_ = 0;
		fieldsL[0].size = 0;
		fieldsL[0].type = VOID_FIELD;
	}

	theApp.Commands_.ReplyHereK(channel, ptrUserInfo.get(), randomSleep());

	return true;
}

bool ProcessorMsgX::processWndStateM(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'M' From h00 WndActive*/
	MSG_FIELD fields[2] = {{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	const char& wnd_state = fields[1].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"WndState");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgWndState, wnd_state);
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	if(!USER_INFO::isUserInList(from, pUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	pUserInfo->wnd_state = wnd_state;
	return true;
}

bool ProcessorMsgX::processChannelsN(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;
	/*'N' From h00*/
	MSG_FIELD fields[1] = {{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Channels");
		consoleio::print_line(wszDbgFrom, from);
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	if(!USER_INFO::isUserInList(from, pUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	theApp.Commands_.ReplyChannelsO(pUserInfo.get());

	return true;
}

bool ProcessorMsgX::processReplyChannelsO(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;
	/*'O' To h00 ListOfChannels '#'*/
	MSG_FIELD fields[2] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& to = fields[0].data.wsz_;
	wchar_t* const& channels = fields[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplyChannels");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgListOfChannels, channels);
	}

	//const USER_INFO* pUserInfo = find_user_by_ip(&from_in->sin_addr);
	//const USER_INFO* pUserInfo = find_user_by_receiver(pcrcvr);
	//if(0 == pUserInfo)

	USER_INFO::ConstIteratorOfUsers it = USER_INFO::findUsersByReceiver(USER_INFO::SetOfUsers_.begin(), pcrcvr);
	if(it == USER_INFO::SetOfUsers_.end())
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	bool bPrintChannels = (0 == _wcsicmp(to, theApp.ptrMe_->getNick()));

	//because of the comment below... we don't analyze a reply that is not for me
	if(!bPrintChannels) return true;

	while(it != USER_INFO::SetOfUsers_.end())
	{
		std::shared_ptr<USER_INFO> const& refPtrUserInfo = *it;
		if(debug_) consoleio::print_line(wszDbgFromByIp, refPtrUserInfo->getNick());

		if(bPrintChannels)
			consoleio::print_line(refPtrUserInfo->color, false, wszUserChannels, theApp.getStrTime(), refPtrUserInfo->getNick());

		/*
		It is not correct to determine whether a channel is secured or not
		only by prefix.
		We should to use findChannelByName(const wchar_t* channel, bool fJoined)
		and add a user to only exists channels.
		We can't create a correct (secured or not) channel!

		It is better to do nothing here!
		Channels should be filled by Here and List requests
		*/
		const wchar_t* seek1 = channels;
		const wchar_t* start = channels;
		do
		{
			seek1 = wcschr( start+1, L'#' );// C4996
			if(seek1)
			{
				if(start && *start && CHANNEL_INFO::checkNamePrefix(start+1, CHANNEL_INFO::SEC_UNKNOWN))
					start++;

				std::wstring strChannel(start, seek1-start);

				//if(pUserInfo != &theApp.ptrMe_->
				//	CHANNEL_INFO::addChannelMember(strChannel.c_str(), pUserInfo, CHANNEL_INFO::SEC_UNKNOWN);

				if(bPrintChannels)
					consoleio::print_line(refPtrUserInfo->color, false, strChannel.c_str());
			}

			start = seek1;
		}
		while(seek1);

		it = USER_INFO::findUsersByReceiver(++it, pcrcvr);
	}

	return true;
}

bool ProcessorMsgX::processPingPongP(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;
	/*'P' '0' To h00 From h00 CurrentTime h00
	LastMsgBoardMessage TotalMsgBoardMessages MsgBoardMessageIDs h00 PubKeySize PubKey*/
	MSG_FIELD fieldsP1[4] = {{CHAR_FIELD,1,0,false}
						,{STRING_FIELD,0,0,false}
						,{STRING_FIELD,0,0,false}
						,{STRING_FIELD,0,0,false}};

	size_t result = parseMessageFields(seek, len, fieldsP1, _ARRAYSIZE(fieldsP1));
	if(result<1) return false;

	const char& param = fieldsP1[0].data.ch_;
	wchar_t* const& to = fieldsP1[1].data.wsz_;
	wchar_t* const& from = fieldsP1[2].data.wsz_;
	wchar_t* const& time = fieldsP1[3].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"PingPong");
		consoleio::print_line(wszDbgParam, param);
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgTime, time);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	switch(param)
	{
	case '0':
		theApp.Commands_.PingPongP(ptrUserInfo.get(), true);
		break;

	case '1':
		{
			if(0 < ptrUserInfo->pings)
			{
				if(debug_)
					consoleio::print_line(ptrUserInfo->color, false, wszDbgPingedsuccessfuly, theApp.getStrTime(true), from, time);
			}

			if(ptrUserInfo->pings>0) --ptrUserInfo->pings;
		}
		break;
	}

	seek+=result;
	len-= result;

	WORD cPubLen = 0;
	if(len>result)
	{
		MSG_FIELD fieldsP2[3] = {{BYTES_FIELD,sizeof(SYSTEMTIME)+sizeof(WORD),0,false}, {STRING_FIELD,0,0,false}//No Message Board messages
							,{NUMBER_FIELD, sizeof(cPubLen),wszEmptyString,false}};

		result = parseMessageFields(seek, len, fieldsP2, _ARRAYSIZE(fieldsP2));
		if(result<1) return false;

		unsigned char* pub_key = 0;
		cPubLen = *(WORD*)fieldsP2[2].data.bytes_;

		if(len>=result+cPubLen)
		{
			seek+=result;
			len-= result;

			pub_key = (unsigned char*)seek;
		}

		if(cPubLen>0 && pub_key)
		{
			ptrUserInfo->pub_key = std::make_unique<unsigned char[]>(cPubLen);

			memcpy(ptrUserInfo->pub_key.get(), pub_key, cPubLen);

			ptrUserInfo->pub_key_size = cPubLen;
		}
	}

	return true;
}

bool ProcessorMsgX::processChangeNickNameU(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'U' From h00 CodePage h00*/
	MSG_FIELD fields[2] = {{STRING_FIELD,0,0,false},{CHAR_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& from = fields[0].data.wsz_;
	const char& codepage = fields[1].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ChangeNickName");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgCP, codepage);
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	if(!USER_INFO::isUserInList(from, pUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	std::wstring wszFromAddr = networkio::sockaddr_to_string(pUserInfo->naddr_info.psaddr_, sizeof(sockaddr_in6));
	consoleio::print_line(pUserInfo->color, false, wszRequireToChangeNick, theApp.getStrTime(), from, wszFromAddr.c_str());
	return true;
}

bool ProcessorMsgX::processFloodZ(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	const size_t len = msglen-1;

	/*'Z' To h00 From h00 Количество секунд h00*/
	MSG_FIELD fields[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fields, _ARRAYSIZE(fields));
	if(result<1) return false;

	wchar_t* const& to = fields[0].data.wsz_;
	wchar_t* const& from = fields[1].data.wsz_;
	wchar_t* const& seconds = fields[2].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"Flood");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgSeconds, seconds);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(to, from, NULL, ptrUserInfo, ptrChInfo))
		return true;

	consoleio::print_line(ptrUserInfo->color, false, wszFloodNotify, theApp.getStrTime(), from, seconds);
	return true;
}

bool ProcessorMsgX::processSecureChannelMsgQ01(const char* pmsg, size_t msglen, bool fMe)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;
	WORD MessageTextLentgh = 0;

	/*'Q' '0' Channel h00 From h00 MessageTextLentgh MessageText h00 SignatureSize Signature*/
	MSG_FIELD fieldsQ00[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{LEN_OF_STRING_FIELD, sizeof(MessageTextLentgh),0,false}};

	size_t result1 = parseMessageFields(seek, len, fieldsQ00, _ARRAYSIZE(fieldsQ00));
	if(result1<1) return false;

	wchar_t* const& channel = fieldsQ00[0].data.wsz_;
	wchar_t* const& from = fieldsQ00[1].data.wsz_;

	MessageTextLentgh = *(WORD*)fieldsQ00[2].data.bytes_;

	if(debug_)
	{
		consoleio::print_line_selected(L"SecureChannelMsg");
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgMessageLen, MessageTextLentgh);
	}

	if(MessageTextLentgh<1)
	{
		if(debug_) consoleio::print_line(wszDbgSecuredTextEmpty);
		return true;
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!checkMessage(NULL, from, channel, ptrUserInfo, ptrChInfo))
		return true;

	if(!ptrChInfo->secured) return true;

	seek+=result1;
	len-=result1;

	if(!decryptData((unsigned char*)seek, MessageTextLentgh, ptrChInfo.get()))
	{
		consoleio::print_line(ptrUserInfo->color, false,  wszUnableDecryptFrom, from, channel);
		return true;
	}

	WORD sig_len = 0;
	MSG_FIELD fieldsQ01[2] = {{STRING_FIELD,MessageTextLentgh,0,false},{NUMBER_FIELD, sizeof(sig_len),0,false}};
	size_t result2 = parseMessageFields(seek, len, fieldsQ01, _ARRAYSIZE(fieldsQ01));
	if(result2<1) return false;

	convertLineBreaks(fieldsQ01[0].data.wsz_);
	wchar_t* const& message = fieldsQ01[0].data.wsz_;
	if(debug_) consoleio::print_line( wszDbgMessage, message);

	seek+=result2;
	len-=result2;

	sig_len = *(WORD*)fieldsQ01[1].data.bytes_;
	const unsigned char* Signature = (const unsigned char*)seek;
	size_t cMessageLen = 1+result1 + result2 -fieldsQ01[1].size;

	bool bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(!bSignature) return true;

	const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

	if(pacchinfo && pacchinfo->name==channel)
	{
		if(fMe)
			consoleio::print_line(ptrUserInfo->color, false, wszInChannelMeMsg, theApp.getStrTime(), from, message);
		else
			consoleio::print_line(ptrUserInfo->color, false, wszInChannelMsg, theApp.getStrTime(), from, message);
	}
	else
	{
		if(fMe)
			consoleio::print_line(ptrUserInfo->color, false, wszNotInChannelMeMsg, channel, theApp.getStrTime(), from, message);
		else
			consoleio::print_line(ptrUserInfo->color, false, wszNotInChannelMsg, channel, theApp.getStrTime(), from, message);
	}

	return true;
}

bool ProcessorMsgX::processSecureTopicMsgQ23(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr, bool fNewTopic)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;
	WORD TopicLentgh = 0;

	/*'Q' '3' Channel h00 TopicLength Topic ' (From) ' SignatureSize Signature*/
	MSG_FIELD fieldsQ30[2] = {{STRING_FIELD,0,0,false}, {LEN_OF_STRING_FIELD, sizeof(TopicLentgh),0,false}};

	size_t result1 = parseMessageFields(seek, len, fieldsQ30, _ARRAYSIZE(fieldsQ30));
	if(result1<1) return false;

	wchar_t* const& channel = fieldsQ30[0].data.wsz_;

	TopicLentgh = *(WORD*)fieldsQ30[1].data.bytes_;

	if(debug_)
	{
		consoleio::print_line_selected(L"SecureTopic");
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgTopicLen, TopicLentgh);

		USER_INFO::ConstIteratorOfUsers it = USER_INFO::findUsersByReceiver(USER_INFO::SetOfUsers_.begin(), pcrcvr);
		if(it == USER_INFO::SetOfUsers_.end())
			consoleio::print_line(wszDbgNotInList);
		else
		{
			while(it != USER_INFO::SetOfUsers_.end())
			{
				std::shared_ptr<USER_INFO> const& refPtrUserInfo = *it;
				consoleio::print_line(wszDbgFromByIp, refPtrUserInfo->getNick());
				it = USER_INFO::findUsersByReceiver(++it, pcrcvr);
			}
		}
	}

	if(TopicLentgh<1)
	{
		if(debug_) consoleio::print_line(wszDbgSecuredTextEmpty);
		return true;
	}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
	{
		if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
		return true;
	}

	if(!ptrChInfo->secured) return true;

	seek+=result1;
	len-=result1;

	if(!decryptData((unsigned char*)seek, TopicLentgh, ptrChInfo.get()))
	{
		USER_INFO::ConstIteratorOfUsers it = USER_INFO::findUsersByReceiver(USER_INFO::SetOfUsers_.begin(), pcrcvr);
		if(it == USER_INFO::SetOfUsers_.end())
			consoleio::print_line( wszUnableDecrypt, channel);
		else
		{
			while(it != USER_INFO::SetOfUsers_.end())
			{
				const USER_INFO* pUserInfo = (*it).get();
				consoleio::print_line(pUserInfo->color, false,  wszUnableDecryptFrom, pUserInfo->getNick(), channel);
				it = USER_INFO::findUsersByReceiver(++it, pcrcvr);
			}
		}

		return true;
	}

	WORD sig_len =0;
	MSG_FIELD fieldsQ31[2] = {{STRING_FIELD,TopicLentgh,0,false},{NUMBER_FIELD, sizeof(sig_len),0,false}};

	size_t result2 = parseMessageFields(seek, len, fieldsQ31, _ARRAYSIZE(fieldsQ31));
	if(result2<1) return false;

	const wchar_t* topic = fieldsQ31[0].data.wsz_;

	if(debug_)
		consoleio::print_line(wszDbgTopic, topic);

	std::wstring strFrom;
	const wchar_t* sk1 = wcsrchr(topic, L'(');
	if(sk1)
	{
		if(sk1>topic && *(sk1-1)==L' ')
		{
			const wchar_t* sk2 = wcsrchr(sk1, L')');
			if(sk2)
			{
				//*(sk1-1) = 0;
				//*sk2 = 0;
				strFrom.assign(sk1+1, sk2-sk1-1);
				//from = sk1+1;
			}
		}
	}

	std::shared_ptr<USER_INFO> pUserInfo;
	if(!USER_INFO::isUserInList(strFrom.c_str(), pUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	if(!ptrChInfo->isMember(pUserInfo.get()))
	{
		if(debug_) consoleio::print_line( wszDbgSenderNotInChannel);
		return true;
	}

	seek+=result2;
	len-=result2;

	sig_len = *(WORD*)fieldsQ31[1].data.bytes_;
	size_t cMessageLen = 1+result1+result2-fieldsQ31[1].size;
	unsigned char* Signature = (unsigned char*)seek;

	bool bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, pUserInfo->pub_key, pUserInfo->pub_key_size);

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if(!bSignature) return true;

	if(fNewTopic || ptrChInfo->topic.empty())
	{
		ptrChInfo->topic = topic;

		const CHANNEL_INFO* pacchinfo = CHANNEL_INFO::getActiveChannel();

		if(fNewTopic)
		{
			if(pacchinfo == ptrChInfo.get())
				consoleio::print_line(pUserInfo->color, false, wszNewTopic, theApp.getStrTime(), strFrom.c_str());
			else
				consoleio::print_line(pUserInfo->color, false, wszChannelNewTopic, channel, theApp.getStrTime(), strFrom.c_str());
		}

		if(pacchinfo == ptrChInfo.get())
			consoleio::print_line(wszTopic, topic);
		else
			consoleio::print_line(wszChannelTopic, channel, topic);

		//avoid destruction
		fieldsQ31[0].data.wsz_ = 0;
		fieldsQ31[0].size = 0;
		fieldsQ31[0].type = VOID_FIELD;
	}

	return true;
}

bool ProcessorMsgX::processReplySecureHereQ4(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	/*'Q' '4' To h00 Channel h00 From h00 RemoteActive Signature SignatureSize*/
	MSG_FIELD fieldsQ4[4] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsQ4, _ARRAYSIZE(fieldsQ4));
	if(result<1) return false;

	wchar_t* const& to = fieldsQ4[0].data.wsz_;
	wchar_t* const& channel = fieldsQ4[1].data.wsz_;
	wchar_t* const& from = fieldsQ4[2].data.wsz_;
	const char& channel_state = fieldsQ4[3].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplySecureHere");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChState, channel_state);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	//It doesn't matter whom this message is for
	if(!checkMessage(NULL, from, channel, ptrUserInfo, ptrChInfo))
		return true;

	if(!ptrChInfo->secured) return true;
	if(USER_INFO::Comparator(theApp.ptrMe_.get()),from) return true;

	bool bSignature = false;
	if(result<len)
	{
		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
	
		if((sig_result == sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);
			const unsigned char* Signature = (const unsigned char*)(seek + len);

			size_t cMessageLen = len+1;//len-- at 2d row

			bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

			if(bSignature)
				ptrChInfo->addMember(ptrUserInfo.get());
		}
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	return true;
}

bool ProcessorMsgX::processSecureJoinQ5(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	/*'Q' '5' From h00 Channel h00 Status Gender (16)MD5Hash Signature SignatureSize*/
	MSG_FIELD fieldsQ5[5] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false},{CHAR_FIELD,1,0,false},{BYTES_FIELD,16,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsQ5, _ARRAYSIZE(fieldsQ5));
	if(result<1) return false;

	wchar_t* const& from = fieldsQ5[0].data.wsz_;
	wchar_t* const& channel = fieldsQ5[1].data.wsz_;
	const char& status = fieldsQ5[2].data.ch_;
	const char& gender = fieldsQ5[3].data.ch_;
	unsigned char* const& hash = fieldsQ5[4].data.bytes_;

	if(debug_)
	{
		consoleio::print_line_selected(L"SecureJoin");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgStatus, status);
		consoleio::print_line(wszDbgGender, gender);

		size_t bufsize = _ARRAYSIZE(wszDbgHash)+3*fieldsQ5[4].size+1;
		std::unique_ptr<wchar_t[]> ptrLine = std::make_unique<wchar_t[]>(bufsize);
		wchar_t* seek_line = ptrLine.get();
		wcscpy_s(seek_line, bufsize, wszDbgHash);
		seek_line +=_ARRAYSIZE(wszDbgHash)-1;

		for(size_t i=0; i<fieldsQ5[4].size && seek_line<ptrLine.get()+bufsize; i++)
		{
			swprintf(seek_line, 4, L"%02X ", hash[i]);
			seek_line +=3;
		}

		consoleio::print_line(ptrLine.get());
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	if(!USER_INFO::isUserInList(from, ptrUserInfo))
	{
		if(debug_) consoleio::print_line( wszDbgNotInList);
		return false;
	}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
	{
		if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
		return false;
	}

	//It doesn't matter whom this message is for
	//It doesn't matter if a sender is a channel member
	//if(!checkMessage(NULL, from, channel, &pUserInfo, &pchinfo))
	//	return true;

	if(!ptrChInfo->secured) return true;

	bool bSignature = false;

	if(result<len)
	{
		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
	
		if((sig_result == sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);
			const unsigned char* Signature = (const unsigned char*)(seek + len);
			size_t cMessageLen = len+1;//len-- at 2d row

			bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

			if(bSignature)
			{
				char chResult = 0x00;
				if(0 == memcmp(hash, ptrChInfo->passwd_hash, sizeof(ptrChInfo->passwd_hash))) chResult = 0x01;

				if(ptrUserInfo != theApp.ptrMe_)
				{
					theApp.Commands_.ReplySecureJoinQ6(ptrChInfo->name, chResult, ptrUserInfo.get(), randomSleep());
				}

				if(chResult == 0x01)
				{
					consoleio::print_line(ptrUserInfo->color, false, wszJoinedToChannel, theApp.getStrTime(), from, ptrChInfo->name.c_str());

					if(ptrUserInfo == theApp.ptrMe_)
					{
						//You have joined to a new channel
						theApp.Commands_.SecureHereQ8(ptrChInfo->name);

						CHANNEL_INFO::setActiveChannel(ptrChInfo.get());
					}
					else
					{
						if(ptrChInfo->addMember(ptrUserInfo.get()))
						{
							if(!ptrChInfo->name.empty() && !ptrChInfo->topic.empty())
								theApp.Commands_.ReplySecureTopicQ2(ptrChInfo->name, ptrChInfo.get(), ptrChInfo->topic.c_str(), ptrUserInfo.get());
						}
					}
				}
			}
		}
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	return true;
}

bool ProcessorMsgX::processReplySecureJoinQ6(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	/*'Q' '6' To h00 Channel h00 Result Signature SignatureSize*/
	MSG_FIELD fieldsQ6[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsQ6, _ARRAYSIZE(fieldsQ6));
	if(result<1) return false;

	wchar_t* const& to = fieldsQ6[0].data.wsz_;
	wchar_t* const& channel = fieldsQ6[1].data.wsz_;
	const char& res = fieldsQ6[2].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"ReplySecureJoin");
		consoleio::print_line(wszDbgTo, to);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgResult, res);
	}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	std::shared_ptr<USER_INFO> ptrUserInfo;
	if(!checkMessage(to, NULL, channel, ptrUserInfo, ptrChInfo))
		return true;

	if(!ptrChInfo->secured) return true;

	USER_INFO::ConstIteratorOfUsers it = USER_INFO::findUsersByReceiver(USER_INFO::SetOfUsers_.begin(), pcrcvr);
	if(it == USER_INFO::SetOfUsers_.end())
	{
		consoleio::print_line( wszDbgNotInList);
		return true;
	}

	bool bSignature = false;

	if(result<len)
	{
		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
	
		if((sig_result == sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);
			const unsigned char* Signature = (const unsigned char*)(seek + len);

			size_t cMessageLen = len+1;//len-- at 2d row

			while(it != USER_INFO::SetOfUsers_.end())
			{
				const USER_INFO* pUserInfo = (*it).get();

				if(debug_) consoleio::print_line( wszDbgFromByIp, pUserInfo->getNick());

				if(pUserInfo == theApp.ptrMe_.get()) continue;

				bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, pUserInfo->pub_key, pUserInfo->pub_key_size);

				if(bSignature)
				{
					if(res == 0x01)
					{
						ptrChInfo->addMember(pUserInfo);
						break;
					}
					else
					{
						consoleio::print_line(wszWrongChannelPassword);
						//theApp.Commands_.SecureLeaveQ7(channel);
					}
				}

				it = USER_INFO::findUsersByReceiver(it, pcrcvr);
			}
		}
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	return true;
}

bool ProcessorMsgX::processSecureLeaveQ7(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	/*'Q' '7' From h00 Channel h00 Gender Signature SignatureSize*/
	MSG_FIELD fieldsQ7[3] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false},{CHAR_FIELD,1,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsQ7, _ARRAYSIZE(fieldsQ7));
	if(result<1) return false;

	wchar_t* const& from = fieldsQ7[0].data.wsz_;
	wchar_t* const& channel = fieldsQ7[1].data.wsz_;
	const char& gender = fieldsQ7[2].data.ch_;

	if(debug_)
	{
		consoleio::print_line_selected(L"SecureLeave");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChannel, channel);
		consoleio::print_line(wszDbgGender, gender);
	}

	std::shared_ptr<USER_INFO> ptrUserInfo;
	if(!USER_INFO::isUserInList(from, ptrUserInfo))
	{
		if(debug_) consoleio::print_line(wszDbgNotInList);
		return true;
	}

	bool bSignature = false;

	if(result<len)
	{
		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
	
		if((sig_result == sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);
			const unsigned char* Signature = (const unsigned char*)(seek + len);

			size_t cMessageLen = len+1;//len-- at 2d row

			bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);
		}
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	if (!bSignature) return true;

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(CHANNEL_INFO::isMyChannel(channel, ptrChInfo) && ptrChInfo && ptrChInfo->secured)
	{
		if (ptrChInfo->removeMember(ptrUserInfo.get()))
		{
			consoleio::print_line(ptrUserInfo->color, false, wszLeftChannel, theApp.getStrTime(), from, ptrChInfo->name.c_str());

			if (ptrChInfo->users.size() < 1)
				CHANNEL_INFO::SetOfChannels_.erase(ptrChInfo);
		}
	}

	return true;
}

bool ProcessorMsgX::processSecureHereQ8(const char* pmsg, size_t msglen)
{
	const char* seek = pmsg+1;
	size_t len = msglen-1;

	/*'Q' '8' From h00 Channel h00 Signature SignatureSize*/
	MSG_FIELD fieldsQ8[2] = {{STRING_FIELD,0,0,false},{STRING_FIELD,0,0,false}};
	size_t result = parseMessageFields(seek, len, fieldsQ8, _ARRAYSIZE(fieldsQ8));
	if(result<1) return false;

	wchar_t* const& from = fieldsQ8[0].data.wsz_;
	wchar_t* const& channel = fieldsQ8[1].data.wsz_;

	if(debug_)
	{
		consoleio::print_line_selected(L"SecureHere");
		consoleio::print_line(wszDbgFrom, from);
		consoleio::print_line(wszDbgChannel, channel);
	}

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;
	if(!CHANNEL_INFO::isMyChannel(channel, ptrChInfo))
	{
		if(debug_) consoleio::print_line( wszDbgYouNotInChannel);
		return true;
	}

	if(!ptrChInfo->secured) return true;

	//is_user_in_channel - is not necessary

	if (theApp.ptrMe_->cmpNick(from)) return true;

	std::shared_ptr<USER_INFO> ptrUserInfo;
	if(!USER_INFO::isUserInList(from, ptrUserInfo))
	{
		if(debug_) consoleio::print_line( wszDbgNotInList);
		return true;
	}

	bool bSignature = false;

	if(result<len)
	{
		const char* psig_len = seek + len - sizeof(WORD);
		//WORD sig_len = *((WORD*)psig_len);
		MSG_FIELD sig_len_fields[1] = {{NUMBER_FIELD,sizeof(WORD),wszEmptyString,false}};
		size_t sig_result = parseMessageFields(psig_len, sizeof(WORD), sig_len_fields, _ARRAYSIZE(sig_len_fields));
		WORD sig_len = *(WORD*)sig_len_fields[0].data.bytes_;
	
		if((sig_result == sizeof(WORD)) && (len - sizeof(WORD) - sig_len > 0))
		{
			len -= sig_len+sizeof(WORD);
			const unsigned char* Signature = (const unsigned char*)(seek + len);

			size_t cMessageLen = len+1;//len-- at 2d row

			bSignature = verifySignature(pmsg, cMessageLen, Signature, sig_len, ptrUserInfo->pub_key, ptrUserInfo->pub_key_size);

			if(bSignature)
			{
				theApp.Commands_.ReplySecureHereQ4(channel, ptrUserInfo.get(), randomSleep());
			}
		}
	}

	if(debug_)
	{
		if(bSignature)
			consoleio::print_line(wszDbgSigValid);
		else
			consoleio::print_line(wszDbgSigNotValid);
	}

	return true;
}
