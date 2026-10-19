// DVD drive / disc state. The state machine that updates these lives in the
// disc polling (RefreshDrives in Drives.cpp; PollDvdDrive 0x000AB9D8 TODO: decompile).
#include <cwchar>

#include "Drives.h"
#include "Storage.h"

int g_nDiscType;                     // 0x0026BBEC DiscType
wchar_t g_szDiscTitle[0x80];         // 0x00276278
static BOOL g_bTrayOpen;             // 0x00276274 (BYTE)
static BOOL g_bTrayOpening;          // 0x00276275 (BYTE)
static BOOL g_bTrayClosed;           // 0x0026BBE8 (BYTE) TODO: verify
static BOOL g_bTrayEmpty;            // 0x0026BBE9 (BYTE) TODO: verify

// 0x000AA903
int GetDiscType()
{
	return g_nDiscType;
}

// 0x000AA909
BOOL IsDiscPresent()
{
	return g_nDiscType != DISC_UNKNOWN && g_nDiscType != DISC_NONE;
}

// 0x000AA91E
const wchar_t *GetDiscTitleIfPresent()
{
	return IsDiscPresent() ? g_szDiscTitle : L"";
}

// 0x000AA7E8
const wchar_t *GetDiscTypeName()
{
	switch (g_nDiscType)
	{
	case DISC_UNKNOWN: return L"Unknown";
	case DISC_GAME: return L"Game";
	case DISC_VIDEO: return L"Video";
	case DISC_DATA:
	case DISC_DATA2: return L"Data";
	case DISC_AUDIO: return L"Audio CD";
	case DISC_CHECKING: return L"Checking";
	case DISC_TRAY_MOVING: return L"Init";
	}
	return g_bTrayOpen ? L"Open" : L"Empty";
}

// 0x000AA85B
// $Tray$ text.
const wchar_t *GetTrayStateName()
{
	switch (g_nDiscType)
	{
	case DISC_UNKNOWN: return L"Unknown";
	case DISC_GAME: return L"Game";
	case DISC_VIDEO: return L"Video";
	case DISC_DATA:
	case DISC_DATA2: return L"Data";
	case DISC_AUDIO: return L"Audio CD";
	case DISC_CHECKING: return L"Checking";
	case DISC_TRAY_MOVING:
		if (g_bTrayOpening)
			return L"Opening";
		return g_bTrayClosed ? L"Closed" : L"Closing";
	default:
		if (g_bTrayOpen)
			return L"Open";
		if (g_bTrayEmpty || !g_bTrayClosed)
			return L"Empty";
		return L"Closed";
	}
}

// 0x000AB285
// $DVDTitle$: a game's title (read from D:\default.xbe once), a data disc's
// DiscTitle.txt name, otherwise the disc type.
const wchar_t *GetDiscTitle()
{
	if (g_nDiscType == DISC_GAME)
	{
		if (wcslen(g_szDiscTitle) == 0)
		{
			std::string name = GetXbeTitleName("D:\\default.xbe");
			if (name.size() != 0)
				MultiByteToWideChar(CP_ACP, 0, name.c_str(), (int)name.size(), g_szDiscTitle, 0x80);
		}
		return g_szDiscTitle;
	}
	if (g_nDiscType == DISC_DATA)
		return g_szDiscTitle;
	return GetDiscTypeName();
}
