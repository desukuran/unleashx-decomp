// RSS newsfeed ticker (<Newsfeed> in Config.xml). The config loader adds
// each <URL> with no content; the network thread downloads the feeds and
// stores their RSS text; the skin's $Newsfeed$/$NewsSource$ texts cycle
// through their items (see SkinText.cpp). See Newsfeed.cpp.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

// A feed: its URL and the RSS text downloaded from it. 0x38 bytes.
struct SNewsfeed
{
	SNewsfeed(const char *url, const char *content) : url(url), content(content) {}   // 0x0002E39E (this in esi)

	std::string url;                       // +0x00
	std::string content;                   // +0x1C
};

class CNewsfeed
{
public:
	// (this in ebx; run at exit by 0x0013FC2A)
	~CNewsfeed();                                          // 0x00137D8D
	// Stores content for url (adding the feed if it's new). E_FAIL for an
	// empty URL. (url in eax)
	HRESULT AddFeed(const char *url, const char *content);   // 0x00138146
	// URL of feed index, or "" past the end. (index in eax)
	std::string GetUrl(int index);                         // 0x00021409
	// The downloaded RSS text of the feed with that URL, "" if not (yet) there.
	std::string GetContent(const char *url);               // 0x001381B3 (result in esi)
	int GetCount() const { return (int)m_feeds.size(); }   // 0x000214C1 (this in eax)

private:
	// The feed with that URL (any case), or NULL.
	SNewsfeed *Find(const char *url);                      // 0x00137E0D
	// Its content. (this in ebx)
	std::string FindContent(const char *url);              // 0x00137E8C

	std::vector<SNewsfeed> m_feeds;        // +0x00
};

extern CNewsfeed g_newsfeed;                             // 0x002784B0
