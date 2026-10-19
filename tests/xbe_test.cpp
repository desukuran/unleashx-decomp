// ReadXbeInfo/GetXbeTitleName on the release Default.xbe, the folder name
// fallback, and GetXbeDevicePath.
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
	// Where D: points when an XBE launches.
	static const char *const devicePaths[][2] = {
		{"E:\\Games\\Halo\\default.xbe", "\\Device\\Harddisk0\\Partition1\\Games\\Halo"},
		{"c:\\xboxdash.xbe", "\\Device\\Harddisk0\\Partition2\\"},
		{"F:\\Apps\\a.xbe", "\\Device\\Harddisk0\\Partition6\\Apps"},
		{"G:\\x.xbe", "\\Device\\Harddisk0\\Partition7\\"},
		{"D:\\default.xbe", "\\Device\\Cdrom0\\"},
		{"h:\\Emus\\x.xbe", "\\Device\\Harddisk0\\Partition8\\Emus"},
		{"H:\\Emus\\x.xbe", ""},   // quirk: uppercase extra partitions
		{"default.xbe", "\\Device\\Cdrom0"},
		{"Apps\\a.xbe", "\\Device\\Cdrom0\\Apps"},
	};
	for (size_t i = 0; i < sizeof(devicePaths) / sizeof(devicePaths[0]); i++)
	{
		std::string device = GetXbeDevicePath(devicePaths[i][0]);
		if (device != devicePaths[i][1])
		{
			printf("FAIL: GetXbeDevicePath(%s) = '%s', expected '%s'\n", devicePaths[i][0], device.c_str(),
			       devicePaths[i][1]);
			failures++;
		}
	}
	return failures != 0;
}
