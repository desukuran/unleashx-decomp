// Audio CDs (from the MS dashboard's code, 0x0007EFA5-0x0007FE11): the
// table of contents, raw reads of CD-DA sectors, and XCDPlayer, the pump
// that streams a track. See CdAudio.cpp.
#pragma once

#include "../common/Platform.h"
#include "AudioPump.h"

#define CD_SECTOR_SIZE 0x930          // a raw CD-DA sector (2352 bytes)
#define CD_FRAMES_PER_SECOND 75
#define CD_LEADIN_FRAMES 150          // 2 s before LBA 0

// The audio CD's table of contents (the DVD drive opened as "CDROM0:").
struct SCdToc
{
	SCdToc();                         // 0x0007F496 (this in ecx)
	~SCdToc();                        // 0x0007F485 (this in eax)

	HANDLE hDevice;        // +0x00 INVALID_HANDLE_VALUE when closed
	int nTracks;           // +0x04
	// Each track's first frame (MSF counted from 00:00:00, so 150 more
	// than its LBA), then the lead-out. TODO: verify the size
	DWORD trackSector[100];   // +0x08
};
extern SCdToc g_cdToc;     // 0x00278748

// Opens the drive and reads the TOC (the device is closed again when the
// TOC can't be read; S_OK either way unless the open fails). (toc in eax)
HRESULT CdTocOpen(SCdToc *toc);          // 0x0007F5FB
void CdTocClose(SCdToc *toc);            // 0x0007F46D (toc in esi)
// The length of a track (0-based: index nTracks - 1 is the last). Any
// output can be NULL. (track in edx, minutes in esi, seconds in edi,
// frames in ebx)
BOOL CdTocGetTrackLength(SCdToc *toc, int track, DWORD *minutes, DWORD *seconds, DWORD *frames);   // 0x0007F4A4
// Reads count raw sectors from frame (MSF-based, see SCdToc), trying
// retries more times. (count in ecx, frame in eax)
HRESULT CdTocReadSectors(SCdToc *toc, DWORD frame, DWORD count, void *buffer, int retries);   // 0x0007F416

// Reads raw CD audio from the TOC's drive, 16 sectors at a time. 0x14
// bytes.
struct SCddaReader
{
	BYTE *buffer;          // +0x00 0x9300 bytes of contiguous memory
	int bufferPos;         // +0x04 how much of the buffer was handed out
	SCdToc *toc;           // +0x08
	int retries;           // +0x0C
	DWORD sector;          // +0x10 the next frame to read (up to the lead-out)
};
// (reader in esi, toc in ecx)
void CddaReaderOpen(SCddaReader *reader, SCdToc *toc, int retries);   // 0x0007FD6E
void CddaReaderClose(SCddaReader *reader);                            // 0x0007FD61 (reader in eax)
// Up to size bytes: the count, 0 at the lead-out, below 0 when a read
// fails. (reader in ebx)
int CddaReaderRead(SCddaReader *reader, void *buffer, int size);      // 0x0007FE11

// A CD track, streamed. 0x80 bytes.
class XCDPlayer : public CAudioPump
{
public:
	XCDPlayer();                                          // 0x0007F147 (this in edi)
	virtual ~XCDPlayer();                                 // 0x0007F115 (this in esi; deleting 0x0003291A)
	// Back to the track's start.
	virtual void Stop();                                  // 2, 0x0007F093
	// Capped at the track's length.
	virtual float GetPosition();                          // 3, 0x0007F060
	virtual float GetDuration();                          // 4, 0x0007EFAF
	virtual const BYTE *GetLastData() { return m_pLastData; }   // 7, 0x0007EFAB
	virtual DWORD GetPacketSize() { return 0x2B110; }     // 8, 0x0007EFA5 one second
	virtual int Read(void *buffer, int size);             // 10, 0x0007F174
	virtual void OnEnd() {}                               // 11, 0x0007F0A7

	// Opens track (0-based) as 44.1 kHz 16-bit stereo; format gets the
	// format. E_INVALIDARG without a CD or for a bad track. (this in ecx,
	// format in eax, track in edx)
	HRESULT Open(WAVEFORMATEX *format, int track);        // 0x0007F0A8

	DWORD m_dwStart;                       // +0x58 the track's first frame
	DWORD m_dwEnd;                         // +0x5C the next track's
	SCddaReader m_reader;                  // +0x60
	const BYTE *m_pLastData;               // +0x74
	int m_nTrack;                          // +0x78
	int m_nTracks;                         // +0x7C

private:
	// (this in eax, track in edx)
	BOOL SetTrack(int track);                             // 0x0007F008
	// The reader's position, under the mutex. (this in esi)
	DWORD GetSector();                                    // 0x0007EFCE
};
