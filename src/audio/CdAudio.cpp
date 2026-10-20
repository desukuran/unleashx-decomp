// Audio CDs: the TOC, raw CD-DA reads and XCDPlayer. See CdAudio.h.
#include "DashUtil.h"
#include "CdAudio.h"

#include <cstring>

#include "../common/XboxKernel.h"

#define IOCTL_CDROM_READ_TOC 0x00024000
#define IOCTL_CDROM_RAW_READ 0x0002403E
#define CDDA_TRACK_MODE 2                 // TRACK_MODE_TYPE CDDA
#define CDDA_READ_SECTORS 16
#define CDDA_BUFFER_SIZE (CDDA_READ_SECTORS * CD_SECTOR_SIZE)
#define TOC_LEADOUT_TRACK 0xAA
#define TOC_DATA_TRACK 0x04               // the control bit of a data track

SCdToc g_cdToc;   // 0x00278748

// The drive's TOC as IOCTL_CDROM_READ_TOC returns it. 0x324 bytes.
struct SCdromToc
{
	BYTE length[2];
	BYTE firstTrack;
	BYTE lastTrack;
	struct
	{
		BYTE reserved;
		BYTE control;      // low 4 bits; the ADR in the high 4
		BYTE trackNumber;
		BYTE reserved1;
		BYTE address[4];   // 0, minutes, seconds, frames
	} tracks[100];
};

// IOCTL_CDROM_RAW_READ's input. 0x10 bytes.
struct SRawReadInfo
{
	LONGLONG diskOffset;
	DWORD sectorCount;
	DWORD trackMode;
};

// 0x000328F2
static DWORD MsfToFrames(const BYTE *address)
{
	return (address[1] * 60 + address[2]) * CD_FRAMES_PER_SECOND + address[3];
}

// 0x000328C8
// Minutes, seconds and frames packed as bytes 0, 1, 2.
static DWORD FramesToMsf(DWORD frames)
{
	DWORD minutes = frames / (60 * CD_FRAMES_PER_SECOND);
	DWORD rest = frames % (60 * CD_FRAMES_PER_SECOND);
	return ((((rest / CD_FRAMES_PER_SECOND) & 0xFFFF) | (rest % CD_FRAMES_PER_SECOND) << 8) << 8) | (minutes & 0xFF);
}

// 0x0007F3C5
// DeviceIoControl as an HRESULT. (returned in eax; NULL uses outSize.)
static HRESULT CdIoctl(SCdToc *toc, DWORD code, void *in, DWORD inSize, void *out, DWORD outSize,
                       DWORD *returned)
{
	if (returned == NULL)
		returned = &outSize;
	if (DeviceIoControl(toc->hDevice, code, in, inSize, out, outSize, returned, NULL))
		return S_OK;
	DWORD error = GetLastError();
	if ((int)error > 0)
		return (HRESULT)((error & 0xFFFF) | 0x80070000);
	return (HRESULT)error;
}

// 0x0007F496 (nTracks cleared through 0x0007F3C0)
SCdToc::SCdToc()
{
	nTracks = 0;
	hDevice = INVALID_HANDLE_VALUE;
}

// 0x0007F485
SCdToc::~SCdToc()
{
	CdTocClose(this);
	nTracks = 0;
}

// 0x0007F46D (nTracks cleared through 0x0007F3B6)
void CdTocClose(SCdToc *toc)
{
	if (toc->hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(toc->hDevice);
		toc->hDevice = INVALID_HANDLE_VALUE;
	}
	toc->nTracks = 0;
}

// 0x0007F525
// The tracks' start frames and the lead-out. On an enhanced CD the data
// track is taken as the lead-out, moved back by 1:30. (toc in edi)
// Quirk: a "not ready" error is meant to be retried, but the loop gives
// up after the first try.
static BOOL CdTocRead(SCdToc *toc)
{
	SCdromToc cdToc;
	HRESULT hr;
	int tries = 0;
	for (;;)
	{
		hr = CdIoctl(toc, IOCTL_CDROM_READ_TOC, NULL, 0, &cdToc, sizeof(cdToc), NULL);
		if (SUCCEEDED(hr))
			break;
		if (hr != (HRESULT)0x80070015 /* ERROR_NOT_READY */)
			return FALSE;
		tries++;
		if (tries >= 1)
			return FALSE;
	}
	if (cdToc.firstTrack > cdToc.lastTrack)
		return FALSE;
	UINT count = cdToc.lastTrack - cdToc.firstTrack + 1;
	UINT i = 0;
	for (;; i++)
	{
		toc->trackSector[i] = MsfToFrames(cdToc.tracks[i].address);
		if (cdToc.tracks[i].trackNumber == TOC_LEADOUT_TRACK)
			break;
		if (cdToc.tracks[i].control & TOC_DATA_TRACK)
		{
			BYTE *address = cdToc.tracks[i].address;
			address[2] += 30;
			address[1] -= 2;
			if (address[2] < 60)
				address[1]--;
			else
				address[2] -= 60;
			toc->trackSector[i] = MsfToFrames(address);
			break;
		}
		if (i + 1 > count)
			break;
	}
	toc->nTracks = i;
	return (int)i > 0;
}

