// Path and file helpers from the MS dashboard's code. Only DashStrCopyN
// (CWMAPump::OpenFile) and DashCreateFile (CdTocOpen) are used; the rest is
// kept as it was linked in (the unused entry points aren't static so they
// stay compiled). Xbox only (XAPI file calls, _mbs*).
#include "DashUtil.h"

#include <cstdio>
#include <cstring>
#ifdef _XBOX
#include <mbstring.h>
#endif

#include "../common/XboxKernel.h"
// 0x0007F367 (dst in ecx, src in edx)
void DashStrCopyN(char *dst, const char *src, int count)
{
	while (count > 0)
	{
		char c = *src++;
		count--;
		*dst++ = c;
		if (c == 0)
			break;
	}
}

// 0x0007F36F (unused): the same into a wide buffer.
void DashStrCopyNWide(wchar_t *dst, const char *src, int count)
{
	while (count > 0)
	{
		wchar_t c = (unsigned char)*src++;
		count--;
		*dst++ = c;
		if (c == 0)
			break;
	}
}

// 0x0007F66F (path in edx; unused)
// dir, a backslash unless it ends in one, then name.
static void DashJoinPath(char *path, const char *dir, const char *name)
{
	size_t length = strlen(dir);
	if (path != dir)
		memcpy(path, dir, length);
	if (path[length - 1] != '\\')
		path[length++] = '\\';
	strcpy(path + length, name);
}

// 0x0007F6C0 (path in edx; unused)
// Quirk: the copy (0x103 characters) isn't terminated for a long path.
static BOOL DashFileExists(const char *path)
{
	char name[MAX_PATH];
	WIN32_FILE_ATTRIBUTE_DATA data;
	DashStrCopyN(name, path, 0x103);
	return GetFileAttributesExA(name, GetFileExInfoStandard, &data) != 0;
}

// 0x0007F6F4 (unused)
// The clusters a folder takes: 1 for itself plus each file's size rounded
// up; with recursive, the subfolders too. -1 when cancel is signalled.
// Quirks: a subfolder's path is built from an uninitialized buffer instead
// of its name, and a cancelled subfolder's -1 is added to the count.
int DashScanFolder(const char *dir, DWORD clusterSize, BOOL recursive, HANDLE cancel)
{
	if (cancel != NULL && WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0)
		return -1;
	char path[MAX_PATH];
	char pattern[MAX_PATH];
	char garbage[MAX_PATH];
	DashJoinPath(path, dir, "*.*");
	int count = 1;
	BOOL cancelled = FALSE;
	DashStrCopyN(pattern, path, 0x104);
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirstFileA(pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	do
	{
		if (cancel != NULL && WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0)
		{
			cancelled = TRUE;
			break;
		}
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			count += (data.nFileSizeLow + clusterSize - 1) / clusterSize;
		}
		else if (recursive && data.cFileName[0] != '.')
		{
			DashJoinPath(path, dir, garbage);
			count += DashScanFolder(path, clusterSize, TRUE, cancel);
		}
	} while (FindNextFileA(find, &data));
	CloseHandle(find);   // (not FindClose)
	return cancelled ? -1 : count;
}

// 0x0007F807 (path in eax; unused)
// The first "<dir>\<8 hex digits>" that doesn't exist: its number, and the
// path in path (if given).
static int DashCountNumberedFiles(char *path, const char *dir)
{
	char buffer[MAX_PATH];
	if (path == NULL)
		path = buffer;
	int i = 0;
	for (;;)
	{
		sprintf(path, "%s\\%08x", dir, i);
		if (!DashFileExists(path))
			break;
		i++;
	}
	return i;
}

// 0x0007F849 (relative in eax, out in ecx, base in ebx; unused)
// relative against base: kept as it is with a drive, a \\ prefix or no
// usable base; otherwise it replaces base's last part, each "../" (or
// "..\") going up one more, joined with '/'.
static void DashResolveRelativePath(const char *relative, char *out, const char *base)
{
	if (_mbschr((const unsigned char *)relative, ':') != NULL || (relative[0] == '\\' && relative[1] == '\\') ||
	    base == NULL ||
	    (_mbschr((const unsigned char *)base, ':') == NULL && _mbschr((const unsigned char *)base, '/') == NULL))
	{
		strcpy(out, relative);
		return;
	}
	strcpy(out, base);
	char *slash = (char *)_mbsrchr((unsigned char *)out, '/');
	if (slash == NULL)
		slash = (char *)_mbsrchr((unsigned char *)out, '\\');
	// (No separator at all reads through NULL.)
	if (*slash != 0)
	{
		while (relative[0] == '.' && relative[1] == '.' && (relative[2] == '/' || relative[2] == '\\'))
		{
			relative += 3;
			*slash = 0;
			slash = (char *)_mbsrchr((unsigned char *)out, '/');
			if (slash == NULL)
				slash = (char *)_mbsrchr((unsigned char *)out, '\\');
			if (slash == NULL)
				break;
		}
		if (slash != NULL)
		{
			*slash = '/';
			strcpy(slash + 1, relative);
			return;
		}
	}
	// Up past the top: over the last character.
	slash = out + strlen(out) - 1;
	*slash = '/';
	strcpy(slash + 1, relative);
}

// The last resolved base (0x002726B8) and the page's base (0x00272AB8).
static char s_szCurrentPath[0x400];
static char *s_pszBasePath;

// 0x0007FA38 (unused)
static void DashResolveRelativePaths(char *out, const char *relative)
{
	char temp[0x400];
	DashResolveRelativePath(s_szCurrentPath, temp, s_pszBasePath);
	DashResolveRelativePath(relative, out, temp);
}

// 0x0007FA6C (unused)
// Makes relative the current path when it resolves to an absolute one.
void DashSetCurrentPath(const char *relative)
{
	char path[0x400];
	DashResolveRelativePaths(path, relative);
	if (_mbschr((const unsigned char *)path, ':') != NULL || (path[0] == '\\' && path[1] == '\\'))
		strcpy(s_szCurrentPath, path);
}

// 0x0007FAC1 (first in eax; unused)
void DashResolveAgainst(const char *first, char *out, const char *relative)
{
	char temp[0x400];
	const char *base = s_pszBasePath;
	if (first != NULL)
	{
		DashResolveRelativePath(first, temp, s_pszBasePath);
		base = temp;
	}
	DashResolveRelativePath(relative, out, base);
}

// 0x0007F946 (url in eax, path in ecx)
void DashUrlToPath(const char *url, char *path)
{
	if (_mbsnbicmp((const unsigned char *)url, (const unsigned char *)"file:", 5) == 0)
		url += 5;
	for (; *url != 0; url++)
	{
		if (*url == '/')
			*path = '\\';
		else if (*url == '?')
			break;
		else
			*path = *url;
		path++;
	}
	*path = 0;
}

// 0x0007F9F9 (unused)
// The next free numbered name in dir, whose folder is then created.
int DashCreateNumberedFolder(const char *dir)
{
	char path[MAX_PATH];
	char local[MAX_PATH];
	int number = DashCountNumberedFiles(path, dir);
	DashUrlToPath(path, local);
	CreateDirectoryA(local, NULL);
	return number;
}

// 0x00032894
HANDLE DashCreateFile(const char *path, DWORD access, DWORD share, void *security, DWORD disposition,
                      DWORD attributes, HANDLE templateFile)
{
	char local[MAX_PATH];
	DashUrlToPath(path, local);
	return CreateFileA(local, access, share, security, disposition, attributes, templateFile);
}
