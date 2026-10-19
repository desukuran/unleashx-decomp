// The soundtrack database writers (0x000BBFE5-0x000BCF48). See
// SoundtrackDB.h.
#include "SoundtrackDB.h"

#include <cstdio>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"

#ifndef ERROR_INVALID_HANDLE
#define ERROR_INVALID_HANDLE 6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_INVALID_PARAMETER 0x57
#define ERROR_ALREADY_EXISTS 0xB7
#endif
#ifndef ERROR_INVALID_INDEX
#define ERROR_INVALID_INDEX 0x585
#endif
#ifndef FILE_ATTRIBUTE_NORMAL
#define FILE_ATTRIBUTE_NORMAL 0x80
#endif

// The open database. (One block in the original, cleared by
// OpenSoundtrackDB.)
static HANDLE s_hDb = INVALID_HANDLE_VALUE;   // 0x00270FD8
static SSoundtrackDbHeader s_header;          // 0x00270FDC
// The sector last read or about to be written.
static union
{
	BYTE bytes[SOUNDTRACK_DB_SECTOR];
	DWORD magic;
	SSoundtrackDbHeader header;
	SSoundtrackRecord soundtrack;
	SSongGroup group;
} s_sector;                                   // 0x0027117C

// A soundtrack's sector, a song group's sector.
static LONG SoundtrackOffset(UINT slot)
{
	return (LONG)((slot + 1) * SOUNDTRACK_DB_SECTOR);
}

static LONG GroupOffset(DWORD group)
{
	return (LONG)((group + SOUNDTRACK_GROUP_BASE) * SOUNDTRACK_DB_SECTOR);
}

// (A wcsncpy of 32 on the Xbox.)
static void CopyName(STDBCHAR *to, const wchar_t *from)
{
	UINT i = 0;
	for (; i < SOUNDTRACK_NAME_LENGTH && from[i] != 0; i++)
		to[i] = (STDBCHAR)from[i];
	for (; i < SOUNDTRACK_NAME_LENGTH; i++)
		to[i] = 0;
}

static void CopyName(STDBCHAR *to, const STDBCHAR *from)
{
	memcpy(to, from, SOUNDTRACK_NAME_LENGTH * sizeof(STDBCHAR));
}

// 0x000BBFE5
// The soundtrack's slot, or SOUNDTRACK_MAX. (All 100 ids are searched,
// not only the used ones.)
static UINT FindSoundtrack(DWORD id)
{
	UINT slot = 0;
	do
	{
		if (s_header.soundtrackIds[slot] == id)
			break;
		slot++;
	} while (slot < SOUNDTRACK_MAX);
	return slot;
}

// 0x000BBFFB
static BOOL WriteSector()
{
	DWORD written;
	if (!WriteFile(s_hDb, s_sector.bytes, SOUNDTRACK_DB_SECTOR, &written, NULL) || written != SOUNDTRACK_DB_SECTOR)
		return FALSE;
	return TRUE;
}

// 0x000BC042
// Reads the next sector; with a magic, it must be that kind. (magic in
// edi)
static BOOL ReadSector(DWORD magic)
{
	DWORD read;
	if (!ReadFile(s_hDb, s_sector.bytes, SOUNDTRACK_DB_SECTOR, &read, NULL) || read != SOUNDTRACK_DB_SECTOR ||
	    (magic != 0 && s_sector.magic != magic))
		return FALSE;
	return TRUE;
}

static void Seek(LONG offset)
{
	SetFilePointer(s_hDb, offset, NULL, FILE_BEGIN);
}

// Back to the start of the sector just read.
static void SeekBack()
{
	SetFilePointer(s_hDb, -SOUNDTRACK_DB_SECTOR, NULL, FILE_CURRENT);
}

// 0x000BC02F
void CloseSoundtrackDB()
{
	CloseHandle(s_hDb);
	s_hDb = INVALID_HANDLE_VALUE;
}

// 0x000BC7F4
// (Only the header's 0x1A0 bytes are refreshed in the sector buffer; the
// rest of the sector is what was there.)
static BOOL WriteHeader()
{
	memcpy(s_sector.bytes, &s_header, sizeof(s_header));
	Seek(0);
	return WriteSector();
}

