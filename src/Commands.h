/*
$Id: Commands.h 36 2011-08-09 07:35:21Z avyatkin $

Interface and configuration of the Commands class and MSG_FIELD struct

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#pragma once

// An enumeration of possible fields types
enum FIELD_TYPE {VOID_FIELD=0,
				BYTES_FIELD,
				CHAR_FIELD,
				NUMBER_FIELD,
				STRING_FIELD,
				LEN_OF_STRING_FIELD,
				SIGNATURE_FIELD,
				QSIGNATURE_FIELD,
				SIGNATURE_LEN_FIELD};

/**
A MSG_FIELD struct describes an object that stores information about one field of a message
*/
struct MSG_FIELD
{
	FIELD_TYPE type;
	size_t size;

	union TYPED_DATA
	{
		const wchar_t* cwsz_;
		wchar_t* wsz_;
		char ch_;
		unsigned char* bytes_;
	} data;

	bool delete_;

	~MSG_FIELD()
	{
		if(!delete_) return;

		switch(type)
		{
		case NUMBER_FIELD:
		case SIGNATURE_LEN_FIELD:
		case LEN_OF_STRING_FIELD:
		case SIGNATURE_FIELD:
		case BYTES_FIELD:
			delete[] data.bytes_;
			break;

		case STRING_FIELD:
			delete[] data.wsz_;
			break;

		default:
			break;
		}
	}
};

/**
A Commands class describes an object that creates chat commands and sends them over the network
*/
class Commands
{
#ifdef CHATTERM_OS_WINDOWS
	class DatagramIdMonitor
	{
	public:
		explicit DatagramIdMonitor()
		{
			switch(WaitForSingleObject(m_, INFINITE))
			{
			// The thread got ownership of the mutex
			case WAIT_OBJECT_0: 
				break;
			// The thread got ownership of an abandoned mutex
			case WAIT_ABANDONED:
				break;
			}
		}

		~DatagramIdMonitor()
		{
			ReleaseMutex(m_);
		}

	private:
		DatagramIdMonitor(const DatagramIdMonitor&);
		DatagramIdMonitor& operator=(const DatagramIdMonitor&);
		DatagramIdMonitor* operator&();

		static void Initialize()
		{
			m_ = CreateMutex(NULL,FALSE,NULL);
		}

		static void Delete()
		{
			CloseHandle(m_);
			m_ = NULL;
		}

		static HANDLE m_;

		friend class Commands;
	};

	class DelayedMsgsMonitor
	{
	public:
		explicit DelayedMsgsMonitor()
		{
			switch(WaitForSingleObject(m_, INFINITE))
			{
			// The thread got ownership of the mutex
			case WAIT_OBJECT_0: 
				break;
			// The thread got ownership of an abandoned mutex
			case WAIT_ABANDONED:
				break;
			}
		}

		~DelayedMsgsMonitor()
		{
			ReleaseMutex(m_);
		}

	private:
		DelayedMsgsMonitor(const DelayedMsgsMonitor&);
		DelayedMsgsMonitor& operator=(const DelayedMsgsMonitor&);
		DelayedMsgsMonitor* operator&();

		static void Initialize()
		{
			m_ = CreateMutex(NULL,FALSE,NULL);
		}

		static void Delete()
		{
			CloseHandle(m_);
			m_ = NULL;
		}

		static HANDLE m_;

		friend class Commands;
	};
#else
	// Controls access to the datagramId_
	class DatagramIdMonitor
	{
	public:
		DatagramIdMonitor()
		{
			pthread_mutex_lock( &m_ );
		}
		
		~DatagramIdMonitor()
		{
			pthread_mutex_unlock( &m_ );
		}
	
	private:
		DatagramIdMonitor(const DatagramIdMonitor&);
		DatagramIdMonitor& operator=(const DatagramIdMonitor&);
		DatagramIdMonitor* operator&();

		static void Initialize() {}
	
		static void Delete() {}

		static pthread_mutex_t m_;

		friend class Commands;
	};
	
	class DelayedMsgsMonitor
	{
	public:
		DelayedMsgsMonitor()
		{
			pthread_mutex_lock( &m_ );
		}
		
		~DelayedMsgsMonitor()
		{
			pthread_mutex_unlock( &m_ );
		}
	
	private:
		DelayedMsgsMonitor(const DelayedMsgsMonitor&);
		DelayedMsgsMonitor& operator=(const DelayedMsgsMonitor&);
		DelayedMsgsMonitor* operator&();

		static void Initialize() {}
	
		static void Delete() {}

		static pthread_mutex_t m_;

		friend class Commands;
	};
#endif // CHATTERM_OS_WINDOWS

