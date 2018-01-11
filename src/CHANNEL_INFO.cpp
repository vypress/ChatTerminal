/**
Implementation of the CHANNEL_INFO class

Copyright (c) 2017 VyPRESS Research, LLC. All rights reserved.
For conditions of distribution and use, see copyright notice in ChatTerminal.h
*/

#include "ChatTerminal.h"

const wchar_t CHANNEL_INFO::wchChPrefix_ = L'#';
const wchar_t CHANNEL_INFO::wchSecureChPrefix_ = L'@';

const wchar_t CHANNEL_INFO::wszMainChannel[6] = L"#Main";
const CHANNEL_INFO* CHANNEL_INFO::ActiveChannel_ = nullptr;
std::set< std::shared_ptr<CHANNEL_INFO>, CHANNEL_INFO::Less > CHANNEL_INFO::SetOfChannels_;

//std::function<bool(wchar_t, wchar_t)> const CHANNEL_INFO::ignore_case_compare = [](wchar_t c1, wchar_t c2) {return std::tolower(c1) < std::tolower(c2); };

CHANNEL_INFO::ConstIteratorOfChannels CHANNEL_INFO::findChannelByName(const std::wstring& channel, bool fJoined)
{
	//It will create a name with a wchChPrefix prefix first
	std::wstring with_prefix(createNameWithPrefix(channel, SEC_UNKNOWN));
	if(with_prefix.empty()) with_prefix = channel;

	/*
	std::function<bool (const CHANNEL_INFO*)> comparator = [&channel](const CHANNEL_INFO* chinfo)
	{
		if (nullptr == chinfo) return false;
		if (chinfo->name.empty()) return false;
		return 0 == _wcsicmp(channel, chinfo->name.c_str());
	};
	*/

	NameComparator comparator(with_prefix);

	CHANNEL_INFO::ConstIteratorOfChannels it_ch = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), comparator);

	if(fJoined && (it_ch != SetOfChannels_.end()))
	{
		if(!(*it_ch) || !(*it_ch)->joined)
			it_ch = SetOfChannels_.end();
	}

	if(it_ch == SetOfChannels_.end())
	{
		if (!with_prefix.empty())
		{
			with_prefix[0] = wchSecureChPrefix_;//try to search secure channels

			//it_ch = SetOfChannels_.find(channel);
			it_ch = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), comparator);

			if (fJoined && (it_ch != SetOfChannels_.end()))
			{
				if (!(*it_ch) || !(*it_ch)->joined)
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

	if(nullptr == channel)
		ActiveChannel_ = nullptr;
	else
	{
		ConstIteratorOfChannels it_ch = findChannelByName(channel, true);

		if(it_ch != SetOfChannels_.end())
			ActiveChannel_ = (*it_ch).get();
		else ActiveChannel_ = nullptr;

		_ASSERTE(ActiveChannel_ != nullptr);
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
		channel = (*it_ch)->name.c_str();
	}
	else
	{
		if(nullptr==ActiveChannel_) return false;

		fSecured = ActiveChannel_->secured;
		channel = ActiveChannel_->name.c_str();
	}

	return true;
}

bool CHANNEL_INFO::checkNamePrefix(const std::wstring& channel, SECURED_STATUS fSecureStatus)
{
	if(channel.length()<1) return false;

	switch(fSecureStatus)
	{
	case NOT_SECURED:
		if(wchChPrefix_ == channel[0]) return true;
		break;

	case SECURED:
		if(wchSecureChPrefix_ == channel[0]) return true;
		break;

	default:
		if(wchChPrefix_ == channel[0] || wchSecureChPrefix_ == channel[0])
			return true;
	}

	return false;
}

