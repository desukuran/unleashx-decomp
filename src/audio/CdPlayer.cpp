// CDDAPlayer: audio CD tracks through XCDPlayer. See AudioPlayer.h.
#include "AudioPlayer.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../settings/AppSettings.h"

// 0x00130AF7
CDDAPlayer::CDDAPlayer()
{
	m_pPump = NULL;
	m_bReady = FALSE;
	m_bActive = FALSE;
	m_nState = PLAYER_STOPPED;
	m_nVolume = 0;
	memset(&m_clock, 0, sizeof(m_clock));
	m_szArtist[0] = 0;
	m_szTitle[0] = 0;
	m_dwLength = 0;
	m_bStarted = FALSE;
	m_nTrack = 0;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x0013095E
CDDAPlayer::~CDDAPlayer()
{
	Stop();
	if (m_pPump != NULL)
	{
		delete m_pPump;
		m_pPump = NULL;
	}
}

// 0x001307D1
HRESULT CDDAPlayer::Init(const char * /*path*/)
{
	QueryPerformanceFrequency(&m_clock.frequency);
	return S_OK;
}

// 0x0013098B
HRESULT CDDAPlayer::Open(const char *file)
{
	if (m_clock.frequency.QuadPart == 0)
		QueryPerformanceFrequency(&m_clock.frequency);
	Stop();
	m_bReady = FALSE;
	m_bActive = FALSE;
	if (m_pPump != NULL)
	{
		delete m_pPump;
		m_pPump = NULL;
	}
	memset(&m_format, 0, sizeof(m_format));
	m_nTrack = atol(file);
	if (m_nTrack <= 0)
		return E_FAIL;
	m_pPump = new XCDPlayer;
	// (A pump that fails to open is kept until the next Open.)
	if (m_pPump == NULL || FAILED(m_pPump->Open(&m_wfx, m_nTrack - 1)))
		return E_FAIL;
	DWORD minutes;
	DWORD seconds;
	CdTocGetTrackLength(&g_cdToc, m_nTrack - 1, &minutes, &seconds, NULL);
	m_dwLength = minutes * 60 + seconds;
	swprintf(m_szTitle, 64, L"Track %u", m_nTrack);
	m_format.bitRate = m_wfx.nAvgBytesPerSec;
	m_format.channels = m_wfx.nChannels;
	m_format.duration = m_dwLength;
	m_format.sampleRate = m_wfx.nSamplesPerSec;
	SetSpeakerConfig(m_pSettings->m_audio.m_bEnable51 ? (const DSMIXBINS *)m_pSettings->m_speakerMixBins : NULL);
	m_pPump->SetVolume((LONG)GetVolume());
	m_bReady = TRUE;
	m_bActive = TRUE;
	return S_OK;
}

// 0x0013070E
HRESULT CDDAPlayer::Play()
{
	if (m_nState == PLAYER_PLAYING)
		return S_FALSE;
	if (m_nState == PLAYER_STOPPED)
	{
		m_bActive = FALSE;
		if (m_pPump != NULL)
		{
			Stop();
			m_bStarted = FALSE;
			if (!m_pPump->Play(FALSE))
				return E_FAIL;
		}
		m_clock.played = 0;
		m_bActive = TRUE;
	}
	else if (m_nState == PLAYER_PAUSED)
	{
		m_bActive = TRUE;
		m_pPump->Pause(FALSE);
	}
	m_bStarted = TRUE;
	m_clock.Start();
	m_nState = PLAYER_PLAYING;
	m_bActive = TRUE;
	return S_OK;
}

// 0x00130699
HRESULT CDDAPlayer::Stop()
{
	if (m_nState == PLAYER_STOPPED)
		return S_FALSE;
	if (m_pPump != NULL)
		m_pPump->Stop();
	m_bStarted = FALSE;
	m_clock.played = 0;
	m_nState = PLAYER_STOPPED;
	return S_OK;
}

// 0x001307AB
HRESULT CDDAPlayer::Pause()
{
	if (m_nState != PLAYER_PLAYING)
		return S_FALSE;
	m_pPump->Pause(TRUE);
	m_nState = PLAYER_PAUSED;
	return S_OK;
}

// 0x00130878
HRESULT CDDAPlayer::SetVolume(float volume)
{
	m_nVolume = (int)volume;
	if (m_pPump != NULL)
		m_pPump->SetVolume(m_nVolume);
	return S_OK;
}

// 0x00130830
HRESULT CDDAPlayer::GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length)
{
	if (artist != NULL)
		wcscpy(artist, m_szArtist);
	if (title != NULL)
		wcscpy(title, m_szTitle);
	if (length != NULL)
		*length = m_dwLength + 1;
	return S_OK;
}

// 0x001307E2
float CDDAPlayer::GetPosition()
{
	if (!IsReady())
		return 0.0f;
	return m_clock.GetSeconds();
}

// 0x001306D2
void CDDAPlayer::SetSpeakerConfig(const DSMIXBINS *mixBins)
{
	if (m_pPump != NULL)
		m_pPump->SetMixBins(mixBins);
}

// 0x00130897
// As CWAVPlayer::Process, and it fails once the CD is gone.
HRESULT CDDAPlayer::Process()
{
	if (m_nState == PLAYER_STOPPED)
		return m_bActive ? E_FAIL : S_FALSE;
	if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
		return E_FAIL;
	if (!m_bActive)
		return S_OK;
	if (m_bStarted)
	{
		if (m_pPump != NULL && !m_pPump->IsPlaying())
			m_bStarted = FALSE;
		if (m_bStarted)
		{
			if (GetState() == PLAYER_PLAYING)
				m_clock.Update();
			return S_OK;
		}
	}
	if (m_pPump == NULL || !m_pPump->IsPlaying())
		return E_FAIL;
	return S_OK;
}
