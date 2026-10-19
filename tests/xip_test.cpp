// CXipArchive on a small hand-built archive: entries, names, extraction.
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/system/XipArchive.h"

static void Put32(std::string &s, DWORD v)
{
	for (int i = 0; i < 4; i++)
		s += (char)(v >> (i * 8));
}

static std::string ReadAll(const char *path)
{
	std::string data;
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return data;
	int c;
	while ((c = fgetc(f)) != EOF)
		data += (char)c;
	fclose(f);
	return data;
}

int main()
{
	// Two files: "a.txt" (5 bytes) and "big.bin" (0x1003 bytes), one name
	// record each.
	std::string big(0x1003, 'x');
	big[0x1002] = 'y';
	const char names[] = "a.txt\0big.bin";
	DWORD namesOffset = 0x10 + 2 * 0x10 + 2 * 4;
	DWORD dataOffset = namesOffset + sizeof(names);
	std::string file = "XIP0";
	Put32(file, dataOffset);
	file += (char)2;
	file += (char)0;   // file count
	file += (char)2;
	file += (char)0;   // name count
	Put32(file, 5 + (DWORD)big.size());
	Put32(file, 0), Put32(file, 5), Put32(file, 0), Put32(file, 0);
	Put32(file, 5), Put32(file, (DWORD)big.size()), Put32(file, 0), Put32(file, 0);
	Put32(file, 0), Put32(file, 6);   // the name records (unused)
	file.append(names, sizeof(names));
	file += "hello";
	file += big;
	FILE *f = fopen("build/test.xip", "wb");
	fwrite(file.data(), 1, file.size(), f);
	fclose(f);

	int failures = 0;
	CXipArchive xip;
	memset(&xip, 0, sizeof(xip));
	if (xip.Open("build/test.xip") != S_OK || xip.m_wFileCount != 2)
	{
		printf("FAIL: open\n");
		return 1;
	}
	SXipItem item;
	if (xip.GetItem(1, &item) != S_OK || strcmp(item.name, "big.bin") != 0 || item.entry.size != big.size())
	{
		printf("FAIL: item 1 '%s'\n", item.name);
		failures++;
	}
	if (xip.GetItem(2, &item) == S_OK)
	{
		printf("FAIL: item 2 exists\n");
		failures++;
	}
	if (xip.Extract(0, "build/xip_a.txt") != S_OK || ReadAll("build/xip_a.txt") != "hello")
	{
		printf("FAIL: extract a.txt\n");
		failures++;
	}
	if (xip.Extract(1, "build/xip_big.bin") != S_OK || ReadAll("build/xip_big.bin") != big)
	{
		printf("FAIL: extract big.bin\n");
		failures++;
	}
	xip.Close();
	memset(&xip, 0, sizeof(xip));
	if (xip.Open("build/no-such.xip") != E_INVALIDARG)
	{
		printf("FAIL: missing file\n");
		failures++;
	}
	return failures != 0;
}