// 0x000BC79A
// A free song group: the first one marked unused, else a new one at the
// end of the file. Quirk: a sector that isn't a song group stops the scan
// short, so the free group appended after it is never reached and the
// loop appends forever.
static DWORD FindFreeSongGroup()
{
	for (;;)
	{
		DWORD group = 0;
		Seek(GroupOffset(0));
		while (ReadSector(SOUNDTRACK_GROUP_MAGIC))
		{
			if (!s_sector.group.inUse)
				return group;
			group++;
		}
		// (The rest of the new sector is whatever was read last.)
		SetFilePointer(s_hDb, 0, NULL, FILE_END);
		s_sector.group.inUse = FALSE;
		s_sector.magic = SOUNDTRACK_GROUP_MAGIC;
		WriteSector();
	}
}

// 0x000BCE2C
BOOL OpenSoundtrackDB()
{
	s_hDb = NULL;
	memset(&s_header, 0, sizeof(s_header));
	memset(&s_sector, 0, sizeof(s_sector));
	CreateDirectoryTree(SOUNDTRACK_MUSIC_DIR);
	s_hDb = CreateFileA(SOUNDTRACK_DB_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                    FILE_ATTRIBUTE_NORMAL, NULL);
	if (s_hDb != INVALID_HANDLE_VALUE)
	{
		if (ReadSector(0))
		{
			memcpy(&s_header, s_sector.bytes, sizeof(s_header));
			return TRUE;
		}
		CloseHandle(s_hDb);
		s_hDb = INVALID_HANDLE_VALUE;
	}
	// A new database: the header (version 1) and 100 empty soundtrack
	// sectors, then opened again.
	if (GetLastError() == 2 /* ERROR_FILE_NOT_FOUND */)
	{
		s_hDb = CreateFileA(SOUNDTRACK_DB_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
		                    FILE_ATTRIBUTE_NORMAL, NULL);
		if (s_hDb != INVALID_HANDLE_VALUE)
		{
			const DWORD size = GroupOffset(0);
			BYTE *data = new BYTE[size];   // (LocalAlloc(LPTR) in the original)
			memset(data, 0, size);
			*(DWORD *)data = 1;
			DWORD written;
			BOOL ok = WriteFile(s_hDb, data, size, &written, NULL) && written == size;
			CloseHandle(s_hDb);
			delete[] data;
			if (ok)
				return OpenSoundtrackDB();
			s_hDb = INVALID_HANDLE_VALUE;
		}
	}
	return s_hDb != INVALID_HANDLE_VALUE;
}

// 0x000BCD3D
BOOL CreateSoundtrack(const wchar_t *name, DWORD *id)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	if (s_header.numSoundtracks == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	memset(&s_sector.soundtrack, 0, sizeof(s_sector.soundtrack));
	s_sector.soundtrack.id = s_header.nextSoundtrackId++;
	*id = s_sector.soundtrack.id;
	s_sector.magic = SOUNDTRACK_MAGIC;
	CopyName(s_sector.soundtrack.name, name);
	UINT slot = s_header.numSoundtracks++;
	s_header.soundtrackIds[slot] = s_sector.soundtrack.id;
	Seek(SoundtrackOffset(slot));
	char folder[MAX_PATH];
	sprintf(folder, "%s%04x", SOUNDTRACK_MUSIC_DIR, *id);
	if (!CreateDirectoryTree(folder) && GetLastError() != ERROR_ALREADY_EXISTS)
		return FALSE;
	if (!WriteSector() || !WriteHeader())
		return FALSE;
	return TRUE;
}

// 0x000BC151
BOOL RenameSoundtrack(DWORD soundtrackId, const wchar_t *name)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	Seek(SoundtrackOffset(slot));
	if (!ReadSector(SOUNDTRACK_MAGIC))
		return FALSE;
	CopyName(s_sector.soundtrack.name, name);
	SeekBack();
	return WriteSector();
}

