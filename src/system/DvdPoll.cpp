// The DVD drive polling: the tray state from the SMC, and once a disc has
// settled, what kind of disc it is and its title. Xbox only (the drive and
// the SMC). The state it keeps is in Dvd.cpp.
#include <cstring>
#include <cwchar>

#include "../audio/CdAudio.h"
#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"
#include "../thirdparty/IoSupport.h"
#include "../thirdparty/Iso9660.h"
#include "Drives.h"
#include "Storage.h"

// CIoSupport::GetTrayState values.
#define TRAY_OPEN 0x10
#define TRAY_CLOSED_NO_MEDIA 0x40
#define TRAY_CLOSED_MEDIA 0x60

#define TRAY_POLL_INTERVAL 333   // ms
#define DETECT_DELAY 800         // ms after the disc was seen

iso9660 *g_pDvdIso9660;               // 0x00276380
CIoSupport g_ioSupport;               // 0x00278658

static BOOL s_bPolling;               // 0x00278D88 against reentry
static DWORD s_dwNextPoll = 1;        // 0x0026BBE0
static DWORD s_nTrayState;            // 0x00278D84 the last state seen
static DWORD s_dwDetectTime = 1;      // 0x0026BBE4 when to look at the new disc (0 = no)
static BOOL s_bDetecting;             // 0x00278D90 a disc waits to be looked at
// Cleared when the tray opens, never set: only an inserted disc is looked
// at.
static BOOL s_bUnknown8C;             // 0x00278D8C

// 0x000AB930
HRESULT RemountDvdDrive()
{
	char device[0x30] = "\\Device\\Cdrom0";
	UnmountDrive('D');
	ANSI_STRING name;
	RtlInitAnsiString(&name, device);
	OBJECT_ATTRIBUTES attributes;
	attributes.ObjectName = &name;
	attributes.RootDirectory = NULL;
	attributes.Attributes = OBJ_CASE_INSENSITIVE;
	HANDLE drive;
	IO_STATUS_BLOCK status;
	if (NtCreateFile(&drive, GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
	                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
	                 FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE) >= 0)
	{
		DWORD returned;
		if (!DeviceIoControl(drive, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &returned, NULL))
		{
			CloseHandle(drive);
			return E_FAIL;
		}
		CloseHandle(drive);
	}
	MountDrive('D');
	return S_OK;
}

// The tray state changed to state (not open). Returns whether the flags
// changed; detect: a disc is in and should be looked at.
static BOOL OnTrayStateChanged(DWORD state, BOOL extended, BOOL *detect)
{
	BOOL changed = FALSE;
	if (extended && state == TRAY_CLOSED_NO_MEDIA)
	{
		// The SMC can miss a disc: ask the drive.
		HANDLE drive = CreateFileA("cdrom0:", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		                           FILE_ATTRIBUTE_NORMAL, NULL);
		// (A failed open is INVALID_HANDLE_VALUE, but NULL is checked.)
		if (drive != NULL)
		{
			DWORD returned;
			if (DeviceIoControl(drive, IOCTL_CDROM_CHECK_VERIFY, NULL, 0, NULL, 0, &returned, NULL) ||
			    GetLastError() == ERROR_UNRECOGNIZED_MEDIA)
				state = TRAY_CLOSED_MEDIA;
			CloseHandle(drive);
		}
	}
	if (state != TRAY_CLOSED_NO_MEDIA && state != TRAY_CLOSED_MEDIA)
	{
		// Moving.
		if (g_bTrayOpen == TRUE || g_bTrayClosed == TRUE)
			changed = TRUE;
		g_bTrayOpening = g_bTrayClosed;
		g_bTrayEmpty = TRUE;
		g_bTrayClosed = FALSE;
		g_nDiscType = DISC_TRAY_MOVING;
		return changed;
	}

	// Closed.
	g_bTrayOpening = g_bTrayClosed;
	if (!g_bTrayClosed)
	{
		g_nDiscType = DISC_NONE;
		if (state == TRAY_CLOSED_MEDIA)
		{
			g_bTrayEmpty = FALSE;
			*detect = TRUE;
			changed = TRUE;
		}
		else
		{
			g_bTrayEmpty = TRUE;
		}
		g_bTrayClosed = TRUE;
	}
	else if (state == TRAY_CLOSED_MEDIA)
	{
		if (g_bTrayEmpty)
		{
			g_bTrayEmpty = FALSE;
			*detect = TRUE;
			changed = TRUE;
			g_nDiscType = DISC_NONE;
		}
	}
	else
	{
		g_bTrayEmpty = TRUE;
		g_nDiscType = DISC_NONE;
	}
	if (g_bTrayOpen == TRUE || !g_bTrayClosed)
		changed = TRUE;
	return changed;
}

