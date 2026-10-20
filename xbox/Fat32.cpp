// UnleashX's FAT32 driver interface (src/thirdparty/Fat32.h: USB drives P:
// to S:) on Rob Riglar's fat_io_lib (thirdparty/fat_io_lib), the library
// UnleashX's driver was an early version of. Written for this project.
//
// The device is the raw mass storage device the memory unit driver opened
// (Storage.cpp). Its partitions (up to four, from the MBR; or the whole
// device when it has a boot sector instead) become volumes 0..3. fat_io_lib
// works on one volume at a time and knows only the first partition, so each
// volume has its own struct fatfs, selected before every call under one
// lock, and sees a disk whose MBR names just its partition.
#include "../src/thirdparty/Fat32.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"   // (fat_list.h's static helpers)
#endif
extern "C"
{
#include "../thirdparty/fat_io_lib/fat_filelib.h"
#include "../thirdparty/fat_io_lib/fat_access.h"
#include "../thirdparty/fat_io_lib/fat_table.h"
#include "../thirdparty/fat_io_lib/fat_write.h"
}

#ifdef _XBOX
#include "../src/common/XboxKernel.h"
#endif

#define SECTOR_SIZE 512
#define MAX_VOLUMES 4
// Device reads go through an aligned buffer this big (in sectors).
#define BOUNCE_SECTORS 128

CFat32Volume *g_fat32Volumes[MAX_VOLUMES];   // 0x002776F4