// 0x000BCB01
BOOL DeleteSoundtrack(DWORD soundtrackId)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	Seek(SoundtrackOffset(slot));
	BOOL ok = ReadSector(SOUNDTRACK_MAGIC);
	if (!ok)
		return FALSE;
	SSoundtrackRecord soundtrack;
	memcpy(&soundtrack, &s_sector.soundtrack, sizeof(soundtrack));

	// Free its song groups.
	UINT song = 0;
	const DWORD *group = soundtrack.songGroups;
	if (soundtrack.numSongs != 0)
	{
		do
		{
			Seek(GroupOffset(*group));
			ok = ReadSector(SOUNDTRACK_GROUP_MAGIC);
			if (ok)
			{
				s_sector.group.inUse = FALSE;
				SeekBack();
				ok = WriteSector();
			}
			song += SOUNDTRACK_GROUP_SONGS;
			group++;
		} while (song < soundtrack.numSongs);
	}
	if (!ok)
		return FALSE;

	// Move the later soundtracks up a slot.
	LONG offset = SoundtrackOffset(slot + 1);
	for (UINT next = slot + 1; next < s_header.numSoundtracks && ok; next++)
	{
		s_header.soundtrackIds[next - 1] = s_header.soundtrackIds[next];
		Seek(offset);
		ok = ReadSector(SOUNDTRACK_MAGIC);
		if (ok)
		{
			SetFilePointer(s_hDb, -2 * SOUNDTRACK_DB_SECTOR, NULL, FILE_CURRENT);
			ok = WriteSector();
		}
		offset += SOUNDTRACK_DB_SECTOR;
	}
	if (!ok)
		return FALSE;
	s_header.numSoundtracks--;
	WriteHeader();

	// Its files and folder. (MAX_PATH in the original.)
	char path[MAX_PATH + 32];
	sprintf(path, "%s%04x\\*.*", SOUNDTRACK_MUSIC_DIR, soundtrackId);
	WIN32_FIND_DATAA found;
	HANDLE find = FindFirstFileA(path, &found);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				sprintf(path, "%s%04x\\%s", SOUNDTRACK_MUSIC_DIR, soundtrackId, found.cFileName);
				if (!DeleteFileA(path))
				{
					SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
					ok = DeleteFileA(path);
				}
			}
		} while (FindNextFileA(find, &found));
		CloseHandle(find);
	}
	if (ok)
	{
		sprintf(path, "%s%04x", SOUNDTRACK_MUSIC_DIR, soundtrackId);
		ok = RemoveDirectoryA(path);
	}
	return ok;
}

// 0x000BC851
BOOL SwapSoundtracks(DWORD firstId, DWORD secondId)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT first = FindSoundtrack(firstId);
	if (first == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	LONG firstOffset = SoundtrackOffset(first);
	Seek(firstOffset);
	if (!ReadSector(SOUNDTRACK_MAGIC))
		return FALSE;
	SSoundtrackRecord saved;
	memcpy(&saved, &s_sector.soundtrack, sizeof(saved));
	UINT second = FindSoundtrack(secondId);
	if (second == SOUNDTRACK_MAX)
		return FALSE;
	LONG secondOffset = SoundtrackOffset(second);
	Seek(secondOffset);
	if (!ReadSector(SOUNDTRACK_MAGIC))
		return FALSE;
	Seek(firstOffset);
	if (!WriteSector())
		return FALSE;
	Seek(secondOffset);
	memcpy(&s_sector.soundtrack, &saved, sizeof(saved));
	if (!WriteSector())
		return FALSE;
	s_header.soundtrackIds[first] = s_header.soundtrackIds[second];
	s_header.soundtrackIds[second] = saved.id;
	WriteHeader();
	return TRUE;
}

// 0x000BC81C
DWORD AllocateSongId(DWORD soundtrackId)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return 0xFFFFFFFF;
	}
	DWORD serial = s_header.nextSongId & 0xFFFF;
	s_header.nextSongId++;
	WriteHeader();
	return serial | (soundtrackId << 16);
}

