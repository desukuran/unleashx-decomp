// CWAVPlayer: WAV files through the dashboard's audio buffers. See
// AudioPlayer.h.
#include "AudioPlayer.h"

#include <cstring>
#include <cwchar>
#include <string>

#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "AudioPump.h"

// Up to this much data goes into one buffer; longer files are streamed.
#define WAV_STATIC_LIMIT 0x10000

// The RIFF header up to the "fmt " chunk's size. 0x14 bytes.
struct SWavHeader
{
	char riff[4];
	DWORD riffSize;
	char wave[4];
	char fmt[4];
	DWORD fmtSize;
};

struct SWavChunk
{
	DWORD id;
	DWORD size;
};

#define WAV_DATA_CHUNK 0x61746164   // "data"

// 0x0013062D
CWAVPlayer::CWAVPlayer()
{
	m_pBuffer = NULL;
	m_bReady = FALSE;
	m_bActive = FALSE;
	m_nState = PLAYER_STOPPED;
	m_nVolume = 0;
	memset(&m_clock, 0, sizeof(m_clock));
	m_szArtist[0] = 0;
	m_szTitle[0] = 0;
	m_dwLength = 0;
	m_bStarted = FALSE;
	m_unknown1C = 0;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x001301EB
CWAVPlayer::~CWAVPlayer()
{
	Stop();
	if (m_pBuffer != NULL)
	{
		delete m_pBuffer;
		m_pBuffer = NULL;
	}
}

// 0x00130125
HRESULT CWAVPlayer::Init(const char * /*path*/)
{
	QueryPerformanceFrequency(&m_clock.frequency);
	return S_OK;
}

// 0x00130218
// Quirks: a file without a "data" chunk makes the chunk walk spin forever
// (reads at the end succeed with nothing, so the last chunk is skipped
// again and again); the title loses the last letter before the
// extension; and the player is "ready" even when the open fails.
HRESULT CWAVPlayer::Open(const char *file)
{
	if (m_clock.frequency.QuadPart == 0)
		QueryPerformanceFrequency(&m_clock.frequency);
	Stop();
	m_szTitle[0] = 0;
	m_dwLength = 0;
	m_bReady = FALSE;
	m_bActive = FALSE;
	if (m_pBuffer != NULL)
	{
		delete m_pBuffer;
		m_pBuffer = NULL;
	}

	HANDLE handle = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
	                            NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		m_bReady = TRUE;
		return E_FAIL;
	}
	SWavHeader header;
	DWORD read;
	if (!ReadFile(handle, &header, sizeof(header), &read, NULL) || read != sizeof(header) ||
	    memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0 ||
	    memcmp(header.fmt, "fmt ", 4) != 0)
	{
		CloseHandle(handle);
		m_bReady = TRUE;
		return E_FAIL;
	}
	// The format (the first 0x14 bytes of the chunk).
	memset(&m_wfx, 0, 0x14);
	DWORD formatSize = header.fmtSize > 0x14 ? 0x14 : header.fmtSize;
	if (!ReadFile(handle, &m_wfx, formatSize, &read, NULL) || read != formatSize)
	{
		CloseHandle(handle);
		m_bReady = TRUE;
		return E_FAIL;
	}
	if (header.fmtSize != formatSize)
		SetFilePointer(handle, header.fmtSize - formatSize, NULL, FILE_CURRENT);

	// The "data" chunk.
	SWavChunk chunk;
	BOOL opened = FALSE;
	while (!opened)
	{
		if (!ReadFile(handle, &chunk, sizeof(chunk), &read, NULL))
			break;
		if (chunk.id == WAV_DATA_CHUNK)
		{
			if (chunk.size == 0)
				break;
			if (chunk.size <= WAV_STATIC_LIMIT)
			{
				// Short: all of it in one buffer.
				m_pBuffer = new CAudioBuf;
				BOOL failed = TRUE;
				if (SUCCEEDED(m_pBuffer->Create(&m_wfx, chunk.size, NULL)))
				{
					void *data = m_pBuffer->Lock();
					failed = !ReadFile(handle, data, chunk.size, &read, NULL) || read != chunk.size;
					m_pBuffer->Unlock(data);
				}
				CloseHandle(handle);
				if (!failed)
				{
					opened = TRUE;
					break;
				}
				delete m_pBuffer;
			}
			else
			{
				CFilePump *pump = new CFilePump;
				m_pBuffer = pump;
				if (pump != NULL && SUCCEEDED(pump->Open(handle, chunk.size, &m_wfx)))
				{
					opened = TRUE;
					break;
				}
				CloseHandle(handle);
				if (pump != NULL)
					delete pump;
			}
			m_pBuffer = NULL;
			m_bReady = TRUE;
			return E_FAIL;
		}
		if (SetFilePointer(handle, chunk.size, NULL, FILE_CURRENT) == 0)
			break;
	}
	if (!opened)
	{
		CloseHandle(handle);
		m_bReady = TRUE;
		return E_FAIL;
	}

	memset(&m_format, 0, sizeof(m_format));
	m_bReady = TRUE;
	if (m_pBuffer == NULL)
		return E_FAIL;
	// The title: the file's name.
	std::string name = file;
	int dot = StrReverseFind(name, ".", -1);
	if (dot != -1)
		name = StrLeft(name, dot - 1);
	int slash = StrReverseFind(name, "\\", -1);
	if (slash != -1)
		name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
	StrUtf8ToWide(name.c_str(), m_szTitle, 0x40);
	m_dwLength = (DWORD)(int)m_pBuffer->GetDuration();
	m_format.bitRate = m_wfx.nAvgBytesPerSec;
	m_format.channels = m_wfx.nChannels;
	m_format.duration = m_dwLength;
	m_format.sampleRate = m_wfx.nSamplesPerSec;
	SetSpeakerConfig(m_pSettings->m_audio.m_bEnable51 ? (const DSMIXBINS *)m_pSettings->m_speakerMixBins : NULL);
	m_pBuffer->SetVolume((LONG)GetVolume());
	m_bActive = TRUE;
	return S_OK;
}