// What's on the disc: a game (D:\default.xbe; its DiskTitle.txt or XBE
// title), a video DVD, a data disc readable through D: or only through
// the ISO 9660 reader, or an audio CD.
static void DetectDisc(SDriveInfo *info)
{
	if (FileExists("D:\\default.xbe"))
	{
		std::string title = "";
		if (FileExists("D:\\DiskTitle.txt"))
			title = ReadTextFile("D:\\DiskTitle.txt");
		if (title.size() == 0)
		{
			title = GetXbeTitleName("D:\\default.xbe");
			if (title.size() == 0)
				title = "Unknown Xbox Title";
		}
		if (title.size() != 0)
			MultiByteToWideChar(CP_ACP, 0, title.c_str(), (int)title.size(), g_szDiscTitle, 0x80);
		g_nDiscType = DISC_GAME;
		return;
	}
	if (FileExists("D:\\VIDEO_TS\\VIDEO_TS.IFO"))
	{
		g_nDiscType = DISC_VIDEO;
		return;
	}

	if (g_pDvdIso9660 != NULL)
	{
		delete g_pDvdIso9660;
		g_pDvdIso9660 = NULL;
	}
	if (g_nDiscType != DISC_CHECKING)
		return;
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirstFileA("D:\\*", &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		g_nDiscType = DISC_DATA2;
		CloseHandle(find);   // (not FindClose)
	}
	if (g_nDiscType != DISC_CHECKING)
		return;

	if (g_cdToc.hDevice != INVALID_HANDLE_VALUE)
		CdTocClose(&g_cdToc);
	CdTocOpen(&g_cdToc);
	if (g_cdToc.hDevice != INVALID_HANDLE_VALUE)
		g_nDiscType = DISC_AUDIO;
	if (g_nDiscType != DISC_CHECKING)
		return;

	// Last: the raw sectors. Sector 16 is the ISO 9660 volume descriptor.
	HANDLE drive = g_ioSupport.OpenCDROM(0);
	if (drive != INVALID_HANDLE_VALUE)
	{
		char sector[0x1000];
		if (g_ioSupport.ReadSector(drive, 16, sector) != -1)
			g_nDiscType = DISC_DATA;
		else if (g_ioSupport.ReadSectorCDDA(drive, 0, sector) != -1)
			g_nDiscType = DISC_AUDIO;
		g_ioSupport.CloseCDROM(drive);
		if (g_nDiscType == DISC_DATA)
		{
			g_pDvdIso9660 = new iso9660;
			if (g_pDvdIso9660 != NULL && info != NULL)
			{
				// All of it counts as used.
				ULONGLONG size = g_pDvdIso9660->GetDiscSize();
				info->freeBytes = 0;
				info->totalBytes = size;
				info->usedBytes = size;
				swprintf(g_szDiscTitle, 0x80, L"%hs", g_pDvdIso9660->GetVolumeName());
			}
		}
		if (g_szDiscTitle[0] == 0)
			wcscpy(g_szDiscTitle, L"ISO9660 Disc");
	}
	if (g_nDiscType == DISC_CHECKING)
		g_nDiscType = g_bTrayEmpty ? DISC_NONE : DISC_UNKNOWN;
}