// 0x000BC965
// Returns whether the song group was read (not whether it was written).
BOOL AddSong(DWORD soundtrackId, DWORD songId, DWORD timeMs, const wchar_t *name)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	LONG offset = SoundtrackOffset(slot);
	Seek(offset);
	if (!ReadSector(SOUNDTRACK_MAGIC))
		return FALSE;
	SSoundtrackRecord soundtrack;
	memcpy(&soundtrack, &s_sector.soundtrack, sizeof(soundtrack));
	if (soundtrack.numSongs >= SOUNDTRACK_MAX_SONGS)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	soundtrack.totalTimeMs += timeMs;
	UINT song = soundtrack.numSongs++;
	UINT groupIndex = song / SOUNDTRACK_GROUP_SONGS;
	UINT place = song % SOUNDTRACK_GROUP_SONGS;
	DWORD group;
	if (place == 0)
		soundtrack.songGroups[groupIndex] = group = FindFreeSongGroup();
	else
		group = soundtrack.songGroups[groupIndex];
	Seek(offset);
	memcpy(&s_sector.soundtrack, &soundtrack, sizeof(soundtrack));
	if (!WriteSector())
		return FALSE;
	Seek(GroupOffset(group));
	BOOL ok = ReadSector(SOUNDTRACK_GROUP_MAGIC);
	if (!ok)
		return FALSE;
	if (place == 0)
	{
		// A new group (0x1C0 bytes cleared).
		memset(&s_sector.group, 0, sizeof(s_sector.group));
		s_sector.group.soundtrackId = soundtrackId;
		s_sector.group.inUse = TRUE;
		s_sector.magic = SOUNDTRACK_GROUP_MAGIC;
		s_sector.group.index = groupIndex;
	}
	s_sector.group.songIds[place] = songId;
	s_sector.group.songTimeMs[place] = timeMs;
	CopyName(s_sector.group.songNames[place], name);
	SeekBack();
	WriteSector();
	return ok;
}

// 0x000BC082
BOOL RenameSong(DWORD soundtrackId, UINT index, const wchar_t *name)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	Seek(SoundtrackOffset(slot));
	if (!ReadSector(SOUNDTRACK_MAGIC))
		return FALSE;
	if (index >= s_sector.soundtrack.numSongs)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	Seek(GroupOffset(s_sector.soundtrack.songGroups[index / SOUNDTRACK_GROUP_SONGS]));
	if (!ReadSector(SOUNDTRACK_GROUP_MAGIC))
		return FALSE;
	CopyName(s_sector.group.songNames[index % SOUNDTRACK_GROUP_SONGS], name);
	SeekBack();
	return WriteSector();
}

// One song's entry in a group.
static void SwapSongEntries(SSongGroup *a, UINT placeA, SSongGroup *b, UINT placeB)
{
	DWORD id = a->songIds[placeA];
	DWORD time = a->songTimeMs[placeA];
	STDBCHAR name[SOUNDTRACK_NAME_LENGTH];
	CopyName(name, a->songNames[placeA]);
	a->songIds[placeA] = b->songIds[placeB];
	a->songTimeMs[placeA] = b->songTimeMs[placeB];
	CopyName(a->songNames[placeA], b->songNames[placeB]);
	b->songIds[placeB] = id;
	b->songTimeMs[placeB] = time;
	CopyName(b->songNames[placeB], name);
}

// 0x000BC1C9
BOOL SwapSongs(DWORD soundtrackId, UINT first, UINT second)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	Seek(SoundtrackOffset(slot));
	if (!ReadSector(SOUNDTRACK_MAGIC) || first >= s_sector.soundtrack.numSongs ||
	    second >= s_sector.soundtrack.numSongs)
		return FALSE;
	UINT placeA = first % SOUNDTRACK_GROUP_SONGS;
	UINT placeB = second % SOUNDTRACK_GROUP_SONGS;
	DWORD groupA = s_sector.soundtrack.songGroups[first / SOUNDTRACK_GROUP_SONGS];
	DWORD groupB = s_sector.soundtrack.songGroups[second / SOUNDTRACK_GROUP_SONGS];
	LONG offsetA = GroupOffset(groupA);
	Seek(offsetA);
	if (!ReadSector(SOUNDTRACK_GROUP_MAGIC))
		return FALSE;
	if (groupA == groupB)
	{
		SwapSongEntries(&s_sector.group, placeA, &s_sector.group, placeB);
		SeekBack();
	}
	else
	{
		SSongGroup a;
		memcpy(&a, &s_sector.group, sizeof(a));
		Seek(GroupOffset(groupB));
		if (!ReadSector(SOUNDTRACK_GROUP_MAGIC))
			return FALSE;
		SwapSongEntries(&a, placeA, &s_sector.group, placeB);
		SeekBack();
		if (!WriteSector())
			return FALSE;
		Seek(offsetA);
		memcpy(&s_sector.group, &a, sizeof(a));
	}
	return WriteSector();
}

