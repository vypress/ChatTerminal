/**
Implementation of the CHANNEL_INFO class

Copyright (c) 2017 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/
#include <algorithm>

#include "ChatTerminal.h"
#include "CHANNEL_INFO.h"

const wchar_t CHANNEL_INFO::wchChPrefix_ = L'#';
const wchar_t CHANNEL_INFO::wchSecureChPrefix_ = L'@';

const wchar_t CHANNEL_INFO::wszMainChannel[6] = L"#Main";
const CHANNEL_INFO* CHANNEL_INFO::ActiveChannel_ = 0;
std::set< CHANNEL_INFO*, CHANNEL_INFO::Less > CHANNEL_INFO::SetOfChannels_;

CHANNEL_INFO::ConstIteratorOfChannels CHANNEL_INFO::findChannelByName(const wchar_t* channel, bool fJoined)
{
	//It will create a name with a wchChPrefix prefix first
	std::unique_ptr<wchar_t> buf(createNameWithPrefix(channel, SEC_UNKNOWN));

	if(nullptr!=buf) channel = buf.get();

	//NameComparator comparator(channel);
	std::function<bool (const CHANNEL_INFO*)> comparator = [&channel](const CHANNEL_INFO* chinfo)
	{
		if (NULL == chinfo) return false;
		if (NULL == chinfo->name) return false;
		return 0 == _wcsicmp(channel, chinfo->name);
	};

	ConstIteratorOfChannels it_ch = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), comparator);

	if(fJoined && (it_ch != SetOfChannels_.end()))
	{
		if(!(*it_ch) || !(*it_ch)->joined)
			it_ch = SetOfChannels_.end();
	}

	if(it_ch == SetOfChannels_.end())
	{
		if(nullptr != buf)
		{
			*buf = wchSecureChPrefix_;//try to search secure channels

			//it_ch = SetOfChannels_.find(channel);
			it_ch = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), comparator);

			if(fJoined && (it_ch != SetOfChannels_.end()))
			{
				if(!(*it_ch) || !(*it_ch)->joined)
					it_ch = SetOfChannels_.end();
			}
		}
	}

	return it_ch;
}

const CHANNEL_INFO* const& CHANNEL_INFO::getActiveChannel()
{
	return ActiveChannel_;
}

const CHANNEL_INFO* CHANNEL_INFO::setActiveChannel(const wchar_t* channel)
{
	consoleio::PrintLinesMonitor PRINT_LINES_MONITOR;

	if(0 == channel)
		ActiveChannel_ = 0;
	else
	{
		ConstIteratorOfChannels it_ch = findChannelByName(channel, true);

		if(it_ch != SetOfChannels_.end())
			ActiveChannel_ = *it_ch;
	}

	return ActiveChannel_;
}

const CHANNEL_INFO* CHANNEL_INFO::setActiveChannel(const CHANNEL_INFO* const& pcchinfo)
{
	consoleio::PrintLinesMonitor PRINT_LINES_MONITOR;

	ActiveChannel_ = pcchinfo;

	return ActiveChannel_;
}

bool CHANNEL_INFO::getNameWithPrefix(const wchar_t*& channel, bool& fSecured)
{
	if(channel && *channel)
	{
		ConstIteratorOfChannels it_ch = findChannelByName(channel, true);
		if(it_ch == SetOfChannels_.end())
			return false;

		fSecured = (*it_ch)->secured;
		channel = (*it_ch)->name;
	}
	else
	{
		if(!ActiveChannel_) return false;

		fSecured = ActiveChannel_->secured;
		channel = ActiveChannel_->name;
	}

	return true;
}

bool CHANNEL_INFO::checkNamePrefix(const wchar_t* channel, SECURED_STATUS fSecureStatus)
{
	if(0==channel || 0==*channel) return false;

	switch(fSecureStatus)
	{
	case NOT_SECURED:
		if(wchChPrefix_ == *channel) return true;
		break;

	case SECURED:
		if(wchSecureChPrefix_ == *channel) return true;
		break;

	default:
		if(wchChPrefix_ == *channel || wchSecureChPrefix_ == *channel)
			return true;
	}

	return false;
}

std::make_unique<wchar_t[]> CHANNEL_INFO::createNameWithPrefix(const wchar_t* channel, SECURED_STATUS fSecureStatus)
{
	if(0==channel) return 0;

	if(checkNamePrefix(channel, fSecureStatus)) return 0;

	size_t buflen = wcslen(channel)+2;

	auto buf_ptr = std::make_unique<wchar_t[]>(buflen);
	*buf = SECURED==fSecureStatus ? wchSecureChPrefix_ : wchChPrefix_;
	wcscpy_s(buf+1, buflen-1, channel);
	return buf;
}

bool CHANNEL_INFO::addMember(const USER_INFO* member)
{
	ConstIteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	if(it == users.end())
		users.push_back(member);

	if(member == &theApp.Me_)
	{
		joined = true;

		setActiveChannel(this);
	}

	return true;
}

const CHANNEL_INFO* CHANNEL_INFO::addChannelMember(const wchar_t* channel, const USER_INFO* member, SECURED_STATUS fSecureStatus)
{
	if(!channel || !*channel) return 0;

	ConstIteratorOfChannels it_ch = findChannelByName(channel, false);

	CHANNEL_INFO* pchinfo = 0;

	if(it_ch == SetOfChannels_.end())
	{
		pchinfo = new CHANNEL_INFO;

		switch(fSecureStatus)
		{
		case NOT_SECURED:
			pchinfo->secured = false;
			break;

		case SECURED:
			pchinfo->secured = true;
			break;

		default:
			if(wchSecureChPrefix_ == *channel)
				pchinfo->secured = true;
			else
				pchinfo->secured = false;
		}

		size_t buflen = wcslen(channel)+1;

		const wchar_t wchPrefix = pchinfo->secured ? wchSecureChPrefix_ : wchChPrefix_;

		if(wchPrefix != *channel)
		{
			pchinfo->name = new wchar_t[buflen+1];
			*pchinfo->name = wchPrefix;
			wcscpy_s(pchinfo->name+1, buflen, channel);
		}
		else
		{
			pchinfo->name = new wchar_t[buflen];
			wcscpy_s(pchinfo->name, buflen, channel);
		}

		SetOfChannels_.insert(pchinfo);
	}
	else
	{
		pchinfo = *it_ch;
	}

	pchinfo->addMember(member);

	return pchinfo;
}

bool CHANNEL_INFO::isMember(const USER_INFO* member) const
{
	ConstIteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	return it!=users.end();
}

bool CHANNEL_INFO::removeMember(const USER_INFO* member)
{
	IteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	if(it != users.end())
	{
		users.erase(it);

		if(member == &theApp.Me_)
		{
			//clear topic
			delete[] topic;
			topic = 0;

			joined = false;

			ConstIteratorOfChannels it_ch_main = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), NameComparator(wszMainChannel));

			if(it_ch_main != SetOfChannels_.end() && (*it_ch_main)->joined)
				setActiveChannel(*it_ch_main);
			else
				setActiveChannel((const wchar_t*)NULL);
		}
	}

	if(users.size()<1)//remove channel
	{
		//if(pchinfo) delete pchinfo;
		//SetOfChannels_.erase(it_ch);

		SetOfChannels_.erase(this);
		delete this;
	}

	return true;
}

bool CHANNEL_INFO::isMyChannel(const wchar_t* channel, CHANNEL_INFO** pchinfo)
{
	ConstIteratorOfChannels it_ch = findChannelByName(channel, true);

	bool result = (it_ch != SetOfChannels_.end());

	if(result && pchinfo) *pchinfo = *it_ch;

	return result;
}

bool CHANNEL_INFO::getChannelsList(std::wstring& strChannels)
{
	ConstIteratorOfChannels it_ch = SetOfChannels_.begin();
	while(it_ch != SetOfChannels_.end())
	{
		if(*it_ch && (*it_ch)->joined && !(*it_ch)->secured && (*it_ch)->name)
		{
			if(*(*it_ch)->name!=L'#')
				strChannels+=L'#';

			strChannels+=(*it_ch)->name;
		}

		++it_ch;
	}

	strChannels+=L'#';
	return true;
}