namespace
{

struct SPartition
{
	DWORD firstSector;
	DWORD sectors;
	BYTE type;
};

// What a volume keeps (behind CFat32Volume's reserved space).
struct SVolumeState
{
	struct fatfs fs;
	int index;
	SPartition partition;
};

// An open file or folder: UnleashX's FAT32_FILE, then ours.
struct SOpenFile
{
	FAT32_FILE pub;
	FL_FILE *file;          // NULL for a folder
	CFat32Volume *volume;
	SOpenFile *next;
};

HANDLE s_hDevice = INVALID_HANDLE_VALUE;
std::vector<SPartition> s_partitions;
BYTE s_mbr[SECTOR_SIZE];
BOOL s_bHasMbr;
BYTE *s_pBounce;
SVolumeState *s_pCurrent;   // the volume the disk callbacks read
SOpenFile *s_pOpenFiles;

#ifdef _XBOX
RTL_CRITICAL_SECTION s_lock;
BOOL s_bLockReady;
void Lock()
{
	if (!s_bLockReady)
	{
		RtlInitializeCriticalSection(&s_lock);
		s_bLockReady = TRUE;
	}
	RtlEnterCriticalSection(&s_lock);
}
void Unlock()
{
	RtlLeaveCriticalSection(&s_lock);
}
#else
void Lock() {}
void Unlock() {}
#endif

SVolumeState *State(CFat32Volume *volume)
{
	return *(SVolumeState **)volume->m_reserved;
}

// Locks and makes volume the one fat_io_lib works on.
struct SSelect
{
	SSelect(CFat32Volume *volume)
	{
		Lock();
		s_pCurrent = State(volume);
		fl_select_fs(&s_pCurrent->fs);
	}
	~SSelect() { Unlock(); }
};

BOOL DeviceIo(DWORD sector, BYTE *buffer, DWORD count, BOOL write)
{
	while (count != 0)
	{
		DWORD n = count < BOUNCE_SECTORS ? count : BOUNCE_SECTORS;
		ULONGLONG offset = (ULONGLONG)sector * SECTOR_SIZE;
		LONG high = (LONG)(offset >> 32);
		if (SetFilePointer(s_hDevice, (LONG)(DWORD)offset, &high, FILE_BEGIN) == (DWORD)-1 && GetLastError() != 0)
			return FALSE;
		DWORD done = 0;
		if (write)
		{
			memcpy(s_pBounce, buffer, n * SECTOR_SIZE);
			if (!WriteFile(s_hDevice, s_pBounce, n * SECTOR_SIZE, &done, NULL) || done != n * SECTOR_SIZE)
				return FALSE;
		}
		else
		{
			if (!ReadFile(s_hDevice, s_pBounce, n * SECTOR_SIZE, &done, NULL) || done != n * SECTOR_SIZE)
				return FALSE;
			memcpy(buffer, s_pBounce, n * SECTOR_SIZE);
		}
		sector += n;
		buffer += n * SECTOR_SIZE;
		count -= n;
	}
	return TRUE;
}

// fat_io_lib's disk: the device, except that sector 0 is an MBR with just
// the current volume's partition in the first slot.
int ReadMedia(uint32 sector, uint8 *buffer, uint32 count)
{
	if (s_pCurrent == NULL)
		return 0;
	if (!s_bHasMbr || sector != 0)
		return DeviceIo(sector, buffer, count, FALSE) ? 1 : 0;
	memcpy(buffer, s_mbr, SECTOR_SIZE);
	BYTE *entry = buffer + 0x1BE;
	memset(entry, 0, 0x40);
	entry[4] = s_pCurrent->partition.type;
	memcpy(entry + 8, &s_pCurrent->partition.firstSector, 4);
	memcpy(entry + 12, &s_pCurrent->partition.sectors, 4);
	if (count > 1)
		return DeviceIo(1, buffer + SECTOR_SIZE, count - 1, FALSE) ? 1 : 0;
	return 1;
}

int WriteMedia(uint32 sector, uint8 *buffer, uint32 count)
{
	if (s_pCurrent == NULL || (s_bHasMbr && sector == 0))
		return 0;   // (the MBR isn't ours to write)
	return DeviceIo(sector, buffer, count, TRUE) ? 1 : 0;
}

BOOL IsFatType(BYTE type)
{
	return type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0B || type == 0x0C || type == 0x0E;
}

DWORD Le32(const BYTE *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((DWORD)p[3] << 24);
}

// The partitions: an MBR's FAT ones, or the whole device when sector 0 is
// a FAT boot sector.
void FindPartitions()
{
	s_partitions.clear();
	s_bHasMbr = FALSE;
	if (!DeviceIo(0, s_mbr, 1, FALSE) || s_mbr[510] != 0x55 || s_mbr[511] != 0xAA)
		return;
	BOOL bootSector = (s_mbr[0] == 0xEB || s_mbr[0] == 0xE9) &&
	                  (memcmp(s_mbr + 0x36, "FAT", 3) == 0 || memcmp(s_mbr + 0x52, "FAT", 3) == 0);
	if (bootSector)
	{
		SPartition whole = { 0, 0, 0x0C };
		s_partitions.push_back(whole);
		return;
	}
	for (int i = 0; i < 4; i++)
	{
		const BYTE *entry = s_mbr + 0x1BE + i * 16;
		SPartition partition = { Le32(entry + 8), Le32(entry + 12), entry[4] };
		if (IsFatType(partition.type) && partition.firstSector != 0 && partition.sectors != 0)
			s_partitions.push_back(partition);
	}
	s_bHasMbr = TRUE;
}

// "P:\dir\file" (or with '/') as fat_io_lib's "/dir/file".
std::string LibPath(const char *path)
{
	std::string out;
	if (path[0] != 0 && path[1] == ':')
		path += 2;
	for (; *path != 0; path++)
		out += *path == '\\' ? '/' : *path;
	if (out.size() == 0 || out[0] != '/')
		out.insert(out.begin(), '/');
	while (out.size() > 1 && out[out.size() - 1] == '/')
		out.erase(out.size() - 1);
	return out;
}

// The part before the last '/', and the name after it.
void SplitPath(const std::string &path, std::string &parent, std::string &name)
{
	size_t slash = path.rfind('/');
	parent = slash == 0 ? "/" : path.substr(0, slash);
	name = path.substr(slash + 1);
}

void Sync(SOpenFile *file)
{
	if (file->file == NULL)
		return;
	file->pub.startCluster = file->file->startcluster;
	file->pub.position = file->file->bytenum;
	file->pub.length = file->file->filelength;
}

SOpenFile *NewOpenFile(CFat32Volume *volume, FL_FILE *file, DWORD folderCluster)
{
	SOpenFile *open = new SOpenFile;
	memset(open, 0, sizeof(*open));
	open->file = file;
	open->volume = volume;
	open->pub.open = TRUE;
	open->pub.startCluster = folderCluster;
	Sync(open);
	open->next = s_pOpenFiles;
	s_pOpenFiles = open;
	return open;
}

void ForgetOpenFile(SOpenFile *file)
{
	for (SOpenFile **p = &s_pOpenFiles; *p != NULL; p = &(*p)->next)
	{
		if (*p == file)
		{
			*p = file->next;
			break;
		}
	}
	delete file;
}

// The folder's cluster (0 if it isn't one).
DWORD FolderCluster(const std::string &path)
{
	FL_DIR dir;
	if (fl_opendir(path.c_str(), &dir) == NULL)
		return 0;
	DWORD cluster = dir.cluster;
	fl_closedir(&dir);
	return cluster;
}

BOOL Match(const char *pattern, const char *name)
{
	for (; *pattern != 0; pattern++, name++)
	{
		if (*pattern == '*')
		{
			for (pattern++; ; name++)
			{
				if (Match(pattern, name))
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

// A FAT date and time (local, 2 s steps) as a FILETIME.
FILETIME FatTime(WORD date, WORD time)
{
	FILETIME ft;
	memset(&ft, 0, sizeof(ft));
	if (date == 0)
		return ft;
	static const int days[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int year = 1980 + (date >> 9), month = (date >> 5) & 15, day = date & 31;
	if (month < 1 || month > 12)
		month = 1;
	LONGLONG y = year - 1601;
	LONGLONG total = y * 365 + y / 4 - y / 100 + y / 400 + days[month - 1] + (day > 0 ? day - 1 : 0);
	if (month > 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
		total++;
	LONGLONG seconds = total * 86400 + (time >> 11) * 3600 + ((time >> 5) & 63) * 60 + (time & 31) * 2;
	ULONGLONG ticks = (ULONGLONG)seconds * 10000000;
	ft.dwLowDateTime = (DWORD)ticks;
	ft.dwHighDateTime = (DWORD)(ticks >> 32);
	return ft;
}

// FindFirst's results. CDirFAT32::FindClose does nothing, so searches are
// never closed: each one takes the next slot (enough for nested folder
// walks).
#define FIND_SLOTS 32
struct SFind
{
	std::vector<WIN32_FIND_DATAA> entries;
	size_t next;
};
SFind s_finds[FIND_SLOTS];
int s_nNextFind;

}   // namespace

// ---- the device ----

BOOL Fat32Init(HANDLE device)
{
	Lock();
	if (device != INVALID_HANDLE_VALUE)
	{
		for (int i = 0; i < MAX_VOLUMES; i++)
		{
			if (g_fat32Volumes[i] != NULL)
			{
				delete State(g_fat32Volumes[i]);
				delete g_fat32Volumes[i];
				g_fat32Volumes[i] = NULL;
			}
		}
		s_hDevice = device;
	}
	if (s_pBounce == NULL)
	{
#ifdef _XBOX
		s_pBounce = (BYTE *)VirtualAlloc(NULL, BOUNCE_SECTORS * SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE);
#else
		s_pBounce = (BYTE *)malloc(BOUNCE_SECTORS * SECTOR_SIZE);
#endif
	}
	FindPartitions();
	Unlock();
	return s_pBounce != NULL && s_partitions.size() != 0;
}

void Fat32Shutdown()
{
	Lock();
	for (int i = 0; i < MAX_VOLUMES; i++)
	{
		if (g_fat32Volumes[i] == NULL)
			continue;
		s_pCurrent = State(g_fat32Volumes[i]);
		fl_select_fs(&s_pCurrent->fs);
		fl_shutdown();   // (writes the FAT back)
		delete s_pCurrent;
		delete g_fat32Volumes[i];
		g_fat32Volumes[i] = NULL;
	}
	s_pCurrent = NULL;
	if (s_hDevice != INVALID_HANDLE_VALUE)
		CloseHandle(s_hDevice);
	s_hDevice = INVALID_HANDLE_VALUE;
	s_partitions.clear();
	Unlock();
}

// ---- volumes ----

CFat32Volume *Fat32MountVolume(int index)
{
	index &= 3;
	if (g_fat32Volumes[index] != NULL)
		return g_fat32Volumes[index];
	Lock();
	if (s_hDevice == INVALID_HANDLE_VALUE || index >= (int)s_partitions.size())
	{
		Unlock();
		return NULL;
	}
	SVolumeState *state = new SVolumeState;
	memset(state, 0, sizeof(*state));
	state->index = index;
	state->partition = s_partitions[index];
	s_pCurrent = state;
	fl_select_fs(&state->fs);
	if (fl_attach_media(ReadMedia, WriteMedia) != FAT_INIT_OK)
	{
		delete state;
		s_pCurrent = NULL;
		Unlock();
		return NULL;
	}
	CFat32Volume *volume = new CFat32Volume;
	memset(volume, 0, sizeof(*volume));
	*(SVolumeState **)volume->m_reserved = state;
	volume->m_pIo = (CFat32Io *)volume;
	volume->m_pDevice = s_hDevice;

	// The size from the boot sector, the label from the root folder.
	BYTE sector[SECTOR_SIZE];
	if (ReadMedia(state->fs.lba_begin, sector, 1))
	{
		DWORD total = sector[0x13] | (sector[0x14] << 8);
		if (total == 0)
			total = Le32(sector + 0x20);
		volume->m_ullTotalBytes = (ULONGLONG)total * SECTOR_SIZE;
	}
	strcpy(volume->m_szLabel, " ");
	for (DWORD n = 0; fatfs_sector_reader(&state->fs, state->fs.rootdir_first_cluster, n, sector); n++)
	{
		BOOL end = FALSE;
		for (int e = 0; e < SECTOR_SIZE && !end; e += 32)
		{
			const BYTE *entry = sector + e;
			if (entry[0] == 0)
				end = TRUE;
			else if (entry[0] != 0xE5 && (entry[11] & 0x0F) == 0x08)
			{
				int length = 11;
				while (length > 0 && entry[length - 1] == ' ')
					length--;
				memcpy(volume->m_szLabel, entry, length);
				volume->m_szLabel[length] = 0;
				end = TRUE;
			}
		}
		// (FAT16 roots are a fixed area: the reader stops by itself.)
		if (end || n > 64)
			break;
	}
	g_fat32Volumes[index] = volume;
	Unlock();
	Fat32GetFreeBytes(volume);
	return volume;
}

CFat32Io *Fat32GetIo(CFat32Volume *volume)
{
	return volume->m_pIo;
}

const char *Fat32GetVolumeLabel(CFat32Volume *volume)
{
	return volume->m_szLabel;
}

ULONGLONG Fat32GetFreeBytes(CFat32Volume *volume)
{
	SSelect select(volume);
	struct fatfs *fs = &s_pCurrent->fs;
	volume->m_ullFreeBytes = (ULONGLONG)fatfs_count_free_clusters(fs) * fs->sectors_per_cluster * SECTOR_SIZE;
	return volume->m_ullFreeBytes;
}

// ---- files ----

FAT32_FILE *Fat32Open(CFat32Io *io, const char *path, const char *mode)
{
	CFat32Volume *volume = (CFat32Volume *)io;
	SSelect select(volume);
	std::string libPath = LibPath(path);
	FL_FILE *file = (FL_FILE *)fl_fopen(libPath.c_str(), mode);
	if (file != NULL)
		return &NewOpenFile(volume, file, 0)->pub;
	// A folder opens too (for reading), as with UnleashX's driver.
	if (strchr(mode, 'w') == NULL && strchr(mode, 'a') == NULL)
	{
		DWORD cluster = libPath == "/" ? s_pCurrent->fs.rootdir_first_cluster : FolderCluster(libPath);
		if (cluster != 0)
			return &NewOpenFile(volume, NULL, cluster)->pub;
	}
	return NULL;
}

int Fat32Read(FAT32_FILE *handle, CFat32Io *, void *buffer, int size)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file->file == NULL)
		return 0;
	SSelect select(file->volume);
	int read = fl_fread(buffer, 1, size, file->file);
	Sync(file);
	return read;
}

int Fat32Write(FAT32_FILE *handle, CFat32Io *, const void *data, int size)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file->file == NULL)
		return -1;
	if (size == 0)
		size = (int)strlen((const char *)data);
	SSelect select(file->volume);
	int written = fl_fwrite(data, 1, size, file->file);
	Sync(file);
	return written;
}

void Fat32Close(FAT32_FILE *handle, CFat32Io *)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file == NULL)
		return;
	SSelect select(file->volume);
	if (file->file != NULL)
		fl_fclose(file->file);
	ForgetOpenFile(file);
}

