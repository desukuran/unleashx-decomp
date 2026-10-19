// ReadXbeInfo/GetXbeTitleName on the release Default.xbe, and the folder
// name fallback.
#include <cstdio>

#include "../src/system/Storage.h"

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "UnleashX_0_39_0528A/Default.xbe";
	SXbeInfo info;
	if (!ReadXbeInfo(path, &info))
	{
		printf("FAIL: ReadXbeInfo(%s)\n", path);
		return 1;
	}
	printf("%s: id=%08x region=%u title='%s'\n", path, info.titleId, info.gameRegion, info.titleName.c_str());
	std::string title = GetXbeTitleName(path);
	int failures = 0;
	if (info.titleId != 0x9e115330 || info.titleName != "UnleashX Xbox Launcher" || title != info.titleName)
	{
		printf("FAIL: expected 9e115330 'UnleashX Xbox Launcher', got title '%s'\n", title.c_str());
		failures++;
	}
	if (ReadXbeInfo("no\\such\\default.xbe", &info))
	{
		printf("FAIL: missing file read\n");
		failures++;
	}
	return failures != 0;
}
