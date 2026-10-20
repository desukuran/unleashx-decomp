// The FAT32 driver adapter (xbox/Fat32.cpp on thirdparty/fat_io_lib) on disk
// images made by mkfs.fat:
//   fat32_test <partitioned.img> <superfloppy.img>
// partitioned.img has an MBR with two FAT32 partitions labelled ONE and TWO;
// superfloppy.img is one FAT32 file system from sector 0, labelled FLOPPY.
// Both are written to; the Makefile runs fsck.fat on them afterwards.
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/thirdparty/Fat32.h"

static int failures = 0;

#define CHECK(cond)                                                         \
	do                                                                      \
	{                                                                       \
		if (!(cond))                                                        \
		{                                                                   \
			printf("%s:%d: FAIL: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                     \
		}                                                                   \
	} while (0)

static BOOL WriteFileTo(CFat32Volume *volume, const char *path, const std::string &data)
{
	FAT32_FILE *file = Fat32Open(Fat32GetIo(volume), path, "w");
	if (file == NULL)
		return FALSE;
	// In pieces, as the file layer writes.
	size_t done = 0;
	BOOL ok = TRUE;
	while (ok && done < data.size())
	{
		int n = (int)(data.size() - done < 3000 ? data.size() - done : 3000);
		ok = Fat32Write(file, Fat32GetIo(volume), data.data() + done, n) == n;
		done += n;
	}
	Fat32Close(file, Fat32GetIo(volume));
	return ok;
}

static std::string ReadFileFrom(CFat32Volume *volume, const char *path)
{
	std::string data;
	FAT32_FILE *file = Fat32Open(Fat32GetIo(volume), path, "r");
	if (file == NULL)
		return "<missing>";
	char buffer[4096];
	int n;
	while ((n = Fat32Read(file, Fat32GetIo(volume), buffer, sizeof(buffer))) > 0)
		data.append(buffer, n);
	Fat32Close(file, Fat32GetIo(volume));
	return data;
}

static int CountEntries(CFat32Volume *volume, const char *pattern, const char *want, BOOL *wantIsDir)
{
	WIN32_FIND_DATAA data;
	HANDLE find = Fat32FindFirst(volume, pattern, &data);
	if (find == INVALID_HANDLE_VALUE)
		return 0;
	int count = 0;
	do
	{
		count++;
		if (want != NULL && strcmp(data.cFileName, want) == 0 && wantIsDir != NULL)
			*wantIsDir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	} while (Fat32FindNext(volume, find, &data));
	return count;
}

static void Exercise(CFat32Volume *volume, const char *drive)
{
	std::string base = std::string(drive) + ":\\";
	std::string big;
	for (int i = 0; i < 200000; i++)
		big += (char)('A' + (i * 7) % 26);

	// Folders, as CDirFAT32::CreateDirectory makes them (one level at a time).
	FAT32_FILE *folder = Fat32CreateDirectory(Fat32GetIo(volume), (base + "Music").c_str(), 0, 1, 0);
	CHECK(folder != NULL);
	Fat32Close(folder, Fat32GetIo(volume));
	folder = Fat32CreateDirectory(Fat32GetIo(volume), (base + "Music\\Album One").c_str(), 0, 1, 0);
	CHECK(folder != NULL);
	Fat32Close(folder, Fat32GetIo(volume));

	ULONGLONG freeBefore = Fat32GetFreeBytes(volume);
	CHECK(WriteFileTo(volume, (base + "Music\\Album One\\song with a long name.mp3").c_str(), big));
	CHECK(WriteFileTo(volume, (base + "readme.txt").c_str(), "Hello, USB!\r\n"));
	CHECK(Fat32GetFreeBytes(volume) < freeBefore);

	CHECK(ReadFileFrom(volume, (base + "readme.txt").c_str()) == "Hello, USB!\r\n");
	CHECK(ReadFileFrom(volume, (base + "music/album one/SONG WITH A LONG NAME.MP3").c_str()) == big);

	// A folder opens for reading (DirectoryExists asks FileExists).
	FAT32_FILE *open = Fat32Open(Fat32GetIo(volume), (base + "Music").c_str(), "r");
	CHECK(open != NULL);
	if (open != NULL)
		Fat32Close(open, Fat32GetIo(volume));
	CHECK(Fat32Open(Fat32GetIo(volume), (base + "nothing.txt").c_str(), "r") == NULL);

	// Seeking, and the size the file layer reads from the open file.
	FAT32_FILE *file = Fat32Open(Fat32GetIo(volume), (base + "Music\\Album One\\song with a long name.mp3").c_str(), "r");
	CHECK(file != NULL && file->length == big.size());
	if (file != NULL)
	{
		char buffer[10];
		CHECK(Fat32Seek(file, Fat32GetIo(volume), 100000, 0) == 0);
		CHECK(Fat32Read(file, Fat32GetIo(volume), buffer, 10) == 10 && memcmp(buffer, big.data() + 100000, 10) == 0);
		BYTE entry[32];
		CHECK(Fat32GetDirEntry(volume, file->startCluster, entry));
		DWORD size;
		memcpy(&size, entry + 0x1C, 4);
		CHECK(size == big.size());
		Fat32Close(file, Fat32GetIo(volume));
	}

	// Listings: Win32-style patterns, no "." or "..".
	BOOL isDir = FALSE;
	CHECK(CountEntries(volume, (base + "*").c_str(), "Music", &isDir) == 2 && isDir);
	CHECK(CountEntries(volume, (base + "Music\\Album One\\*.mp3").c_str(), NULL, NULL) == 1);
	CHECK(CountEntries(volume, (base + "Music\\Album One\\*.wma").c_str(), NULL, NULL) == 0);

	// Deleting: a full folder can't go, an empty one can.
	CHECK(!Fat32RemoveDirectory(volume, (base + "Music").c_str()));
	CHECK(Fat32Remove(Fat32GetIo(volume), (base + "Music\\Album One\\song with a long name.mp3").c_str()) == 0);
	CHECK(Fat32RemoveDirectory(volume, (base + "Music\\Album One").c_str()));
	CHECK(CountEntries(volume, (base + "Music\\*").c_str(), NULL, NULL) == 0);

	// Something left behind for fsck to look at.
	CHECK(WriteFileTo(volume, (base + "Music\\kept.bin").c_str(), big.substr(0, 70000)));
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("usage: fat32_test <partitioned.img> <superfloppy.img>\n");
		return 2;
	}

	HANDLE device = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(device != INVALID_HANDLE_VALUE);
	CHECK(Fat32Init(device));
	CFat32Volume *one = Fat32MountVolume(0);
	CFat32Volume *two = Fat32MountVolume(1);
	CHECK(one != NULL && two != NULL && Fat32MountVolume(2) == NULL);
	if (one != NULL && two != NULL)
	{
		CHECK(strcmp(Fat32GetVolumeLabel(one), "ONE") == 0);
		CHECK(strcmp(Fat32GetVolumeLabel(two), "TWO") == 0);
		CHECK(one->m_ullTotalBytes > 30000000 && two->m_ullTotalBytes > 30000000);
		Exercise(one, "P");
		// The same names on the other partition don't clash with the first.
		FAT32_FILE *held = Fat32Open(Fat32GetIo(one), "P:\\readme.txt", "r");
		Exercise(two, "Q");
		if (held != NULL)
			Fat32Close(held, Fat32GetIo(one));
		CHECK(ReadFileFrom(one, "P:\\readme.txt") == "Hello, USB!\r\n");
	}
	Fat32Shutdown();
	CHECK(Fat32GetVolume(0) == NULL);

	device = CreateFileA(argv[2], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(Fat32Init(device));
	CFat32Volume *floppy = Fat32MountVolume(0);
	CHECK(floppy != NULL && Fat32MountVolume(1) == NULL);
	if (floppy != NULL)
	{
		CHECK(strcmp(Fat32GetVolumeLabel(floppy), "FLOPPY") == 0);
		Exercise(floppy, "P");
	}
	Fat32Shutdown();

	if (failures == 0)
		printf("ok\n");
	return failures != 0;
}
