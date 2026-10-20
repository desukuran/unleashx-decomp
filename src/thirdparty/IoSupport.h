// XBMC's CIoSupport (the 2003-2004 version, with an instance): the DVD
// tray, raw CD/DVD sector reads and drive letter links. Library code, not
// part of the decomp; the Xbox build links a version written from the binary
// (xbox/IoSupport.cpp). The methods are LTCG copies with register arguments
// (noted).
#pragma once

#include "../common/Platform.h"

class CIoSupport
{
public:
	CIoSupport();                                         // 0x000B343C (this in esi)
	virtual ~CIoSupport();                                // 0x000B33E1

	// The SMC's tray state: 0x10 open, 0x40 closed without media, 0x60
	// closed with media, anything else moving.
	DWORD GetTrayState();                                 // 0x000B329C
	HRESULT EjectTray();                                  // 0x000B32C8
	HRESULT CloseTray();                                  // 0x000B32AE
	// Turns the Xbox off (unless IsDebug).
	HRESULT Shutdown();                                   // 0x000B3282
	BOOL IsDebug();                                       // 0x000B30EF (always FALSE)

	HRESULT Mount(const char *drive, const char *device);   // 0x000B3339
	HRESULT Unmount(const char *drive);                     // 0x000B32E2
	// Unmount, FSCTL_DISMOUNT_VOLUME, Mount.
	HRESULT Remount(const char *drive, const char *device);   // 0x000B35EE
	// The DVD drive's device, NULL on failure; with remount, D: is
	// remounted (and then unlinked) first. (this in edi)
	HANDLE OpenCDROM(DWORD remount);                      // 0x000B36EA
	// A 2048-byte data sector, -1 on failure. (this in ecx, sector in eax)
	int ReadSector(HANDLE device, DWORD sector, char *buffer);       // 0x000B31D5
	// A raw 2352-byte audio sector (5 tries), -1 on failure. (this in ecx,
	// sector in eax)
	int ReadSectorCDDA(HANDLE device, DWORD sector, char *buffer);   // 0x000B30F7
	void CloseCDROM(HANDLE device);                       // 0x000B30F2 (CloseHandle)

	void *m_hSectorBuffer;      // +0x04 LocalAlloc'd, 0x930 bytes
	BYTE *m_pSectorBuffer;      // +0x08 locked; reads go through it
	DWORD m_dwTrayState;        // +0x0C from HalReadSMCTrayState
	DWORD m_dwTrayChangeCount;  // +0x10
	DWORD m_unknown14;          // +0x14
};
extern CIoSupport g_ioSupport;                            // 0x00278658