// 0x00130062
HRESULT CWAVPlayer::Play()
{
	if (m_nState == PLAYER_PLAYING)
		return S_FALSE;
	if (m_nState == PLAYER_STOPPED)
	{
		m_bActive = FALSE;
		if (m_pBuffer != NULL)
		{
			Stop();
			m_bStarted = FALSE;
			if (!m_pBuffer->Play(FALSE))
				return E_FAIL;
		}
		m_clock.played = 0;
		m_bActive = TRUE;
	}
	else if (m_nState == PLAYER_PAUSED)
	{
		m_bActive = TRUE;
		m_pBuffer->Pause(FALSE);
	}
	m_bStarted = TRUE;
	m_clock.Start();
	m_nState = PLAYER_PLAYING;
	m_bActive = TRUE;
	return S_OK;
}

// 0x0012FF6C
HRESULT CWAVPlayer::Stop()
{
	if (m_nState == PLAYER_STOPPED)
		return S_FALSE;
	if (m_pBuffer != NULL)
		m_pBuffer->Stop();
	m_bStarted = FALSE;
	m_clock.played = 0;
	m_nState = PLAYER_STOPPED;
	return S_OK;
}

// 0x001300FF
HRESULT CWAVPlayer::Pause()
{
	if (m_nState != PLAYER_PLAYING)
		return S_FALSE;
	m_pBuffer->Pause(TRUE);
	m_nState = PLAYER_PAUSED;
	return S_OK;
}

// 0x001301CC
HRESULT CWAVPlayer::SetVolume(float volume)
{
	m_nVolume = (int)volume;
	if (m_pBuffer != NULL)
		m_pBuffer->SetVolume(m_nVolume);
	return S_OK;
}

// 0x00130184
HRESULT CWAVPlayer::GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length)
{
	if (artist != NULL)
		wcscpy(artist, m_szArtist);
	if (title != NULL)
		wcscpy(title, m_szTitle);
	if (length != NULL)
		*length = m_dwLength + 1;
	return S_OK;
}

// 0x00130136
float CWAVPlayer::GetPosition()
{
	if (!IsReady())
		return 0.0f;
	return m_clock.GetSeconds();
}

// 0x0012FFA8
HRESULT CWAVPlayer::Process()
{
	if (m_nState == PLAYER_STOPPED)
		return m_bActive ? E_FAIL : S_FALSE;
	if (!m_bActive)
		return S_OK;
	if (m_bStarted)
	{
		if (m_pBuffer != NULL && !m_pBuffer->IsPlaying())
			m_bStarted = FALSE;
		if (m_bStarted)
		{
			if (GetState() == PLAYER_PLAYING)
				m_clock.Update();
			return S_OK;
		}
	}
	if (m_pBuffer == NULL || !m_pBuffer->IsPlaying())
		return E_FAIL;
	return S_OK;
}
