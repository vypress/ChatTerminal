/**
$Id: USER_INFO.h 36 2011-08-09 07:35:21Z avyatkin $

Interface and configuration of USER_INFO and PERSONAL_INFO classes

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#pragma once

#ifndef CHATTERM_OS_WINDOWS
#include <uuid/uuid.h>
#endif // CHATTERM_OS_WINDOWS

/**
A CHANNEL_INFO class describes an object that stores personal
information about a user
*/
struct PERSONAL_INFO
{
	PERSONAL_INFO() {}

#ifdef CHATTERM_OS_WINDOWS
	/**
	Get current operation system name
	*/
	bool getOS(std::wstring& strOS);

	/**
	Get current domain name
	*/
	bool getDomainName(std::wstring& strDomain);
#endif // CHATTERM_OS_WINDOWS

	/**
	Initializes system user name, computer name and other members
	*/
	void initialize();

	std::wstring user_name;
	std::wstring computer_name;
	std::wstring domain_name;
	std::wstring os;
	std::wstring chat_software;
	std::wstring full_name;
	std::wstring job;
	std::wstring department;
	std::wstring phone_work;
	std::wstring phone_mob;
	std::wstring www;
	std::wstring email;
	std::wstring address;
};

/**
A USER_INFO class describes an object that stores information about a user
This class also contains static members to manage (add, remove, find)
all existing users*/
class USER_INFO
{
public:
	// The comparison functor for a set of USER_INFO
	struct Less
	{
		bool operator()(USER_INFO* const& _xVal, USER_INFO* const& _yVal) const
		{
			if(NULL == _xVal) return true;
			if(NULL == _xVal->nick_) return true;
			if(NULL == _yVal) return false;
			if(NULL == _yVal->nick_) return false;
			return (_wcsicmp(_xVal->nick_, _yVal->nick_)<0);
		}
	};

	// The comparison functor for comparing user objects
	struct Comparator
	{
		const USER_INFO* _pm;

		Comparator(const USER_INFO* pi) : _pm(pi)
		{
		}

		bool operator()(const USER_INFO* pi) const
		{
			if(NULL == pi) return false;
			if(NULL == _pm->nick_) return (NULL == pi->nick_);
			if(NULL == pi->nick_) return false;
			return 0 == _wcsicmp(_pm->nick_, pi->nick_);
		}
	};

	// The comparison functor for searching a user by nickname
	struct NameComparator
	{
		const wchar_t* _pwsz;

		NameComparator(const wchar_t* pwsz) : _pwsz(pwsz)
		{
		}

		bool operator()(const USER_INFO* pi) const
		{
			if(NULL == pi) return false;
			if(NULL == pi->nick_) return false;
			return 0 == _wcsicmp(_pwsz, pi->nick_);
		}
	};

	using ConstIteratorOfUsers = std::set< std::shared_ptr<USER_INFO>, Less >::const_iterator;

	// Universally Unique Identifiers (UUIDs) of the user
#ifdef CHATTERM_OS_WINDOWS
	UUID uuid;
#else
	uuid_t uuid;
#endif // CHATTERM_OS_WINDOWS

	// Communication protocol version which is used by user's chat software
	DWORD ver;

	// Identificator of user's license chat software
	DWORD license_id;

	// Color of user's messages text
	unsigned char color;

	// User's gender
	char gender;

	// User's messages codepage, always '1' only UTF-8 supported
	char codepage;

	// User's status
	char status;

	// User's application window state
	char wnd_state;

	// User's icon
	unsigned char icon;

	// Size of user's public key
	unsigned short pub_key_size;

	// Buffer that contains user's public key
	unsigned char* pub_key;

	// Time when the latest message from the user was received
	time_t last_activity;

	//mutable for the Commands class functions

	// Flood time duration
	mutable double flood;

	// Amount of unconfirmed ping requests to the user
	mutable volatile int pings;

	// Time when the latest ping was sent to the user
	mutable time_t last_ping;

	// Amount of unconfirmed beep requests to the user
	mutable volatile int beeps;

	// Amount of unconfirmed info requests to the user
	mutable volatile int infos;

	// A network address of the user
	networkio::NETADDR_INFO naddr_info;