int Fat32Seek(FAT32_FILE *handle, CFat32Io *, long offset, int origin)
{
	SOpenFile *file = (SOpenFile *)handle;
	if (file->file == NULL)
		return -1;
	SSelect select(file->volume);
	int result = fl_fseek(file->file, offset, origin);
	Sync(file);
	return result;
}

int Fat32Remove(CFat32Io *io, const char *path)
{
	SSelect select((CFat32Volume *)io);
	return fl_remove(LibPath(path).c_str());
}

FAT32_FILE *Fat32CreateDirectory(CFat32Io *io, const char *path, int, int, int)
{
	CFat32Volume *volume = (CFat32Volume *)io;
	SSelect select(volume);
	std::string libPath = LibPath(path);
	if (!fl_createdirectory(libPath.c_str()))
		return NULL;
	DWORD cluster = FolderCluster(libPath);
	if (cluster == 0)
		return NULL;
	return &NewOpenFile(volume, NULL, cluster)->pub;
}

// Only the entry's size and start cluster are filled in: that's what the
// file layer reads.
BOOL Fat32GetDirEntry(CFat32Volume *volume, DWORD cluster, BYTE entry[32])
{
	Lock();
	for (SOpenFile *file = s_pOpenFiles; file != NULL; file = file->next)
	{
		if (file->volume == volume && file->pub.startCluster == cluster)
		{
			memset(entry, 0, 32);
			WORD high = (WORD)(cluster >> 16), low = (WORD)cluster;
			memcpy(entry + 0x14, &high, 2);
			memcpy(entry + 0x1A, &low, 2);
			memcpy(entry + 0x1C, &file->pub.length, 4);
			Unlock();
			return TRUE;
		}
	}
	Unlock();
	return FALSE;
}

