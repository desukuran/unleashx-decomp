#include "Drives.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "Storage.h"
#include "../net/FtpServer.h"

BOOL g_bDriveFMapped;                    // 0x0026BBF4
BOOL g_bDriveGMapped;                    // 0x0027637C
BOOL g_bMemoryUnitsChanged;              // 0x00276276 (BYTE)

SDriveInfo g_drives[DRIVE_INFO_COUNT];   // 0x00276388

// Letters MountAllDrives() walks. 0x0026BBF8
static const char kAllDrives[] = "CDEFGHIJKLMNOPQRSTUVXYZ";

static void InitAnsiString(ANSI_STRING &s, char *text)
{
	s.Length = (USHORT)strlen(text);
	s.MaximumLength = (USHORT)(s.Length + 1);
	s.Buffer = text;
}

// Returns the slot for a letter, initializing it (letter + type) on first use.
static SDriveInfo *InitDriveSlot(int index, char letter, int type)
{
	SDriveInfo *info = &g_drives[index];
	if (info->letter != 0)
		return info;
	info->mounted = FALSE;
	// sprintf(info, "%c", letter) in the original: the letter plus a NUL in linkName[0].
	info->letter = letter;
	info->linkName[0] = 0;
	info->type = type;
	return info;
}

// 0x000AA69E
SDriveInfo *GetDriveInfo(char drive)
{
	char letter = (char)toupper(drive);
	if (letter < 'T')
	{
		if (letter >= 'P')   // USB partitions
			return InitDriveSlot(letter - 'C', letter, DRIVE_USB);
		if (letter >= 'C' && letter <= 'G')
		{
			if ((letter == 'F' && !g_bDriveFMapped) || (letter == 'G' && !g_bDriveGMapped))
				return NULL;
			return InitDriveSlot(letter - 'C', letter, letter == 'D' ? DRIVE_DVD : DRIVE_HDD);
		}
		if (letter >= 'H' && letter <= 'O')   // memory units
			return InitDriveSlot(letter - 'C', letter, DRIVE_MU);
		return NULL;
	}
	// T U V X Y Z (no W). Note: T lands on S's slot.
	if ((letter < 'W') || (letter > 'W' && letter <= 'Z'))
		return InitDriveSlot(letter - 'D', letter, DRIVE_HDD);
	return NULL;
}

// 0x000AA512
// Device path and symbolic link name for the fixed letters.
static BOOL GetDriveLinkInfo(char drive, char *devicePath, char *linkName)
{
	const char *device;
	const char *link;
	switch (toupper(drive))
	{
	case 'T': device = "\\Device\\Harddisk0\\Partition1\\TDATA\\"; link = "\\??\\T:"; break;
	case 'C': device = "\\Device\\Harddisk0\\Partition2"; link = "\\??\\C:"; break;
	case 'D': device = "\\Device\\Cdrom0"; link = "\\??\\D:"; break;
	case 'E': device = "\\Device\\Harddisk0\\Partition1"; link = "\\??\\E:"; break;
	case 'F': device = "\\Device\\Harddisk0\\Partition6"; link = "\\??\\F:"; break;
	case 'G': device = "\\Device\\Harddisk0\\Partition7"; link = "\\??\\G:"; break;
	case 'U': device = "\\Device\\Harddisk0\\Partition1\\UDATA\\"; link = "\\??\\U:\\"; break;
	case 'V': device = "\\Device\\CdRom1"; link = "\\??\\V:"; break;
	case 'X': device = "\\Device\\Harddisk0\\Partition3"; link = "\\??\\X:"; break;
	case 'Y': device = "\\Device\\Harddisk0\\Partition4"; link = "\\??\\Y:"; break;
	case 'Z': device = "\\Device\\Harddisk0\\Partition5"; link = "\\??\\Z:"; break;
	default: return FALSE;
	}
	sprintf(devicePath, "%s", device);
	sprintf(linkName, "%s", link);
	return TRUE;
}

// 0x0009D6A3
void GetDiskSpace(char drive, ULONGLONG *total, ULONGLONG *free, ULONGLONG *used, int unit)
{
	*total = 0;
	*free = 0;
	*used = 0;

	// The original special-cases c/d/e/f/x/y/z, which gives the same result.
	char root[4];
	sprintf(root, "%c", toupper(tolower(drive)));
	strcat(root, ":\\");

	ULONGLONG freeAvailable, totalBytes, totalFree;
	if (!GetDiskFreeSpaceExA(root, XDK_CAST(PULARGE_INTEGER, &freeAvailable), XDK_CAST(PULARGE_INTEGER, &totalBytes), XDK_CAST(PULARGE_INTEGER, &totalFree)))
		return;
	if (unit == 0)
		unit = 1;
	*total = totalBytes / unit;
	*free = freeAvailable / unit;
	*used = (totalBytes - freeAvailable) / unit;
}

