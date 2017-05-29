/*
$Id: StrResources.h 32 2010-09-03 18:17:36Z avyatkin $

String resources declarations

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#pragma once

namespace resources
{
	extern unsigned char pNullBytes[64];//!important
	extern const wchar_t wszEmptyString[64];//!important

	extern const wchar_t wchInvite;

	extern const wchar_t wszWelcome[];
	extern const wchar_t wszDefaultNick[17];

	extern const wchar_t wszDefaultNetIf[];
	extern const wchar_t wszUserDropped[];
	extern const wchar_t wszLocalAddrsChanged[];
	extern const wchar_t wszLeavingNet[];
	extern const wchar_t wszInitializingNet[];
	extern const wchar_t wszReinitializedNet[];

	extern const wchar_t wszNickNameLimit[];
	extern const wchar_t wszChannelNameLimit[];
	extern const wchar_t wszTopicNameLimit[];
	extern const wchar_t wszLineMsgLimit[];
	extern const wchar_t wszMassMsgLimit[];

	//Command line parsing
	extern const wchar_t wszVersionInfo[];
	extern const wchar_t wszUsageText[];
	extern const wchar_t wszAccessDenied[];
	extern const wchar_t wszFileNotFound[];
	extern const wchar_t wszFileInvalidParam[];
	extern const wchar_t wszIncorrectColor[];
	extern const wchar_t wszNoColor[];
	extern const wchar_t wszUnknownParam[];

	//Information about a user
	extern const wchar_t wszGender[];
	extern const wchar_t wszUuid[];
	extern const wchar_t wszFullName[];
	extern const wchar_t wszUserName[];
	extern const wchar_t wszComputerName[];
	extern const wchar_t wszDomainName[];
	extern const wchar_t wszOS[];
	extern const wchar_t wszChatSoftware[];
	extern const wchar_t wszJob[];
	extern const wchar_t wszDepartment[];
	extern const wchar_t wszWorkPhone[];
	extern const wchar_t wszMobilePhone[];
	extern const wchar_t wszWebAddr[];
	extern const wchar_t wszEmailAddr[];
	extern const wchar_t wszPostAddr[];

	//Console Input/Output strings
	extern const wchar_t wszOn[];
	extern const wchar_t wszOff[];
	extern const wchar_t wszMan[];
	extern const wchar_t wszWoman[];

	extern const wchar_t wszEchoOn[];
	extern const wchar_t wszEchoOff[];
	extern const wchar_t wszWaitingFor[];
	extern const wchar_t wszWaitingFinished[];
	extern const wchar_t wszNoTimeout[];

	extern const wchar_t wszNoPassword[];
	extern const wchar_t wszNoUsersInList[];
	extern const wchar_t wszNoChannelUsers[];
	extern const wchar_t wszYouNotJoinedToChannels[];

	extern const wchar_t wszNoChannel[];
	extern const wchar_t wszYouNotJoinedToChannel[];

	extern const wchar_t wszNoAddTopic[];
	extern const wchar_t wszNoTopic[];

	extern const wchar_t wszUnknownCmd[];

	//Commands string
	extern const wchar_t wszNoChannels[];
	extern const wchar_t wszYouNotInChannel[];
	extern const wchar_t wszNoUsers[];
	extern const wchar_t wszIncorrectTimeOut[];
	extern const wchar_t wszNoNewNick[];
	extern const wchar_t wszNoNewTopic[];
	extern const wchar_t wszNoMsgText[];

	extern const wchar_t wszRecipientBlocked[];
	extern const wchar_t wszRecipientNotInList[];
	extern const wchar_t wszNoRecipient[];

	extern const wchar_t wszNoBlockYourself[];
	extern const wchar_t wszUnableToSendMsg[];
	extern const wchar_t wszFloodProtection[];

	//Process commands string
	extern const wchar_t wszStatusUnknown[];
	extern const wchar_t wszStatusNormal[];
	extern const wchar_t wszStatusDnd[];
	extern const wchar_t wszStatusAway[];
	extern const wchar_t wszStatusOff[];

	extern const wchar_t wszInChannelMeMsg[];
	extern const wchar_t wszInChannelMsg[];
	extern const wchar_t wszNotInChannelMeMsg[];
	extern const wchar_t wszNotInChannelMsg[];

	extern const wchar_t wszHerNickChanged[];
	extern const wchar_t wszHisNickChanged[];

	extern const wchar_t wszHerStatus[];
	extern const wchar_t wszHisStatus[];

	extern const wchar_t wszJoinedToChannel[];
	extern const wchar_t wszLeftChannel[];

	extern const wchar_t wszMassMsg[];
	extern const wchar_t wszReceivedMassMsg[];

	extern const wchar_t wszNewTopic[];
	extern const wchar_t wszTopic[];
	extern const wchar_t wszChannelNewTopic[];
	extern const wchar_t wszChannelTopic[];

	extern const wchar_t wszFloodNotify[];
	extern const wchar_t wszRequireToChangeNick[];

	extern const wchar_t wszUserChannels[];
	extern const wchar_t wszBeepConfirmed[];
	extern const wchar_t wszInfoAbout[];

	extern const wchar_t wszUnableDecryptFrom[];
	extern const wchar_t wszUnableDecrypt[];

	extern const wchar_t wszWrongChannelPassword[];

	//Error strings
	extern const wchar_t wszErrCsbi[];
	extern const wchar_t wszErrInputStream[];
	extern const wchar_t wszErrOutputStream[];
	extern const wchar_t wszErrSetMode[];

	extern const wchar_t wszErrWsa[];
	extern const wchar_t wszErrNotBindSender[];
	extern const wchar_t wszErrNotBindRcvr[];
	extern const wchar_t wszErrNotBindDestAddr[];
	extern const wchar_t wszErrRecvFrom[];

	extern const wchar_t wszErrKeyPair[];
	extern const wchar_t wszErrNoNetIfs[];
	extern const wchar_t wszErrReceiverXml[];
	extern const wchar_t wszErrReceiverXmlUtf8[];
	extern const wchar_t wszErrDestinationXml[];
	extern const wchar_t wszErrDestinationXmlUtf8[];
	extern const wchar_t wszErrCsp[];
	extern const wchar_t wszErrInitScriptFile[];

	extern const wchar_t wszErrLoadXmlFile[];

	//Debug commands string
	extern const wchar_t wszDbgNetIfs[];
	extern const wchar_t wszDbgPacket[];
	extern const wchar_t wszDbgPacketSentToFrom6[];
	extern const wchar_t wszDbgPacketSentToFrom[];
	extern const wchar_t wszDbgPacketRcvdFromTo6[];
	extern const wchar_t wszDbgPacketRcvdFromTo[];
	extern const wchar_t wszDbgPacketDuplicated[];

	extern const wchar_t wszDbgTo[];
	extern const wchar_t wszDbgFrom[];
	extern const wchar_t wszDbgGender[];
	extern const wchar_t wszDbgChannel[];
	extern const wchar_t wszDbgStatus[];
	extern const wchar_t wszDbgWndState[];
	extern const wchar_t wszDbgChState[];
	extern const wchar_t wszDbgCP[];
	extern const wchar_t wszDbgVer[];
	extern const wchar_t wszDbgColor[];
	extern const wchar_t wszDbgIcon[];
	extern const wchar_t wszDbgLicenseId[];
	extern const wchar_t wszDbgMessage[];
	extern const wchar_t wszDbgMessageLen[];
	extern const wchar_t wszDbgNewNick[];
	extern const wchar_t wszDbgCurrentAA[];
	extern const wchar_t wszDbgDatagramID[];
	extern const wchar_t wszDbgTopic[];
	extern const wchar_t wszDbgTopicLen[];
	extern const wchar_t wszDbgFromByIp[];
	extern const wchar_t wszDbgIpAddrs[];
	extern const wchar_t wszDbgListOfChannels[];
	extern const wchar_t wszDbgParam[];
	extern const wchar_t wszDbgResult[];
	extern const wchar_t wszDbgTime[];
	extern const wchar_t wszDbgSeconds[];
	extern const wchar_t wszDbgHash[16];

	extern const wchar_t wszDbgPingedsuccessfuly[];

	extern const wchar_t wszDbgSecuredTextEmpty[];
	extern const wchar_t wszDbgNotInList[];
	extern const wchar_t wszDbgYouNotRecipient[];
	extern const wchar_t wszDbgYouNotInChannel[];
	extern const wchar_t wszDbgSenderNotInChannel[];

	extern const wchar_t wszDbgSigValid[];
	extern const wchar_t wszDbgSigNotValid[];

	extern const wchar_t wszHelp[];
}
