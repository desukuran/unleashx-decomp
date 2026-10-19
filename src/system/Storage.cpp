// Memory units, USB mass storage (FAT32), formatting and the drives' space.
// Xbox only (XAPI's memory unit calls, the FAT32 driver).
#include "Storage.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../thirdparty/Fat32.h"
#include "../thirdparty/Iso9660.h"
#include "Drives.h"
#include "XBMemUnit.h"

std::vector<CXBMemUnit> g_memoryUnits;   // 0x00278738
CStorageInfo g_storageInfo;              // 0x002788E0

// The lock around the space updates, made by the first CStorageInfo.
static RTL_CRITICAL_SECTION s_driveLock;   // 0x00271AB8
static int s_nStorageInfos;                // 0x00276378

#define MEMORY_UNIT_COUNT 8
#define USB_DRIVE_COUNT 4

// 0x000AA977
CStorageInfo::CStorageInfo()
{
	if (s_nStorageInfos == 0)
		RtlInitializeCriticalSection(&s_driveLock);
	s_nStorageInfos++;
}

// 0x000AA953
CStorageInfo::~CStorageInfo()
{
	if (g_pDvdIso9660 != NULL)
	{
		delete g_pDvdIso9660;
		g_pDvdIso9660 = NULL;
	}
	s_nStorageInfos--;
}

void InitMemoryUnitTable()
{
	if (g_memoryUnits.size() == 0)
		g_memoryUnits.resize(MEMORY_UNIT_COUNT);
}

// The slot as USB mass storage: the device opened raw for the FAT32
// driver, each partition it finds on P..S. The memory unit letter itself
// stays unmounted.
static BOOL MountUsbStorage(SDriveInfo *info)
{
	char deviceName[0x20];
	ANSI_STRING name;
	name.Length = 0;
	name.MaximumLength = 0x1F;
	name.Buffer = deviceName;
	BOOL found = FALSE;
	if (MU_CreateDeviceObject(info->port, info->slot, &name) != 0)
		return FALSE;

	HANDLE device = INVALID_HANDLE_VALUE;
	OBJECT_ATTRIBUTES attributes;
	attributes.RootDirectory = NULL;
	attributes.ObjectName = &name;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	IO_STATUS_BLOCK status;
	if (NtOpenFile(&device, GENERIC_ALL | SYNCHRONIZE, &attributes, &status, FILE_SHARE_READ | FILE_SHARE_WRITE,
	               FILE_SYNCHRONOUS_IO_ALERT | FILE_NO_INTERMEDIATE_BUFFERING) == 0)
	{
		Fat32Init(device);
		for (int i = 0; i < USB_DRIVE_COUNT; i++)
		{
			CFat32Volume *volume = Fat32MountVolume(i);
			if (volume == NULL)
				continue;
			found = TRUE;
			info->muOrUsb = FALSE;
			SDriveInfo *usb = GetDriveInfo((char)('P' + i));
			if (usb == NULL)
				continue;
			usb->mounted = TRUE;
			usb->hasMedia = TRUE;
			usb->muIndex = info->muIndex;
			usb->port = info->port;
			usb->slot = info->slot;
			usb->letter = (char)('P' + i);
			// (Only the first character of the link name is set.)
			usb->linkName[0] = (char)('P' + i);
			strncpy(usb->devicePath, Fat32GetVolumeLabel(volume), 11);
			memcpy(usb->label, usb->devicePath, 11);
			usb->totalBytes = volume->m_ullTotalBytes;
			usb->freeBytes = Fat32GetFreeBytes(volume);
			// (usedBytes is left as it was.)
		}
		if (found)
			return FALSE;
	}
	MU_CloseDeviceObject(info->port, info->slot);
	return FALSE;
}

