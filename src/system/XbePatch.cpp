// Changing XBE files in place: the "patch for HDD" of the file manager and
// copied DVDs (allowed media plus four byte patches), and renaming an XBE's
// title in its certificate.
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "Storage.h"

// The longest hex string the patches use, in bytes.
#define PATCH_MAX_BYTES 100
// The search window; consecutive windows overlap by the pattern's length.
#define PATCH_WINDOW 600

// The four patches: what's searched for, and what it's replaced with.
// 1, 2: a "jge" after a call becomes "jmp" (the media check's result is
// ignored); 3: a "FATX....D:\" string is cleared; 4: a "test al, 1 / jnz"
// becomes nops.
static const char *const s_patchFind[4] = {
	// 0x0026D124
	"744BE8CAFDFFFF85C07D0633C05050EB44F605",
	"E8CAFDFFFF85C07D",
	"4641545800000000443A5C",
	"A801750CC745",
};
static const char *const s_patchReplace[4] = {
	// 0x0026D114
	"744BE8CAFDFFFF85C0EB0633C05050EB44F605",
	"E8CAFDFFFF85C0EB",
	"0000000000000000000000",
	"90909090C745",
};

// 0x0009F58D (hex in eax)
// 0 when written; -1 bad hex, -2 the file can't be opened, -3 short write
// (then the file is left open).
int WriteXbePatchBytes(const char *hex, const char *xbePath, long offset)
{
	if (strlen(hex) & 1)
		return -1;
	size_t count = strlen(hex) / 2;
	BYTE bytes[PATCH_MAX_BYTES];
	if (HexToBytes(hex, bytes) != 0)
		return -1;
	SetFileAttributesA(xbePath, FILE_ATTRIBUTE_NORMAL);
	FILE *file = fopen(xbePath, "r+b");
	if (file == NULL)
		return -2;
	fseek(file, offset, SEEK_SET);
	if (fwrite(bytes, 1, count, file) != count)
		return -3;
	fclose(file);
	return 0;
}

// 0x0009F62A (hex in eax)
// The file offset of the first match of hex from start on; -1 if none (or
// bad hex), -2 if the file can't be opened. Quirks: after a mismatch the
// byte isn't tried as the pattern's start, and the scan goes on to the
// window's end after a match (comparing past the pattern; a second match
// moves the result again).
long CheckXbePatchBytes(const char *hex, const char *xbePath, long start)
{
	if (strlen(hex) & 1)
		return -1;
	int count = (int)(strlen(hex) / 2);
	BYTE pattern[PATCH_MAX_BYTES];
	if (HexToBytes(hex, pattern) != 0)
		return -1;
	FILE *file = fopen(xbePath, "rb");
	if (file == NULL)
		return -2;

	long offset = start;
	BOOL found = FALSE;
	for (;;)
	{
		fseek(file, offset, SEEK_SET);
		BYTE window[PATCH_WINDOW];
		int read = (int)fread(window, 1, PATCH_WINDOW, file);
		if (read < count)
			break;
		int matched = 0;
		for (int i = 0; i < read; i++)
		{
			if (pattern[matched] == window[i])
			{
				if (matched == count - 1)
				{
					offset += i - count + 1;
					found = TRUE;
				}
				matched++;
			}
			else
			{
				matched = 0;
			}
		}
		if (found)
			break;
		offset += PATCH_WINDOW - count;
	}
	fclose(file);
	return found ? offset : -1;
}

// 0x000A173D (index in eax)
// 1 if the pattern isn't there, 2 when patched, 3 if the write failed.
static int ApplyXbePatch(int index, const char *xbePath)
{
	long offset = CheckXbePatchBytes(s_patchFind[index], xbePath, 0);
	if (offset < 0)
		return 1;
	return WriteXbePatchBytes(s_patchReplace[index], xbePath, offset) != 0 ? 3 : 2;
}

