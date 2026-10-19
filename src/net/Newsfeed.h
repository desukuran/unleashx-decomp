// RSS newsfeed ticker (<Newsfeed> in Config.xml).
#pragma once

#include "../common/Platform.h"

class CNewsfeed
{
public:
	HRESULT AddFeed(const char *url, const char *title);   // 0x00138146 TODO: decompile
};

extern CNewsfeed g_newsfeed;                             // 0x002784B0
