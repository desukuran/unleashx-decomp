// Reading the title name, ID and region of an XBE (from its certificate).
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "Storage.h"

// XBE header fields: base address at 0x104, certificate address at 0x118.
// The certificate starts with size, time stamp and title ID, then the
// 40-character title name; the game region is at +0xA0.
#define XBE_BASE_ADDRESS 0x104
#define XBE_CERTIFICATE_ADDRESS 0x118
#define XBE_CERT_TITLE_ID 0x08
#define XBE_CERT_TITLE_NAME 0x0C
#define XBE_CERT_GAME_REGION 0xA0

static BOOL ReadDword(HANDLE file, DWORD offset, DWORD *value)
{
	DWORD read;
	return SetFilePointer(file, offset, NULL, FILE_BEGIN) == offset && ReadFile(file, value, 4, &read, NULL);
}

// The certificate's file offset, or 0.
static DWORD GetCertificateOffset(HANDLE file)
{
	DWORD base, certificate;
	if (!ReadDword(file, XBE_BASE_ADDRESS, &base) || !ReadDword(file, XBE_CERTIFICATE_ADDRESS, &certificate))
		return 0;
	return certificate - base;
}

static std::string ReadTitleName(HANDLE file, DWORD offset)
{
	std::string title;
	if (SetFilePointer(file, offset, NULL, FILE_BEGIN) == offset)
	{
		wchar_t name[41];
		memset(name, 0, sizeof(name));
		DWORD read;
		// The XBE stores UTF-16; wchar_t is wider on some hosts.
		WORD raw[40];
		memset(raw, 0, sizeof(raw));
		if (ReadFile(file, raw, 0x50, &read, NULL))
		{
			for (int i = 0; i < 40; i++)
				name[i] = raw[i];
			title = StrFromWide(name);
		}
	}
	return title;
}

// 0x000A608D
// Without a title name, the XBE's folder name is used
// ("E:\Games\Halo\default.xbe" -> "Halo"). Regions above 7 count as 0.
BOOL ReadXbeInfo(const char *xbePath, SXbeInfo *info)
{
	if (xbePath == NULL || strlen(xbePath) == 0)
		return FALSE;
	HANDLE file = CreateFileA(xbePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0x10000000, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;

	// Too small to be an XBE (if the size can be read at all).
	BY_HANDLE_FILE_INFORMATION fileInfo;
	if (GetFileInformationByHandle(file, &fileInfo) && (fileInfo.nFileSizeLow == 0 || fileInfo.nFileSizeLow < 0x78))
	{
		CloseHandle(file);
		return FALSE;
	}

	DWORD titleId = 0;
	DWORD region = 0;
	std::string title;
	DWORD certificate = GetCertificateOffset(file);
	if (certificate != 0)
	{
		DWORD read;
		if (SetFilePointer(file, certificate + XBE_CERT_TITLE_ID, NULL, FILE_BEGIN) == certificate + XBE_CERT_TITLE_ID)
		{
			titleId = 0;
			ReadFile(file, &titleId, 4, &read, NULL);
			if (read != 4)
				titleId = 0;
		}
		title = ReadTitleName(file, certificate + XBE_CERT_TITLE_NAME);
		if (SetFilePointer(file, certificate + XBE_CERT_GAME_REGION, NULL, FILE_BEGIN) ==
		    certificate + XBE_CERT_GAME_REGION)
		{
			region = 0;
			ReadFile(file, &region, 4, &read, NULL);
			if (read != 4 || region > 7)
				region = 0;
		}
	}
	CloseHandle(file);

	info->titleId = titleId;
	info->gameRegion = region;
	info->titleName = title;
	if (info->titleName.size() == 0)
	{
		std::string folder = xbePath;
		folder = StrLeft(folder, StrReverseFind(folder, "\\"));
		int slash = StrReverseFind(folder, "\\");
		folder = StrMid(folder, slash + 1, (int)folder.size() - (slash + 1));
		StrReplaceAll(folder, "\\", "");
		info->titleName = folder;
	}
	return TRUE;
}

// 0x000A5F49
std::string GetXbeTitleName(const char *xbePath)
{
	if (xbePath == NULL || strlen(xbePath) == 0)
		return "";
	std::string title;
	HANDLE file = CreateFileA(xbePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		DWORD certificate = GetCertificateOffset(file);
		if (certificate != 0)
			title = ReadTitleName(file, certificate + XBE_CERT_TITLE_NAME);
		CloseHandle(file);
	}
	StrTrimRight(StrTrimLeft(title));
	return title;
}

// 0x0009E08A
BOOL ReadXbeHeader(const char *xbePath, SXbeHeaderInfo *info)
{
	memset(info, 0, sizeof(*info));
	BOOL ok = FALSE;
	HANDLE file = CreateFileA(xbePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		DWORD read;
		SetFilePointer(file, 0, NULL, FILE_BEGIN);
		if (ReadFile(file, info, 0x178, &read, NULL))
		{
			SetFilePointer(file, info->sizeOfImageHeader, NULL, FILE_BEGIN);
			if (ReadFile(file, &info->certificate, 0x1D0, &read, NULL) == TRUE)
				ok = TRUE;
		}
		CloseHandle(file);
	}
	return ok;
}

// The kernel device behind each drive letter.
static const char *const g_szHarddiskPartition = "\\Device\\Harddisk0\\Partition";   // 0x0026BC58
static const char *const g_szCdrom = "\\Device\\Cdrom0";                              // 0x0026BC54

// 0x000A5C05
std::string GetXbeDevicePath(const char *xbePath)
{
	std::string dir = GetPathDirectory(xbePath);
	std::string device;
	std::string rest;
	if (dir.size() >= 2)
		rest = StrMid(dir, 2, (int)dir.size() - 2);
	// (Each letter is looked for with a search that must hit at position 0.)
	if (StrFind(dir, "C:") == 0 || StrFind(dir, "c:") == 0)
		StrFormat(device, "%s2%s", g_szHarddiskPartition, rest.c_str());
	else if (StrFind(dir, "E:") == 0 || StrFind(dir, "e:") == 0)
		StrFormat(device, "%s1%s", g_szHarddiskPartition, rest.c_str());
	else if (StrFind(dir, "F:") == 0 || StrFind(dir, "f:") == 0)
		StrFormat(device, "%s6%s", g_szHarddiskPartition, rest.c_str());
	else if (StrFind(dir, "D:") == 0 || StrFind(dir, "d:") == 0)
		StrFormat(device, "%s%s", g_szCdrom, rest.c_str());
	else if (StrFind(dir, "G:") == 0 || StrFind(dir, "g:") == 0)
		StrFormat(device, "%s7%s", g_szHarddiskPartition, rest.c_str());
	else if (dir.size() >= 2 && dir[1] == ':')
	{
		// Other letters: a lowercase one counts from 'a' as the partition
		// number. Quirk: uppercase H-Z (and a, b) give an empty path.
		int partition = -1;
		char letter = dir[0];
		if (letter > 'e' && letter <= 'z')
			partition = letter - 0x60;
		if (partition >= 1)
			StrFormat(device, "%s%d%s", g_szHarddiskPartition, partition, rest.c_str());
	}
	else if (dir.size() == 0)
		device = g_szCdrom;
	else if (dir[0] == '\\')
		StrFormat(device, "%s%s", g_szCdrom, dir.c_str());
	else
		StrFormat(device, "%s\\%s", g_szCdrom, dir.c_str());
	return device;
}