// 0x000BC434
// Quirks: deleting the last song of a group whose songs all come before
// it (e.g. the only song) leaves that group marked in use, so it's never
// reused; and a song group that can't be read makes the move-up loop spin
// forever.
BOOL DeleteSong(DWORD soundtrackId, UINT index)
{
	if (s_hDb == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	UINT slot = FindSoundtrack(soundtrackId);
	if (slot == SOUNDTRACK_MAX)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	LONG offset = SoundtrackOffset(slot);
	Seek(offset);
	if (!ReadSector(SOUNDTRACK_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	SSoundtrackRecord soundtrack;
	memcpy(&soundtrack, &s_sector.soundtrack, sizeof(soundtrack));
	if (index >= soundtrack.numSongs)
	{
		SetLastError(ERROR_INVALID_INDEX);
		return FALSE;
	}
	UINT place = index % SOUNDTRACK_GROUP_SONGS;
	UINT groupIndex = index / SOUNDTRACK_GROUP_SONGS;
	Seek(GroupOffset(soundtrack.songGroups[groupIndex]));
	BOOL ok = ReadSector(SOUNDTRACK_GROUP_MAGIC);
	if (!ok)
		return FALSE;
	SSongGroup group;
	memcpy(&group, &s_sector.group, sizeof(group));
	soundtrack.totalTimeMs -= group.songTimeMs[place];
	DWORD songId = group.songIds[place];

	// Close the gap in its group...
	UINT after = SOUNDTRACK_GROUP_SONGS - 1 - place;
	memmove(&group.songTimeMs[place], &group.songTimeMs[place + 1], after * sizeof(DWORD));
	memmove(&group.songIds[place], &group.songIds[place + 1], after * sizeof(DWORD));
	memmove(group.songNames[place], group.songNames[place + 1], after * sizeof(group.songNames[0]));

	// ...and pull each later group's first song into the group before.
	UINT next = index + (SOUNDTRACK_GROUP_SONGS - place);
	while (next < soundtrack.numSongs)
	{
		Seek(GroupOffset(soundtrack.songGroups[groupIndex + 1]));
		BOOL read = ReadSector(SOUNDTRACK_GROUP_MAGIC);
		SSongGroup following;
		memcpy(&following, &s_sector.group, sizeof(following));
		if (!read)
			continue;
		group.songIds[SOUNDTRACK_GROUP_SONGS - 1] = following.songIds[0];
		CopyName(group.songNames[SOUNDTRACK_GROUP_SONGS - 1], following.songNames[0]);
		group.songTimeMs[SOUNDTRACK_GROUP_SONGS - 1] = following.songTimeMs[0];
		memcpy(&s_sector.group, &group, sizeof(group));
		Seek(GroupOffset(soundtrack.songGroups[groupIndex]));
		WriteSector();
		groupIndex++;
		memcpy(&group, &following, sizeof(group));
		// It held only the song just moved: free it.
		if (soundtrack.numSongs == next + 1)
			group.inUse = FALSE;
		next += SOUNDTRACK_GROUP_SONGS;
		memmove(&group.songTimeMs[0], &group.songTimeMs[1], 5 * sizeof(DWORD));
		memmove(&group.songIds[0], &group.songIds[1], 5 * sizeof(DWORD));
		memmove(group.songNames[0], group.songNames[1], 5 * sizeof(group.songNames[0]));
	}
	memcpy(&s_sector.group, &group, sizeof(group));
	Seek(GroupOffset(soundtrack.songGroups[groupIndex]));
	ok = WriteSector();
	if (!ok)
		return FALSE;

	soundtrack.numSongs--;
	memcpy(&s_sector.soundtrack, &soundtrack, sizeof(soundtrack));
	Seek(offset);
	if (!WriteSector())
		return FALSE;

	// The song's file.
	char path[MAX_PATH];
	sprintf(path, "%s%04x\\%08x.WMA", SOUNDTRACK_MUSIC_DIR, soundtrackId, songId);
	if (DeleteFileA(path))
		return ok;
	SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
	return DeleteFileA(path);
}