// 0x0007F5FB
HRESULT CdTocOpen(SCdToc *toc)
{
	toc->hDevice = DashCreateFile("CDROM0:", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
	                              NULL);
	if (toc->hDevice == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		if ((int)error > 0)
			return (HRESULT)((error & 0xFFFF) | 0x80070000);
		return (HRESULT)error;
	}
	if (!CdTocRead(toc))
		CdTocClose(toc);
	return S_OK;
}

// 0x0007F4A4
BOOL CdTocGetTrackLength(SCdToc *toc, int track, DWORD *minutes, DWORD *seconds, DWORD *frames)
{
	if (toc->hDevice == INVALID_HANDLE_VALUE || track < 0 || track > toc->nTracks - 1)
		return FALSE;
	DWORD msf = FramesToMsf(toc->trackSector[track + 1] - toc->trackSector[track]);
	if (minutes != NULL)
		*minutes = msf & 0xFF;
	if (seconds != NULL)
		*seconds = (msf >> 8) & 0xFF;
	if (frames != NULL)
		*frames = (msf >> 16) & 0xFF;
	return TRUE;
}

// 0x0007F416
HRESULT CdTocReadSectors(SCdToc *toc, DWORD frame, DWORD count, void *buffer, int retries)
{
	SRawReadInfo info;
	info.diskOffset = (LONGLONG)(ULONGLONG)(frame - CD_LEADIN_FRAMES) * 0x800;
	info.sectorCount = count;
	info.trackMode = CDDA_TRACK_MODE;
	HRESULT hr;
	do
	{
		hr = CdIoctl(toc, IOCTL_CDROM_RAW_READ, &info, sizeof(info), buffer, count * CD_SECTOR_SIZE, NULL);
		if (SUCCEEDED(hr))
			break;
	} while (retries-- != 0);
	return hr;
}

// ---- SCddaReader ----

// 0x0007FD6E
void CddaReaderOpen(SCddaReader *reader, SCdToc *toc, int retries)
{
	reader->toc = toc;
	reader->bufferPos = 0;
	reader->retries = retries;
	reader->sector = 0;
	reader->buffer = (BYTE *)XPhysicalAlloc(CDDA_BUFFER_SIZE, (uintptr_t)-1, 0, 4 /* PAGE_READWRITE */);
}

// 0x0007FD61
void CddaReaderClose(SCddaReader *reader)
{
	if (reader->buffer != NULL)
		XPhysicalFree(reader->buffer);   // (MmFreeContiguousMemory)
}

// 0x0007FD96
// Up to count sectors (stopping at the lead-out) into buffer: the bytes,
// 0 at the end, -1 on errors (the buffer is then zeroed). (reader in
// ecx, count in eax)
static int CddaReaderReadSectors(SCddaReader *reader, DWORD count, void *buffer)
{
	SCdToc *toc = reader->toc;
	int last = toc->hDevice == INVALID_HANDLE_VALUE ? 0 : toc->nTracks;
	DWORD leadOut = toc->hDevice == INVALID_HANDLE_VALUE ? 0 : toc->trackSector[last];
	if (count >= leadOut - reader->sector)
		count = leadOut - reader->sector;
	HRESULT hr = CdTocReadSectors(toc, reader->sector, count, buffer, reader->retries);
	if (FAILED(hr))
	{
		memset(buffer, 0, count * CD_SECTOR_SIZE);
		return hr == E_INVALIDARG ? 0 : -1;
	}
	reader->sector += count;
	return count * CD_SECTOR_SIZE;
}