// 0x000AA99A
// TRUE when unmounted or not mounted.
BOOL UnmountDrive(char drive)
{
	drive = (char)toupper(drive);
	SDriveInfo *info = GetDriveInfo(drive);
	if (info == NULL)
		return FALSE;

	info->label[0] = 0;
	if (info->type == DRIVE_MU)
	{
		if (!UnmountMemoryUnit(info))
			return FALSE;
	}
	else
	{
		if (!info->mounted)
			return TRUE;
		if (!GetDriveLinkInfo(drive, info->devicePath, info->linkName))
			return FALSE;
		ANSI_STRING link;
		InitAnsiString(link, info->linkName);
		if (IoDeleteSymbolicLink(&link) != 0)
			return FALSE;
	}
	info->mounted = FALSE;
	info->totalBytes = 0;
	info->freeBytes = 0;
	info->usedBytes = 0;
	return TRUE;
}

// 0x000AAC8B
BOOL MountDrive(char drive)
{
	drive = (char)toupper(drive);
	SDriveInfo *info = GetDriveInfo(drive);
	if (info == NULL)
		return FALSE;

	if (info->type == DRIVE_MU)
		return MountMemoryUnit(info, drive);   // (inlined; see Storage.cpp)

	if (info->mounted)
		return TRUE;
	info->label[0] = 0;

	// G: is only usable on large disks: F: must exist and be over 1 GB.
	if (drive == 'G')
	{
		SDriveInfo *f = GetDriveInfo('F');
		if (f == NULL)
			return FALSE;
		if (f->letter == 0 && !MountDrive('F'))
			return FALSE;
		if (!f->mounted || f->totalBytes < 0x40000000ull)
			return FALSE;
	}

	if (!GetDriveLinkInfo(drive, info->devicePath, info->linkName))
		return FALSE;

	ANSI_STRING device, link;
	InitAnsiString(device, info->devicePath);
	InitAnsiString(link, info->linkName);
	IoDeleteSymbolicLink(&link);
	if (IoCreateSymbolicLink(&link, &device) != 0)
		return FALSE;

	info->hasMedia = drive != 'D';
	info->mounted = TRUE;

	char root[4] = { 0 };
	sprintf(root, "%c", drive);
	strcat(root, ":\\");
	char fileSystem[10] = { 0 };
	info->volumeSerial = 0;
	info->maxComponentLength = 0;
	GetVolumeInformationA(root, NULL, 0, &info->volumeSerial, &info->maxComponentLength, NULL, fileSystem,
	                      sizeof(fileSystem));
	memset(info->fileSystem, 0, sizeof(info->fileSystem));
	if (fileSystem[0] != 0)
		strcpy(info->fileSystem, fileSystem);

	GetDiskSpace(drive, &info->totalBytes, &info->freeBytes, &info->usedBytes, 1);
	if (info->totalBytes == 0 && (drive == 'X' || drive == 'Y' || drive == 'Z'))
	{
		// An unformatted cache partition: format it.
		return SUCCEEDED(FormatDrive(drive));
	}
	info->hasMedia = info->totalBytes != 0;
	return TRUE;
}

// 0x000AAA8A
BOOL MapFolderToDrive(SDriveInfo *info, char drive, const char *folder)
{
	if (info == NULL || folder == NULL || strlen(folder) == 0 || !DirectoryExists(folder))
		return FALSE;

	drive = (char)toupper(drive);
	UnmountDrive(drive);
	char source = (char)toupper(folder[0]);
	memset(info, 0, sizeof(*info));
	info->mounted = FALSE;

	char link[12];
	sprintf(link, "\\??\\%c:", drive);
	const char *partition;
	switch (source)
	{
	case 'C': partition = "\\Device\\Harddisk0\\Partition2"; break;
	case 'E': partition = "\\Device\\Harddisk0\\Partition1"; break;
	case 'F': partition = "\\Device\\Harddisk0\\Partition6"; break;
	case 'G': partition = "\\Device\\Harddisk0\\Partition7"; break;
	case 'X': partition = "\\Device\\Harddisk0\\Partition3"; break;
	case 'Y': partition = "\\Device\\Harddisk0\\Partition4"; break;
	case 'Z': partition = "\\Device\\Harddisk0\\Partition5"; break;
	default: return FALSE;
	}
	char device[100];
	sprintf(device, "%s", partition);
	strcat(device, folder + 2);   // the folder without its "X:"

	ANSI_STRING deviceName, linkName;
	InitAnsiString(deviceName, device);
	InitAnsiString(linkName, link);
	info->letter = 0;
	IoDeleteSymbolicLink(&linkName);
	if (IoCreateSymbolicLink(&linkName, &deviceName) != 0)
		return FALSE;   // TODO: verify the tail of the original

	info->type = DRIVE_HDD;
	info->letter = drive;
	info->mounted = TRUE;
	strcpy(info->devicePath, device);
	strcpy(info->linkName, link);
	info->hasMedia = TRUE;
	GetDiskSpace(drive, &info->totalBytes, &info->freeBytes, &info->usedBytes, 1);
	return TRUE;
}