	USER_INFO()
		: ver(0x00020001),//0x00010009 1.9 for TextMsg
		license_id(0),
		color(0x0E),//light yellow
		gender('0'),
		codepage('1'),
		status('0'),
		wnd_state('1'),
		icon(0),
		pub_key_size(0),
		pub_key(0),
		last_activity(0),
		flood(0),
		pings(0),
		last_ping(0),
		beeps(0),
		infos(0),
		nick_(0)
	{
#ifdef CHATTERM_OS_WINDOWS
		UuidCreateNil(&uuid);
#else
		uuid_clear(uuid);
#endif // CHATTERM_OS_WINDOWS
	}

	/**
	Frees object's resources
	*/
	void freeFields()
	{
		if(nick_)
		{
			delete[] nick_;
			nick_ = 0;
		}

		if(pub_key)
		{
			delete[] pub_key;
			pub_key = 0;
			pub_key_size = 0;
		}
	}

	~USER_INFO()
	{
		freeFields();
	}

	/**
	Initializes user object from an XML file
	@file_path - path to file
	@return - true if successful
	*/
	bool loadFromXml(const wchar_t* file_path);

	/**
	Accessors to the nick_ member
	*/
	#ifdef CHATTERM_OS_WINDOWS
	const wchar_t* const& getNick() const;
	#else
	//returning const wchar_t* const& doesn't work with gcc's -O optimizations
	const wchar_t* const getNick() const;
	#endif // CHATTERM_OS_WINDOWS

	bool setNick(const wchar_t* new_nick, size_t len)
	{
		if(!new_nick || !*new_nick) return false;
		if(len<1) return false;

		delete[] nick_;

		if(len > MaxNickLength) len = MaxNickLength;

		nick_ = new wchar_t[len+1];
		wcsncpy_s(nick_, len+1, new_nick, len);

		checkNickName(nick_);
		return true;
	}

	bool setNick(wchar_t*& new_nick)
	{
		if(!new_nick || !*new_nick) return false;

		delete[] nick_;

		nick_ = new_nick;

		if( MaxNickLength == wcsnlen_s(new_nick, MaxNickLength))
			nick_[MaxNickLength] = 0;

		checkNickName(nick_);
		return true;
	}

	/**
	Find a user object by name;
	@name - name of a user to find
	@ppinfo - reference to a pointer that receives a user object
	@return - true if user is in the list of users and it is not blocked because of flood,
	          false otherwise. This function returns in ppinfo a pointer to a user object
	          regardless of blocking of the user
	*/
	static bool isUserInList(const wchar_t* name, std::shared_ptr<USER_INFO>& ppinfo);

	/**
	Removes a user with name from the list of users;
	@name - name of a user to find
	*/
	static void removeUserFromList(const wchar_t* name);

	/**
	Remove a user object at specified position from the list of users;
	@it_u - position in the list
	@return - next position in the list or end
	*/
	static ConstIteratorOfUsers removeUserFromList(ConstIteratorOfUsers it_u);

	/**
	Find user information by IP address;
	In most cases we should to determine a sender of a message by the name or uuid;
	Try to avoid using this function;
	@it - start searching position in the SetOfUsers_
	@pcrcvr - pointer to a receiver object that has just received a message from a user
	@return - position of found user object in the list of users if successful, SetOfUsers_.end() otherwise
	*/
	static ConstIteratorOfUsers findUsersByReceiver(ConstIteratorOfUsers it, const networkio::Receiver* pcrcvr);

	// String initializer for undefined nicknames
	static const wchar_t* const NullNick_;

	// Set of all users discovered from a network
	static std::set< std::shared_ptr<USER_INFO>, Less > SetOfUsers_;

	// Maximum nickname length
	static const size_t MaxNickLength = 35;

	// user's text colors
	static const wchar_t* const colors_[16];

private:

	/**
	Replaces all disallowed characters in the nickname to underline _
	@str - pointer to nickname buffer
	*/
	void checkNickName(wchar_t* str)
	{
		if(!str) return;

		const wchar_t wrongs[]=L"\\/:*?\"<>|\r\n\t";
		while(*str)
		{
			if(wcschr(wrongs, *str))
				*str=L'_';
			str++;
		}
	}

	// User's nickname
	wchar_t* nick_;
};
