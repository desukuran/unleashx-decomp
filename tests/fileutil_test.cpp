// MakeFatxPath / IsFatxName / GetPathDirectory on a few names, and CMD5.
#include <cstdio>
#include <cstring>

#include "../src/common/Base64.h"
#include "../src/common/FileUtil.h"
#include "../src/system/SystemInfo.h"

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
	const char *md5Cases[][2] = {
		{ "", "d41d8cd98f00b204e9800998ecf8427e" },
		{ "abc", "900150983cd24fb0d6963f7d28e17f72" },
		{ "The quick brown fox jumps over the lazy dog", "9e107d9d372bb6826bd81d3542a419d6" },
		{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		  "57edf4a22be3c955ac49da2e2107b67a" },
	};
	for (int i = 0; i < 4; i++)
	{
		CMD5 md5(NULL, -1);
		if (!md5.HashMemory(md5Cases[i][0], -1) || strcmp(md5.m_szHex, md5Cases[i][1]) != 0)
		{
			printf("CMD5(\"%s\") = %s\n", md5Cases[i][0], md5.m_szHex);
			s_failures++;
		}
	}
	// Base64 (the FTP site passwords).
	static const char *const base64Cases[][2] = {
		{ "xbox", "eGJveA==" }, { "pass1", "cGFzczE=" }, { "abc", "YWJj" }, { "", "" },
	};
	for (int i = 0; i < 4; i++)
	{
		std::string encoded, decoded;
		Base64Encode(base64Cases[i][0], encoded);
		Base64Decode(base64Cases[i][1], decoded);
		if (encoded != base64Cases[i][1] || decoded != base64Cases[i][0])
		{
			printf("Base64(\"%s\") = %s / %s\n", base64Cases[i][0], encoded.c_str(), decoded.c_str());
			s_failures++;
		}
	}
	std::string spaced;
	Base64Decode("eGJv\r\n eA==", spaced);
	if (spaced != "xbox")
		s_failures++;
	// CFileFinder over a small folder: files only, then a pattern.
	CreateDirectoryTree("build\\finder\\sub");
	WriteTextFile("build\\finder\\a.txt", "a", FALSE);
	WriteTextFile("build\\finder\\b.log", "b", FALSE);
	{
		CFileFinder finder;
		finder.m_nFilter = 1;
		int files = 0, folders = 0;
		for (const char *name = finder.FindFirst("build\\finder", "*"); name != NULL; name = finder.FindNext())
		{
			if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
				continue;
			if (finder.GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
				folders++;
			else
				files++;
		}
		if (files != 2 || folders != 0)
			printf("FAIL CFileFinder files: %d files, %d folders\n", files, folders), s_failures++;
		const char *txt = finder.FindFirst("build\\finder", "*.txt");
		if (txt == NULL || strcmp(txt, "a.txt") != 0 || finder.FindNext() != NULL)
			printf("FAIL CFileFinder pattern: %s dir=%s\n", txt ? txt : "NULL", finder.m_strDir.c_str()), s_failures++;
		if (finder.FindFirst("build\\no_such_folder", "*") != NULL)
			printf("FAIL CFileFinder missing folder\n"), s_failures++;
	}
	printf(s_failures == 0 ? "fileutil_test passed\n" : "fileutil_test failed\n");
	return s_failures != 0;
}