// ---- folders ----

HANDLE Fat32FindFirst(CFat32Volume *volume, const char *pattern, WIN32_FIND_DATAA *data)
{
	SSelect select(volume);
	std::string folder, name;
	SplitPath(LibPath(pattern), folder, name);
	if (name.size() == 0 || name == "*.*")
		name = "*";
	FL_DIR dir;
	if (fl_opendir(folder.c_str(), &dir) == NULL)
		return INVALID_HANDLE_VALUE;
	int slot = s_nNextFind;
	s_nNextFind = (s_nNextFind + 1) % FIND_SLOTS;
	SFind &find = s_finds[slot];
	find.entries.clear();
	find.next = 0;
	fl_dirent entry;
	while (fl_readdir(&dir, &entry) == 0)
	{
		// (Like the hard disk's FATX folders: no "." or "..".)
		if (strcmp(entry.filename, ".") == 0 || strcmp(entry.filename, "..") == 0 || !Match(name.c_str(), entry.filename))
			continue;
		WIN32_FIND_DATAA found;
		memset(&found, 0, sizeof(found));
		found.dwFileAttributes = entry.is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		found.nFileSizeLow = entry.is_dir ? 0 : entry.size;
		found.ftCreationTime = FatTime(entry.create_date, entry.create_time);
		found.ftLastWriteTime = FatTime(entry.write_date, entry.write_time);
		found.ftLastAccessTime = FatTime(entry.access_date, 0);
		strncpy(found.cFileName, entry.filename, sizeof(found.cFileName) - 1);
		find.entries.push_back(found);
	}
	fl_closedir(&dir);
	if (find.entries.size() == 0)
		return INVALID_HANDLE_VALUE;
	*data = find.entries[find.next++];
	return (HANDLE)(size_t)(slot + 1);
}

