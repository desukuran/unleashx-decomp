// An ISO 9660 reader (with Joliet names) for DVD and CD data discs, in place
// of XBMC's iso9660 class that UnleashX linked (src/thirdparty/Iso9660.h).
// Written for this project: same interface, our own code. Sectors come from
// g_ioSupport.ReadSector on the device the first reader opens; the last one
// to go closes it (the DVD poll closes it once more, as with XBMC's).
#include "../src/thirdparty/Iso9660.h"
#include "../src/thirdparty/IoSupport.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#define SECTOR_SIZE 2048

namespace
{

// What the reader keeps in the class's reserved space.
struct SVolume
{
	DWORD rootLba;
	DWORD rootSize;
	BOOL joliet;
	ULONGLONG discSize;
	char name[64];
};

struct SEntry
{
	std::string name;
	DWORD lba;
	DWORD size;
	BOOL directory;
	FILETIME time;
};

struct SFind
{
	std::vector<SEntry> entries;
	size_t next;
};

struct SOpenFile
{
	DWORD lba;
	DWORD size;
	ULONGLONG position;
	DWORD cachedSector;   // 0xFFFFFFFF: none
	BYTE cache[SECTOR_SIZE];
};

int s_nInstances;

DWORD Le32(const BYTE *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((DWORD)p[3] << 24);
}

BOOL ReadSectors(DWORD lba, BYTE *out)
{
	return g_ioSupport.ReadSector(g_hIso9660Device, lba, (char *)out) != -1;
}

// A directory record's 7-byte date (years since 1900, month, day, hour,
// minute, second, offset from GMT in 15 minutes) as a FILETIME in UTC.
FILETIME RecordTime(const BYTE *d)
{
	static const int days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int year = 1900 + d[0], month = d[1] < 1 || d[1] > 12 ? 1 : d[1];
	LONGLONG day = (LONGLONG)(year - 1601) * 365 + (year - 1601) / 4 - (year - 1601) / 100 + (year - 1601) / 400 +
	               days[month - 1] + (d[2] > 0 ? d[2] - 1 : 0);
	if (month > 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
		day++;
	LONGLONG seconds = day * 86400 + d[3] * 3600 + d[4] * 60 + d[5] - (signed char)d[6] * 15 * 60;
	ULONGLONG ticks = (ULONGLONG)seconds * 10000000;
	FILETIME ft;
	ft.dwLowDateTime = (DWORD)ticks;
	ft.dwHighDateTime = (DWORD)(ticks >> 32);
	return ft;
}

// A record's name: Joliet's UCS-2 (big-endian; past Latin-1 becomes '?')
// or ISO 9660's, without the ";1" version and a lone trailing dot.
std::string RecordName(const BYTE *id, int length, BOOL joliet)
{
	std::string name;
	if (joliet)
	{
		for (int i = 0; i + 1 < length; i += 2)
		{
			unsigned c = (id[i] << 8) | id[i + 1];
			name += c < 0x100 ? (char)c : '?';
		}
	}
	else
	{
		name.assign((const char *)id, length);
	}
	size_t semicolon = name.find(';');
	if (semicolon != std::string::npos)
		name.erase(semicolon);
	if (name.size() > 1 && name[name.size() - 1] == '.')
		name.erase(name.size() - 1);
	return name;
}

// The entries of the directory at lba (size bytes), without "." and "..".
BOOL ReadDirectory(const SVolume *volume, DWORD lba, DWORD size, std::vector<SEntry> &entries)
{
	BYTE sector[SECTOR_SIZE];
	for (DWORD offset = 0; offset < size; offset += SECTOR_SIZE)
	{
		if (!ReadSectors(lba + offset / SECTOR_SIZE, sector))
			return FALSE;
		// Records don't cross sectors; a zero length pads to the next one.
		for (int p = 0; p < SECTOR_SIZE && sector[p] != 0;)
		{
			const BYTE *record = sector + p;
			int length = record[0];
			if (p + length > SECTOR_SIZE || length < 34)
				break;
			int nameLength = record[32];
			const BYTE *id = record + 33;
			if (!(nameLength == 1 && (id[0] == 0 || id[0] == 1)))
			{
				SEntry entry;
				entry.name = RecordName(id, nameLength, volume->joliet);
				entry.lba = Le32(record + 2);
				entry.size = Le32(record + 10);
				entry.directory = (record[25] & 2) != 0;
				entry.time = RecordTime(record + 18);
				entries.push_back(entry);
			}
			p += length;
		}
	}
	return TRUE;
}

BOOL MatchNoCase(const char *pattern, const char *name)
{
	for (; *pattern != 0; pattern++, name++)
	{
		if (*pattern == '*')
		{
			for (pattern++; ; name++)
			{
				if (MatchNoCase(pattern, name))
					return TRUE;
				if (*name == 0)
					return FALSE;
			}
		}
		if (*name == 0 || (*pattern != '?' && tolower((unsigned char)*pattern) != tolower((unsigned char)*name)))
			return FALSE;
	}
	return *name == 0;
}

// The parts of a path: "D:\dir\sub/file" → dir, sub, file.
std::vector<std::string> SplitPath(const char *path)
{
	std::vector<std::string> parts;
	if (path[0] != 0 && path[1] == ':')
		path += 2;
	std::string part;
	for (; ; path++)
	{
		if (*path == '\\' || *path == '/' || *path == 0)
		{
			if (part.size() != 0)
				parts.push_back(part);
			part.clear();
			if (*path == 0)
				break;
		}
		else
		{
			part += *path;
		}
	}
	return parts;
}

// Walks parts[0..count) from the root; the last one found in *found.
BOOL Lookup(const SVolume *volume, const std::vector<std::string> &parts, size_t count, SEntry *found)
{
	found->name = "\\";
	found->lba = volume->rootLba;
	found->size = volume->rootSize;
	found->directory = TRUE;
	memset(&found->time, 0, sizeof(found->time));
	for (size_t i = 0; i < count; i++)
	{
		if (!found->directory)
			return FALSE;
		std::vector<SEntry> entries;
		if (!ReadDirectory(volume, found->lba, found->size, entries))
			return FALSE;
		size_t j = 0;
		while (j < entries.size() && !MatchNoCase(parts[i].c_str(), entries[j].name.c_str()))
			j++;
		if (j == entries.size())
			return FALSE;
		*found = entries[j];
	}
	return TRUE;
}

void FillFindData(const SEntry &entry, WIN32_FIND_DATAA *data)
{
	memset(data, 0, sizeof(*data));
	data->dwFileAttributes = entry.directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_READONLY;
	data->ftCreationTime = data->ftLastAccessTime = data->ftLastWriteTime = entry.time;
	data->nFileSizeLow = entry.directory ? 0 : entry.size;
	strncpy(data->cFileName, entry.name.c_str(), sizeof(data->cFileName) - 1);
}

}   // namespace

static SVolume *GetVolume(iso9660 *iso)
{
	return (SVolume *)iso->m_reserved;
}

// Opens the drive for the first reader and reads the volume descriptors
// (sector 16 on): the primary one, then a Joliet supplementary one if any.
iso9660::iso9660()
{
	memset(m_reserved, 0, sizeof(m_reserved));
	if (g_hIso9660Device == NULL || g_hIso9660Device == INVALID_HANDLE_VALUE)
		g_hIso9660Device = g_ioSupport.OpenCDROM(0);
	s_nInstances++;

	SVolume *volume = GetVolume(this);
	BYTE sector[SECTOR_SIZE];
	for (DWORD lba = 16; lba < 32 && ReadSectors(lba, sector); lba++)
	{
		if (memcmp(sector + 1, "CD001", 5) != 0 || sector[0] == 0xFF)
			break;
		BOOL joliet = sector[0] == 2 && sector[88] == '%' && sector[89] == '/' &&
		              (sector[90] == '@' || sector[90] == 'C' || sector[90] == 'E');
		if (sector[0] != 1 && !joliet)
			continue;
		volume->rootLba = Le32(sector + 156 + 2);
		volume->rootSize = Le32(sector + 156 + 10);
		volume->joliet = joliet;
		if (sector[0] == 1)
		{
			volume->discSize = (ULONGLONG)Le32(sector + 80) * (sector[128] | (sector[129] << 8));
			int n = 32;
			while (n > 0 && sector[40 + n - 1] == ' ')
				n--;
			memcpy(volume->name, sector + 40, n);
			volume->name[n] = 0;
		}
	}
}

iso9660::~iso9660()
{
	if (--s_nInstances == 0 && g_hIso9660Device != NULL && g_hIso9660Device != INVALID_HANDLE_VALUE)
	{
		g_ioSupport.CloseCDROM(g_hIso9660Device);
		g_hIso9660Device = NULL;
	}
}

// pattern is a folder followed by a name or wildcard ("D:\dir\*").
HANDLE iso9660::FindFirstFile(const char *pattern, WIN32_FIND_DATAA *data)
{
	SVolume *volume = GetVolume(this);
	if (volume->rootLba == 0)
		return INVALID_HANDLE_VALUE;
	std::vector<std::string> parts = SplitPath(pattern);
	if (parts.size() == 0)
		parts.push_back("*");
	SEntry folder;
	if (!Lookup(volume, parts, parts.size() - 1, &folder) || !folder.directory)
		return INVALID_HANDLE_VALUE;
	std::vector<SEntry> entries;
	if (!ReadDirectory(volume, folder.lba, folder.size, entries))
		return INVALID_HANDLE_VALUE;
	SFind *find = new SFind;
	find->next = 0;
	const std::string &name = parts[parts.size() - 1];
	for (size_t i = 0; i < entries.size(); i++)
		if (MatchNoCase(name.c_str(), entries[i].name.c_str()))
			find->entries.push_back(entries[i]);
	if (find->entries.size() == 0)
	{
		delete find;
		return INVALID_HANDLE_VALUE;
	}
	FillFindData(find->entries[find->next++], data);
	return (HANDLE)find;
}

int iso9660::FindNextFile(HANDLE handle, WIN32_FIND_DATAA *data)
{
	SFind *find = (SFind *)handle;
	if (find == NULL || find == INVALID_HANDLE_VALUE || find->next >= find->entries.size())
		return 0;
	FillFindData(find->entries[find->next++], data);
	return 1;
}

bool iso9660::FindClose(HANDLE handle)
{
	if (handle == NULL || handle == INVALID_HANDLE_VALUE)
		return false;
	delete (SFind *)handle;
	return true;
}

HANDLE iso9660::OpenFile(const char *path)
{
	SVolume *volume = GetVolume(this);
	std::vector<std::string> parts = SplitPath(path);
	SEntry entry;
	if (volume->rootLba == 0 || parts.size() == 0 || !Lookup(volume, parts, parts.size(), &entry) || entry.directory)
		return INVALID_HANDLE_VALUE;
	SOpenFile *file = new SOpenFile;
	file->lba = entry.lba;
	file->size = entry.size;
	file->position = 0;
	file->cachedSector = 0xFFFFFFFF;
	return (HANDLE)file;
}

void iso9660::CloseFile(HANDLE handle)
{
	if (handle != NULL && handle != INVALID_HANDLE_VALUE)
		delete (SOpenFile *)handle;
}

long iso9660::ReadFile(HANDLE handle, BYTE *buffer, long size)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file == NULL || file == INVALID_HANDLE_VALUE || size <= 0)
		return -1;
	if (file->position >= file->size)
		return 0;
	if ((ULONGLONG)size > file->size - file->position)
		size = (long)(file->size - file->position);
	long done = 0;
	while (done < size)
	{
		DWORD sector = (DWORD)(file->position / SECTOR_SIZE);
		DWORD offset = (DWORD)(file->position % SECTOR_SIZE);
		if (file->cachedSector != sector)
		{
			if (!ReadSectors(file->lba + sector, file->cache))
				return done > 0 ? done : -1;
			file->cachedSector = sector;
		}
		long chunk = SECTOR_SIZE - offset;
		if (chunk > size - done)
			chunk = size - done;
		memcpy(buffer + done, file->cache + offset, chunk);
		done += chunk;
		file->position += chunk;
	}
	return done;
}

// whence: SEEK_SET, SEEK_CUR or SEEK_END (= FILE_BEGIN, ...). The new
// position, -1 for a bad handle.
LONGLONG iso9660::Seek(HANDLE handle, LONGLONG offset, int whence)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file == NULL || file == INVALID_HANDLE_VALUE)
		return -1;
	LONGLONG position = whence == 1 ? (LONGLONG)file->position + offset
	                  : whence == 2 ? (LONGLONG)file->size + offset : offset;
	if (position < 0)
		position = 0;
	file->position = (ULONGLONG)position;
	return position;
}

LONGLONG iso9660::GetFileSize(HANDLE handle)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file == NULL || file == INVALID_HANDLE_VALUE)
		return -1;
	return file->size;
}

ULONGLONG iso9660::GetDiscSize()
{
	return GetVolume(this)->discSize;
}

const char *iso9660::GetVolumeName()
{
	return GetVolume(this)->name;
}
