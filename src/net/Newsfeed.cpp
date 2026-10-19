// The newsfeed list: feed URLs and the RSS text downloaded from them.
#include "Newsfeed.h"

#include <cstring>

#include "../common/XboxKernel.h"

CNewsfeed g_newsfeed;   // 0x002784B0

// 0x00137D8D
CNewsfeed::~CNewsfeed()
{
	// Quirk: meant to empty each feed's text, but it clears a copy.
	for (std::vector<SNewsfeed>::iterator it = m_feeds.begin(); it != m_feeds.end(); ++it)
	{
		SNewsfeed feed = *it;
		if (feed.content.size() != 0)
			feed.content.clear();
	}
	m_feeds.clear();
}

// 0x00137E0D
SNewsfeed *CNewsfeed::Find(const char *url)
{
	if (url == NULL || strlen(url) == 0)
		return NULL;
	// (Each feed is copied to be compared.)
	for (size_t i = 0; i < m_feeds.size(); i++)
	{
		if (_mbsicmp(url, m_feeds[i].url.c_str()) == 0)
			return &m_feeds[i];
	}
	return NULL;
}

// 0x00137E8C
std::string CNewsfeed::FindContent(const char *url)
{
	if (url == NULL || strlen(url) == 0)
		return "";
	for (size_t i = 0; i < m_feeds.size(); i++)
	{
		if (_mbsicmp(url, m_feeds[i].url.c_str()) == 0)
			return m_feeds[i].content;
	}
	// Quirk: the original builds the string from a NULL pointer here, which
	// would crash; callers only ask for URLs from GetUrl.
	return "";
}

// 0x00138146
HRESULT CNewsfeed::AddFeed(const char *url, const char *content)
{
	if (content == NULL || url == NULL || strlen(url) == 0)
		return E_FAIL;
	SNewsfeed *feed = Find(url);
	if (feed != NULL)
	{
		feed->content = content;
		return S_OK;
	}
	m_feeds.push_back(SNewsfeed(url, content));
	return S_OK;
}

// 0x00021409
std::string CNewsfeed::GetUrl(int index)
{
	std::string url = "";
	if (index < GetCount())
		url = m_feeds[index].url;
	return url;
}

// 0x001381B3
std::string CNewsfeed::GetContent(const char *url)
{
	return FindContent(url);
}