// 0x000A177A
// Writes the hex bytes over the certificate's allowed media field (the
// certificate is taken to start at the image header's size). Its offset,
// or 0 on failure.
static long PatchXbeMediaFlags(const char *xbePath, const char *hex)
{
	FILE *file = fopen(xbePath, "rb");
	if (file == NULL)
		return 0;
	SXbeHeaderInfo info;
	// (Neither read is checked.)
	size_t read = fread(info.header, 1, 0x178, file);
	fseek(file, (long)info.sizeOfImageHeader, SEEK_SET);
	// (The certificate is read but not used.)
	read = fread(&info.certificate, 1, sizeof(info.certificate), file);
	(void)read;
	fclose(file);
	long offset = (long)info.sizeOfImageHeader + 0x9C;
	if (WriteXbePatchBytes(hex, xbePath, offset) != 0)
		return 0;
	return offset;
}

// 0x000A2245 (path in edi)
BOOL PatchXbe(const char *xbePath)
{
	if (!FileExists(xbePath))
		return FALSE;
	// Allowed media 0x400001FF: every medium, including the hard disk.
	PatchXbeMediaFlags(xbePath, "FF010040");
	BOOL patched = FALSE;
	for (int i = 0; i < 4; i++)
	{
		if (ApplyXbePatch(i, xbePath) == 2)
			patched = TRUE;
	}
	return patched;
}

// The title as stored: 40 UTF-16 characters.
#define XBE_TITLE_LENGTH 40

static size_t TitleLength(const WORD *s)
{
	size_t n = 0;
	while (s[n] != 0)
		n++;
	return n;
}

static BOOL TitleEquals(const wchar_t *title, const WORD *stored)
{
	size_t i = 0;
	for (; title[i] != 0 && stored[i] != 0; i++)
	{
		if ((WORD)title[i] != stored[i])
			return FALSE;
	}
	return title[i] == 0 && stored[i] == 0;
}

// 0x0009DB0C (path in eax)
// Quirk: 41 characters are written, so the low word of the first
// alternate title ID is cleared too.
BOOL SetXbeTitleName(const char *xbePath, const wchar_t *title)
{
	if (xbePath == NULL || strlen(xbePath) == 0 || title == NULL || wcslen(title) == 0)
		return FALSE;
	SetFileAttributesA(xbePath, FILE_ATTRIBUTE_NORMAL);
	HANDLE file = CreateFileA(xbePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
	                          NULL);
	if (file == NULL || file == INVALID_HANDLE_VALUE)
		return FALSE;

	BOOL result = FALSE;
	DWORD baseAddress, certificateAddress, read;
	if (SetFilePointer(file, 0x104, NULL, FILE_BEGIN) == 0x104 && ReadFile(file, &baseAddress, 4, &read, NULL) &&
	    SetFilePointer(file, 0x118, NULL, FILE_BEGIN) == 0x118 &&
	    ReadFile(file, &certificateAddress, 4, &read, NULL))
	{
		// The certificate's title, as a file offset.
		LONG offset = (LONG)(certificateAddress + 0x0C - baseAddress);
		WORD stored[XBE_TITLE_LENGTH + 2];
		if (SetFilePointer(file, offset, NULL, FILE_BEGIN) == (DWORD)offset)
		{
			memset(stored, 0, (XBE_TITLE_LENGTH + 1) * sizeof(WORD));
			if (ReadFile(file, stored, XBE_TITLE_LENGTH * sizeof(WORD), &read, NULL))
			{
				if (TitleEquals(title, stored))
				{
					CloseHandle(file);
					return TRUE;
				}
				// (Compared again when the stored title isn't empty; it can't
				// match by then.)
				if (!(TitleLength(stored) != 0 && TitleEquals(title, stored)) &&
				    SetFilePointer(file, offset, NULL, FILE_BEGIN) == (DWORD)offset)
				{
					DWORD written = 0;
					memset(stored, 0, XBE_TITLE_LENGTH * sizeof(WORD));
					size_t length = wcslen(title);
					if (length > XBE_TITLE_LENGTH)
						length = XBE_TITLE_LENGTH;
					for (size_t i = 0; i < length; i++)
						stored[i] = (WORD)title[i];
					if (TitleLength(stored) != 0 &&
					    WriteFile(file, stored, (XBE_TITLE_LENGTH + 1) * sizeof(WORD), &written, NULL) &&
					    written != 0)
						result = TRUE;
				}
			}
		}
	}
	CloseHandle(file);
	return result;
}
