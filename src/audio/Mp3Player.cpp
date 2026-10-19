// CMP3Player: MP3 files through CAudioDecoder. See AudioPlayer.h.
#include "AudioPlayer.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "Mp3Decoder.h"

// After the decoder runs dry, the song still counts as playing this long.
#define MP3_END_GRACE_MS 2500

static DWORD s_dwEndTime;   // 0x00278D08

// 0x001096D3
CMP3Player::CMP3Player()
{
	m_strPath.clear();
	m_bActive = FALSE;
	m_bReady = FALSE;
	m_nState = PLAYER_STOPPED;
	m_nVolume = 0;
	memset(&m_clock, 0, sizeof(m_clock));
	m_szArtist[0] = 0;
	m_szTitle[0] = 0;
	m_dwLength = 0;
	m_bStarted = FALSE;
	m_unknown14 = 0;
	m_pDecoder = NULL;
	m_mixBins.dwMixBinCount = 0;
	m_mixBins.lpMixBinVolumePairs = NULL;
	memset(m_mixBinPairs, 0, sizeof(m_mixBinPairs));
	m_pSettings = CAppSettings::GetInstance();
}

// 0x001092E5
CMP3Player::~CMP3Player()
{
	Stop();
	if (m_pDecoder != NULL)
	{
		delete m_pDecoder;
		m_pDecoder = NULL;
	}
}

// 0x00109197
HRESULT CMP3Player::Init(const char * /*path*/)
{
	QueryPerformanceFrequency(&m_clock.frequency);
	m_bReady = TRUE;
	return S_OK;
}

// 0x00109320
// The ID3v1 tag (the last 128 bytes: "TAG", title, artist, album; 30
// characters each): the artist line is "album - artist" (only when there's
// an album), the title the tag's or the file's name.
HRESULT CMP3Player::UpdateSongInfo(wchar_t *artist, wchar_t *title, DWORD *length)
{
	if (m_strPath.size() == 0)
		return E_FAIL;
	if (m_pDecoder != NULL)
	{
		memset(&m_format, 0, sizeof(m_format));
		m_format.bitRate = m_pDecoder->m_dwBitRate * 1000;
		m_format.channels = (m_pDecoder->m_bStereo != 0) + 1;
		m_format.duration = m_pDecoder->m_dwDuration;
		m_format.sampleRate = m_pDecoder->m_dwSampleRate;
		if (length != NULL)
			*length = m_format.duration * 1000;
	}
	char tagTitle[31];
	char tagArtist[31];
	char tagAlbum[31];
	memset(tagTitle, 0, sizeof(tagTitle));
	memset(tagArtist, 0, sizeof(tagArtist));
	memset(tagAlbum, 0, sizeof(tagAlbum));
	HANDLE file = CreateFileA(m_strPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                          FILE_ATTRIBUTE_NORMAL, NULL);
	if (file != INVALID_HANDLE_VALUE)
	{
		char tag[4];
		DWORD read;
		SetFilePointer(file, -128, NULL, FILE_END);
		ReadFile(file, tag, 3, &read, NULL);
		tag[3] = 0;
		if (strcmp(tag, "TAG") == 0)
		{
			ReadFile(file, tagTitle, 30, &read, NULL);
			tagTitle[30] = 0;
			ReadFile(file, tagArtist, 30, &read, NULL);
			tagArtist[30] = 0;
			ReadFile(file, tagAlbum, 30, &read, NULL);
			tagAlbum[30] = 0;
		}
		CloseHandle(file);
	}
	std::string text;
	if (artist != NULL && tagAlbum[0] != 0)
	{
		text = tagAlbum;
		if (tagArtist[0] != 0)
		{
			std::string album = text;
			StrTrimRight(StrTrimLeft(album));
			StrFormat(text, "%s - %s", album.c_str(), tagArtist);
		}
		StrTrimRight(text);
		MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), artist, 0x3F);
	}
	if (title != NULL)
	{
		if (tagTitle[0] == 0)
		{
			text = m_strPath;
			int slash = StrReverseFind(text, "\\", -1);
			if (slash != -1)
				text = StrMid(text, slash + 1, (int)text.size() - (slash + 1));
			int dot = StrReverseFind(text, ".", -1);
			if (dot != -1)
				text = StrLeft(text, dot);
			strncpy(tagTitle, text.c_str(), 30);
		}
		MultiByteToWideChar(CP_ACP, 0, tagTitle, (int)strlen(tagTitle) + 1, title, 0x20);
	}
	return S_OK;
}