// 0x000AAEAA (the DRIVE_MU branch of MountDrive)
BOOL MountMemoryUnit(SDriveInfo *info, char drive)
{
	int index = drive - 'H';
	if ((CXBMemUnit::GetDevices() & CXBMemUnit::GetMask(index)) == 0)
		return FALSE;
	info->label[0] = 0;
	info->muIndex = index;
	info->port = CXBMemUnit::GetPort(index);
	info->slot = CXBMemUnit::GetSlot(index);
	CXBMemUnit *unit = &g_memoryUnits[index];
	unit->SetPortSlot(info->port, info->slot);
	try
	{
		info->muOrUsb = TRUE;
		info->letter = drive;
		DWORD error;
		if (!unit->Mount(&error))
		{
			unit->Unmount();
			info->totalBytes = 0;
			info->freeBytes = 0;
			info->usedBytes = 0;
			info->mounted = FALSE;
			info->hasMedia = FALSE;
			info->letter = drive;
			return MountUsbStorage(info);
		}

		info->mounted = TRUE;
		info->hasMedia = TRUE;
		wchar_t name[0x36];
		unit->GetName(name);
		char mounted = unit->GetDrive();
		if (mounted != 0)
		{
			// The drive XAPI chose. Quirk: the letter is copied as a string
			// from a single char, so whatever follows it on the stack lands in
			// the link name too.
			if (mounted != info->letter)
				info->letter = mounted;
			info->linkName[0] = mounted;
			info->linkName[1] = 0;
		}
		std::string narrow = StrFromWide(name);   // (W2A in the original)
		strncpy(info->devicePath, narrow.c_str(), 0x1D);
		// (The space is read through the requested letter, not XAPI's.)
		GetDiskSpace(drive, &info->totalBytes, &info->freeBytes, &info->usedBytes, 1);
		memcpy(info->label, info->devicePath, 11);
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

// 0x000AAA31 (the DRIVE_MU branch of UnmountDrive)
BOOL UnmountMemoryUnit(SDriveInfo *info)
{
	try
	{
		CXBMemUnit *unit = &g_memoryUnits[info->muIndex];
		if (!unit->IsMounted())
			return FALSE;
		unit->Unmount();
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

// 0x000AB587
void RefreshMemoryUnits()
{
	g_bMemoryUnitsChanged = FALSE;
	DWORD inserted, removed;
	if (!CXBMemUnit::GetDeviceChanges(&inserted, &removed))
		return;

	for (unsigned i = 0; i < MEMORY_UNIT_COUNT; i++)
	{
		if ((removed & CXBMemUnit::GetMask(i)) == 0)
			continue;
		SDriveInfo *info = &g_drives['H' - 'C' + i];
		info->mounted = FALSE;
		info->hasMedia = FALSE;
		info->muOrUsb = FALSE;
		g_memoryUnits[i].Clear();
		g_bMemoryUnitsChanged = TRUE;
		// USB storage in that slot: shut the driver down and forget all of
		// P..S from the first one that came from it.
		for (unsigned j = 0; j < USB_DRIVE_COUNT; j++)
		{
			SDriveInfo *usb = &g_drives['P' - 'C' + j];
			if (usb->muIndex != (int)i || !usb->mounted)
				continue;
			Fat32Shutdown();
			try
			{
				MU_CloseDeviceObject(usb->port, usb->slot);
			}
			catch (...)
			{
			}
			for (; j < USB_DRIVE_COUNT; j++)
			{
				SDriveInfo *rest = &g_drives['P' - 'C' + j];
				rest->mounted = FALSE;
				rest->hasMedia = FALSE;
				rest->muOrUsb = FALSE;
			}
		}
	}

	for (unsigned i = 0; i < MEMORY_UNIT_COUNT; i++)
	{
		if ((inserted & CXBMemUnit::GetMask(i)) == 0)
			continue;
		SDriveInfo *info = &g_drives['H' - 'C' + i];
		info->muOrUsb = TRUE;
		info->letter = (char)('H' + i);
		MountDrive((char)('H' + i));
		g_bMemoryUnitsChanged = TRUE;
	}
}

// 0x000AB6D4 (info in esi)
void UpdateDriveSpace(SDriveInfo *info)
{
	if (info == NULL)
		return;
	if (info->type == DRIVE_MU)
	{
		RefreshMemoryUnits();
		if (!info->mounted)
			return;
	}
	RtlEnterCriticalSection(&s_driveLock);
	if (info->type == DRIVE_DVD)
	{
		// Measured once per disc: a data disc by its ISO 9660 size (all of
		// it used), an audio CD not at all.
		info->hasMedia = IsDiscPresent();
		if (info->totalBytes == 0 && info->hasMedia)
		{
			if (g_nDiscType == DISC_DATA)
			{
				if (g_pDvdIso9660 != NULL)
				{
					info->totalBytes = g_pDvdIso9660->GetDiscSize();
					info->usedBytes = info->totalBytes;
				}
			}
			else if (g_nDiscType != DISC_AUDIO)
			{
				GetDiskSpace(info->letter, &info->totalBytes, &info->freeBytes, &info->usedBytes, 1);
			}
		}
	}
	else if (info->type == DRIVE_USB)
	{
		CFat32Volume *volume = Fat32MountVolume(info->letter - 'P');
		if (volume != NULL)
		{
			info->totalBytes = volume->m_ullTotalBytes;
			info->freeBytes = Fat32GetFreeBytes(volume);
			info->usedBytes = info->totalBytes - info->freeBytes;
		}
		else
		{
			info->totalBytes = 0;
			info->freeBytes = 0;
			info->usedBytes = 0;
		}
	}
	else if (info->hasMedia)
	{
		GetDiskSpace(info->letter, &info->totalBytes, &info->freeBytes, &info->usedBytes, 1);
	}
	else
	{
		info->totalBytes = 0;
		info->freeBytes = 0;
		info->usedBytes = 0;
	}
	RtlLeaveCriticalSection(&s_driveLock);
}

// What NtQuerySymbolicLinkObject writes the link's target into. Quirk: a
// constant string of 48 spaces in the original.
static char s_szLinkTarget[] = "                                                ";   // 0x0025AA40

// 0x0009DE6C (drive in eax)
HRESULT GetUtilityDriveDevice(const char *drive, char *devicePath)
{
	HRESULT hr = E_FAIL;
	char linkPath[48];
	sprintf(linkPath, "\\??\\%s", drive);
	ANSI_STRING linkName;
	RtlInitAnsiString(&linkName, linkPath);
	OBJECT_ATTRIBUTES attributes;
	attributes.RootDirectory = NULL;
	attributes.ObjectName = &linkName;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	HANDLE link;
	if (NtOpenSymbolicLinkObject(&link, &attributes) < 0)
		return hr;
	ANSI_STRING target;
	RtlInitAnsiString(&target, s_szLinkTarget);
	ULONG length = 0;
	if (link != NULL && NtQuerySymbolicLinkObject(link, &target, &length) >= 0)
	{
		strcpy(devicePath, target.Buffer);
		hr = S_OK;
	}
	NtClose(link);
	return hr;
}

// 0x0009DA9B
HRESULT FormatMemoryUnit(char drive)
{
	if (drive < 'H' || drive > 'O')
		return E_FAIL;
	DWORD index = (DWORD)(drive - 'H');
	DWORD port = CXBMemUnit::GetPort(index);
	DWORD slot = CXBMemUnit::GetSlot(index);
	char deviceName[0x20];
	ANSI_STRING name;
	name.Length = 0;
	name.MaximumLength = 0x1F;
	name.Buffer = deviceName;
	if (MU_CreateDeviceObject(port, slot, &name) != 0)
		return E_FAIL;
	HRESULT hr = XapiFormatFATVolume(&name) ? S_OK : E_FAIL;
	MU_CloseDeviceObject(port, slot);
	return hr;
}

// 0x000A94F0
DWORD FormatPartition(int partition)
{
	std::string device;
	StrFormat(device, "\\Device\\Harddisk0\\Partition%d", partition);
	DWORD result = (DWORD)MapDriveLink("\\??\\Z:", device.c_str(), TRUE);
	if ((NTSTATUS)result >= 0)
		result = XFormatUtilityDrive() ? 0 : GetLastError();
	RemountDriveLetter('Z');
	return result;
}

// 0x000A956B
// Quirks: any hard disk partition (even C: and E:) is formatted through
// Z:; USB letters P..S are unmounted and then fail.
HRESULT FormatDrive(char drive)
{
	drive = (char)toupper(drive);
	int partition;
	switch (drive)
	{
	case 'C': partition = 2; break;
	case 'E': partition = 1; break;
	case 'F': partition = 6; break;
	case 'G': partition = 7; break;
	case 'X': partition = 3; break;
	case 'Y': partition = 4; break;
	case 'Z': partition = 5; break;
	default:
		if (drive >= 'H' && drive < 'T')
		{
			UnmountDrive(drive);
			if (FAILED(FormatMemoryUnit(drive)))
				return E_FAIL;
			Sleep(500);
			MountDrive(drive);
			return S_OK;
		}
		return E_FAIL;
	}
	UnmountDrive(drive);
	HRESULT hr = (HRESULT)FormatPartition(partition);
	MountDrive(drive);
	return hr;
}
