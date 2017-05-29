/*
$Id: StrResources.cpp 36 2011-08-09 07:35:21Z avyatkin $

String resources definitions

Copyright (c) 2011 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include "StrResources.h"

namespace resources
{
	unsigned char pNullBytes[64] = {0};
	const wchar_t wszEmptyString[64] = {0};

	const wchar_t wchInvite = L'>';

	const wchar_t wszWelcome[] = L"Welcome to the Chat Terminal, %ls!";
	const wchar_t wszDefaultNick[17] = L"ChatTerminalUser";

	const wchar_t wszDefaultNetIf[] = L"Default network interface %ls is used, multicast address is %ls";
	const wchar_t wszUserDropped[] = L"[%ls] %ls (%ls) has been dropped from the network";
	const wchar_t wszLocalAddrsChanged[] = L"The list of local transport addresses was changed";
	const wchar_t wszLeavingNet[] = L"Leaving the network...";
	const wchar_t wszInitializingNet[] = L"Initializing network configuration...";
	const wchar_t wszReinitializedNet[] = L"Network configuration has been reinitialized successfully";

	const wchar_t wszNickNameLimit[] = L"Nick name length is limited to %d characters";
	const wchar_t wszChannelNameLimit[] = L"Channel name length is limited to %d characters";
	const wchar_t wszTopicNameLimit[] = L"Channel topic length is limited to %d characters";
	const wchar_t wszLineMsgLimit[] = L"Message length is limited to %d characters";
	const wchar_t wszMassMsgLimit[] = L"Mass message length is limited to %d characters";

	//Command line parsing
	const wchar_t wszVersionInfo[] = L"Chat Terminal 1.0";
	const wchar_t wszUsageText[] = L"Command-line utility to communicate with Vypress Chat\n\
The syntax of this command is:\n\
\n\
ChatTerm [options]\n\
\n\
Options:\n\
	/?\n\
	/help\n\
	--help        display usage text\n\
	/c {filename} path to configuration file\n\
	/d[N]         verbose level\n\
	/i {filename} path to initialization script file\n\
	/u {filename} path to personal user information configuration file\n\
	/v\n\
	--version     display version info";

	const wchar_t wszAccessDenied[] = L"Access denied: the file's permission setting does not allow specified access";
	const wchar_t wszFileNotFound[] = L"File name or path not found";
	const wchar_t wszFileInvalidParam[] = L"Invalid parameter";
	const wchar_t wszUnknownParam[] = L"Unknown parameter - ";

	//Information about a user
	const wchar_t wszGender[] = L"Gender: %ls";
	const wchar_t wszUuid[] = L"UUID: %ls";
	const wchar_t wszFullName[] = L"Full name: %ls";
	const wchar_t wszUserName[] = L"User name: %ls";
	const wchar_t wszComputerName[] = L"Computer name: %ls";
	const wchar_t wszDomainName[] = L"Domain name: %ls";
	const wchar_t wszOS[] = L"OS: %ls";
	const wchar_t wszChatSoftware[] = L"Chat software: %ls";
	const wchar_t wszJob[] = L"Job: %ls";
	const wchar_t wszDepartment[] = L"Department: %ls";
	const wchar_t wszWorkPhone[] = L"Work phone number: %ls";
	const wchar_t wszMobilePhone[] = L"Mobile phone number: %ls";
	const wchar_t wszWebAddr[] = L"Web address: %ls";
	const wchar_t wszEmailAddr[] = L"E-mail address: %ls";
	const wchar_t wszPostAddr[] = L"Post address: %ls";

	//Console Input/Output strings
	const wchar_t wszOn[] = L"on";
	const wchar_t wszOff[] = L"off";
	const wchar_t wszMan[] = L"Male";
	const wchar_t wszWoman[] = L"Female";

	const wchar_t wszEchoOn[] = L"Echo on";
	const wchar_t wszEchoOff[] = L"Echo off";
	const wchar_t wszWaitingFor[] = L"Waiting for %d seconds...";
	const wchar_t wszWaitingFinished[] = L"The time-out interval elapsed";
	const wchar_t wszNoTimeout[] = L"Timeout is not specified";

	const wchar_t wszNoPassword[] = L"Password is not specified";
	const wchar_t wszNoUsersInList[] = L"List of users is empty";
	const wchar_t wszNoChannelUsers[] = L"There are no users joined to the channel";
	const wchar_t wszYouNotJoinedToChannels[] = L"You are NOT joined to any channels";

	const wchar_t wszNoChannel[] = L"Channel is not specified";
	const wchar_t wszYouNotJoinedToChannel[] = L"You are NOT joined to channel %ls";

	const wchar_t wszNoAddTopic[] = L"Additional topic is not specified";
	const wchar_t wszNoTopic[] = L"Topic is not specified";

	const wchar_t wszIncorrectColor[] = L"Incorrect color was specified";
	const wchar_t wszNoColor[] = L"Color is not specified";

	const wchar_t wszUnknownCmd[] = L"Unknown command";

	//Commands string
	const wchar_t wszNoChannels[] = L"There are no channels in the network";
	const wchar_t wszYouNotInChannel[] = L"You are NOT joined to channel '%ls'";
	const wchar_t wszNoUsers[] = L"There are no users in chat";
	const wchar_t wszIncorrectTimeOut[] = L"Incorrect timeout value specified";
	const wchar_t wszNoNewNick[] = L"New nick name is not specified";
	const wchar_t wszNoNewTopic[] = L"New topic is not specified";
	const wchar_t wszNoMsgText[] = L"Message text is not specified";

	const wchar_t wszRecipientBlocked[] = L"Recipient is temporary blocked";
	const wchar_t wszRecipientNotInList[] = L"Recipient is not in the list of users";
	const wchar_t wszNoRecipient[] = L"Recipient is not specified";

	const wchar_t wszNoBlockYourself[] = L"It is impossible to block yourself";
	const wchar_t wszUnableToSendMsg[] = L"Unable to send a message to %ls";
	const wchar_t wszFloodProtection[] = L"[%ls] [Flood protection] Ignoring %ls's messages for %ls seconds...";

	//Process commands string
	const wchar_t wszStatusUnknown[] = L"Unknown";
	const wchar_t wszStatusNormal[] = L"Normal";
	const wchar_t wszStatusDnd[] = L"DND";
	const wchar_t wszStatusAway[] = L"Away";
	const wchar_t wszStatusOff[] = L"Offline";

	const wchar_t wszInChannelMeMsg[] = L"[%ls] * %ls %ls";
	const wchar_t wszInChannelMsg[] = L"[%ls] <%ls> %ls";
	const wchar_t wszNotInChannelMeMsg[] = L"%ls: [%ls] * %ls %ls";
	const wchar_t wszNotInChannelMsg[] = L"%ls: [%ls] <%ls> %ls";

	const wchar_t wszHerNickChanged[] = L"%ls has changed her nickname to %ls";
	const wchar_t wszHisNickChanged[] = L"%ls has changed his nickname to %ls";

	const wchar_t wszHerStatus[] = L"[%ls] %ls has changed her status to %ls ('%ls')";
	const wchar_t wszHisStatus[] = L"[%ls] %ls has changed his status to %ls ('%ls')";

	const wchar_t wszJoinedToChannel[] = L"[%ls] %ls has joined to %ls channel";
	const wchar_t wszLeftChannel[] = L"[%ls] %ls has left %ls channel";

	const wchar_t wszMassMsg[] = L"[%ls] Message from %ls: '%ls'";
	const wchar_t wszReceivedMassMsg[] = L"%ls received the message in %ls mode";

	const wchar_t wszNewTopic[] = L"[%ls] %ls has set new topic";
	const wchar_t wszTopic[] = L"The channel topic is '%ls'";
	const wchar_t wszChannelNewTopic[] = L"%ls: [%ls] %ls has set new topic";
	const wchar_t wszChannelTopic[] = L"%ls: The channel topic is '%ls'";

	const wchar_t wszFloodNotify[] = L"[%ls] [Flood protection notification] %ls is ignoring our messages for %ls seconds";
	const wchar_t wszRequireToChangeNick[] = L"[%ls] %ls (%ls) requires you to change nickname.";

	const wchar_t wszUserChannels[] = L"[%ls] %ls's channels:";
	const wchar_t wszBeepConfirmed[] = L"[%ls] %ls confirmed beep";
	const wchar_t wszInfoAbout[] = L"[%ls] Information about %ls";

	const wchar_t wszUnableDecryptFrom[] = L"Unable to decrypt a message from %ls on channel %ls";
	const wchar_t wszUnableDecrypt[] = L"Unable to decrypt a message on channel %ls";

	const wchar_t wszWrongChannelPassword[] = L"Secured channel password is wrong; You have to leave the channel";

	//Error strings
	const wchar_t wszErrCsbi[] = L"GetConsoleScreenBufferInfo failed! Error: ";
	const wchar_t wszErrInputStream[] = L"Unable to open the input stream. Error: ";
	const wchar_t wszErrOutputStream[] = L"Unable to open the output stream. Error: ";
	const wchar_t wszErrSetMode[] = L"SetConsoleMode failed! Error: ";

	const wchar_t wszErrWsa[] = L"WSAStartup failed! Error: ";
	const wchar_t wszErrNotBindSender[] = L"Unable to bind sender to %ls.";
	const wchar_t wszErrNotBindRcvr[] = L"Unable to bind receiver to %ls.";
	const wchar_t wszErrNotBindDestAddr[] = L"Unable to bind destination address %ls to a sender.";
	const wchar_t wszErrRecvFrom[] = L"recvfrom failed! Error: %d";

	const wchar_t wszErrKeyPair[] = L"Unable to generate asymmetric keys pair";
	const wchar_t wszErrNoNetIfs[] = L"Unable to find an appropriate network interface for communication over the TCP/IP network";
	const wchar_t wszErrReceiverXml[] = L"Receiver configuration <receiver interface='%ls' bindport='%ls' mcastgroups='%ls' sender='%ls' /> is not valid";
	const wchar_t wszErrReceiverXmlUtf8[] = L"Receiver configuration <receiver interface='%s' bindport='%s' mcastgroups='%s' sender='%s' /> is not valid";
	const wchar_t wszErrDestinationXml[] = L"Destination configuration <destination address='%ls' port='%ls' sender='%ls' /> is not valid";
	const wchar_t wszErrDestinationXmlUtf8[] = L"Destination configuration <destination address='%s' port='%s' sender='%s' /> is not valid";
	const wchar_t wszErrCsp[] = L"Unable to acquire %ls";
	const wchar_t wszErrInitScriptFile[] = L"Unable to open initialization script file %ls, error %d.";

	const wchar_t wszErrLoadXmlFile[] = L"Unable to load the XML file %ls";

	//Debug commands string
	const wchar_t wszDbgNetIfs[] = L"Available network interfaces:";

	const wchar_t wszDbgPacket[] = L"Packet %d bytes at %ls";
	const wchar_t wszDbgPacketSentToFrom6[] = L"sent to %ls from [%ls]:%d";
	const wchar_t wszDbgPacketSentToFrom[] = L"sent to %ls from %ls:%d";
	const wchar_t wszDbgPacketRcvdFromTo6[] = L"received from %ls on [%ls]:%d";
	const wchar_t wszDbgPacketRcvdFromTo[] = L"received from %ls on %ls:%d";
	const wchar_t wszDbgPacketDuplicated[] = L"Duplicated UDP packet was discarded";

	const wchar_t wszDbgTo[] = L"To: %ls";
	const wchar_t wszDbgFrom[] = L"From: %ls";
	const wchar_t wszDbgGender[] = L"Gender: %c";
	const wchar_t wszDbgChannel[] = L"Channel: %ls";
	const wchar_t wszDbgStatus[] = L"Status: %c";
	const wchar_t wszDbgWndState[] = L"Window state: %c";
	const wchar_t wszDbgChState[] = L"Channel state: %c";
	const wchar_t wszDbgCP[] = L"Codepage: %c";
	const wchar_t wszDbgVer[] = L"Version: %08X";
	const wchar_t wszDbgColor[] = L"Color: %08X";
	const wchar_t wszDbgIcon[] = L"Icon: %d";
	const wchar_t wszDbgLicenseId[] = L"License ID: %08X";
	const wchar_t wszDbgMessage[] = L"Message: %ls";
	const wchar_t wszDbgMessageLen[] = L"Message length: %d";
	const wchar_t wszDbgNewNick[] = L"New nick name: %ls";
	const wchar_t wszDbgCurrentAA[] = L"Current auto answer: %ls";
	const wchar_t wszDbgDatagramID[] = L"DatagramID: %ls";
	const wchar_t wszDbgTopic[] = L"Topic: %ls";
	const wchar_t wszDbgTopicLen[] = L"Topic length: %d";
	const wchar_t wszDbgFromByIp[] = L"From by IP: %ls";
	const wchar_t wszDbgIpAddrs[] = L"IP addresses: %ls";
	const wchar_t wszDbgListOfChannels[] = L"List of channels: %ls";
	const wchar_t wszDbgParam[] = L"Parameter: %c";
	const wchar_t wszDbgResult[] = L"Result: %02d";
	const wchar_t wszDbgTime[] = L"CurrentTime: %ls";
	const wchar_t wszDbgSeconds[] = L"Seconds: %ls";
	const wchar_t wszDbgHash[16] = L"Password hash: ";

	const wchar_t wszDbgPingedsuccessfuly[] = L"[%ls] %ls was pinged successfully at %ls";

	const wchar_t wszDbgSecuredTextEmpty[] = L"Text of a secured message is empty";
	const wchar_t wszDbgNotInList[] = L"Sender is NOT in the list of users";
	const wchar_t wszDbgYouNotRecipient[] = L"You are NOT a recipient of the message";
	const wchar_t wszDbgYouNotInChannel[] = L"You are NOT joined to channel";
	const wchar_t wszDbgSenderNotInChannel[] = L"Sender is NOT joined to channel";

	const wchar_t wszDbgSigValid[] = L"Digital signature is valid";
	const wchar_t wszDbgSigNotValid[] = L"Digital signature is NOT valid";

	const wchar_t wszHelp[] = L"Chat Terminal Help\n\
First commands that should to be executed after the program starts\n\
are \"/list\" and \"/join\".\n\
To send a message, type it, and then press Enter.\n\
To edit a command line, use the left and right arrow keys to position\n\
within the line,\n\
and the Backspace and Delete keys to delete characters to the left or\n\
right of the cursor, respectively. To insert text, simply type it.\n\
You can press Enter with the cursor located anywhere on the line to\n\
execute the command.\n\
You can scroll through the history of previously entered commands and\n\
messages using the up and down arrow keys.\n\
This enables you to easily re-enter a command or re-send a message,\n\
either exactly as you previously entered it or after editing.\n\
\n\
Chat Terminal commands:\n\
\"add\"                   - add a current channel topic\n\
\"allchs\", \"allchannels\" - list all channel names that are created in\n\
                          a network\n\
\"allusers\"              - list all users in a network\n\
\"beep NICK\"             - send a beep signal to a user with name NICK\n\
\"cc CHANNEL\"            - set a channel with name CHANNEL as a current channel\n\
\"channels NICK\"         - request a user with name NICK for list of joined\n\
                          channels\n\
\"cls\"                   - clear the screen\n\
\"echo on\", \"echo off\"   - display or don't display entered commands and\n\
                          messages on the standard output.\n\
\"exit\", \"quit\"          - quit the program, you can also use key sequence\n\
                          Ctrl+C to exit the program\n\
\"flood NICK SECONDS\"    - ban a user with name NICK for SECONDS seconds\n\
\"help\"                  - display this information\n\
\"here\"                  - refresh list of users of a current channel\n\
\"here CHANNEL\"          - refresh list of users of a channel with name CHANNEL\n\
\"info NICK\"             - request personal information about a user with\n\
                          name NICK\n\
\"join\"                  - join to '#Main' channel\n\
\"join CHANNEL\"          - join to a channel with name CHANNEL\n\
\"leave\"                 - leave a current channel\n\
\"leave CHANNEL\"         - leave a channel with name CHANNEL\n\
\"list\"                  - list all users in a network\n\
\"mass TEXT\"             - send a mass message with TEXT\n\
\"me\"                    - send an activity message to the current channel\n\
\"msg NICK TEXT\"         - send a mass message with TEXT to one user with\n\
                          name NICK\n\
\"my\"                    - list channels which you are joined to\n\
\"nick NICK\"             - change your nick name to NICK\n\
\"quit\", \"exit\"          - quit the program\n\
\"sjoin PASSWD CHANNEL\"  - join to a secured channel CHANNEL with\n\
                          a password PASSWD\n\
\"topic TOPIC\"           - set current channel's topic to TOPIC\n\
\"wait SECONDS\"          - wait for SECONDS seconds\n\
\"whoim\"                 - display detailed information about you\n\
\"users\"                 - list users who are joined to the current channel\n\
\n\
In commands where two parameters are required the first parameter must be\n\
enclosed with any symbols,\n\
for example \"/msg 'name' text\" or \"/join /pass'\"word/ CHANNEL";
}
