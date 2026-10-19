// The soundtrack database writers on a fresh ST.DB: soundtracks, songs
// across two song groups, rename, swap, delete. Checks the sectors as XAPI
// would read them.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../src/audio/SoundtrackDB.h"

static int s_failures;

#define CHECK(cond)                                                     \
	do                                                                  \
	{                                                                   \
		if (!(cond))                                                    \
		{                                                               \
			fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
			s_failures++;                                               \
		}                                                               \
	} while (0)

static std::string HostFile(const char *rel)
{
	const char *root = getenv("UNLEASHX_ROOT");
	return std::string(root != NULL ? root : ".") + "/E/TDATA/fffe0000/music/" + rel;
}

static bool ReadSectorAt(DWORD sector, void *out, size_t size)
{
	FILE *f = fopen(HostFile("ST.DB").c_str(), "rb");
	if (f == NULL)
		return false;
	fseek(f, (long)(sector * SOUNDTRACK_DB_SECTOR), SEEK_SET);
	bool ok = fread(out, 1, size, f) == size;
	fclose(f);
	return ok;
}

static std::string Name(const STDBCHAR *name)
{
	std::string s;
	for (int i = 0; i < SOUNDTRACK_NAME_LENGTH && name[i] != 0; i++)
		s += (char)name[i];
	return s;
}

static void Touch(DWORD soundtrack, DWORD song)
{
	char rel[64];
	sprintf(rel, "%04x/%08x.WMA", soundtrack, song);
	FILE *f = fopen(HostFile(rel).c_str(), "wb");
	if (f != NULL)
		fclose(f);
}

static bool Exists(const std::string &path)
{
	FILE *f = fopen(path.c_str(), "rb");
	if (f == NULL)
		return false;
	fclose(f);
	return true;
}

// Song i of a soundtrack, through its song groups.
static bool GetSong(const SSoundtrackRecord &st, UINT i, DWORD *id, DWORD *time, std::string *name)
{
	SSongGroup group;
	if (!ReadSectorAt(SOUNDTRACK_GROUP_BASE + st.songGroups[i / 6], &group, sizeof(group)))
		return false;
	if (group.magic != SOUNDTRACK_GROUP_MAGIC || !group.inUse || group.soundtrackId != st.id)
		return false;
	*id = group.songIds[i % 6];
	*time = group.songTimeMs[i % 6];
	*name = Name(group.songNames[i % 6]);
	return true;
}

int main()
{
	char sizes[sizeof(SSoundtrackDbHeader) == 0x1A0 && sizeof(SSoundtrackRecord) == 0x1A0 &&
	                   sizeof(SSongGroup) == 0x1C0
	               ? 1
	               : -1];
	(void)sizes;

	CHECK(OpenSoundtrackDB());
	SSoundtrackDbHeader header;
	CHECK(ReadSectorAt(0, &header, sizeof(header)));
	CHECK(header.version == 1 && header.numSoundtracks == 0);

	DWORD first, second;
	CHECK(CreateSoundtrack(L"First", &first));
	CHECK(CreateSoundtrack(L"Second", &second));
	CHECK(first == 0 && second == 1);

	// Eight songs: two song groups.
	DWORD ids[8];
	for (int i = 0; i < 8; i++)
	{
		ids[i] = AllocateSongId(first);
		wchar_t name[16];
		swprintf(name, 16, L"Song %d", i);
		Touch(first, ids[i]);
		CHECK(AddSong(first, ids[i], 1000 * (i + 1), name));
	}
	CHECK(ids[0] == 0 && ids[7] == 7);
	CloseSoundtrackDB();

	CHECK(ReadSectorAt(0, &header, sizeof(header)));
	CHECK(header.numSoundtracks == 2 && header.nextSoundtrackId == 2 && header.nextSongId == 8);
	CHECK(header.soundtrackIds[0] == first && header.soundtrackIds[1] == second);
	SSoundtrackRecord st;
	CHECK(ReadSectorAt(1, &st, sizeof(st)));
	CHECK(st.magic == SOUNDTRACK_MAGIC && st.numSongs == 8 && st.totalTimeMs == 36000);
	CHECK(Name(st.name) == "First");
	CHECK(st.songGroups[0] == 0 && st.songGroups[1] == 1);
	DWORD id, time;
	std::string name;
	CHECK(GetSong(st, 6, &id, &time, &name) && id == 6 && time == 7000 && name == "Song 6");

	// Rename and swap across the groups.
	CHECK(OpenSoundtrackDB());
	CHECK(RenameSoundtrack(first, L"Renamed"));
	CHECK(RenameSong(first, 7, L"Last"));
	CHECK(SwapSongs(first, 1, 7));
	CHECK(!RenameSong(first, 8, L"Nope"));
	CloseSoundtrackDB();
	CHECK(ReadSectorAt(1, &st, sizeof(st)));
	CHECK(Name(st.name) == "Renamed");
	CHECK(GetSong(st, 1, &id, &time, &name) && id == 7 && time == 8000 && name == "Last");
	CHECK(GetSong(st, 7, &id, &time, &name) && id == 1 && name == "Song 1");

	// Delete song 0: the others move up, the last group's first song
	// moves into the first group, and its file goes.
	CHECK(OpenSoundtrackDB());
	CHECK(DeleteSong(first, 0));
	CloseSoundtrackDB();
	CHECK(!Exists(HostFile("0000/00000000.WMA")));
	CHECK(ReadSectorAt(1, &st, sizeof(st)));
	CHECK(st.numSongs == 7 && st.totalTimeMs == 35000);
	CHECK(GetSong(st, 0, &id, &time, &name) && id == 7 && name == "Last");
	CHECK(GetSong(st, 5, &id, &time, &name) && id == 6);
	CHECK(GetSong(st, 6, &id, &time, &name) && id == 1);

	// Delete the first soundtrack: the second moves into slot 1 and the
	// song groups are free again.
	CHECK(OpenSoundtrackDB());
	CHECK(DeleteSoundtrack(first));
	CloseSoundtrackDB();
	CHECK(ReadSectorAt(0, &header, sizeof(header)));
	CHECK(header.numSoundtracks == 1 && header.soundtrackIds[0] == second);
	CHECK(ReadSectorAt(1, &st, sizeof(st)));
	CHECK(st.id == second && Name(st.name) == "Second");
	SSongGroup group;
	CHECK(ReadSectorAt(SOUNDTRACK_GROUP_BASE, &group, sizeof(group)) && !group.inUse);
	CHECK(!Exists(HostFile("0000/00000007.WMA")));

	// A new song in the second soundtrack reuses group 0.
	CHECK(OpenSoundtrackDB());
	CHECK(AddSong(second, AllocateSongId(second), 500, L"Again"));
	CloseSoundtrackDB();
	CHECK(ReadSectorAt(1, &st, sizeof(st)));
	CHECK(st.numSongs == 1 && st.songGroups[0] == 0);
	CHECK(GetSong(st, 0, &id, &time, &name) && id == 0x10008 && name == "Again");

	if (s_failures != 0)
		return 1;
	printf("soundtrackdb_test: ok\n");
	return 0;
}