BOOL Fat32FindNext(CFat32Volume *, HANDLE handle, WIN32_FIND_DATAA *data)
{
	int slot = (int)(DWORD)(size_t)handle - 1;
	if (slot < 0 || slot >= FIND_SLOTS)
		return FALSE;
	SFind &find = s_finds[slot];
	if (find.next >= find.entries.size())
		return FALSE;
	*data = find.entries[find.next++];
	return TRUE;
}

// An empty folder: its clusters freed and its entry marked deleted.
BOOL Fat32RemoveDirectory(CFat32Volume *volume, const char *path)
{
	SSelect select(volume);
	struct fatfs *fs = &s_pCurrent->fs;
	std::string libPath = LibPath(path);
	if (libPath == "/")
		return FALSE;
	FL_DIR dir;
	if (fl_opendir(libPath.c_str(), &dir) == NULL)
		return FALSE;
	fl_dirent entry;
	BOOL empty = TRUE;
	while (empty && fl_readdir(&dir, &entry) == 0)
		empty = strcmp(entry.filename, ".") == 0 || strcmp(entry.filename, "..") == 0;
	fl_closedir(&dir);
	if (!empty)
		return FALSE;

	std::string parent, name;
	SplitPath(libPath, parent, name);
	DWORD parentCluster = parent == "/" ? fs->rootdir_first_cluster : FolderCluster(parent);
	if (parentCluster == 0)
		return FALSE;
	struct fat_dir_entry record;
	std::vector<char> nameCopy(name.begin(), name.end());
	nameCopy.push_back(0);
	if (!fatfs_get_file_entry(fs, parentCluster, &nameCopy[0], &record) || !(record.Attr & FILE_ATTR_DIRECTORY))
		return FALSE;
	DWORD cluster = ((DWORD)record.FstClusHI << 16) | record.FstClusLO;
	if (!fatfs_free_cluster_chain(fs, cluster))
		return FALSE;
	char shortName[12];
	memcpy(shortName, record.Name, 11);
	shortName[11] = 0;
	if (!fatfs_mark_file_deleted(fs, parentCluster, shortName))
		return FALSE;
	fatfs_fat_purge(fs);
	return TRUE;
}
