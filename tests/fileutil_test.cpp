// MakeFatxPath / IsFatxName / GetPathDirectory on a few names.
#include <cstdio>
#include <cstring>

#include "../src/common/FileUtil.h"

static int s_failures;

static void Check(const char *path, BOOL changed, const char *expected)
{
	std::string out = "(unchanged)";
	BOOL result = MakeFatxPath(path, out);
	if (result != changed || out != expected)
	{
		printf("MakeFatxPath(\"%s\") = %d \"%s\", expected %d \"%s\"\n", path, result, out.c_str(), changed, expected);
		s_failures++;
	}
}

static void CheckDir(const char *path, const char *expected)
{
	std::string dir = GetPathDirectory(path);
	if (dir != expected)
	{
		printf("GetPathDirectory(\"%s\") = \"%s\", expected \"%s\"\n", path, dir.c_str(), expected);
		s_failures++;
	}
}

int main()
{
	CheckDir("E:\\Games\\Halo\\default.xbe", "E:\\Games\\Halo");
	CheckDir("E:\\Games\\Halo", "E:\\Games");          // no dot: a file name
	CheckDir("E:\\Games\\Halo\\", "E:\\Games\\Halo");
	CheckDir("E:\\a.b\\c", "E:\\a.b\\c");              // a dot before the last part: a folder
	CheckDir("E:\\file.txt", "E:\\");
	CheckDir("file.txt", "");
	CheckDir("name", "name");
	Check("E:\\Games\\Halo\\default.xbe", FALSE, "(unchanged)");
	Check("E:\\a\\b?c.txt", TRUE, "E:\\a\\bc.txt");
	Check("E:\\x\\this is a very long file name for fatx limits.tar.gz", TRUE,
	      "E:\\x\\this is a very long file name for fatx.gz");
	Check("E:\\x\\abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz.bin", TRUE,
	      "E:\\x\\abcdefghijklmnopqrstuvwxyzabcdefghijkl.bin");
	if (!IsFatxName("default.xbe") || IsFatxName("a*b") || IsFatxName(""))
	{
		printf("IsFatxName\n");
		s_failures++;
	}
	printf(s_failures == 0 ? "fileutil_test passed\n" : "fileutil_test failed\n");
	return s_failures != 0;
}