// 0x000AB172
// Mounts every letter except T and U. If Z: can't be mounted the XDK
// utility drive is used instead.
void MountAllDrives()
{
	InitMemoryUnitTable();

	SDriveInfo *slot = g_drives;
	for (int i = 0; i < 0x17; i++, slot++)
	{
		char letter = kAllDrives[i];
		if (letter == 'U' || letter == 'T')
			continue;
		if (MountDrive(letter) || letter != 'Z')
			continue;
		if (XMountUtilityDrive(FALSE) == 0)
			continue;

		char device[0x29] = { 0 };
		// Quirk: the letter is passed where a pointer to the drive name
		// belongs (the device would be read from address 0x5A).
		if (GetUtilityDriveDevice((const char *)'Z', device) < 0)
			continue;
		slot->letter = 'Z';
		slot->linkName[0] = 0;
		strcpy(slot->devicePath, device);
		slot->mounted = TRUE;
		slot->hasMedia = TRUE;
		strcpy(slot->linkName, "\\??\\Z:");
		GetDiskSpace('Z', &slot->totalBytes, &slot->freeBytes, &slot->usedBytes, 1);
	}
}

// 0x000AB915
BOOL RemountDrive(SDriveInfo *info)
{
	if (info == NULL)
		return FALSE;
	UnmountDrive(info->letter);
	return MountDrive(info->letter);
}

// 0x000ABFFE
BOOL RemountDriveLetter(char drive)
{
	SDriveInfo *info = GetDriveInfo(drive);
	if (info == NULL)
		return FALSE;
	return RemountDrive(info);
}

// 0x000AA696
BOOL GetMemoryUnitsChanged()
{
	return g_bMemoryUnitsChanged;
}

// 0x000AB8F3
void SetDriveFMapped(BOOL mapped)
{
	if (g_bDriveFMapped == mapped)
		return;
	if (!mapped)
		UnmountDrive('F');
	else
		MountDrive('F');
	g_bDriveFMapped = mapped;
}

// 0x000AB8D1
void SetDriveGMapped(BOOL mapped)
{
	if (g_bDriveGMapped == mapped)
		return;
	if (!mapped)
		UnmountDrive('G');
	else
		MountDrive('G');
	g_bDriveGMapped = mapped;
}

// 0x000AB2FF
void MountTitleDataDrive(const char *xbePath)
{
	if (xbePath == NULL || strlen(xbePath) == 0)
		return;
	MountDrive('E');
	SDriveInfo *e = GetDriveInfo('E');
	if (e == NULL || !e->mounted)
		return;

	SXbeInfo xbe;
	if (FileExists(xbePath) && ReadXbeInfo(xbePath, &xbe))
	{
		std::string dir;
		StrFormat(dir, "E:\\UDATA\\%08x", xbe.titleId);
		BOOL created = !DirectoryExists(dir.c_str());
		if (created)
			CreateDirectoryTree(dir.c_str());

		char device[52];
		char link[12];
		sprintf(device, "%s%08x\\", "\\Device\\Harddisk0\\Partition1\\UDATA\\", xbe.titleId);
		sprintf(link, "%s", "\\??\\U:\\");
		ANSI_STRING deviceName, linkName;
		InitAnsiString(deviceName, device);
		InitAnsiString(linkName, link);
		IoDeleteSymbolicLink(&linkName);
		if (IoCreateSymbolicLink(&linkName, &deviceName) == 0)
		{
			std::string image(dir);
			image += "\\TitleImage.xbx";
			if (created || !FileExists(image.c_str()))
				ExtractXbeTitleImage(xbePath, image.c_str());

			std::string meta;
			StrFormat(meta, "E:\\UDATA\\%08x\\TitleMeta.xbx", xbe.titleId);
			if (created || !FileExists(meta.c_str()))
			{
				std::string text;
				StrFormat(text, "TitleName=%s", xbe.titleName.c_str());
				WriteTextFile(meta.c_str(), text.c_str(), FALSE);
			}
		}
	}
}

// 0x000AB7F1
void RefreshDrives(BOOL force)
{
	static DWORD s_nextSpaceUpdate = GetTickCount() + 30000;                  // 0x0027A3FC
	static DWORD s_lastFilesReceived = g_ftpServer.m_dwFilesReceived;         // 0x0027A3F8
	static DWORD s_nextUnitCheck = GetTickCount() + 1000;                     // 0x0027A3F4

	DWORD now = GetTickCount();
	int distance = (int)(now - s_nextSpaceUpdate);
	if (!force || (distance < 0 ? -distance : distance) < 0x1F5)
	{
		if (s_lastFilesReceived != g_ftpServer.m_dwFilesReceived)
		{
			s_lastFilesReceived = g_ftpServer.m_dwFilesReceived;
			s_nextSpaceUpdate = 0;
		}
	}
	else
	{
		s_nextSpaceUpdate = 0;
	}

	if (s_nextUnitCheck < now || s_nextSpaceUpdate == 0)
	{
		s_nextUnitCheck = now + 1000;
		RefreshMemoryUnits();
	}
	if (s_nextSpaceUpdate <= now)
	{
		s_nextSpaceUpdate = now + 30000;
		for (int i = 0; i < DRIVE_INFO_COUNT; i++)
			UpdateDriveSpace(&g_drives[i]);
	}
}
