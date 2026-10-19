// CNewsfeed: feeds added from the config (no content), then filled in by
// the download; URLs match in any case.
#include <cstdio>

#include "../src/net/Newsfeed.h"

static int failures = 0;

static void expect(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main()
{
	CNewsfeed feeds;
	expect(feeds.GetCount() == 0, "empty at first");
	expect(feeds.AddFeed("", "x") == E_FAIL, "an empty URL is refused");
	expect(feeds.AddFeed(NULL, "x") == E_FAIL, "a NULL URL is refused");

	// As the config loader adds them.
	expect(feeds.AddFeed("http://a.example/rss", "") == S_OK, "first feed");
	expect(feeds.AddFeed("http://b.example/rss", "") == S_OK, "second feed");
	expect(feeds.GetCount() == 2, "two feeds");
	expect(feeds.GetUrl(0) == "http://a.example/rss", "GetUrl(0)");
	expect(feeds.GetUrl(1) == "http://b.example/rss", "GetUrl(1)");
	expect(feeds.GetUrl(2) == "", "GetUrl past the end");
	expect(feeds.GetContent("http://a.example/rss") == "", "no content before the download");

	// As the network thread stores what it downloaded (any case).
	expect(feeds.AddFeed("HTTP://A.EXAMPLE/RSS", "<rss>a</rss>") == S_OK, "content for the first feed");
	expect(feeds.GetCount() == 2, "no feed added for a known URL");
	expect(feeds.GetContent("http://a.example/rss") == "<rss>a</rss>", "GetContent");
	expect(feeds.GetContent("http://b.example/rss") == "", "the other feed is untouched");
	expect(feeds.GetContent("") == "", "GetContent of an empty URL");

	if (failures == 0)
		printf("newsfeed_test: ok\n");
	return failures != 0;
}