// 0x001095F3
HRESULT CMP3Player::OpenDecoder()
{
	m_bActive = FALSE;
	if (m_pDecoder == NULL)
	{
		m_pDecoder = new CAudioDecoder;
		if (m_pDecoder == NULL)
			return E_FAIL;
	}
	memset(m_szArtist, 0, sizeof(m_szArtist));
	memset(m_szTitle, 0, sizeof(m_szTitle));
	if (!m_pDecoder->Open(m_strPath.c_str(), TRUE))
	{
		memset(m_szArtist, 0, sizeof(m_szArtist));
		memset(m_szTitle, 0, sizeof(m_szTitle));
		return E_FAIL;
	}
	if (m_mixBins.dwMixBinCount != 0)
		SetSpeakerConfig(&m_mixBins);
	m_pDecoder->SetVolume((LONG)GetVolume());
	m_bActive = TRUE;
	UpdateSongInfo(m_szArtist, m_szTitle, &m_dwLength);
	return S_OK;
}

// 0x0010976A
// (Quirk: the _strdup copy of the path is never freed.)
HRESULT CMP3Player::Open(const char *file)
{
	Stop();
	m_bActive = FALSE;
	m_dwLength = 0;
	m_strPath = strdup(file);
	return FAILED(OpenDecoder()) ? E_FAIL : S_OK;
}

// 0x001097AF
HRESULT CMP3Player::Play()
{
	if (m_nState == PLAYER_PLAYING)
		return S_FALSE;
	if (m_nState == PLAYER_STOPPED)
	{
		m_bActive = FALSE;
		if (m_pDecoder == NULL)
		{
			Stop();
			m_bStarted = FALSE;
			if (FAILED(OpenDecoder()))
				return E_FAIL;
		}
		m_pDecoder->SetVolume(m_pSettings->GetMusicVolume());
		m_clock.played = 0;
		m_pDecoder->Start();
		m_bActive = TRUE;
	}
	else if (m_nState == PLAYER_PAUSED)
	{
		m_bActive = FALSE;
		m_pDecoder->Resume();
		m_bActive = TRUE;
	}
	m_bStarted = TRUE;
	m_clock.Start();
	m_nState = PLAYER_PLAYING;
	m_bActive = TRUE;
	return S_OK;
}

// 0x00109078
// (The decoder is closed, not deleted.)
HRESULT CMP3Player::Stop()
{
	if (m_nState == PLAYER_STOPPED)
		return S_FALSE;
	if (m_pDecoder != NULL)
		m_pDecoder->Stop();
	m_bStarted = FALSE;
	m_clock.played = 0;
	m_nState = PLAYER_STOPPED;
	return S_OK;
}

// 0x001092C1
HRESULT CMP3Player::Pause()
{
	if (m_nState != PLAYER_PLAYING)
		return S_FALSE;
	m_pDecoder->Pause();
	m_nState = PLAYER_PAUSED;
	return S_OK;
}

// 0x0002346D
HRESULT CMP3Player::SetVolume(float volume)
{
	m_nVolume = (int)volume;
	if (m_pDecoder != NULL)
		m_pDecoder->SetVolume(m_nVolume);
	return S_OK;
}

// 0x00109204
HRESULT CMP3Player::GetSongInfo(wchar_t *artist, wchar_t *title, DWORD *length)
{
	if (artist != NULL)
		wcscpy(artist, m_szArtist);
	if (title != NULL)
		wcscpy(title, m_szTitle);
	if (length != NULL)
		*length = m_dwLength + 1;
	return S_OK;
}

// 0x001091B6
float CMP3Player::GetPosition()
{
	if (!IsReady())
		return 0.0f;
	return m_clock.GetSeconds();
}

// 0x0010924C
void CMP3Player::SetSpeakerConfig(const DSMIXBINS *mixBins)
{
	if (mixBins == NULL)
	{
		m_mixBins.dwMixBinCount = 0;
		m_mixBins.lpMixBinVolumePairs = NULL;
	}
	else if (mixBins != &m_mixBins)
	{
		m_mixBins.dwMixBinCount = mixBins->dwMixBinCount;
		for (UINT i = 0; i < m_mixBins.dwMixBinCount; i++)
			m_mixBinPairs[i] = mixBins->lpMixBinVolumePairs[i];
		m_mixBins.lpMixBinVolumePairs = m_mixBinPairs;
	}
	if (m_pDecoder != NULL)
		m_pDecoder->SetMixBins(mixBins);
}

// 0x001090B0
// When the decoder runs dry the song counts as playing for 2.5 s more,
// then it's over.
HRESULT CMP3Player::Process()
{
	if (m_nState == PLAYER_STOPPED)
		return m_bActive ? E_FAIL : S_FALSE;
	if (!m_bActive)
		return S_OK;
	if (m_bStarted)
	{
		if (m_pDecoder != NULL && !m_pDecoder->Decode())
		{
			m_bStarted = FALSE;
			s_dwEndTime = GetTickCount() + MP3_END_GRACE_MS;
		}
		if (m_bStarted)
		{
			if (GetState() == PLAYER_PLAYING)
				m_clock.Update();
			return S_OK;
		}
	}
	if (m_pDecoder == NULL || !m_pDecoder->IsPlaying())
		return GetTickCount() < s_dwEndTime ? S_OK : E_FAIL;
	return S_OK;
}
