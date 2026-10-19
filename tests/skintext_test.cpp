// Prints every skin text source's value, using the host stand-ins.
#include <cstdio>
#include <cwchar>

#include "../src/settings/AppSettings.h"
#include "../src/skin/Skin.h"
#include "../src/skin/SkinText.h"

int main()
{
	CAppSettings::GetInstance()->m_strHomePath = "e:\\dash\\";
	int failures = 0;
	for (int source = 0; source <= TEXT_SOURCE_COUNT; source++)
	{
		const wchar_t *value = GetSkinTextValue(source);
		if (value == NULL)
		{
			printf("0x%02X: NULL\n", source);
			failures++;
			continue;
		}
		if (*value)
			printf("0x%02X: %ls\n", source, value);
	}

	// Spot checks of fixed values.
	struct
	{
		int source;
		const wchar_t *expected;
	} checks[] = {
		{TEXT_APPVERSION, L"0.39.0528A"},
		{TEXT_APPVERSIONBUILD, L"0.39.0528A Build 584"},
		{TEXT_HOMEPATH, L"e:\\dash\\"},
		{TEXT_LINKSTATUS, L"No Link"},
		{TEXT_VIDEOMODE, L"NTSC M"},
		{TEXT_HDDSERIAL, L"HOSTDISK1  FW: 3.10"},
		{TEXT_DVDSERIAL, L"(Not Supported)  FW: 3.10"},
		{TEXT_CPUTEMPC, L"40.00 C"},
		{TEXT_CPUTEMPF, L"104.50 F"},
		{TEXT_FREEMEMORY, L"40960 KB"},
		{TEXT_C_FREEMB, L"0"},
		{TEXT_G_PFREE, L"0%"},
		{TEXT_THREAD, L"0"},
	};
	for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++)
	{
		const wchar_t *value = GetSkinTextValue(checks[i].source);
		if (wcscmp(value, checks[i].expected) != 0)
		{
			printf("FAIL 0x%02X: '%ls', expected '%ls'\n", checks[i].source, value, checks[i].expected);
			failures++;
		}
	}
	InvalidateSkinText(TEXT_APPVERSION);
	if (wcscmp(GetSkinTextValue(TEXT_APPVERSION), L"0.39.0528A") != 0)
		failures++;
	return failures != 0;
}
