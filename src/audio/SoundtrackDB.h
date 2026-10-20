// The Xbox soundtrack database, E:\TDATA\fffe0000\music\ST.DB: the MS
// dashboard's list of the soundtracks ripped from audio CDs. XAPI only
// reads it (XFindFirstSoundtrack, XGetSoundtrackSongInfo); these are the
// writers the soundtrack tools use (0x000BBFE5-0x000BCF48). See
// SoundtrackDB.cpp and docs/MUSIC.md.
//
// The file is made of 0x200-byte sectors: the header, then 100 soundtrack
// sectors (one per slot, in list order), then the song groups (six songs
// each) from 0xCA00 on. Songs are WMA files named
// <music>\<soundtrack id %04x>\<song id %08x>.wma.
//
// One database is open at a time (a global handle and sector buffer):
// OpenSoundtrackDB, the edits, CloseSoundtrackDB. The edits fail with
// ERROR_INVALID_HANDLE when it isn't open and ERROR_INVALID_INDEX (0x585)
// for an unknown soundtrack or song.
#pragma once

#include <string>

#include "../common/Platform.h"

#define SOUNDTRACK_MUSIC_DIR "E:\\TDATA\\fffe0000\\music\\"
#define SOUNDTRACK_DB_PATH "E:\\TDATA\\fffe0000\\music\\ST.DB"

#define SOUNDTRACK_DB_SECTOR 0x200
#define SOUNDTRACK_MAX 100                // soundtracks
#define SOUNDTRACK_MAX_SONGS 500          // songs in one soundtrack
#define SOUNDTRACK_GROUP_SONGS 6          // songs in one song group
#define SOUNDTRACK_NAME_LENGTH 32         // in UTF-16 units, not always terminated
#define SOUNDTRACK_MAGIC 0x00021371       // a soundtrack sector
#define SOUNDTRACK_GROUP_MAGIC 0x00031073 // a song group sector
// The first song group's sector (after the header and 100 soundtracks).
#define SOUNDTRACK_GROUP_BASE (SOUNDTRACK_MAX + 1)

// Names are stored as UTF-16 (wchar_t on the Xbox; WORD keeps the layout
// on hosts where wchar_t is wider).
typedef WORD STDBCHAR;

// Sector 0. 0x1A0 bytes are used.
struct SSoundtrackDbHeader
{
	DWORD version;                          // +0x000 1
	UINT numSoundtracks;                    // +0x004
	DWORD nextSoundtrackId;                 // +0x008
	DWORD soundtrackIds[SOUNDTRACK_MAX];    // +0x00C in list order (sector i + 1)
	DWORD nextSongId;                       // +0x19C the low word of new song ids
};

// Sectors 1-100. 0x1A0 bytes are used.
struct SSoundtrackRecord
{
	DWORD magic;                            // +0x000 SOUNDTRACK_MAGIC
	DWORD id;                               // +0x004
	UINT numSongs;                          // +0x008
	DWORD songGroups[84];                   // +0x00C song group numbers (500 / 6, rounded up)
	DWORD totalTimeMs;                      // +0x15C
	STDBCHAR name[SOUNDTRACK_NAME_LENGTH];  // +0x160
};

// From sector 101 on. 0x1C0 bytes are used.
struct SSongGroup
{
	DWORD magic;                            // +0x000 SOUNDTRACK_GROUP_MAGIC
	DWORD soundtrackId;                     // +0x004
	DWORD index;                            // +0x008 its place in the soundtrack (song / 6)
	BOOL inUse;                             // +0x00C free when 0
	DWORD songIds[SOUNDTRACK_GROUP_SONGS];  // +0x010 (soundtrack id << 16) | serial
	DWORD songTimeMs[SOUNDTRACK_GROUP_SONGS];   // +0x028
	STDBCHAR songNames[SOUNDTRACK_GROUP_SONGS][SOUNDTRACK_NAME_LENGTH];   // +0x040
};

// Opens (creating it if missing: the header and 100 empty soundtrack
// sectors) and reads the header.
BOOL OpenSoundtrackDB();                                              // 0x000BCE2C
void CloseSoundtrackDB();                                             // 0x000BC02F
// A new, empty soundtrack and its folder. Fails with
// ERROR_NOT_ENOUGH_MEMORY when there are 100. (id in eax)
BOOL CreateSoundtrack(const wchar_t *name, DWORD *id);                // 0x000BCD3D
BOOL RenameSoundtrack(DWORD soundtrackId, const wchar_t *name);       // 0x000BC151
// Frees its song groups, removes it from the list and deletes its folder.
BOOL DeleteSoundtrack(DWORD soundtrackId);                            // 0x000BCB01
// Swaps two soundtracks' places in the list. (Unused.)
BOOL SwapSoundtracks(DWORD firstId, DWORD secondId);                  // 0x000BC851
// The id for a new song: (soundtrackId << 16) | the next serial.
// 0xFFFFFFFF when the database isn't open. (soundtrackId in eax)
DWORD AllocateSongId(DWORD soundtrackId);                             // 0x000BC81C
// Adds a song at the end (its file must already be in place).
BOOL AddSong(DWORD soundtrackId, DWORD songId, DWORD timeMs, const wchar_t *name);   // 0x000BC965
// (index in eax)
BOOL RenameSong(DWORD soundtrackId, UINT index, const wchar_t *name);   // 0x000BC082
// Swaps two songs (the editor's Move Up/Move Down).
BOOL SwapSongs(DWORD soundtrackId, UINT first, UINT second);          // 0x000BC1C9
// Removes a song, moving the later ones up, and deletes its file.
BOOL DeleteSong(DWORD soundtrackId, UINT index);                      // 0x000BC434

// Unused (nothing makes one; its code sits between ftpparse and the ST.DB
// writers): a name and a text with two flag bytes. vtable 0x002589DC.
// (SoundtrackDB.cpp; this in esi) TODO: verify what it describes
class CFileDescription
{
public:
	CFileDescription();                                   // 0x000BBF8C
	CFileDescription(const std::string &name, const std::string &text, BYTE flag4, BYTE flag5);   // 0x000BBF07
	CFileDescription(const CFileDescription &other);      // 0x000BBF56 (this in eax, other in edi)
	virtual ~CFileDescription();                          // 0x000BBEB1 (deleting 0x0002F8E3)
	CFileDescription &operator=(const CFileDescription &other);   // 0x000BBED4 (other in edi)
	// Both strings to s.
	void Set(const char *s);                              // 0x000BBF78

	BYTE m_bFlag4;                 // +0x04
	BYTE m_bFlag5;                 // +0x05
	std::string m_strName;         // +0x08
	std::string m_strText;         // +0x24
};
