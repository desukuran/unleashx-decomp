// Stand-ins for the third-party libraries the Xbox build doesn't compile yet.
// Each fails cleanly, so the feature is just unavailable:
//  - unrar (XBMC's UnrarXLib, with UnleashX's sink interface): RAR archives
//    list empty and don't extract;
//  - XBMC's iso9660 reader: DVD data discs can't be browsed in the file
//    manager (launching from D: goes through the kernel and still works);
//  - the FAT32 driver (Rob Riglar's early fat_io_lib): no USB storage.
#include "../src/thirdparty/Fat32.h"
#include "../src/thirdparty/Iso9660.h"
#include "../src/thirdparty/Unrar.h"

// ---- unrar ----

int urarlib_list(const char *, ArchiveList_struct **list, const char *)
{
	*list = NULL;
	return 0;
}

int urarlib_get(const char *, const char *, const char *, const char *, UnrarSink *)
{
	return 0;
}

void urarlib_freelist(ArchiveList_struct *)
{
}

// ---- iso9660 ----

iso9660::iso9660()
{
	memset(m_reserved, 0, sizeof(m_reserved));
}

iso9660::~iso9660()
{
}

HANDLE iso9660::FindFirstFile(const char *, WIN32_FIND_DATAA *)
{
	return INVALID_HANDLE_VALUE;
}

int iso9660::FindNextFile(HANDLE, WIN32_FIND_DATAA *)
{
	return 0;
}

bool iso9660::FindClose(HANDLE)
{
	return true;
}

HANDLE iso9660::OpenFile(const char *)
{
	return INVALID_HANDLE_VALUE;
}

void iso9660::CloseFile(HANDLE)
{
}

long iso9660::ReadFile(HANDLE, BYTE *, long)
{
	return -1;
}

LONGLONG iso9660::Seek(HANDLE, LONGLONG, int)
{
	return -1;
}

LONGLONG iso9660::GetFileSize(HANDLE)
{
	return -1;
}

ULONGLONG iso9660::GetDiscSize()
{
	return 0;
}

const char *iso9660::GetVolumeName()
{
	return "";
}

// ---- FAT32 ----

CFat32Volume *g_fat32Volumes[4];   // 0x002776F4

BOOL Fat32Init(HANDLE)
{
	return FALSE;
}

void Fat32Shutdown()
{
}

const char *Fat32GetVolumeLabel(CFat32Volume *volume)
{
	return volume->m_szLabel;
}

ULONGLONG Fat32GetFreeBytes(CFat32Volume *volume)
{
	return volume->m_ullFreeBytes;
}

CFat32Volume *Fat32MountVolume(int)
{
	return NULL;
}

CFat32Io *Fat32GetIo(CFat32Volume *volume)
{
	return volume->m_pIo;
}

FAT32_FILE *Fat32Open(CFat32Io *, const char *, const char *)
{
	return NULL;
}

int Fat32Read(FAT32_FILE *, CFat32Io *, void *, int)
{
	return -1;
}

int Fat32Write(FAT32_FILE *, CFat32Io *, const void *, int)
{
	return -1;
}

void Fat32Close(FAT32_FILE *, CFat32Io *)
{
}

int Fat32Seek(FAT32_FILE *, CFat32Io *, long, int)
{
	return -1;
}

int Fat32Remove(CFat32Io *, const char *)
{
	return -1;
}

FAT32_FILE *Fat32CreateDirectory(CFat32Io *, const char *, int, int, int)
{
	return NULL;
}

HANDLE Fat32FindFirst(CFat32Volume *, const char *, WIN32_FIND_DATAA *)
{
	return INVALID_HANDLE_VALUE;
}

BOOL Fat32FindNext(CFat32Volume *, HANDLE, WIN32_FIND_DATAA *)
{
	return FALSE;
}

BOOL Fat32GetDirEntry(CFat32Volume *, DWORD, BYTE *)
{
	return FALSE;
}

BOOL Fat32RemoveDirectory(CFat32Volume *, const char *)
{
	return FALSE;
}