std::wstring CHANNEL_INFO::createNameWithPrefix(const std::wstring& channel, SECURED_STATUS fSecureStatus)
{
	if(channel.length()<1) return std::wstring();

	if(checkNamePrefix(channel, fSecureStatus)) return std::wstring();

	std::wstring name_with_prefix;
	name_with_prefix.reserve(channel.length() + 2);

	name_with_prefix.assign(1, SECURED == fSecureStatus ? wchSecureChPrefix_ : wchChPrefix_);
	name_with_prefix.append(channel);

	return name_with_prefix;
}

bool CHANNEL_INFO::addMember(const USER_INFO* member)
{
	ConstIteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	if(it == users.end())
		users.push_back(member);

	if(member == theApp.ptrMe_.get())
	{
		joined = true;

		setActiveChannel(this);
	}

	return true;
}

const CHANNEL_INFO* CHANNEL_INFO::addChannelMember(const std::wstring& channel, const USER_INFO* member, SECURED_STATUS fSecureStatus)
{
	if(channel.empty()) return nullptr;

	ConstIteratorOfChannels it_ch = findChannelByName(channel, false);

	std::shared_ptr<CHANNEL_INFO> ptrChInfo;

	if(it_ch == SetOfChannels_.end())
	{
		ptrChInfo = std::make_shared<CHANNEL_INFO>();

		switch(fSecureStatus)
		{
		case NOT_SECURED:
			ptrChInfo->secured = false;
			break;

		case SECURED:
			ptrChInfo->secured = true;
			break;

		default:
			if(wchSecureChPrefix_ == channel[0])
				ptrChInfo->secured = true;
			else
				ptrChInfo->secured = false;
		}

		const wchar_t wchPrefix = ptrChInfo->secured ? wchSecureChPrefix_ : wchChPrefix_;
		if(wchPrefix != channel[0])
		{
			size_t buflen = channel.length() + 1;
			ptrChInfo->name.reserve(buflen);

			ptrChInfo->name.assign(1, wchPrefix);
			ptrChInfo->name.append(channel);
		}
		else
		{
			ptrChInfo->name = channel;
		}

		SetOfChannels_.insert(ptrChInfo);
	}
	else
	{
		ptrChInfo = *it_ch;
	}

	ptrChInfo->addMember(member);

	return ptrChInfo.get();
}

bool CHANNEL_INFO::isMember(const USER_INFO* member) const
{
	ConstIteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	return it!=users.end();
}

size_t CHANNEL_INFO::removeMember(const USER_INFO* member)
{
	IteratorOfChannelUsers it = std::find_if(users.begin(), users.end(), USER_INFO::Comparator(member));
	if(it != users.end())
	{
		users.erase(it);

		if(member == theApp.ptrMe_.get())
		{
			//clear topic
			topic.clear();

			joined = false;

			ConstIteratorOfChannels it_ch_main = std::find_if(SetOfChannels_.begin(), SetOfChannels_.end(), NameComparator(wszMainChannel));

			if(it_ch_main != SetOfChannels_.end() && (*it_ch_main)->joined)
				setActiveChannel((*it_ch_main).get());
			else
				setActiveChannel((const wchar_t*)nullptr);
		}
	}

	return users.size();
}

bool CHANNEL_INFO::isMyChannel(const std::wstring& channel, std::shared_ptr<CHANNEL_INFO>& ptrChInfo)
{
	ConstIteratorOfChannels it_ch = findChannelByName(channel, true);

	bool result = (it_ch != SetOfChannels_.end());

	if(result) ptrChInfo = *it_ch;

	return result;
}

bool CHANNEL_INFO::getChannelsList(std::wstring& strChannels)
{
	ConstIteratorOfChannels it_ch = SetOfChannels_.begin();
	while(it_ch != SetOfChannels_.end())
	{
		if(*it_ch && (*it_ch)->joined && !(*it_ch)->secured && !(*it_ch)->name.empty())
		{
			if((*it_ch)->name[0]!=L'#')
				strChannels+=L'#';

			strChannels+=(*it_ch)->name;
		}

		++it_ch;
	}

	strChannels+=L'#';
	return true;
}
