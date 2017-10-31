/*
$Id: ProcessorMsgX.h 36 2011-08-09 07:35:21Z avyatkin $

Interface and configuration of the ProcessorMsgX class

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#pragma once

/**
Class describes an object that processes UDP commands
*/
class ProcessorMsgX
{
public:
	explicit ProcessorMsgX();
	~ProcessorMsgX(void);

	/**
	Processes packets using one of appropriate private functions process...
	@pmsg - pointer to a message buffer
	@msglen - message size in bytes
	@pcrcvr - receiver which trough a message was received
	@packet_id - 9 bytes length packet signature
	@return true if the message was processed successfully
	*/
	bool process(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr, const char* packet_id);

	// Debugging flag; If true then detailed information about processed messages displayed
	static bool debug_;

private:

#ifndef CHATTERM_OS_WINDOWS
	// Big endian flag; True on big endian processors
	bool fBe_;
#endif

	/**
	Parses a message packet according to pFields array
	@pMessage - const pointer to a message buffer
	@len - size of the message packet
	@pFields - array of fields that describes the message structure
	@nFields - number of fields in the pFields array
	@return - number of successfully parsed bytes
	*/
	size_t parseMessageFields(const char* pMessage, size_t len, MSG_FIELD* pFields, int nFields);

	/**
	Veryfies an MD5 digital signature of a message
	@pMessage - source message
	@cMessageLen - length of the message
	@pSignatute - pointer to the signature buffer
	@cSignatureLen - size of the buffer
	@pPubKey - pointer to a signer's public key
	@cPubLen - size of the signer's public key
	@return - number of bytes copied to the buffer
	*/
	bool verifySignature(const char* pMessage, size_t cMessageLen, const unsigned char* pSignatute, size_t cSignatureLen, const unsigned char* pPubKey, size_t cPubLen);

	/**
	These functions process related messages that is easily determined from a function name
	These functions must be called in a locked ContainersMonitor critical section
	@pmsg - pointer to a message buffer
	@msglen - message size in bytes
	@pcrcvr - receiver which trough a message was received
	@fMe - if true a messages is ChannelMeMsg (/me command) else ChannelMsg
	@fNewTopic if true a message is SecureTopic else SecureNewTopic
	@packet_id - 9 bytes length packet signature
	@return true if the message was processed successfully
	*/
	bool processList0(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processReplyList1(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processChannelMsg2A(const char* pmsg, size_t msglen, bool fMe);
	bool processNickName3(const char* pmsg, size_t msglen);
	bool processJoin4(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processLeave5(const char* pmsg, size_t msglen);
	bool processMassTextMsgConfirm7(const char* pmsg, size_t msglen);
	bool processNewTopicB(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processTopicC(const char* pmsg, size_t msglen);
	bool processNewStatusD(const char* pmsg, size_t msglen);
	bool processMassTextMsgE(const char* pmsg, size_t msglen, const char* packet_id);
	bool processInfoF(const char* pmsg, size_t msglen);
	bool processReplyInfoG(const char* pmsg, size_t msglen);
	bool processBeepH(const char* pmsg, size_t msglen);
	bool processReplyHereK(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processHereL(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processWndStateM(const char* pmsg, size_t msglen);
	bool processChannelsN(const char* pmsg, size_t msglen);
	bool processReplyChannelsO(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processPingPongP(const char* pmsg, size_t msglen);
	bool processChangeNickNameU(const char* pmsg, size_t msglen);
	bool processFloodZ(const char* pmsg, size_t msglen);

	bool processSecureChannelMsgQ01(const char* pmsg, size_t msglen, bool fMe);
	bool processSecureTopicMsgQ23(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr, bool fNewTopic);
	bool processReplySecureHereQ4(const char* pmsg, size_t msglen);
	bool processSecureJoinQ5(const char* pmsg, size_t msglen);
	bool processReplySecureJoinQ6(const char* pmsg, size_t msglen, const networkio::Receiver* pcrcvr);
	bool processSecureLeaveQ7(const char* pmsg, size_t msglen);
	bool processSecureHereQ8(const char* pmsg, size_t msglen);

	/**
	return random time interval or
	when redirected console input is used
	sleeps thread for a random time interval (between 0-500ms)
	*/
	int randomSleep();

	/**
	Converts an RGB color to the nearest console specific byte color (16 colors)
	@dwColor - the RGB color to convert
	@return - byte color
	*/
	unsigned char dwordToByteColor(DWORD dwColor);

	/**
	Returns a pointer to a static string that describes a status
	@chMode - char mode '0','1','2','3' etc.
	@return - pointer to a static buffer
	*/
	const wchar_t* getMode(char chMode);

	/**
	Converts inplace any possible line breaks to traditional L'\n'
	Vypress Chat uses \r (without followed \n) line break
	@pwszStr - pointer to the NULL terminated string
	@return new length of the string
	*/
	size_t convertLineBreaks(wchar_t* pwszStr);

	/**
	Decrypts fields in a secured channels messages
	@data - a pointer to a buffer that contains the data to be decrypted;
	        After the decryption has been performed, the plaintext is placed back into this same buffer
	@size - size of the data
	@hKey - descriptor of a encryption symmetric key
	@return - true if successful, false otherwise
	*/
	bool decryptData(unsigned char* data, int size, const CHANNEL_INFO* pcchinfo);

	/**Checks a message and return user object of a sender of the message and channel info object if possible
	@to - user name who is message for
	@from - user name who is message from
	@channel - channel name which is message in
	@ppUserInfo - returned pointer to an object which describes a user with name @from
	@ppchinfo - returned pointer to an object which describes a channel with name @channel
	@return true if a message is intended for you and from a user from your network and channel
	*/
	bool checkMessage(const wchar_t* to, const wchar_t* from, const wchar_t* channel, std::shared_ptr<USER_INFO>& ptrUserInfo, std::shared_ptr<CHANNEL_INFO>& ptrChInfo);

#ifdef GTEST_PROJECT
	friend class LineBreaks_Parsing_Test;
	friend class MessagesTest_Parsing_Test;
	friend class MessagesXTest_Creation_Test;
	friend class MessagesXTest_Colors_Test;
	friend class MessagesXTest;
#endif
};
