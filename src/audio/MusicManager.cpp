// CMusicManager: the soundtrack list, the song list of the current
// soundtrack, random/global play, the Info.xml cache of folder and song
// titles, and the player thread. See MusicManager.h.
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../input/XBInput.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../xml/XmlDocument.h"
#include "../xml/XmlUtil.h"
#include "MusicManager.h"

// Volume while muted (hundredths of a dB).
static const float MUTED_VOLUME = -6400.0f;

// .mp3 / .wma (and, for PlayFile, .wav) by the last four characters.
static int GetAudioType(const std::string &file, BOOL allowWav)
{
	std::string extension = StrRight(file, 4);
	if (StrCmpNoCase(extension, ".mp3") == 0)
		return AUDIO_MP3;
	if (StrCmpNoCase(extension, ".wma") == 0)
		return AUDIO_WMA;
	if (allowWav && StrCmpNoCase(extension, ".wav") == 0)
		return AUDIO_WAV;
	return AUDIO_NONE;
}

// 0x00106D35
CMusicManager::CMusicManager(BOOL useSoundtracks, BOOL useFolder, const char *folder) : m_md5(NULL, -1)
{
	m_bUseSoundtracks = useSoundtracks;
	m_bReady = FALSE;
	m_bLoadingSong = FALSE;
	m_bShuffled = FALSE;
	m_bUseMusicFolder = useFolder;
	if (useFolder && folder != NULL && DirectoryExists(folder))
	{
		m_bUseMusicFolder = TRUE;
		m_strMusicFolder = _strdup(folder);   // (leaked)
	}
	else
	{
		m_bUseMusicFolder = FALSE;
	}
	for (int i = 0; i < 4; i++)
		m_pPlayers[i] = NULL;
	m_nPlayers = 0;
	m_pPlayer = NULL;
	m_hWakeEvent = NULL;
	m_unknown18 = 0;
	m_bAdvance = FALSE;
	m_bRandom = FALSE;
	m_bGlobal = FALSE;
	m_nSoundtrack = SOUNDTRACK_FIRST_USER;
	m_nSong = 0;
	m_nSongNumber = 0;
	m_szSoundtrack[0] = L'\0';
	m_szSong[0] = L'\0';
	m_dwSongLength = 0;
	m_unknownBC = 0;
	m_pSoundtrack = NULL;
	m_fVolume = 0.0f;
	m_strUnknown98 = "|";
	m_bNewSoundtrack = TRUE;
	m_bSongEnded = TRUE;
	m_bLastSong = FALSE;
	m_bSongActive = FALSE;
	m_dwNextCheck = 0;
	m_nState = MUSIC_STOPPED;
	m_bUserPaused = FALSE;
	m_bPlayingCD = FALSE;
	m_bMuted = FALSE;
	m_nOrderPos = 0;
	m_pInfo = NULL;
	m_pFolderInfo = NULL;
	m_pAlbumArt = NULL;
	m_strAlbumArt.clear();
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00106283
CMusicManager::~CMusicManager()
{
	Sleep(5);
	if (m_soundtracks.size() != 0)
	{
		Resume();
		Sleep(5);
		if (m_hWakeEvent != NULL)
		{
			SetEvent(m_hWakeEvent);
			Sleep(5);
			CloseHandle(m_hWakeEvent);
			m_hWakeEvent = NULL;
		}
		Thread::Stop();
		m_songs.clear();
		m_soundtracks.clear();
		m_order.clear();
		m_shuffled.clear();
	}
	for (UINT i = 0; i < m_nPlayers; i++)
	{
		delete m_pPlayers[i];
		m_pPlayers[i] = NULL;
	}
	if (m_pInfo != NULL)
	{
		m_pInfo->Clear();
		delete m_pInfo;
		m_pInfo = NULL;
	}
	if (m_pAlbumArt != NULL)
		ReleaseTexture(m_pAlbumArt);
}

// 0x00105FB7
float CMusicManager::GetVolume()
{
	return m_bMuted ? MUTED_VOLUME : m_fVolume;
}

// 0x00105FD5
SSongFormat *CMusicManager::GetSongFormat()
{
	if (m_pPlayer != NULL && GetState() != MUSIC_STOPPED)
		return &m_pPlayer->m_format;
	return NULL;
}

// 0x00105FEC
UINT CMusicManager::GetUserSoundtrackCount()
{
	if (m_soundtracks.size() <= 2)
		return 0;
	return (UINT)m_soundtracks.size() - 2;
}

// 0x00106017
float CMusicManager::GetPlaybackPosition()
{
	if (m_pPlayer != NULL)
		return m_pPlayer->GetPosition();
	return 0.0f;
}

// 0x0010602C
HRESULT CMusicManager::Pause()
{
	if (m_nState != MUSIC_PLAYING)
		return 1;
	m_nState = MUSIC_PAUSED;
	Sleep(50);
	if (m_pPlayer != NULL)
		m_pPlayer->Pause();
	Suspend();
	return S_OK;
}

// 0x0010605F
void CMusicManager::StopPlayer()
{
	m_bAdvance = FALSE;
	Sleep(50);
	if (m_pPlayer != NULL)
		m_pPlayer->Stop();
}

// 0x0010607A
HRESULT CMusicManager::SetVolume(float volume)
{
	if (!m_bMuted)
		m_fVolume = volume;
	else
		volume = GetVolume();
	if (m_pPlayer != NULL)
		m_pPlayer->SetVolume(volume);
	return S_OK;
}

// 0x001060CF
CAudioPlayer *CMusicManager::FindPlayer(int type)
{
	for (UINT i = 0; i < m_nPlayers; i++)
	{
		if (m_pPlayers[i] != NULL && m_pPlayers[i]->GetType() == type)
		{
			m_bPlayingCD = type == AUDIO_CD;
			return m_pPlayers[i];
		}
	}
	return NULL;
}

// 0x00106113
void CMusicManager::SeedRandom()
{
	SYSTEMTIME time;
	GetLocalTime(&time);
	srand(GetTickCount() + (time.wHour * 60 + time.wMinute) * 60 + time.wDay * 24 + time.wMilliseconds +
	      time.wSecond);
}

// 0x0010615A
void CMusicManager::SetMute(BOOL mute)
{
	if (m_bMuted != mute)
	{
		m_bMuted = mute;
		SetVolume(GetVolume());
	}
}

// 0x00106173
// Quirk: TRUE when ready without a player.
BOOL CMusicManager::IsPlaying()
{
	if (!m_bReady)
		return FALSE;
	if (m_pPlayer == NULL)
		return m_bReady;
	return m_pPlayer->IsReady();
}

// 0x00106189
void CMusicManager::GetSoundtrackName(wchar_t *name)
{
	if (m_pSoundtrack != NULL)
		wcsncpy(name, m_pSoundtrack->name, 31);
}

// 0x0010619B
UINT CMusicManager::GetSongCount()
{
	return (UINT)m_songs.size();
}

// 0x001061A5
HRESULT CMusicManager::Stop()
{
	StopPlayer();
	if (GetState() == MUSIC_STOPPED)
		return 1;
	Resume();
	Sleep(5);
	if (m_hWakeEvent != NULL)
		SetEvent(m_hWakeEvent);
	Thread::Stop();
	Sleep(50);
	for (UINT i = 0; i < m_nPlayers; i++)
	{
		if (m_pPlayers[i] != NULL)
			m_pPlayers[i]->Stop();
	}
	m_nState = MUSIC_STOPPED;
	m_bUserPaused = FALSE;
	Sleep(5);
	return S_OK;
}

// 0x00106225
IDirect3DTexture8 *CMusicManager::GetAlbumArt()
{
	if (!m_bReady)
		return NULL;
	if (m_pAlbumArt == NULL && m_strAlbumArt.size() != 0)
	{
		if (!LoadTextureFromFile(m_strAlbumArt.c_str(), g_pd3dDevice, &m_pAlbumArt, 0x80, 0x80, NULL))
		{
			m_pAlbumArt = NULL;
			m_strAlbumArt.clear();
		}
	}
	return m_pAlbumArt;
}

// 0x001063DD
void CMusicManager::ShuffleSongs(std::vector<SSong> &songs)
{
	if (!IsRandom() || songs.size() == 0)
	{
		m_bShuffled = FALSE;
		return;
	}
	m_shuffled.clear();
	SeedRandom();
	unsigned drawn = 0;
	while (songs.size() != 0)
	{
		unsigned pick = (unsigned)rand() % (unsigned)songs.size();
		m_shuffled.push_back(songs[pick]);
		songs.erase(songs.begin() + pick);
		if (++drawn % 5 == 0)
			SeedRandom();
	}
	songs.clear();
	m_bShuffled = TRUE;
}

// 0x001064A1
// (Also used for the CD and Media Player entries, which so count as
// folders.)
BOOL CMusicManager::AddSoundtrack(const char *path, const wchar_t *name)
{
	SSoundtrack soundtrack;
	memset(&soundtrack, 0, sizeof(soundtrack));
	soundtrack.isFolder = TRUE;
	soundtrack.index = (UINT)m_soundtracks.size();
	soundtrack.songCount = 0;
	strcpy(soundtrack.path, path);
	if (name != NULL && name[0] != L'\0')
		wcsncpy(soundtrack.name, name, 31);
	else
		wcscpy(soundtrack.name, L"Custom Soundtrack");
	m_soundtracks.push_back(soundtrack);
	m_pSoundtrack = &m_soundtracks.back();
	return m_pSoundtrack != NULL;
}

// 0x0010656E
void CMusicManager::FindAlbumArt(const char *path)
{
	m_strAlbumArt = GetPathDirectory(path);
	m_strAlbumArt.append("folder.jpg");
	if (!FileExists(m_strAlbumArt.c_str()))
	{
		StrReplaceAll(m_strAlbumArt, ".jpg", ".png");
		if (!FileExists(m_strAlbumArt.c_str()))
			m_strAlbumArt.clear();
	}
}

// 0x001065FC
void CMusicManager::AddFolderSongs(const char *path, std::vector<SSong> *shuffle)
{
	CFileFinder finder;
	if (finder.FindFirst(path, "*") != NULL)
	{
		UINT index = 0;
		std::string dir = path;
		StrAddBackslash(dir);
		while (strlen(finder.GetName()) != 0)
		{
			std::string file = dir;
			file.append(finder.GetName());
			int type = GetAudioType(file, FALSE);
			if (type != AUDIO_NONE)
			{
				SSong song;
				memset(&song, 0, sizeof(song));
				song.type = type;
				// The name is the file name without its extension.
				std::string name = finder.GetName();
				int dot = StrReverseFind(name, ".");
				if (dot == -1)
					StrUtf8ToWide(name.c_str(), song.name, 0x20);
				else
					StrUtf8ToWide(StrLeft(name, dot).c_str(), song.name, 0x20);
				strcpy(song.path, file.c_str());
				song.index = index;
				song.length = SONG_LENGTH_UNKNOWN;
				if (IsRandom())
					shuffle->push_back(song);
				m_songs.push_back(song);
				index++;
			}
			finder.FindNext();
		}
	}
	finder.Close();

	// The subfolders.
	if (!m_bTerminate && finder.FindFirst(path, "*") != NULL)
	{
		while (!m_bTerminate && strlen(finder.GetName()) != 0)
		{
			std::string dir = path;
			StrAddBackslash(dir);
			if (finder.GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
			{
				dir.append(finder.GetName());
				dir.append("\\");
				AddFolderSongs(dir.c_str(), shuffle);
			}
			finder.FindNext();
		}
	}
	finder.Close();
}

// 0x0010693B
// A folder is a soundtrack if it has an MP3 or WMA file; its title is name
// (Info.xml can rename it), or the folder's name. Subfolders are scanned
// with their own names.
void CMusicManager::ScanMusicFolder(const char *path, const wchar_t *name)
{
	CFileFinder finder;
	if (finder.FindFirst(path, "*") != NULL)
	{
		while (!m_bTerminate && strlen(finder.GetName()) != 0)
		{
			std::string file = finder.GetName();
			if (file.size() <= 4 || GetAudioType(file, FALSE) == AUDIO_NONE)
			{
				finder.FindNext();
				continue;
			}
			wchar_t title[32];
			memset(title, 0, sizeof(title));
			if (name == NULL || wcslen(name) == 0)
			{
				// Quirk: the "folder name" starts at the last backslash, which
				// for a path ending in one is just that backslash.
				std::string folder = _strdup(path);   // (leaked)
				int slash = StrReverseFind(folder, "\\");
				folder = StrRight(folder, (int)folder.size() - slash);
				MultiByteToWideChar(0, 0, folder.c_str(), (int)folder.size(), title, 31);
			}
			else
			{
				wcscpy(title, name);
				if (m_pInfo != NULL)
				{
					m_md5.HashMemory(path, (int)strlen(path));
					m_pFolderInfo = m_pInfo->m_root.FindFirstChild(m_md5.m_szHex);
					if (m_pFolderInfo != NULL && m_pFolderInfo->GetAttribute("Title").size() != 0)
						StrUtf8ToWide(m_pFolderInfo->GetAttribute("Title").c_str(), title, 0x1F);
				}
			}
			AddSoundtrack(path, title);
			break;
		}
	}
	finder.Close();

	if (!m_bTerminate && finder.FindFirst(path, "*") != NULL)
	{
		while (!m_bTerminate && strlen(finder.GetName()) != 0)
		{
			std::string dir = path;
			StrAddBackslash(dir);
			if (finder.GetAttributes() & FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string folder = finder.GetName();
				dir.append(finder.GetName());
				dir.append("\\");
				wchar_t title[32];
				memset(title, 0, sizeof(title));
				swprintf(title, 32, L"%hs", StrLeft(folder, 31).c_str());
				ScanMusicFolder(dir.c_str(), title);
			}
			finder.FindNext();
		}
	}
	finder.Close();
}

// 0x00106EF1
BOOL CMusicManager::SetSoundtrackTitle(UINT index, const char *title)
{
	if (title == NULL || strlen(title) == 0)
		return FALSE;
	SSoundtrack *soundtrack = &m_soundtracks[index];
	if (m_pInfo == NULL || !soundtrack->isFolder)
		return FALSE;
	m_md5.HashMemory(soundtrack->path, (int)strlen(soundtrack->path));
	CXmlElement *folder = m_pInfo->m_root.FindFirstChild(m_md5.m_szHex);
	if (folder == NULL)
		return FALSE;
	folder->SetAttribute("Title", title);
	m_pInfo->Save(FALSE);
	StrUtf8ToWide(title, soundtrack->name, 0x1F);
	return TRUE;
}

// 0x00106FA9
HRESULT CMusicManager::GetCurrentInfo(wchar_t *soundtrack, wchar_t *song, DWORD *length)
{
	if (m_pPlayer == NULL)
		return E_FAIL;
	if (m_dwSongLength != 0)
	{
		if (soundtrack != NULL)
			wcscpy(soundtrack, m_szSoundtrack);
		if (song != NULL)
			wcscpy(song, m_szSong);
		if (length != NULL)
			*length = m_dwSongLength;
		return S_OK;
	}
	if (m_nSong == MUSIC_NO_SONG)
		return S_OK;

	// Ask the player; remember the length (in Info.xml too, with the
	// title from the song's tags unless that's just its file name).
	m_pPlayer->GetSongInfo(0, m_szSong, &m_dwSongLength);
	if (m_dwSongLength == 0)
		return S_OK;
	SSong *record;
	if (IsRandom() && m_shuffled.size() != 0)
		record = &m_shuffled[m_nSong];
	else
		record = &m_songs[m_nSong];
	if (record->length != 0 && record->length != SONG_LENGTH_UNKNOWN && record->length == m_dwSongLength)
		return S_OK;
	record->length = m_dwSongLength;
	if (m_dwSongLength <= 1000 || !m_pSoundtrack->isFolder || m_pFolderInfo == NULL)
		return S_OK;
	std::string key = record->path;
	int slash = StrReverseFind(key, "\\");
	if (slash != -1)
		key = StrRight(key, (int)key.size() - slash - 1);
	StrRemoveChar(key, ' ');
	CXmlElement *element = m_pFolderInfo->FindFirstChild(key.c_str());
	if (element != NULL)
	{
		StrFormat(key, "%d", m_dwSongLength / 1000);
		element->SetAttribute("Length", key.c_str());
		std::string path = record->path;
		key = element->GetAttribute("Title");
		if (key.size() == 0)
		{
			char title[33];
			memset(title, 0, sizeof(title));
			StrWideToUtf8(m_szSong, title, 0x20);
			if (path.find(title) == std::string::npos)
				element->SetAttribute("Title", title);
		}
		if (m_pInfo->m_strFileName.size() > 0)
			m_pInfo->Save(FALSE);
	}
	return S_OK;
}

// 0x00107256
// shuffled: index is a position in m_shuffled (when random).
HRESULT CMusicManager::PlaySong(UINT index, BOOL shuffled)
{
	if (m_pSoundtrack == NULL || m_songs.size() == 0)
		return E_FAIL;
	if (index == m_nSong && m_pPlayer != NULL)
		return S_OK;

	BOOL suspended = FALSE;
	m_bReady = FALSE;
	if (m_hThread != NULL)
	{
		Sleep(10);
		Suspend();
		suspended = TRUE;
	}
	m_dwNextCheck = GetTickCount() + 3000;
	if (m_pPlayer != NULL)
	{
		StopPlayer();
		m_pPlayer = NULL;
	}
	// From the last song on, playback stops at the end (unless there's
	// another soundtrack to move to); indexes past the end wrap.
	if (index >= m_songs.size() - 1)
	{
		m_bLastSong = m_soundtracks.size() > 1 || m_songs.size() == 1;
		index %= (UINT)m_songs.size();
	}
	else
	{
		m_bLastSong = FALSE;
	}
	memset(m_szSong, 0, sizeof(m_szSong));
	m_dwSongLength = 0;
	m_nSong = index;
	m_nSongNumber = index;
	SSong *song;
	if (shuffled && IsRandom() && m_shuffled.size() != 0)
	{
		song = &m_shuffled[index];
		m_nSongNumber = song->index;
	}
	else
	{
		song = &m_songs[index];
	}

	HRESULT hr = E_FAIL;
	m_pPlayer = FindPlayer(song->type);
	if (m_pPlayer == NULL)
	{
		m_dwNextCheck = GetTickCount() + 2000;
	}
	else
	{
		m_pPlayer->SetVolume(GetVolume());
		m_bLoadingSong = TRUE;
		if (!m_pSoundtrack->isFolder && song->type != AUDIO_CD)
			hr = m_pPlayer->OpenSoundtrackSong(m_pSoundtrack->soundtrackId, song->songId, song->index);
		else
			hr = m_pPlayer->Open(song->path);
		Sleep(50);
		memset(m_szSong, 0, sizeof(m_szSong));
		if (hr >= 0)
		{
			GetCurrentInfo(NULL, NULL, NULL);
			m_bNewSoundtrack = FALSE;
			m_dwNextCheck = GetTickCount() + 2000;
			if (m_nSoundtrack == SOUNDTRACK_MEDIA_PLAYER)
			{
				// The soundtrack is named after the song's folder.
				FindAlbumArt(song->path);
				memset(m_szSoundtrack, 0, sizeof(m_szSoundtrack));
				std::string folder = GetPathDirectory(song->path);
				int slash = StrReverseFind(folder, "\\");
				folder = StrRight(folder, (int)folder.size() - slash);
				StrReplaceAll(folder, "\\", "");
				StrUtf8ToWide(folder.c_str(), m_szSoundtrack, 0x40);
			}
		}
		m_bLoadingSong = FALSE;
	}
	if (suspended)
	{
		Resume();
		Sleep(100);
	}
	m_bReady = TRUE;
	return hr;
}

// 0x0010756E
HRESULT CMusicManager::NextSong()
{
	if (!m_bReady)
		return 1;
	StopPlayer();
	m_dwNextCheck = GetTickCount() + 3000;
	UINT index = 0;
	if (!m_bNewSoundtrack)
	{
		if (m_nSong == MUSIC_NO_SONG)
			m_nSong = 0;
		index = m_nSong + 1;
	}
	PlaySong(index, TRUE);
	if (m_pPlayer == NULL)
		return E_FAIL;
	m_dwNextCheck = GetTickCount() + 3000;
	return S_OK;
}

// 0x001075DD
HRESULT CMusicManager::LoadSoundtrack(UINT index, BOOL reshuffle)
{
	if (!reshuffle && index == m_nSoundtrack)
	{
		m_bReady = TRUE;
		return S_OK;
	}
	m_bReady = FALSE;
	if (m_pPlayer != NULL)
	{
		m_pPlayer->Stop();
		m_pPlayer = NULL;
	}
	m_nSong = 0;
	m_nSongNumber = 0;
	m_bLastSong = FALSE;
	if (m_soundtracks.size() == 0)
		return E_FAIL;
	m_songs.clear();
	m_shuffled.clear();
	m_strUnknown98 = "|";
	if (m_pAlbumArt != NULL)
		ReleaseTexture(m_pAlbumArt);
	m_strAlbumArt.clear();

	// Global random play: a new random order of the user soundtracks, and
	// its first one instead of index.
	if (m_bRandom)
	{
		SetRandom(TRUE);
		SeedRandom();
		if ((reshuffle || m_order.size() == 0) && m_soundtracks.size() > 3)
		{
			std::vector<SSoundtrackOrder> all;
			for (UINT i = SOUNDTRACK_FIRST_USER; i < m_soundtracks.size(); i++)
			{
				SSoundtrackOrder entry = {i, 0};
				all.push_back(entry);
			}
			m_order.clear();
			while (all.size() != 0)
			{
				unsigned pick = (unsigned)rand() % (unsigned)all.size();
				m_order.push_back(all[pick]);
				all.erase(all.begin() + pick);
			}
			if (m_order.size() != 0)
				index = m_order[0].soundtrack;
		}
	}

	m_bNewSoundtrack = TRUE;
	m_nSoundtrack = index % (UINT)m_soundtracks.size();
	m_pSoundtrack = &m_soundtracks[m_nSoundtrack];
	memset(m_szSoundtrack, 0, sizeof(m_szSoundtrack));
	m_pFolderInfo = NULL;
	std::vector<SSong> shuffle;

	if (m_nSoundtrack == SOUNDTRACK_CD)
	{
		// The audio CD: one song per track.
		if (g_cdToc.hDevice != INVALID_HANDLE_VALUE)
			CdTocClose(&g_cdToc);
		CdTocOpen(&g_cdToc);
		if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
			return E_FAIL;
		m_nCdTracks = g_cdToc.nTracks;
		if (m_nCdTracks > 0)
		{
			if (m_pPlayers[2] == NULL)
			{
				m_pPlayers[2] = new CDDAPlayer;
				if (m_pPlayers[2] == NULL)
					return E_FAIL;
			}
			std::string number;
			std::string name;
			// Quirk: the track number (from 1) goes to a 0-based lookup, so
			// each track gets the next one's length, and the last keeps the
			// previous values.
			DWORD minutes = 0;
			DWORD seconds = 0;
			for (int track = 1; track < m_nCdTracks + 1; track++)
			{
				StrFormat(number, "%u", track);
				StrFormat(name, "Track %u", track);
				CdTocGetTrackLength(&g_cdToc, track, &minutes, &seconds, NULL);
				SSong song;
				memset(&song, 0, sizeof(song));
				StrUtf8ToWide(name.c_str(), song.name, 0x1F);
				strcpy(song.path, number.c_str());
				song.length = (minutes * 60 + seconds) * 1000;
				song.index = track - 1;
				song.type = AUDIO_CD;
				if (IsRandom())
					shuffle.push_back(song);
				m_songs.push_back(song);
			}
			ShuffleSongs(shuffle);
		}
	}
	else if (m_nSoundtrack != SOUNDTRACK_MEDIA_PLAYER)
	{
		if (m_pSoundtrack->isFolder)
		{
			// A music folder: its MP3/WMA files, with titles and lengths
			// from Info.xml (new files get an entry).
			CFileFinder finder;
			BOOL infoChanged = FALSE;
			if (finder.FindFirst(m_pSoundtrack->path, "*") != NULL)
			{
				if (m_pInfo != NULL)
				{
					m_md5.HashMemory(m_pSoundtrack->path, (int)strlen(m_pSoundtrack->path));
					m_pFolderInfo = m_pInfo->m_root.FindFirstChild(m_md5.m_szHex);
					if (m_pFolderInfo == NULL)
					{
						char title[33];
						memset(title, 0, sizeof(title));
						StrWideToUtf8(m_pSoundtrack->name, title, 0x20);
						m_pFolderInfo = AddChildElement(&m_pInfo->m_root, m_md5.m_szHex, NULL, NULL);
						m_pFolderInfo->SetAttribute("Title", title);
					}
					else if (m_pFolderInfo->GetAttribute("Title").size() != 0)
					{
						StrUtf8ToWide(m_pFolderInfo->GetAttribute("Title").c_str(), m_pSoundtrack->name, 0x1F);
					}
				}
				UINT count = 0;
				std::string dir = m_pSoundtrack->path;
				StrAddBackslash(dir);
				while (strlen(finder.GetName()) != 0)
				{
					std::string file = dir;
					file.append(finder.GetName());
					int type = GetAudioType(file, FALSE);
					if (type != AUDIO_NONE)
					{
						SSong song;
						memset(&song, 0, sizeof(song));
						song.type = type;
						std::string key = finder.GetName();
						int dot = StrReverseFind(key, ".");
						if (dot == -1)
							StrUtf8ToWide(key.c_str(), song.name, 0x20);
						else
							StrUtf8ToWide(StrLeft(key, dot).c_str(), song.name, 0x20);
						strcpy(song.path, file.c_str());
						song.length = SONG_LENGTH_UNKNOWN;
						song.index = count;
						if (m_pFolderInfo != NULL)
						{
							// Keyed by the file name without spaces.
							StrRemoveChar(key, ' ');
							CXmlElement *element = m_pFolderInfo->FindFirstChild(key.c_str());
							if (element == NULL)
							{
								AddChildElement(m_pFolderInfo, key.c_str(), NULL, NULL);
								infoChanged = TRUE;
							}
							else
							{
								key = element->GetAttribute("Title");
								if ((int)key.size() > 0)
									StrUtf8ToWide(key.c_str(), song.name, 0x20);
								key = element->GetAttribute("Length");
								unsigned seconds = (unsigned)atol(key.c_str());
								if (seconds > 1)
									song.length = seconds * 1000;
							}
						}
						if (IsRandom())
							shuffle.push_back(song);
						m_songs.push_back(song);
						count++;
					}
					finder.FindNext();
				}
				ShuffleSongs(shuffle);
				if (m_songs.size() != 0)
					m_pSoundtrack->songCount = (UINT)m_songs.size();
				if (m_pInfo != NULL && infoChanged)
				{
					std::string info = m_strMusicFolder;
					info.append("\\Info.xml");
					m_pInfo->SaveAs(info.c_str());
				}
				m_strAlbumArt = dir;
				m_strAlbumArt.append("x.jpg");
				FindAlbumArt(m_strAlbumArt.c_str());
			}
			finder.Close();
		}
		else
		{
			// An Xbox soundtrack: its songs from the XDK.
			for (UINT i = 0; i < m_pSoundtrack->songCount; i++)
			{
				DWORD songId;
				DWORD length;
				wchar_t name[100];
				if (!XGetSoundtrackSongInfo(m_pSoundtrack->soundtrackId, i, &songId, &length, name, 100))
					continue;
				SSong song;
				memset(&song, 0, sizeof(song));
				song.length = length;
				song.songId = songId;
				song.index = i;
				song.type = AUDIO_WMA;
				wcsncpy(song.name, name, 31);
				if (IsRandom())
					shuffle.push_back(song);
				m_songs.push_back(song);
			}
			ShuffleSongs(shuffle);
		}
	}

	if (m_songs.size() == 0)
		return E_FAIL;
	// Quirk: index as given, not wrapped (nor the random pick's position).
	m_nSoundtrack = index;
	m_bNewSoundtrack = TRUE;
	if (index == SOUNDTRACK_CD)
	{
		wcscpy(m_szSoundtrack, L"Audio CD");
	}
	else if (index == SOUNDTRACK_MEDIA_PLAYER)
	{
		wcscpy(m_szSoundtrack, L"Media Player");
	}
	else
	{
		m_nSong = 1;   // (so PlaySong(0) isn't skipped as already playing)
		GetSoundtrackName(m_szSoundtrack);
	}
	PlaySong(0, TRUE);
	m_bReady = TRUE;
	return S_OK;
}

// 0x00107FDA
HRESULT CMusicManager::SelectSoundtrack(DWORD soundtrackId)
{
	if (soundtrackId == 0)
		return E_FAIL;
	int found = -1;
	for (UINT i = 0; i < m_soundtracks.size(); i++)
	{
		SSoundtrack *soundtrack = &m_soundtracks[i];
		if (soundtrack->isFolder || soundtrack->soundtrackId != soundtrackId)
			continue;
		found = (int)i;
		if (i > 2 && IsRandom())
		{
			for (UINT j = 0; j < m_order.size(); j++)
			{
				if (m_order[j].soundtrack == i)
				{
					// Quirk: the soundtrack index, not the position j.
					m_nOrderPos = i;
					break;
				}
			}
		}
		break;
	}
	if (found == -1)
	{
		// Not listed (new since startup): look it up.
		XSOUNDTRACK_DATA data;
		HANDLE find = XFindFirstSoundtrack(&data);
		if (find != INVALID_HANDLE_VALUE)
		{
			for (;;)
			{
				if (data.uSongCount > 0 && data.uSoundtrackId == soundtrackId)
				{
					SSoundtrack soundtrack;
					memset(&soundtrack, 0, sizeof(soundtrack));
					soundtrack.isFolder = FALSE;
					soundtrack.soundtrackId = data.uSoundtrackId;
					soundtrack.songCount = data.uSongCount;
					soundtrack.index = (UINT)m_soundtracks.size();
					wcsncpy(soundtrack.name, data.szName, 31);
					found = (int)soundtrack.index;
					m_soundtracks.push_back(soundtrack);
					break;
				}
				if (m_bTerminate || !XFindNextSoundtrack(find, &data))
					break;
			}
			XFindClose(find);
		}
	}
	if (found == -1)
		return E_FAIL;
	return LoadSoundtrack((UINT)found, FALSE);
}

// 0x001081AE
HRESULT CMusicManager::NextSoundtrack()
{
	if (!m_bReady)
		return 1;
	if (m_soundtracks.size() < 4)
	{
		if (m_soundtracks.size() == 3)
			return LoadSoundtrack(SOUNDTRACK_FIRST_USER, FALSE);
		return E_FAIL;
	}
	StopPlayer();
	m_bReady = FALSE;
	Sleep(50);
	m_pPlayer = NULL;
	UINT index;
	if (IsRandom() && m_soundtracks.size() >= 5)
	{
		// The next in the random order; a new order after the last.
		m_nOrderPos++;
		if (m_nOrderPos > m_order.size() - 1)
			m_nOrderPos = 0;
		index = m_order.size() != 0 ? m_order[m_nOrderPos].soundtrack : m_nOrderPos;
		if (m_nOrderPos == 0)
			return LoadSoundtrack(index, TRUE);
	}
	else
	{
		index = (m_nSoundtrack + 1) % (UINT)m_soundtracks.size();
	}
	if (index < SOUNDTRACK_FIRST_USER)
		index = SOUNDTRACK_FIRST_USER;
	m_unknown18C = 0;
	if (LoadSoundtrack(index, FALSE) >= 0)
		return S_OK;
	return E_FAIL;
}

// 0x00108297
// Starts the player, then waits (40 ms at a time) while it plays; feeds it
// through Process and notes the end of the song for Update.
unsigned CMusicManager::Run()
{
	if (m_pPlayer == NULL)
		return (unsigned)-1;
	if (m_hWakeEvent == NULL)
		m_hWakeEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	Sleep(100);
	m_pPlayer->Play();
	m_pPlayer->Process();
	BOOL stopped = FALSE;
	Sleep(100);
	GetCurrentInfo(NULL, NULL, NULL);
	while (!m_bTerminate && !stopped && WaitForSingleObject(m_hWakeEvent, 40) != WAIT_OBJECT_0)
	{
		switch (GetState())
		{
		case MUSIC_STOPPED:
			stopped = TRUE;
			break;
		case MUSIC_PAUSED:
			Sleep(250);
			break;
		case MUSIC_PLAYING:
			if (!m_bReady || !m_bSongActive || !m_bAdvance || m_bLoadingSong)
			{
				Sleep(200);
			}
			else if (m_pPlayer != NULL && m_pPlayer->Process() < 0)
			{
				// The song is over: Update moves on after 2 s.
				m_dwNextCheck = GetTickCount() + 2000;
				m_bSongActive = FALSE;
				m_bSongEnded = TRUE;
				Sleep(2000);
			}
			break;
		}
	}
	if (m_pPlayer != NULL)
		m_pPlayer->Stop();
	if (m_hWakeEvent != NULL)
	{
		CloseHandle(m_hWakeEvent);
		m_hWakeEvent = NULL;
	}
	return 0;
}

// 0x001083B0
HRESULT CMusicManager::Play()
{
	if (m_bUserPaused || m_songs.size() == 0)
		return 1;
	if (m_nSong == MUSIC_NO_SONG)
		PlaySongAt(0);
	if (GetState() == MUSIC_PLAYING && m_pPlayer != NULL)
	{
		m_pPlayer->Play();
		m_bSongActive = TRUE;
		Sleep(50);
		m_bAdvance = TRUE;
		m_dwNextCheck = GetTickCount() + 2500;
		Resume();
		return S_OK;
	}
	HRESULT hr = E_FAIL;
	if (GetState() != MUSIC_STOPPED && m_pPlayer != NULL)
	{
		// Paused: continue.
		if (m_nState == MUSIC_PAUSED && m_pPlayer->Play() >= 0)
		{
			m_nState = MUSIC_PLAYING;
			hr = S_OK;
			m_bReady = TRUE;
			m_bSongActive = TRUE;
			Resume();
		}
		return hr;
	}
	// Stopped: (re)start the thread.
	if (m_hWakeEvent != NULL)
		SetEvent(m_hWakeEvent);
	Thread::Stop();
	if (m_pPlayer == NULL)
	{
		NextSong();
		if (m_pPlayer == NULL)
			return E_FAIL;
	}
	Sleep(50);
	if (m_hThread == NULL)
		Start(0x10000, true);
	Resume();
	m_nState = MUSIC_PLAYING;
	m_bSongActive = TRUE;
	m_bReady = TRUE;
	m_bAdvance = TRUE;
	return S_OK;
}

// 0x001084E7
HRESULT CMusicManager::PlayAudioCD()
{
	if (LoadSoundtrack(SOUNDTRACK_CD, FALSE) >= 0 && Play() >= 0)
	{
		m_bSongActive = TRUE;
		m_dwNextCheck = GetTickCount() + 3000;
		return S_OK;
	}
	return E_FAIL;
}

// 0x00108523
// Quirk: a folder only plays with random play on (the songs are only
// collected for the shuffle then).
BOOL CMusicManager::PlayFile(const char *path, BOOL folder)
{
	if (path == NULL || strlen(path) == 0)
		return FALSE;
	while (!m_bReady)
		Sleep(10);
	m_bReady = FALSE;
	if (m_pPlayer != NULL)
	{
		m_pPlayer->Stop();
		m_pPlayer = NULL;
	}
	m_nSong = 0;
	m_bLastSong = FALSE;
	m_songs.clear();
	m_shuffled.clear();
	std::string file = path;
	if (folder)
	{
		std::vector<SSong> shuffle;
		AddFolderSongs(path, &shuffle);
		if (m_songs.size() == 0 || shuffle.size() == 0)
		{
			m_bReady = TRUE;
			return FALSE;
		}
		ShuffleSongs(shuffle);
	}
	else
	{
		int type = GetAudioType(file, TRUE);
		if (type == AUDIO_NONE)
		{
			m_bReady = TRUE;
			return FALSE;
		}
		std::string name = GetPathFileName(path, TRUE);
		SSong song;
		memset(&song, 0, sizeof(song));
		StrUtf8ToWide(name.c_str(), song.name, 0x1F);
		strcpy(song.path, file.c_str());
		song.index = 0;
		song.length = SONG_LENGTH_UNKNOWN;
		song.type = type;
		m_songs.push_back(song);
	}
	m_pSoundtrack = &m_soundtracks[SOUNDTRACK_MEDIA_PLAYER];
	m_nSoundtrack = SOUNDTRACK_MEDIA_PLAYER;
	m_bNewSoundtrack = TRUE;
	memset(m_szSoundtrack, 0, sizeof(m_szSoundtrack));
	if (PlaySongAt(0) >= 0)
	{
		if (Play() >= 0)
		{
			m_bSongEnded = FALSE;
			return TRUE;
		}
		m_bSongEnded = TRUE;
	}
	m_bReady = TRUE;
	return FALSE;
}

// 0x001087DF
void CMusicManager::SetSpeakerConfig(const void *config)
{
	BOOL playing = GetState() == MUSIC_PLAYING;
	try
	{
		for (UINT i = 0; i < m_nPlayers; i++)
		{
			if (m_pPlayers[i] != NULL)
				m_pPlayers[i]->SetSpeakerConfig((const DSMIXBINS *)config);
		}
	}
	catch (...)
	{
	}
	if (playing)
		Play();
}

// 0x00108855
HRESULT CMusicManager::PrevSong()
{
	if (!m_bReady)
		return 1;
	StopPlayer();
	m_dwNextCheck = GetTickCount() + 3000;
	if (m_nSong == MUSIC_NO_SONG)
		m_nSong = 0;
	UINT index = m_nSong;
	if (index != 0)
		index--;
	else if (m_songs.size() != 0)
		index = (UINT)m_songs.size() - 1;   // wraps to the last
	if (PlaySongAt(index) < 0 || m_pPlayer == NULL)
		return E_FAIL;
	m_dwNextCheck = GetTickCount() + 3000;
	return S_OK;
}

// 0x001088DC
HRESULT CMusicManager::PrevSoundtrack()
{
	if (!m_bReady)
		return 1;
	if (m_soundtracks.size() <= 3)
		return 1;
	StopPlayer();
	m_bReady = FALSE;
	Sleep(50);
	if (m_pPlayer != NULL)
	{
		m_pPlayer->Stop();
		m_pPlayer = NULL;
	}
	UINT count = (UINT)m_soundtracks.size();
	UINT index = (count + m_nSoundtrack - 1) % count;
	if (index <= 2)
		index = count - 1;
	if (LoadSoundtrack(index, FALSE) < 0)
		return E_FAIL;
	return S_OK;
}

// 0x0010896C
// Once the thread has seen the song end (and its 2 s have passed): the
// next song, or with global random play a coin flip for the next
// soundtrack. A removed CD moves on to the next soundtrack. At the end of
// the CD it stops.
HRESULT CMusicManager::Update()
{
	if (GetState() == MUSIC_STOPPED)
		return E_FAIL;
	if (!m_bAdvance)
		return S_OK;
	if (m_pPlayer == NULL)
		return E_FAIL;
	if (m_bSongActive || m_bLoadingSong || !m_bReady)
		return S_OK;
	if (m_dwNextCheck >= GetTickCount())
		return E_FAIL;
	m_bSongEnded = TRUE;
	BOOL nextSoundtrack = FALSE;
	if (m_bPlayingCD && g_cdToc.hDevice == INVALID_HANDLE_VALUE)
	{
		nextSoundtrack = TRUE;
	}
	else if (m_bLastSong || (m_bGlobal && m_soundtracks.size() > 3 && rand() % 2 != 0))
	{
		if (!m_bPlayingCD)
		{
			nextSoundtrack = TRUE;
		}
		else if (m_bLastSong)
		{
			Stop();
			return S_OK;
		}
	}
	if (nextSoundtrack)
		NextSoundtrack();
	if (NextSong() >= 0)
	{
		Play();
		m_dwNextCheck = GetTickCount() + 8000;
	}
	else
	{
		m_dwNextCheck = GetTickCount() + 3000;
		if (!m_bUseSoundtracks && !m_bUseMusicFolder)
			Stop();
	}
	return E_FAIL;
}

// 0x00108A75
HRESULT CMusicManager::LoadSoundtracks()
{
	if (m_soundtracks.size() > 2)
		return S_OK;
	Sleep(100);
	AddSoundtrack("cdrom:", L"Audio CD");
	AddSoundtrack("", L"Media Player");
	if (m_bUseSoundtracks)
	{
		try
		{
			XSOUNDTRACK_DATA data;
			HANDLE find = XFindFirstSoundtrack(&data);
			if (find != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (data.uSongCount > 0)
					{
						SSoundtrack soundtrack;
						memset(&soundtrack, 0, sizeof(soundtrack));
						soundtrack.isFolder = FALSE;
						soundtrack.soundtrackId = data.uSoundtrackId;
						soundtrack.songCount = data.uSongCount;
						soundtrack.index = (UINT)m_soundtracks.size();
						wcsncpy(soundtrack.name, data.szName, 31);
						m_soundtracks.push_back(soundtrack);
					}
				} while (!m_bTerminate && XFindNextSoundtrack(find, &data));
				XFindClose(find);
				if (m_soundtracks.size() != 0)
					m_pSoundtrack = &m_soundtracks.back();
			}
		}
		catch (...)
		{
		}
	}
	if (m_bUseMusicFolder)
	{
		if (m_pInfo == NULL)
		{
			m_pInfo = new CXmlDocument;
			if (m_pInfo != NULL)
			{
				std::string info = m_strMusicFolder;
				info.append("\\Info.xml");
				if (!m_pInfo->Load(info.c_str()))
					m_pInfo->m_root.m_strName = "Music";
			}
		}
		ScanMusicFolder(m_strMusicFolder.c_str(), L"Custom Soundtrack");
	}
	Sleep(200);
	return m_soundtracks.size() != 0 ? S_OK : E_FAIL;
}

// 0x00108CF2
HRESULT CMusicManager::Init(const char *dataPath)
{
	if (dataPath == NULL || strlen(dataPath) == 0 || LoadSoundtracks() < 0 || m_soundtracks.size() == 0)
		return E_FAIL;
	if (m_pPlayers[0] != NULL)
		return S_OK;
	Sleep(50);
	m_pPlayers[0] = new CWMAPlayer;
	m_pPlayers[1] = new CMP3Player;
	m_pPlayers[3] = new CWAVPlayer;
	if (m_pPlayers[0] == NULL)
		return E_FAIL;
	// Quirk: the count assumes the slots fill from the front, so a missing
	// MP3 player hides the WAV one.
	m_nPlayers = 1;
	if (m_pPlayers[1] != NULL)
		m_nPlayers = 2;
	if (m_pPlayers[3] != NULL)
		m_nPlayers++;
	m_nPlayers++;
	// Quirk: a player that fails to set up is deleted but stays in its slot.
	CAudioPlayer *player = FindPlayer(AUDIO_WMA);
	if (player != NULL && player->Init(dataPath) < 0)
		delete player;
	player = FindPlayer(AUDIO_MP3);
	if (player != NULL && player->Init(dataPath) < 0)
		delete player;
	SeedRandom();
	UINT index = SOUNDTRACK_FIRST_USER;
	if (m_soundtracks.size() > 2)
	{
		do
			index = (UINT)rand() % (UINT)m_soundtracks.size();
		while (index < SOUNDTRACK_FIRST_USER);
	}
	m_pSoundtrack = NULL;
	LoadSoundtrack(index, TRUE);
	m_bReady = TRUE;
	return S_OK;
}

// 0x00108E69
void CMusicManager::Resume()
{
	if (!m_bUserPaused && m_nState == MUSIC_PAUSED)
		Play();
}

// 0x00108E87
BOOL CMusicManager::HandleInput(int *action, SGamepadState **pad)
{
	if (!IsPlaying() || pad == NULL || m_pSettings->m_audio.m_nMusicVolume < 5)
		return TRUE;
	Update();
	SGamepadState *gamepad = pad[1] != NULL ? pad[1] : pad[0];
	BOOL whiteHeld = gamepad->bAnalogButtons[5] > 30;
	if (!whiteHeld && (*action <= INPUT_IR_DISPLAY || *action >= INPUT_IR_TITLE))
	{
		// (Only the right stick click gets here: the remote's play, stop
		// and pause are 20..26 and take the other path.)
		BOOL playing = GetState() == MUSIC_PLAYING;
		switch (*action)
		{
		case INPUT_RTHUMB_CLICK:
		case INPUT_IR_PAUSE:
			m_bUserPaused = !m_bUserPaused;
			if (m_bUserPaused)
				Pause();
			else
				Play();
			break;
		case INPUT_IR_PLAY:
			if (m_bUserPaused)
			{
				m_bUserPaused = FALSE;
				Resume();
			}
			else if (!playing)
			{
				Play();
			}
			else
			{
				return TRUE;
			}
			break;
		case INPUT_IR_STOP:
			m_bUserPaused = FALSE;
			if (!playing)
				return TRUE;
			Stop();
			break;
		default:
			return TRUE;
		}
		*action = INPUT_NONE;
		return FALSE;
	}

	// WHITE + d-pad, or the remote's transport keys.
	BOOL play = GetState() == MUSIC_PLAYING;
	BOOL global = IsGlobal();
	BOOL random = IsRandom();
	switch (*action)
	{
	case INPUT_UP:
	case INPUT_IR_SKIP_NEXT:
		SetRandom(FALSE);
		SetGlobal(FALSE);
		m_bLastSong = FALSE;
		NextSoundtrack();
		SetGlobal(global);
		break;
	case INPUT_DOWN:
	case INPUT_IR_SKIP_PREV:
		SetRandom(FALSE);
		SetGlobal(FALSE);
		m_bLastSong = FALSE;
		PrevSoundtrack();
		SetGlobal(global);
		break;
	case INPUT_LEFT:
	case INPUT_IR_REVERSE:
		m_bLastSong = FALSE;
		PrevSong();
		break;
	case INPUT_RIGHT:
	case INPUT_IR_FORWARD:
		m_bLastSong = FALSE;
		NextSong();
		break;
	case INPUT_IR_PLAY:
		play = TRUE;
		break;
	case INPUT_IR_STOP:
		play = FALSE;
		break;
	case INPUT_IR_PAUSE:
		if (play)
			Pause();
		else
			Play();
		*action = INPUT_NONE;
		return FALSE;
	default:
		return TRUE;
	}
	SetRandom(random);
	if (play)
	{
		Sleep(50);
		Play();
	}
	else
	{
		Stop();
	}
	*action = INPUT_NONE;
	return FALSE;
}