// 0x0007FE11
// Whole 16-sector reads go straight into the caller's buffer; the rest
// through the reader's own, keeping what's left for the next call.
// (Quirks: an error or the end returns 0 or -1 even when bytes were
// already copied; and the leftover position is set to the bytes wanted,
// not the bytes copied.)
int CddaReaderRead(SCddaReader *reader, void *buffer, int size)
{
	if (reader->buffer == NULL)
		return -1;
	BYTE *out = (BYTE *)buffer;
	if (reader->bufferPos != 0)
	{
		int left = CDDA_BUFFER_SIZE - reader->bufferPos;
		int copy = left <= size ? left : size;
		memcpy(out, reader->buffer + reader->bufferPos, copy);
		size -= copy;
		out += copy;
		reader->bufferPos = (reader->bufferPos + copy) % CDDA_BUFFER_SIZE;
	}
	while (size >= CDDA_BUFFER_SIZE)
	{
		int read = CddaReaderReadSectors(reader, CDDA_READ_SECTORS, out);
		if (read < 1)
			return read;
		out += read;
		size -= read;
	}
	if (size != 0)
	{
		int read = CddaReaderReadSectors(reader, CDDA_READ_SECTORS, reader->buffer);
		if (read < 1)
			return read;
		if (read < CDDA_BUFFER_SIZE)
			memset(reader->buffer + read, 0, CDDA_BUFFER_SIZE - read);
		int copy = size <= read ? size : read;
		reader->bufferPos = size;
		memcpy(out, reader->buffer, copy);
		out += copy;
	}
	return out - (BYTE *)buffer;
}

// ---- XCDPlayer ----

// 0x0007F147
XCDPlayer::XCDPlayer()
{
	CddaReaderOpen(&m_reader, &g_cdToc, 0);
	m_dwStart = 0;
	m_dwEnd = 0;
	m_pLastData = NULL;
}

// 0x0007F115
XCDPlayer::~XCDPlayer()
{
	// The refill thread reads the CD: stop it before the reader goes.
	if (m_hThread != NULL)
	{
		if (m_hStopEvent != NULL)
			SetEvent(m_hStopEvent);
		WaitForSingleObject(m_hThread, INFINITE);
	}
	CddaReaderClose(&m_reader);
}

// 0x0007F008
BOOL XCDPlayer::SetTrack(int track)
{
	m_nTrack = track;
	if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
		return FALSE;
	m_nTracks = g_cdToc.nTracks;
	if (track < 0 || track >= m_nTracks)
		return FALSE;
	m_dwStart = g_cdToc.hDevice == INVALID_HANDLE_VALUE ? 0 : g_cdToc.trackSector[track];
	m_dwEnd = g_cdToc.hDevice == INVALID_HANDLE_VALUE ? 0 : g_cdToc.trackSector[track + 1];
	m_reader.bufferPos = 0;
	m_reader.sector = m_dwStart;
	return TRUE;
}

// 0x0007F0A8
HRESULT XCDPlayer::Open(WAVEFORMATEX *format, int track)
{
	if (!SetTrack(track))
		return E_INVALIDARG;
	memset(format, 0, sizeof(WAVEFORMATEX));
	format->nAvgBytesPerSec = 0x2B110;
	format->wFormatTag = WAVE_FORMAT_PCM;
	format->nChannels = 2;
	format->nSamplesPerSec = 44100;
	format->wBitsPerSample = 16;
	format->nBlockAlign = 4;
	HRESULT hr = Create(4, 0, format, 0x2B110, 1);
	if (SUCCEEDED(hr))
		IDirectSoundBuffer_SetHeadroom(m_pBuffer, 0);
	return hr;
}

// 0x0007EFCE
DWORD XCDPlayer::GetSector()
{
	WaitForSingleObject(m_hMutex, INFINITE);
	DWORD sector = m_reader.sector;
	ReleaseMutex(m_hMutex);
	return sector;
}

// 0x0007F174
int XCDPlayer::Read(void *buffer, int size)
{
	DWORD sector = GetSector();
	if (sector >= m_dwEnd)
		return 0;
	if ((DWORD)((size + CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE) + sector >= m_dwEnd)
		size = (m_dwEnd - sector) * CD_SECTOR_SIZE;
	int read = CddaReaderRead(&m_reader, buffer, size);
	if (read > 0)
		m_pLastData = (const BYTE *)buffer;
	return read;
}

// 0x0007F093
void XCDPlayer::Stop()
{
	CAudioPump::Stop();
	m_reader.bufferPos = 0;
	m_reader.sector = m_dwStart;
}

// 0x0007EFAF
float XCDPlayer::GetDuration()
{
	return (float)(m_dwEnd - m_dwStart) * (1.0f / CD_FRAMES_PER_SECOND);
}

// 0x0007F060
float XCDPlayer::GetPosition()
{
	float position = CAudioPump::GetPosition();
	float duration = GetDuration();
	return duration < position ? duration : position;
}
