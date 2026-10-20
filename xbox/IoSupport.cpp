// CIoSupport (XBMC's, the 2003-2004 version) for the Xbox build, written from
// the binary: the third-party code isn't part of the decomp (see
// src/thirdparty/IoSupport.h), but this much is small and the build needs it.
#include "../src/thirdparty/IoSupport.h"
#include "../src/common/XboxKernel.h"

#include <stdio.h>
#include <string.h>

#define IOCTL_CDROM_RAW_READ 0x0002403E
#define RAW_SECTOR_SIZE 0x930
#define TRAY_OPEN 0x10

// The kernel's RAW_READ_INFO.
struct SRawReadInfo
{
	LARGE_INTEGER DiskOffset;
	ULONG SectorCount;
	ULONG TrackMode;   // 2: CDDA
};

static void InitString(ANSI_STRING *string, char *text)
{
	string->Length = (USHORT)strlen(text);
	string->MaximumLength = (USHORT)(strlen(text) + 1);
	string->Buffer = text;
}

// 0x000B343C
CIoSupport::CIoSupport()
{
	m_unknown14 = 0;
	m_hSectorBuffer = LocalAlloc(LMEM_ZEROINIT, RAW_SECTOR_SIZE);
	m_pSectorBuffer = NULL;
	if (m_hSectorBuffer != NULL)
		m_pSectorBuffer = (BYTE *)LocalLock(m_hSectorBuffer);
}

// 0x000B33E1
CIoSupport::~CIoSupport()
{
	if (m_hSectorBuffer != NULL)
	{
		LocalUnlock(m_hSectorBuffer);
		LocalFree(m_hSectorBuffer);
	}
}

// 0x000B30EF
BOOL CIoSupport::IsDebug()
{
	return FALSE;
}

// 0x000B30F2
void CIoSupport::CloseCDROM(HANDLE device)
{
	CloseHandle(device);
}

// 0x000B30F7
int CIoSupport::ReadSectorCDDA(HANDLE device, DWORD sector, char *buffer)
{
	SRawReadInfo info;
	info.DiskOffset.QuadPart = sector << 11;   // 32-bit shift, as the original
	info.SectorCount = 1;
	info.TrackMode = 2;
	for (int i = 0; i < 5; i++)
	{
		DWORD returned;
		// Quirk: the output size is sizeof(the buffer pointer).
		if (DeviceIoControl(device, IOCTL_CDROM_RAW_READ, &info, sizeof(info), m_pSectorBuffer, sizeof(m_pSectorBuffer),
		                    &returned, NULL))
		{
			memcpy(buffer, m_pSectorBuffer, RAW_SECTOR_SIZE);
			return RAW_SECTOR_SIZE;
		}
	}
	return -1;
}

// 0x000B31D5
int CIoSupport::ReadSector(HANDLE device, DWORD sector, char *buffer)
{
	for (int i = 0; i < 5; i++)
	{
		LARGE_INTEGER offset;
		offset.QuadPart = (LONGLONG)sector * 0x800;
		SetFilePointer(device, offset.LowPart, &offset.HighPart, FILE_BEGIN);
		DWORD read;
		if (ReadFile(device, m_pSectorBuffer, 0x800, &read, NULL))
		{
			memcpy(buffer, m_pSectorBuffer, 0x800);
			return read;
		}
	}
	return -1;
}

// 0x000B3282
HRESULT CIoSupport::Shutdown()
{
	if (IsDebug())
		return E_FAIL;
	HalInitiateShutdown();
	return S_OK;
}

// 0x000B329C
DWORD CIoSupport::GetTrayState()
{
	HalReadSMCTrayState(&m_dwTrayState, &m_dwTrayChangeCount);
	return m_dwTrayState;
}

// 0x000B32AE
HRESULT CIoSupport::CloseTray()
{
	if (GetTrayState() == TRAY_OPEN)
		HalWriteSMBusValue(0x20, 0x0C, FALSE, 1);
	return S_OK;
}

// 0x000B32C8
HRESULT CIoSupport::EjectTray()
{
	if (GetTrayState() != TRAY_OPEN)
		HalWriteSMBusValue(0x20, 0x0C, FALSE, 0);
	return S_OK;
}

// 0x000B32E2: removes \??\<drive>.
HRESULT CIoSupport::Unmount(const char *drive)
{
	char link[16];
	sprintf(link, "\\??\\%s", drive);
	ANSI_STRING linkName;
	InitString(&linkName, link);
	IoDeleteSymbolicLink(&linkName);
	return S_OK;
}

// 0x000B3339: links \??\<drive> to \Device\<device>.
HRESULT CIoSupport::Mount(const char *drive, const char *device)
{
	char deviceName[256];
	char link[16];
	sprintf(deviceName, "\\Device\\%s", device);
	sprintf(link, "\\??\\%s", drive);
	ANSI_STRING deviceString, linkName;
	InitString(&deviceString, deviceName);
	InitString(&linkName, link);
	IoCreateSymbolicLink(&linkName, &deviceString);
	return S_OK;
}

// 0x000B35EE: the volume is dismounted in between, so the file system
// mounts again (a new disc).
HRESULT CIoSupport::Remount(const char *drive, const char *device)
{
	char deviceName[64];
	sprintf(deviceName, "\\Device\\%s", device);
	Unmount(drive);
	ANSI_STRING deviceString;
	RtlInitAnsiString(&deviceString, deviceName);
	OBJECT_ATTRIBUTES attributes;
	attributes.RootDirectory = NULL;
	attributes.ObjectName = &deviceString;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	IO_STATUS_BLOCK status;
	HANDLE handle;
	if (NtCreateFile(&handle, GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
	                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
	                 FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE) >= 0)
	{
		DWORD returned;
		BOOL ok = DeviceIoControl(handle, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &returned, NULL);
		CloseHandle(handle);
		if (!ok)
			return E_FAIL;
	}
	Mount(drive, device);
	return S_OK;
}

// 0x000B36EA
HANDLE CIoSupport::OpenCDROM(DWORD remount)
{
	if (remount)
	{
		Remount("D:", "Cdrom0");
		Unmount("D:");
	}
	if (m_pSectorBuffer == NULL)
		return NULL;
	ANSI_STRING deviceString;
	RtlInitAnsiString(&deviceString, "\\Device\\Cdrom0");
	OBJECT_ATTRIBUTES attributes;
	attributes.RootDirectory = NULL;
	attributes.ObjectName = &deviceString;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	IO_STATUS_BLOCK status;
	HANDLE handle;
	if (NtCreateFile(&handle, GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
	                 FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE) < 0)
		return NULL;
	return handle;
}
