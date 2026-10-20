// The MS dashboard's audio streaming classes. See AudioPump.h.
#include "DashUtil.h"
#include "AudioPump.h"

#include <cstring>

#include "../common/XboxKernel.h"
#include "AudioDevice.h"

// ---- CAudioBuf ----

// 0x00080012
CAudioBuf::CAudioBuf()
{
	m_pBuffer = NULL;
	m_bPaused = FALSE;
	m_bLoop = FALSE;
	m_dwPacketSize = 0;
	memset(&m_format, 0, sizeof(m_format));
}

// 0x000805DE
CAudioBuf::~CAudioBuf()
{
	if (m_pBuffer != NULL)
	{
		IDirectSoundBuffer_Stop(m_pBuffer);
		IDirectSoundBuffer_Release(m_pBuffer);
	}
}

// 0x0008085E
HRESULT CAudioBuf::Create(const WAVEFORMATEX *format, DWORD size, const void *data)
{
	m_dwPacketSize = size;
	m_dwBytesPerSecond = format->nAvgBytesPerSec;
	m_format = *format;
	HRESULT hr = CAudioDevice::GetInstance()->CreateSoundBuffer(format, size,
	                                                            DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME, &m_pBuffer);
	if (FAILED(hr))
		return hr;
	if (m_pBuffer == NULL)
		return E_INVALIDARG;
	if (data != NULL)
		CAudioDevice::FillBuffer(m_pBuffer, 0, size, data);
	return hr;
}

// 0x000805B8
void *CAudioBuf::Lock()
{
	void *data = NULL;
	DWORD size;
	IDirectSoundBuffer_Lock(m_pBuffer, 0, m_dwPacketSize, &data, &size, NULL, NULL, 0);
	return data;
}

// 0x000805A2
void CAudioBuf::Unlock(void *data)
{
	IDirectSoundBuffer_Unlock(m_pBuffer, data, m_dwPacketSize, NULL, 0);
}

// 0x000803EB
void CAudioBuf::SetVolume(LONG volume)
{
	if (m_pBuffer == NULL)
		return;
	if (volume < DSBVOLUME_MIN)
		volume = DSBVOLUME_MIN;
	else if (volume > DSBVOLUME_MAX)
		volume = DSBVOLUME_MAX;
	IDirectSoundBuffer_SetVolume(m_pBuffer, volume);
}

// 0x00080566
BOOL CAudioBuf::Play(BOOL loop)
{
	m_bLoop = (BYTE)loop;
	m_bPaused = FALSE;
	if (m_pBuffer == NULL)
		return FALSE;
	IDirectSoundBuffer_SetCurrentPosition(m_pBuffer, 0);
	IDirectSoundBuffer_Play(m_pBuffer, 0, 0, m_bLoop ? DSBPLAY_LOOPING : 0);
	return TRUE;
}

// 0x00080558
void CAudioBuf::Stop()
{
	if (m_pBuffer != NULL)
		IDirectSoundBuffer_Stop(m_pBuffer);
}

// 0x000803B3
float CAudioBuf::GetPosition()
{
	if (m_pBuffer == NULL)
		return 0.0f;
	DWORD play;
	IDirectSoundBuffer_GetCurrentPosition(m_pBuffer, &play, NULL);
	return (float)play / (float)(int)m_dwBytesPerSecond;
}

// 0x0008000A
float CAudioBuf::GetDuration()
{
	return (float)(int)m_dwPacketSize / (float)(int)m_dwBytesPerSecond;
}

// 0x00080503
BOOL CAudioBuf::IsPlaying()
{
	if (m_pBuffer == NULL)
		return FALSE;
	DWORD status;
	IDirectSoundBuffer_GetStatus(m_pBuffer, &status);
	return status & DSBSTATUS_PLAYING;
}

