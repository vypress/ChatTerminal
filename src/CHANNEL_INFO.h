/**
$Id: CHANNEL_INFO.h 28 2010-08-29 18:06:44Z avyatkin $

Interface and configuration of the CHANNEL_INFO class

Copyright (c) 2010 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#pragma once

/**
A CHANNEL_INFO class describes an object that stores information about a channel
This class also contains static members to manage (add, remove, find)
all existing channels*/
class CHANNEL_INFO
{
public:
	// An enumeration of possible channel types: generic, secured, unknown
	enum SECURED_STATUS {NOT_SECURED = 0, SECURED, SEC_UNKNOWN};

	// The comparison functor for a set of CHANNEL_INFO
	struct Less
	{
		bool operator()(CHANNEL_INFO* const& _xVal, CHANNEL_INFO* const& _yVal) const
		{
			if(NULL == _xVal) return true;
			if(NULL == _xVal->name) return true;
			if(NULL == _yVal) return false;
			if(NULL == _yVal->name) return false;
			return (_wcsicmp(_xVal->name, _yVal->name)<0);
		}
	};

	// The comparison functor for searching a channel by name
	struct NameComparator
	{
		const wchar_t* _pwsz;

		NameComparator(const wchar_t* pwsz) : _pwsz(pwsz)
		{
		}

		bool operator()(const CHANNEL_INFO* chinfo) const
		{
			if(NULL == chinfo) return false;
			if(NULL == chinfo->name) return false;
			return 0 == _wcsicmp(_pwsz, chinfo->name);
		}
	};

	typedef std::vector <const USER_INFO*>::const_iterator ConstIteratorOfChannelUsers;
	typedef std::vector <const USER_INFO*>::iterator IteratorOfChannelUsers;

	typedef std::set< CHANNEL_INFO*, Less >::const_iterator ConstIteratorOfChannels;
	//typedef std::set< CHANNEL_INFO*, Less >::iterator IteratorOfChannels;

	// Name of channel
	wchar_t* name;
	// Topic of channel
	wchar_t* topic;
	// Flag is true if you are joined to this channel
	bool joined;
	// Flag is true if a channel is secured
	bool secured;
	// Users who joined to a channel
	std::vector< const USER_INFO* > users;

	// Password MD5 hash of a secured channel
	// mutable for Commands::SecureJoinQ5()
	mutable unsigned char passwd_hash[16];
#ifdef CHATTERM_OS_WINDOWS
	// Symmetric encryption key of a secured channel
	// derived from a password
	// mutable for Commands::SecureJoinQ5()
	mutable HCRYPTKEY hKey;
#endif // CHATTERM_OS_WINDOWS

	CHANNEL_INFO()
		: name(0),
		topic(0),
		joined(false),
#ifdef CHATTERM_OS_WINDOWS
		hKey(0),
#endif // CHATTERM_OS_WINDOWS
		secured(false)
	{
		memset(passwd_hash, sizeof(passwd_hash), 0x00);
	}

	// Destructor of a CHANNEL_INFO object
	~CHANNEL_INFO()
	{
		delete[] name;
		delete[] topic;
#ifdef CHATTERM_OS_WINDOWS
		if(hKey) CryptDestroyKey(hKey);
#endif // CHATTERM_OS_WINDOWS
	}

	/**
	Adds, removes, and checks user to (if he is joined to) a channel
	@member - const pointer to an object which describes a user
	@return - true if adding or deleting was successful or if user is joined to a channel, false otherwise
	*/
	bool addMember(const USER_INFO* member);
	bool removeMember(const USER_INFO* member);
	bool isMember(const USER_INFO* member) const;

	/**
	Adds user to a channel
	@channel - a channel name
	@member - const pointer to an object which describes a user
	@fSecureStatus - required channel type
	@return - true if a channel with name @channel had existed or was created successfully
	and a user was added to a list of channel's users
	*/
	static const CHANNEL_INFO* addChannelMember(const wchar_t* channel, const USER_INFO* member, SECURED_STATUS fSecureStatus);

	/**
	Accessors to ActiveChannel_
	@channel - a channel name
	@return - const pointer to a current active channel
	*/
	static const CHANNEL_INFO* setActiveChannel(const wchar_t* channel);
	static const CHANNEL_INFO* setActiveChannel(const CHANNEL_INFO* const& pcchinfo);
	static const CHANNEL_INFO* const& getActiveChannel();

	/**
	Check whether you are joined to a channel
	@channel - a channel name
	@pchinfo - returned pointer to an object which describes a channel with name @channel
	@return - true if you are joined to the channel with name @channel
	*/
	static bool isMyChannel(const wchar_t* channel, CHANNEL_INFO** pchinfo);

	/**
	Creates string of your joined channels; Channels are separated by '#'
	@strChannels - returned string object reference
	@return - true if successful
	*/
	static bool getChannelsList(std::wstring& strChannels);

	/**
	Checks if a channel name has a correct prefix for specified type
	@channel - a channel name
	@fSecureStatus - a channel type
	@return - true if name has right prefix, false otherwise
	*/
	static bool checkNamePrefix(const wchar_t* channel, SECURED_STATUS fSecureStatus);

	/**
	Checks if a channel name has a correct prefix for specified type and suggests a new name otherwise
	@channel - a channel name
	@fSecureStatus - a channel type
	@return - NULL if name has right prefix or a pointer to a buffer that contains new suggested name;
	Returned buffer must be freed by delete[] operator
	*/
	static wchar_t* createNameWithPrefix(const wchar_t* channel, SECURED_STATUS fSecureStatus);

	/**
	Find a channel by name (uses findChannelByName) and return real name and type of a channel
	@channel - a channel name; At return contains real channel name if NULL then information about a current channel returned
	@fSecureStatus - reference to variable which get a channel type
	@return - true if successful, false otherwise
	*/
	static bool getNameWithPrefix(const wchar_t*& channel, bool& fSecured);

	// Set of all channels discovered from a network
	static std::set< CHANNEL_INFO*, Less > SetOfChannels_;

	// Name of a main channel, usually "#Main"
	static const wchar_t wszMainChannel[6];

	// Maximum channel name length
	static const size_t MaxNameLength = 35;

	// Maximum channel topic length
	static const size_t MaxTopicLength = 965;//1000 - MAX_NICKNAME_LEN 

private:

	// Pointer to an object which describes current channel
	static const CHANNEL_INFO* ActiveChannel_;

	/**
	Find a channel by name
	@channel - the channel name
	@fJoined - flag, if true then search in channels that you are joined to
	*/
	static ConstIteratorOfChannels findChannelByName(const wchar_t* channel, bool fJoined);

	// Prefixes of a generic and secured channels
	static const wchar_t wchChPrefix_;
	static const wchar_t wchSecureChPrefix_;
};
