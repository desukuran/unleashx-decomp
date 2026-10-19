// HexToBytes, PatchXbe on a made-up file, and SetXbeTitleName on a copy of
// the release Default.xbe.
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/common/StrUtil.h"
#include "../src/system/Storage.h"

static int s_failures = 0;

static void Check(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		s_failures++;
	}
}

static std::vector<BYTE> ReadAll(const char *path)
{
	std::vector<BYTE> data;
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return data;
	BYTE buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) != 0)
		data.insert(data.end(), buffer, buffer + n);
	fclose(f);
	return data;
}

static void WriteAll(const char *path, const std::vector<BYTE> &data)
{
	FILE *f = fopen(path, "wb");
	fwrite(&data[0], 1, data.size(), f);
	fclose(f);
}

static void Put(std::vector<BYTE> &data, size_t offset, const char *hex)
{
	BYTE bytes[100];
	HexToBytes(hex, bytes);
	memcpy(&data[offset], bytes, strlen(hex) / 2);
}

static bool Has(const std::vector<BYTE> &data, size_t offset, const char *hex)
{
	BYTE bytes[100];
	HexToBytes(hex, bytes);
	return memcmp(&data[offset], bytes, strlen(hex) / 2) == 0;
}

int main()
{
	BYTE bytes[4];
	Check(HexToBytes("0aFF10", bytes) == 0 && bytes[0] == 0x0A && bytes[1] == 0xFF && bytes[2] == 0x10, "HexToBytes");
	Check(HexToBytes("ABC", bytes) == 1, "HexToBytes odd length");

	// A fake XBE: certificate at 0x180, the first and last patterns inside.
	const char *path = "build/patch_test.xbe";
	std::vector<BYTE> data(0x2000, 0);
	data[0x110] = 0x80;
	data[0x111] = 0x01;
	Put(data, 0x700, "744BE8CAFDFFFF85C07D0633C05050EB44F605");
	Put(data, 0x1000, "A801750CC745");
	WriteAll(path, data);
	Check(CheckXbePatchBytes("A801750CC745", path, 0) == 0x1000, "CheckXbePatchBytes offset");
	Check(CheckXbePatchBytes("DEADBEEF", path, 0) == -1, "CheckXbePatchBytes missing");
	Check(PatchXbe(path) == TRUE, "PatchXbe result");
	data = ReadAll(path);
	Check(Has(data, 0x180 + 0x9C, "FF010040"), "allowed media");
	Check(Has(data, 0x700, "744BE8CAFDFFFF85C0EB0633C05050EB44F605"), "patch 1");
	Check(Has(data, 0x1000, "90909090C745"), "patch 4");
	Check(PatchXbe(path) == FALSE, "PatchXbe again");
	Check(PatchXbe("build/no_such.xbe") == FALSE, "PatchXbe missing file");

	// The title: written, read back, and the quirk's extra word.
	const char *copy = "build/title_test.xbe";
	std::vector<BYTE> xbe = ReadAll("UnleashX_0_39_0528A/Default.xbe");
	if (xbe.empty())
	{
		printf("FAIL: no Default.xbe\n");
		return 1;
	}
	WriteAll(copy, xbe);
	Check(SetXbeTitleName(copy, L"Renamed Launcher") == TRUE, "SetXbeTitleName");
	Check(GetXbeTitleName(copy) == "Renamed Launcher", "title read back");
	Check(SetXbeTitleName(copy, L"Renamed Launcher") == TRUE, "SetXbeTitleName same");
	Check(SetXbeTitleName(copy, L"") == FALSE, "SetXbeTitleName empty");
	DWORD base, certificate;
	memcpy(&base, &xbe[0x104], 4);
	memcpy(&certificate, &xbe[0x118], 4);
	std::vector<BYTE> renamed = ReadAll(copy);
	size_t alternate = certificate - base + 0x5C;
	Check(renamed[alternate] == 0 && renamed[alternate + 1] == 0 && renamed[alternate + 2] == xbe[alternate + 2],
	      "41st character clears the alternate title ID's low word");

	if (s_failures == 0)
		printf("xbepatch_test: ok\n");
	return s_failures != 0;
}