// 0x00080524
void CAudioBuf::Pause(BOOL pause)
{
	if (m_pBuffer == NULL)
		return;
	if (!(BYTE)pause)
	{
		m_bPaused = FALSE;
		IDirectSoundBuffer_Play(m_pBuffer, 0, 0, m_bLoop ? DSBPLAY_LOOPING : 0);
	}
	else
	{
		m_bPaused = TRUE;
		IDirectSoundBuffer_Stop(m_pBuffer);
	}
}

// 0x00080411
HRESULT CAudioBuf::SetMixBins(const DSMIXBINS *mixBins)
{
	if (m_pBuffer == NULL || m_format.nChannels > 2)
		return E_FAIL;
	if (mixBins != NULL)
	{
		DSVOICEPROPS props;
		IDirectSoundBuffer_GetVoiceProperties(m_pBuffer, &props);
		if (props.dwMixBinCount == mixBins->dwMixBinCount)
			return S_OK;
		if (mixBins->dwMixBinCount > 2)
		{
			// 5.1: an extensible format with all six speakers.
			WAVEFORMATEXTENSIBLE format;
			memset(&format, 0, sizeof(format));
			format.Format = m_format;
			format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			format.Format.cbSize = 0x16;
			format.Samples.wValidBitsPerSample = 0;
			format.dwChannelMask = 0x3F;
			format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			IDirectSoundBuffer_SetFormat(m_pBuffer, &format.Format);
		}
	}
	return IDirectSoundBuffer_SetMixBins(m_pBuffer, mixBins);
}

// ---- CAudioPump ----

// 0x0008082D
CAudioPump::CAudioPump()
{
	m_hThread = NULL;
	m_hStopEvent = NULL;
	m_hMutex = NULL;
	m_dwPacketSize = 0;
	m_nWraps = 0;
	m_dwLastPlayCursor = 0;
	m_nFilled = 0;
	m_nState = PUMP_STOPPED;
	m_phPacketEvents = NULL;
	m_nPackets = 0;
	m_pFilled = NULL;
}

// 0x00080789
CAudioPump::~CAudioPump()
{
	if (m_hStopEvent != NULL)
		SetEvent(m_hStopEvent);
	if (m_hThread != NULL)
	{
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
	}
	if (m_hStopEvent != NULL)
		CloseHandle(m_hStopEvent);
	if (m_hMutex != NULL)
		CloseHandle(m_hMutex);
	if (m_pBuffer != NULL)
	{
		IDirectSoundBuffer_Stop(m_pBuffer);
		IDirectSoundBuffer_Release(m_pBuffer);
		m_pBuffer = NULL;
	}
	if (m_phPacketEvents != NULL)
	{
		for (int i = 0; i < m_nPackets; i++)
		{
			if (m_phPacketEvents[i] != NULL)
				CloseHandle(m_phPacketEvents[i]);
		}
		delete[] m_phPacketEvents;
	}
	if (m_pFilled != NULL)
		delete[] m_pFilled;
}