// 0x000AB9D8
BOOL PollDvdDrive(BOOL extended)
{
	if (s_bPolling)
		return FALSE;
	s_bPolling = TRUE;
	BOOL changed = FALSE;
	BOOL detect = FALSE;
	DWORD now = GetTickCount();

	if (s_dwNextPoll < now)
	{
		DWORD next = now + TRAY_POLL_INTERVAL;
		s_dwNextPoll = next;
		DWORD state = g_ioSupport.GetTrayState();
		BOOL open = FALSE;
		if (s_nTrayState != state)
		{
			s_nTrayState = state;
			SDriveInfo *info = GetDriveInfo('D');
			if (info != NULL && state != info->trayState)
			{
				info->previousTrayState = info->trayState;
				info->trayState = state;
			}
			if (state == TRAY_OPEN)
			{
				open = TRUE;
			}
			else
			{
				changed = OnTrayStateChanged(state, extended, &detect);
				g_bTrayOpen = FALSE;
			}
		}
		else if (state == TRAY_OPEN)
		{
			open = TRUE;
		}
		else if (state == TRAY_CLOSED_NO_MEDIA)
		{
			g_bTrayOpening = TRUE;
			g_nDiscType = DISC_NONE;
			g_bTrayEmpty = TRUE;
			g_bTrayOpen = FALSE;
		}
		else if (state == TRAY_CLOSED_MEDIA)
		{
			g_bTrayOpen = FALSE;
			g_bTrayEmpty = FALSE;
			g_bTrayClosed = TRUE;
		}
		else
		{
			g_nDiscType = DISC_TRAY_MOVING;
		}
		if (open)
		{
			if (!g_bTrayOpen || g_bTrayClosed == TRUE)
				changed = TRUE;
			g_bTrayOpening = TRUE;
			g_bTrayOpen = TRUE;
			g_bTrayClosed = FALSE;
			g_bTrayEmpty = TRUE;
			g_nDiscType = DISC_NONE;
			s_bUnknown8C = FALSE;
		}
		s_dwNextPoll = next;
	}

	if (!g_bTrayClosed || g_bTrayEmpty)
	{
		// No disc: forget the old one. Quirk: only the first half of the
		// title is cleared, and the reader's device handle is read before
		// its destructor closes it and then closed again.
		s_bDetecting = FALSE;
		s_dwDetectTime = 0;
		if (g_szDiscTitle[0] != 0)
			memset(g_szDiscTitle, 0, sizeof(g_szDiscTitle) / 2);
		if (g_pDvdIso9660 != NULL)
		{
			HANDLE device = g_hIso9660Device;
			delete g_pDvdIso9660;
			g_pDvdIso9660 = NULL;
			g_ioSupport.CloseCDROM(device);
			UnmountDrive('D');
		}
		if (g_cdToc.hDevice != INVALID_HANDLE_VALUE)
			CdTocClose(&g_cdToc);
		s_bPolling = FALSE;
		return changed;
	}

	if (detect && !s_bDetecting)
	{
		// A new disc: remount D: and look at it in 800 ms.
		g_nDiscType = DISC_CHECKING;
		RemountDvdDrive();
		s_dwDetectTime = now + DETECT_DELAY;
		s_bDetecting = TRUE;
		memset(g_szDiscTitle, 0, sizeof(g_szDiscTitle) / 2);
	}
	if ((!s_bUnknown8C && g_nDiscType != DISC_CHECKING) || !s_bDetecting || s_dwDetectTime == 0 ||
	    s_dwDetectTime >= now)
	{
		s_bPolling = FALSE;
		return changed;
	}

	SDriveInfo *info = GetDriveInfo('D');
	DetectDisc(info);
	s_dwDetectTime = 0;
	if (info != NULL)
		UpdateDriveSpace(info);
	s_bPolling = FALSE;
	return TRUE;
}

// The tray state CheckDvdMedia last returned.
static int s_nMediaState;   // 0x00278D80

// 0x000AC019
int CheckDvdMedia(BOOL extended)
{
	if (PollDvdDrive(extended))
	{
		SDriveInfo *info = GetDriveInfo('D');
		if (info == NULL)
			return 1;
		s_nMediaState = (int)info->trayState;
		info->hasMedia = g_nDiscType != DISC_UNKNOWN && g_nDiscType != DISC_NONE;
		if (!info->hasMedia)
		{
			info->freeBytes = 0;
			info->usedBytes = 0;
			info->totalBytes = 0;
		}
	}
	return s_nMediaState;
}