	/**
	A DELAYED_MSG_DATA structure describes an object that stores information
	about a delayed message (usually a reply)*/
	struct DELAYED_MSG_DATA
	{
		DELAYED_MSG_DATA(const char* buf, int len, const USER_INFO* pinfo, int delay)
			:
#ifdef CHATTERM_OS_WINDOWS
			when_(GetTickCount()+delay),
#endif // CHATTERM_OS_WINDOWS
			what_(0),
			what_len_(len),
			by_(pinfo->naddr_info.preceiver_->getSender())
		{
#ifndef CHATTERM_OS_WINDOWS
			gettimeofday(&when_, NULL);
			when_.tv_usec+=delay*1000;
			if(when_.tv_usec>=1000000)
			{
				when_.tv_usec -= 1000000;
				when_.tv_sec+=1;
			}
			
#endif // CHATTERM_OS_WINDOWS		
			what_=new char[what_len_];
			memcpy(what_, buf, what_len_);
			memcpy(&to_, pinfo->naddr_info.psaddr_, sizeof(sockaddr_in6));
		}

		~DELAYED_MSG_DATA()
		{
			delete[] what_;
		}

		//Time when to send the message
#ifdef CHATTERM_OS_WINDOWS
		DWORD when_;
#else
		struct timeval when_;
#endif // CHATTERM_OS_WINDOWS
		//Pointer to data to be sent
		char* what_;

		//Size of data
		int what_len_;

		//Inet address where to send the message
		sockaddr_in6 to_;

		//Sender that have to used to send the message
		const networkio::Sender* by_;
	};

public:
	Commands();
	~Commands(void);

	/**
	These functions creates related commands and sends generated packed over the UDP protocol
	@channel - name of channel
	@pcchinfo - pointer to an object which describes a channel with name @channel
	@text - text of a message
	@fMe - if true the message is an activity (/me) message
	@nick - new nick name
	@topic - new channel topic
	@to - name os a user who is message for
	@fPong - if true the message is a Pong (not Ping) message
	@pinfo - const pointer to an object describes a user who is message for
	@nsecs - number of seconds for Flood message
	@datagramId - Id of a packet that is reply for
	@hKey - symmetric encryption key descriptor for secured messages
	@delay - delay in miliseconds for sending the command
	@return - 0 if successful, error code otherwise
	*/
	int List0();
	int ChannelMsg2A(const wchar_t* channel, const wchar_t* text, bool fMe);
	int NickName3(const wchar_t* nick);
	int Join4(const wchar_t* channel);
	int Leave5(const wchar_t* channel);
	int Leave5(const CHANNEL_INFO* pcchinfo);
	int NewTopicB(const wchar_t* channel, const wchar_t* topic);
	int MassTextMsgE(const wchar_t* text);
	int MassTextMsgToE(const wchar_t* to, const wchar_t* text);
	int InfoF(const wchar_t* to);
	int BeepH(const wchar_t* to);
	int HereL(const wchar_t* channel);
	int ChannelsN(const wchar_t* to);
	int PingPongP(const wchar_t* to, bool fPong);
	int PingPongP(const USER_INFO* pinfo, bool fPong);
	int FloodZ(const wchar_t* to, int nsecs);
	int FloodZ(const USER_INFO* pinfo, int nsecs);

	int ReplyList1(const USER_INFO* pinfo, int delay=0);
	int ReplyConfirmMassTextMsg7(const wchar_t* datagramId, const USER_INFO* pinfo, int delay=0);
	int ReplyTopicC(const wchar_t* channel, const wchar_t* topic, const USER_INFO* pinfo, int delay=0);
	int ReplyInfoG(const USER_INFO* pinfo);
	int ReplyConfirmBeepH(const USER_INFO* pinfo);
	int ReplyHereK(const wchar_t* channel, const USER_INFO* pinfo, int delay=0);
	int ReplyChannelsO(const USER_INFO* pinfo);

	int SecureChannelMsgQ01(const wchar_t* channel, const CHANNEL_INFO* pcchinfo, const wchar_t* text, bool fMe);
	int SecureNewTopicQ3(const wchar_t* channel, const CHANNEL_INFO* pcchinfo, const wchar_t* topic);

	int SecureJoinQ5(const wchar_t* channel, const wchar_t* passwd);
	int SecureLeaveQ7(const wchar_t* channel);
	int SecureLeaveQ7(const CHANNEL_INFO* pcchinfo);
	int SecureHereQ8(const wchar_t* channel);

	int ReplySecureHereQ4(const wchar_t* channel, const USER_INFO* pinfo, int delay=0);

	int ReplySecureTopicQ2(const wchar_t* channel, const CHANNEL_INFO* pcchinfo, const wchar_t* topic, const USER_INFO* pinfo);

	int ReplySecureJoinQ6(const wchar_t* channel, char result, const USER_INFO* pinfo, int delay=0);

	/**
	Checks queue of delayed messages and send that are on time
	*/
	void sendDelayedMsgs();