// 0x00080622
// (Quirk: CreateEventA and CreateMutexA failures are tested against
// INVALID_HANDLE_VALUE, not NULL, so they go unnoticed; so does the
// thread's.)
HRESULT CAudioPump::Create(int packets, DWORD stackSize, const WAVEFORMATEX *format, DWORD packetSize,
                           int prefill)
{
	m_format = *format;
	m_dwBytesPerSecond = format->nAvgBytesPerSec;
	m_nPackets = packets;
	m_dwPacketSize = packetSize;
	m_nPrefill = prefill;
	m_phPacketEvents = new HANDLE[packets];
	if (m_phPacketEvents == NULL)
		return E_OUTOFMEMORY;
	for (int i = 0; i < m_nPackets; i++)
		m_phPacketEvents[i] = NULL;
	m_hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (m_hStopEvent == INVALID_HANDLE_VALUE)
		return 6;
	m_hMutex = CreateMutexA(NULL, FALSE, NULL);
	if (m_hMutex == INVALID_HANDLE_VALUE)
		return 6;
	for (int i = 0; i < m_nPackets; i++)
	{
		m_phPacketEvents[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (m_phPacketEvents[i] == INVALID_HANDLE_VALUE)
			return E_OUTOFMEMORY;
	}
	m_pFilled = new BYTE[m_nPackets];
	if (m_pFilled == NULL)
		return E_OUTOFMEMORY;
	DWORD threadId;
	m_hThread = CreateThread(NULL, stackSize, ThreadProc, this, 0, &threadId);
	if (m_hThread == INVALID_HANDLE_VALUE)
		return 6;   // (ERROR_INVALID_HANDLE as an HRESULT)

	HRESULT hr = CAudioDevice::GetInstance()->CreateSoundBuffer(
	    format, m_nPackets * m_dwPacketSize, DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPOSITIONNOTIFY,
	    &m_pBuffer);
	if (m_pBuffer == NULL)
		return hr;
	// Each packet's event fires a block before its end.
	DSBPOSITIONNOTIFY *notifies = new DSBPOSITIONNOTIFY[m_nPackets];
	if (notifies == NULL)
		return E_OUTOFMEMORY;
	for (int i = 0; i < m_nPackets; i++)
	{
		notifies[i].dwOffset = m_dwPacketSize * (i + 1) - format->nBlockAlign;
		notifies[i].hEventNotify = m_phPacketEvents[i];
	}
	CAudioDevice::GetInstance();
	hr = IDirectSoundBuffer_SetNotificationPositions(m_pBuffer, m_nPackets, notifies);
	delete[] notifies;
	return hr;
}

// 0x000803A7
DWORD __stdcall CAudioPump::ThreadProc(void *param)
{
	return ((CAudioPump *)param)->Run();
}

// 0x00080087
BOOL CAudioPump::FillPacket(int index)
{
	WaitForSingleObject(m_hMutex, INFINITE);
	ResetEvent(m_phPacketEvents[index]);
	int read = 0;
	void *data;
	DWORD size;
	IDirectSoundBuffer_Lock(m_pBuffer, m_dwPacketSize * index, m_dwPacketSize, &data, &size, NULL, NULL, 0);
	if (m_nState != PUMP_ENDING)
		read = Read(data, m_dwPacketSize);
	if (read < (int)m_dwPacketSize)
	{
		// The end: silence after the last of the data.
		m_nState = PUMP_ENDING;
		if (read > 0)
			memset((BYTE *)data + read, 0, m_dwPacketSize - read);
		else
			memset(data, 0, m_dwPacketSize);
	}
	IDirectSoundBuffer_Unlock(m_pBuffer, data, size, NULL, 0);
	if (read > 0)
	{
		m_pFilled[index] = TRUE;
		m_nFilled++;
		if (m_nState == PUMP_STARTING && m_nFilled >= m_nPackets)
			m_nState = PUMP_PLAYING;
	}
	ReleaseMutex(m_hMutex);
	return read > 0;
}

// 0x0008026C
// Waits for a packet event (or the stop event), then refills that packet.
// While starting it fills the packets in order; once ending, it stops when
// the last filled packet has played.
DWORD CAudioPump::Run()
{
	HANDLE mutexWait[2];
	mutexWait[0] = m_hStopEvent;
	mutexWait[1] = m_hMutex;
	HANDLE *events = new HANDLE[m_nPackets + 1];
	if (events == NULL)
		return (DWORD)-1;
	events[0] = m_hStopEvent;
	for (int i = 0; i < m_nPackets; i++)
		events[i + 1] = m_phPacketEvents[i];
	int packet = 0;
	BOOL locked = FALSE;
	for (;;)
	{
		if (locked)
		{
			ReleaseMutex(m_hMutex);
			locked = FALSE;
		}
		if (WaitForMultipleObjects(m_nPackets + 1, events, FALSE, INFINITE) == WAIT_OBJECT_0 ||
		    WaitForMultipleObjects(2, mutexWait, FALSE, INFINITE) == WAIT_OBJECT_0)
			break;
		locked = TRUE;
		if (m_nState == PUMP_STOPPED)
			continue;
		if (m_nState == PUMP_STARTING)
		{
			FillPacket(m_nFilled);
		}
		else if (m_nState == PUMP_ENDING && m_nFilled < 1)
		{
			Stop();
			OnEnd();
		}
		else
		{
			int i = 0;
			for (; i < m_nPackets; i++)
			{
				if (WaitForSingleObject(m_phPacketEvents[i], 0) == WAIT_OBJECT_0)
				{
					packet = i;
					break;
				}
			}
			if (i < m_nPackets)
			{
				if (m_pFilled[packet])
				{
					m_pFilled[packet] = FALSE;
					m_nFilled--;
				}
				FillPacket(packet);
			}
		}
	}
	Stop();
	if (locked)
		ReleaseMutex(m_hMutex);
	delete[] events;
	return 0;
}

// 0x000801F0
BOOL CAudioPump::Play(BOOL loop)
{
	m_bLoop = (BYTE)loop;
	if (m_pBuffer == NULL)
		return FALSE;
	WaitForSingleObject(m_hMutex, INFINITE);
	Stop();
	m_nState = PUMP_STARTING;
	for (int i = 0; i < m_nPackets; i++)
	{
		SetEvent(m_phPacketEvents[i]);
		m_pFilled[i] = FALSE;
	}
	for (int i = 0; i < m_nPrefill; i++)
		FillPacket(i);
	IDirectSoundBuffer_Play(m_pBuffer, 0, 0, DSBPLAY_LOOPING);
	ReleaseMutex(m_hMutex);
	return TRUE;
}

// 0x00080197
void CAudioPump::Stop()
{
	if (m_pBuffer == NULL)
		return;
	WaitForSingleObject(m_hMutex, INFINITE);
	IDirectSoundBuffer_Stop(m_pBuffer);
	IDirectSoundBuffer_SetCurrentPosition(m_pBuffer, 0);
	DirectSoundDoWork();
	for (int i = 0; i < m_nPackets; i++)
		ResetEvent(m_phPacketEvents[i]);
	m_nWraps = 0;
	m_nFilled = 0;
	m_nState = PUMP_STOPPED;
	ReleaseMutex(m_hMutex);
}

// 0x00080034
float CAudioPump::GetPosition()
{
	if (m_pBuffer == NULL)
		return 0.0f;
	DWORD play;
	IDirectSoundBuffer_GetCurrentPosition(m_pBuffer, &play, NULL);
	if (play < m_dwLastPlayCursor)
		m_nWraps++;
	m_dwLastPlayCursor = play;
	DWORD bytes = play + m_nPackets * m_nWraps * m_dwPacketSize;
	return (float)bytes / (float)(int)m_dwBytesPerSecond;
}

// 0x00080152
void CAudioPump::Pause(BOOL pause)
{
	if (m_pBuffer == NULL)
		return;
	WaitForSingleObject(m_hMutex, INFINITE);
	if (!(BYTE)pause)
	{
		m_bPaused = FALSE;
		IDirectSoundBuffer_Play(m_pBuffer, 0, 0, DSBPLAY_LOOPING);
	}
	else
	{
		m_bPaused = TRUE;
		IDirectSoundBuffer_Stop(m_pBuffer);
	}
	ReleaseMutex(m_hMutex);
}

// ---- CFilePump ----

// 0x00080ABC
CFilePump::CFilePump()
{
	m_hFile = INVALID_HANDLE_VALUE;
	m_pLastData = NULL;
	m_dwDataStart = 0;
	m_dwFilePacketSize = 0;
}

// 0x00080A9D
CFilePump::~CFilePump()
{
	if (m_hFile != INVALID_HANDLE_VALUE)
		CloseHandle(m_hFile);
}

// 0x00080A41
HRESULT CFilePump::Open(HANDLE file, DWORD dataSize, const WAVEFORMATEX *format)
{
	m_dwFilePacketSize = (0x2000 / format->nBlockAlign) * format->nBlockAlign;
	HRESULT hr = Create(4, 0x2000, format, m_dwFilePacketSize, 1);
	if (SUCCEEDED(hr))
	{
		m_hFile = file;
		m_fDuration = (float)(int)dataSize / (float)(int)m_dwBytesPerSecond;
		m_dwDataStart = SetFilePointer(file, 0, NULL, FILE_CURRENT);
	}
	return hr;
}

// 0x0007FF79
int CFilePump::Read(void *buffer, int size)
{
	DWORD read;
	if (!ReadFile(m_hFile, buffer, size, &read, NULL))
		return -1;
	if (m_bLoop && (int)read != size)
	{
		SetFilePointer(m_hFile, m_dwDataStart, NULL, FILE_BEGIN);
		DWORD more;
		if (!ReadFile(m_hFile, (BYTE *)buffer + read, size - read, &more, NULL))
			return -1;
		read += more;
	}
	m_pLastData = (const BYTE *)buffer;
	return read;
}

// 0x00080603
void CFilePump::Stop()
{
	if (m_hFile == INVALID_HANDLE_VALUE)
		return;
	CAudioPump::Stop();
	SetFilePointer(m_hFile, m_dwDataStart, NULL, FILE_BEGIN);
}

// ---- CWMAPump ----

// 0x000809DA
CWMAPump::~CWMAPump()
{
	if (m_pDecoder != NULL)
	{
		// Stop the refill thread before the decoder goes.
		SignalObjectAndWait(m_hStopEvent, m_hThread, INFINITE, FALSE);
		IXMediaObject_Release(m_pDecoder);
		m_pDecoder = NULL;
	}
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

// 0x0007FF18
// A short packet is the end: the decoder is told of the discontinuity and
// 0 is returned. Otherwise the result is the packet size, not the bytes.
int CWMAPump::Read(void *buffer, int size)
{
	DWORD completed;
	XMEDIAPACKET packet;
	memset(&packet, 0, sizeof(packet));
	packet.pvBuffer = buffer;
	packet.dwMaxSize = size;
	packet.pdwCompletedSize = &completed;
	if (FAILED(IXMediaObject_Process(m_pDecoder, NULL, &packet)))
		return -1;
	if (completed < (DWORD)size)
	{
		IXMediaObject_Discontinuity(m_pDecoder);
		return 0;
	}
	m_pLastData = (const BYTE *)buffer;
	return 0x10000;
}

// 0x000808B8
// (Quirks: the decoder is created before the packets exist, so it's told
// of 0 packets; with a handle the path passed is an uninitialized buffer.)
HRESULT CWMAPump::OpenFile(const char *path, HANDLE file, WAVEFORMATEX *format)
{
	char name[MAX_PATH];
	if (path == NULL)
		m_hFile = file;
	else
		DashStrCopyN(name, path, 0x104);
	HRESULT hr = WmaCreateDecoder(name, file, FALSE, 0x40000, m_nPackets, 0, format, XDK_CAST(XFileMediaObject **, &m_pDecoder));
	if (hr == S_OK)
	{
		hr = Create(4, 0, format, 0x10000, 1);
		if (SUCCEEDED(hr))
		{
			IDirectSoundBuffer_SetHeadroom(m_pBuffer, 0);
			return hr;
		}
		IXMediaObject_Release(m_pDecoder);
		m_pDecoder = NULL;
	}
	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
	return hr;
}

// 0x00080951
HRESULT CWMAPump::OpenSoundtrackSong(DWORD songId, WAVEFORMATEX *format)
{
	HANDLE file = XOpenSoundtrackSong(songId, FALSE);
	if (file == INVALID_HANDLE_VALUE)
		return 0x80070006;
	// The song's length, from its soundtrack.
	DWORD id;
	DWORD length;
	for (UINT i = 0; XGetSoundtrackSongInfo(songId >> 16, i, &id, &length, NULL, 0); i++)
	{
		if (id == songId)
		{
			m_fDuration = (float)length * 0.001f;
			break;
		}
	}
	return OpenFile(NULL, file, format);
}
