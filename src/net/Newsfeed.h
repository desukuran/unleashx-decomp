// RSS newsfeed ticker (<Newsfeed> in Config.xml). Feeds are downloaded in the
// background; the skin's $Newsfeed$/$NewsSource$ texts cycle through their
// items (see SkinText.cpp).
#pragma once

#include <string>

#include "../common/Platform.h"

class CNewsfeed
{
public:
	HRESULT AddFeed(const char *url, const char *title);   // 0x00138146 TODO: decompile
	// URL of feed index, or "" past the end.
	std::string GetUrl(int index);                         // 0x00021409 TODO: decompile
	// The downloaded RSS text of the feed with that URL, "" if not (yet) there.
	std::string GetContent(const char *url);               // 0x001381B3 TODO: decompile
};

extern CNewsfeed g_newsfeed;                             // 0x002784B0