	/**
	Sends a command (request, response, or notification)
	to all users using broadcast and/or multicast addresses
	@buf - buffer which contains the message
	@len - length of the buffer
	@return - number of bytes that were successfully sent
	*/
	int sendBroadcastMsg(const char* buf, int len);

	//Addresses to which broadcast or multicast messages are sent
	//We do not use std::auto_ptr<> because pointers to Sender are temporary shared
	//between several containers (mapIdIf, mapIdSender, mapIdIfSender) in ChatTerminalApp::initNetConfigFromXml()
	//We could to use here a shared pointer or a pointer with reference counting
	//but there are no such safe pointers in the C++ STL
	std::vector< networkio::DESTADDR_INFO* > Destinations_;

	// queue of delayed messages
	std::vector<DELAYED_MSG_DATA*> delayedMsgs_;

	// Debugging flag; If true then detailed information about commands displayed
	static bool debug_;

private:

	/**
	Builds a message packet according to pFields array
	@pMessage - reference to a message buffer, returned buffer must be freed by delete[] operator
	@chType - type of the message
	@pFields - array of fields that describes the message structure
	@nFields - number of fields in the pFields array
	@return - number of bytes copied to the message buffer, it is a size of the created message
	*/
	int createMessageFields(char*& pMessage, char chType, MSG_FIELD* pFields, int nFields);

#ifndef CHATTERM_OS_WINDOWS
	// Big endian flag; True on big endian processors
	bool fBe_;
#endif
	// Id of a command message
	volatile unsigned int datagramId_;

	/**
	Sends a command (request, response, or notification) to specified user
	@buf - buffer which contains the message
	@len - length of the buffer
	@pinfo - const pointer to an object describes the user who is message for
	@delay - delay in miliseconds for sending the command
	@return - number of bytes that were successfully sent
	*/
	int sendMsgTo(const char* buf, int len, const USER_INFO* pinfo, int delay=0);

	/**
	Creates an MD5 digital signature of a message
	@pMessage - source message
	@cMessageLen - length of the message
	@pSignatute - pointer to a buffer for created signature
	@cSignatureLen - size of the buffer
	@return - number of bytes copied to the buffer
	*/
	static int signMessage(const char* pMessage, size_t cMessageLen, unsigned char* pSignatute, size_t cSignatureLen);

	/**
	Converts a byte color (16 colors) to DWORD (RGB) color
	@color - byte color to convert
	@return - an RGB color
	*/
	static DWORD byteToDwordColor(unsigned char color);

	/**
	Finds a user object by his name, displays messages in case of error
	@to - user name
	@pinfo - pointer to returned pointer to the user object
	@return - true if successful, false otherwise
	*/
	static bool getToInfo(const wchar_t* to, const USER_INFO** ppinfo);

	/**
	Encrypts fields in a secured channels commands
	@data - a pointer to a buffer that contains the plaintext to be encrypted.
	        The plaintext in this buffer is overwritten with the ciphertext created by this function.
	@size - size of the data
	@hKey - descriptor of a encryption symmetric key
	@return - true if successful, false otherwise
	*/
	static bool encryptData(unsigned char* data, size_t size, const CHANNEL_INFO* pcchinfo);

	/**
	Creates an MD5 hash and derived 40 bits symmetric encryption key from provided password for secured channels
	@pass - password of a secured channel
	@len - length of the password
	@hash - pointer to a buffer for created hash
	@hash_len - size of the hash buffer
	@phKey - pointer to the descriptor of the created derived encryption key
	@return - true if successful, false otherwise
	*/
#ifdef CHATTERM_OS_WINDOWS
	static bool getPasswordHash(const wchar_t* pass, size_t len, unsigned char* hash, unsigned long hash_len, HCRYPTKEY* phKey);
#else
	static bool getPasswordHash(const wchar_t* pass, size_t len, unsigned char* hash, unsigned long hash_len);
#endif // CHATTERM_OS_WINDOWS

	/**
	Sends a command (request, response, or notification) to specified network address
	using specified Sender object
	@buf - buffer which contains the message
	@len - length of the buffer
	@paddr - const pointer to the network address
	@psndr - const pointer to the Sender object which must be used to send the message
	@return - number of bytes that were successfully sent
	*/
	static int sendMsgToAddr(const char* buf, int len, const sockaddr* paddr, const networkio::Sender* psndr);

	// Maximum message text length
	static const size_t MaxMsgLength = 960;

	// Maximum mass message textlength
	static const size_t MaxMassMsgLength = 960;

#ifdef GTEST_PROJECT
	friend class MessagesXTest_Parsing_Test;
	friend class MessagesXTest_Creation_Test;
	friend class MessagesXTest_Colors_Test;
	friend class MessagesXTest;
#endif
};
