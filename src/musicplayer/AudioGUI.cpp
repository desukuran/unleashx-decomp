// CAudioGUI, the music player screen: construction, the built-in layout,
// the song info and the pop-ups (0x00133728-0x00134853, 0x00135A18). See
// AudioGUI.h.
#include "AudioGUI.h"

#include <cmath>
#include <cwchar>

#include "../audio/AudioPlayer.h"
#include "../audio/MusicManager.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../system/Drives.h"
#include "../ui/FileBrowser.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "SongList.h"
#include "TrackOptions.h"

UINT g_nPlayerSoundtrack = MUSIC_NO_SONG;          // 0x00269BC8
std::vector<SPlayerTrack> g_playerTracks;          // 0x00277710

// Rounds a font scale to quarters (0.76 and up is 1).
static float RoundScale(float scale)
{
	return (float)floor((scale + 0.24f) * 4.0f) * 0.25f;
}

// 0x00134640
CAudioGUI::CAudioGUI()
{
	m_buttons.clear();
	m_nSong = MUSIC_NO_SONG;
	m_nShownSong = MUSIC_NO_SONG;
	m_bFileOpened = FALSE;
	m_bOpen = FALSE;
	m_pKeyboardTexture = NULL;
	m_pButtonTexture = NULL;
	m_pFont = NULL;
	m_szChannels[0] = 0;
	m_szBitRate[0] = 0;
	m_szSampleRate[0] = 0;
	m_szLength[0] = 0;
	m_bSongListShown = FALSE;
	m_unknownEC = 0;
	m_unknownF0 = 0;
	m_nListSong = 0;
	m_bInfoReady = TRUE;
	m_bMenuOpen = FALSE;
	m_bNoDisc = TRUE;
	m_pDialog = NULL;
	m_bDialogOpen = FALSE;
	m_nSavedState = g_pMusicManager->GetState();
	m_pBrowser = NULL;
	m_strBrowsePath.clear();
	m_bBrowserOpen = FALSE;
	m_bKeepMusic = TRUE;
	m_pTrackOptions = NULL;
	m_bTrackOptionsOpen = FALSE;
	m_pSongList = NULL;
	m_pSettings = CAppSettings::GetInstance();
	// (Not set by the original.)
	m_nSavedSoundtrack = 0;
	m_nSavedSong = 0;
	m_nButton = BUTTON_NONE;
	m_nButtonPos = 0;
	m_unknown1DC = 0;
}

// 0x0013361E
CAudioGUI::~CAudioGUI()
{
	if (m_pSongList != NULL)
	{
		delete m_pSongList;
		m_pSongList = NULL;
	}
	if (m_pButtonTexture != NULL)
	{
		m_pButtonTexture->Release();
		m_pButtonTexture = NULL;
	}
	m_pButtonTexture = NULL;
	m_buttons.clear();
	if (m_pDialog != NULL)
	{
		delete m_pDialog;
		m_pDialog = NULL;
	}
	if (m_pBrowser != NULL)
	{
		delete m_pBrowser;
		m_pBrowser = NULL;
	}
	if (m_pTrackOptions != NULL)
	{
		delete m_pTrackOptions;
		m_pTrackOptions = NULL;
	}
}

// 0x00134283
BOOL CAudioGUI::Init(CXBFontEx *font)
{
	m_pFont = font;
	g_textures.Find("Keyboard", &m_pKeyboardTexture);
	if (m_pButtonTexture == NULL)
	{
		std::string path = m_pSettings->m_strDataPath;
		path.append("MPlayer.png");
		if (!LoadTextureFromFile(path.c_str(), g_pd3dDevice, &m_pButtonTexture, 0, 0, NULL))
		{
			m_pButtonTexture = NULL;
			return E_FAIL;   // (an HRESULT, as from the other screens)
		}
	}
	if (!m_buttons.empty())
		return S_OK;

	m_menu.Create(font, GetString(STR_AUDIO_OPTIONS), 0.0f, 0.0f, 0.0f, 0.0f, 10);
	m_menu.AddOption(GetString(STR_AUDIO_PLAYDISC), 0);
	m_menu.AddOption(GetString(STR_AUDIO_MANAGETRACKS), 1);
	m_menu.AddOption(L"", 2);
	m_menu.AddOption(GetString(STR_AUDIO_OPENFILE), 3);
	m_menu.AddOption(GetString(STR_BROWSE_FOLDER), 4);

	UINT songs = g_pMusicManager->GetSongCount();
	UINT soundtracks = g_pMusicManager->GetUserSoundtrackCount();
	for (int id = BUTTON_PREV; id <= BUTTON_VOLUME; id++)
	{
		SPlayerButton button;
		button.id = id;
		button.enabled = true;
		button.on = false;
		button.left = button.top = button.right = button.bottom = 0.0f;
		switch (id)
		{
		case BUTTON_PREV:
			button.enabled = songs > 1;
			break;
		case BUTTON_NEXT:
			// (Show then asks for more than one, like BUTTON_PREV.)
			button.enabled = songs != 0;
			break;
		case BUTTON_PREVTRACK:
		case BUTTON_NEXTTRACK:
			button.enabled = soundtracks > 1;
			break;
		case BUTTON_RANDOM:
			button.on = g_pMusicManager->IsRandom() != FALSE;
			break;
		case BUTTON_MUTE:
			button.on = g_pMusicManager->m_bMuted != FALSE;
			break;
		}
		m_buttons.push_back(button);
	}

	// The focus starts on the button of the music's state.
	m_nButtonPos = g_skin.m_nButtonStop;
	switch (g_pMusicManager->GetState())
	{
	case MUSIC_STOPPED:
		m_nButton = BUTTON_STOP;
		m_nButtonPos = g_skin.m_nButtonStop;
		break;
	case MUSIC_PAUSED:
		m_nButton = BUTTON_PAUSE;
		m_nButtonPos = g_skin.m_nButtonPause;
		break;
	case MUSIC_PLAYING:
		m_nButton = BUTTON_PLAY;
		m_nButtonPos = g_skin.m_nButtonPlay;
		break;
	}
	return S_OK;
}

// 0x00133A5D
void CAudioGUI::Show()
{
	CMusicManager *music = g_pMusicManager;
	m_buttons[BUTTON_PREV].enabled = music->GetSongCount() >= 2 && g_skin.m_nButtonPrev < 0xFF;
	m_buttons[BUTTON_NEXT].enabled = m_buttons[BUTTON_PREV].enabled && g_skin.m_nButtonNext < 0xFF;
	m_buttons[BUTTON_PREVTRACK].enabled = music->GetUserSoundtrackCount() >= 2 && g_skin.m_nButtonPrevTrack < 0xFF;
	m_buttons[BUTTON_NEXTTRACK].enabled = m_buttons[BUTTON_PREVTRACK].enabled && g_skin.m_nButtonNextTrack < 0xFF;
	m_buttons[BUTTON_RANDOM].on = music->IsRandom() && g_skin.m_nButtonRandom < 0xFF;
	m_buttons[BUTTON_MUTE].on = music->m_bMuted && g_skin.m_nButtonMute < 0xFF;
	// (Not used for these two.)
	m_buttons[BUTTON_PAUSE].on = g_skin.m_nButtonPause < 0xFF;
	m_buttons[BUTTON_STOP].on = g_skin.m_nButtonStop < 0xFF;

	// The built-in layout, in skin coordinates.
	float savedScaleX = m_pFont->m_fScaleFactorX;
	float savedScaleY = m_pFont->m_fScaleFactorY;
	m_fTitleScale = 40.0f / m_pFont->m_fCellHeight;
	m_fInfoScale = 18.0f / m_pFont->m_fCellHeight;
	m_fSmallScale = 10.0f / m_pFont->m_fCellHeight;
	m_fInfoHeight = 90.0f;
	m_fHeight = 135.0f;
	m_fWidth = 390.0f;
	m_fListWidth = 390.0f;
	m_fListHeight = 265.0f;
	m_fTitleY = 55.0f;
	m_fY = 40.0f;
	m_fMargin = 10.0f;
	m_fTitleX = 155.0f;
	m_fX = 125.0f;
	m_fListY = 175.0f;
	m_fListX = 125.0f;
	m_pFont->SetScaleFactors(m_fTitleScale, m_fTitleScale);
	m_fTimeX = m_pFont->GetTextWidth(L"00:00") + m_fX + m_fMargin + 40.0f;
	m_fMeterY = m_fY + 10.0f;
	m_fMeterWidth = 120.0f;
	m_fMeterHeight = 50.0f;
	m_fTimeY = m_fTitleY;
	m_fMeterX = ((m_fX + m_fWidth) - 10.0f) - 120.0f;
	m_fStateY = ((m_fInfoHeight + m_fY) - 30.0f) + 5.0f;
	m_fStateX = m_fX + 5.0f;

	// The buttons: a row of 30x30 at the bottom left.
	float buttonX = m_fX + 5.0f;
	float buttonY = ((m_fY + m_fHeight) - 5.0f) - 30.0f;
	float buttonWidth = 30.0f;
	float buttonHeight = 30.0f;
	m_fScaleY = 1.0f;
	m_fScaleX = 1.0f;
	ScaleToScreen(&m_fX, &m_fScaleX, &m_fY, &m_fScaleY, FALSE);
	m_fHeight *= m_fScaleY;
	m_fWidth *= m_fScaleX;
	m_fInfoHeight *= m_fScaleY;
	ScaleToScreen(&buttonX, &buttonWidth, &buttonY, &buttonHeight, FALSE);
	buttonX = (float)ceil(buttonX);
	buttonWidth = (float)floor(ceil(buttonWidth));
	buttonHeight = (float)ceil(buttonHeight);
	for (int id = BUTTON_PREV; id <= BUTTON_NEXTTRACK; id++)
	{
		SPlayerButton &button = m_buttons[id];
		button.left = buttonX;
		buttonX += buttonWidth;
		button.top = buttonY;
		button.right = buttonX;
		button.bottom = buttonY + buttonHeight;
	}
	// Random over mute, half height.
	buttonX = (float)ceil(m_fScaleX * 5.0f + buttonX);
	float halfHeight = (float)ceil(m_fScaleX * 15.0f);
	m_buttons[BUTTON_RANDOM].right = buttonX + buttonWidth;
	m_buttons[BUTTON_RANDOM].top = buttonY;
	m_buttons[BUTTON_RANDOM].left = buttonX;
	m_buttons[BUTTON_RANDOM].bottom = buttonY + halfHeight;
	m_buttons[BUTTON_MUTE].left = buttonX;
	m_buttons[BUTTON_MUTE].right = buttonX + buttonWidth;
	m_buttons[BUTTON_MUTE].top = buttonY + halfHeight;
	m_buttons[BUTTON_MUTE].bottom = buttonY + halfHeight + halfHeight;
	// The volume bar, 120x30. (Quirk: its height scales with the width.)
	buttonX = (float)ceil(m_fScaleX * 5.0f + buttonX + buttonWidth);
	float volumeWidth = (float)ceil(m_fScaleX * 120.0f);
	float volumeHeight = (float)ceil(m_fScaleX * 30.0f);
	m_buttons[BUTTON_VOLUME].left = buttonX;
	m_buttons[BUTTON_VOLUME].right = buttonX + volumeWidth;
	m_buttons[BUTTON_VOLUME].top = buttonY;
	m_buttons[BUTTON_VOLUME].bottom = buttonY + volumeHeight;

	ScaleToScreen(&m_fTitleX, NULL, &m_fTitleY, NULL, FALSE);
	ScaleToScreen(&m_fTimeX, NULL, &m_fTimeY, NULL, FALSE);
	ScaleToScreen(&m_fMeterX, &m_fMeterWidth, &m_fMeterY, &m_fMeterHeight, FALSE);
	m_fMeterX = (float)ceil(m_fMeterX);
	m_fMeterY = (float)ceil(m_fMeterY);
	m_fMeterWidth = (float)ceil(m_fMeterWidth);
	m_fMeterHeight = (float)ceil(m_fMeterHeight);
	if (m_fScaleY > 1.0f)
	{
		m_fTitleScale *= m_fScaleY;
		m_fInfoScale *= m_fScaleY;
		m_fSmallScale *= m_fScaleY;
	}
	m_fTitleScale = RoundScale(m_fTitleScale);
	m_fInfoScale = RoundScale(m_fInfoScale);
	m_fSmallScale = RoundScale(m_fSmallScale);
	m_fMargin *= m_fScaleY;
	ScaleToScreen(&m_fStateX, NULL, &m_fStateY, NULL, FALSE);
	m_pFont->SetScaleFactors(savedScaleX, savedScaleY);

	m_bOpen = TRUE;
	if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
		m_bNoDisc = TRUE;

	// The song list takes the skin's <PlayList> box (zeros keep the
	// built-in one).
	if (m_bSongListShown && m_pSongList != NULL)
	{
		SkinRectToFloats(g_skin.m_rcPlayList.left, g_skin.m_rcPlayList.top, g_skin.m_rcPlayList.width,
		                 g_skin.m_rcPlayList.height, &m_fListX, &m_fListY, &m_fListWidth, &m_fListHeight);
		m_pSongList->SetLayout(g_skin.m_pPlayListBackground, m_fListHeight, m_fListX, m_fListY, m_fListWidth,
		                       (float)(DWORD)g_skin.m_nPlayListOffsetX, (float)(DWORD)g_skin.m_nPlayListOffsetY,
		                       (float)(DWORD)g_skin.m_nPlayListOffsetX2, (float)(DWORD)g_skin.m_nPlayListOffsetY2);
	}
}

// 0x00133728
void CAudioGUI::UpdateSongInfo()
{
	if (!m_bInfoReady)
		return;
	CMusicManager *music = g_pMusicManager;
	if (m_bSongListShown && g_nPlayerSoundtrack != music->m_nSoundtrack)
	{
		// Another soundtrack: start the list over.
		BOOL several = music->GetSongCount() > 1;
		m_buttons[BUTTON_PREV].enabled = several != FALSE;
		m_buttons[BUTTON_NEXT].enabled = several != FALSE;
		m_bInfoReady = FALSE;
		m_nListSong = 0;
		m_unknownEC = 0;
		m_unknownF0 = 0;
		m_szChannels[0] = 0;
		m_szBitRate[0] = 0;
		m_szSampleRate[0] = 0;
		m_szLength[0] = 0;
		g_nPlayerSoundtrack = music->m_nSoundtrack;
		m_nSong = MUSIC_NO_SONG;
		g_playerTracks.clear();
		m_pSongList->SetSoundtrack(g_nPlayerSoundtrack);
		SSong *song = g_pMusicManager->GetSong(g_pMusicManager->GetCurrentSong());
		if (song != NULL && m_pSongList != NULL)
			m_pSongList->SetSelection(song->index);
		m_bInfoReady = TRUE;
		return;
	}

	if (m_nSong == music->GetCurrentSong())
		return;
	// A new song: mark it in the list and show its format.
	if (m_pSongList != NULL)
		m_pSongList->ClearMarks();
	m_nSong = music->GetCurrentSong();
	SSong *song = music->GetSong(m_nSong);
	if (song != NULL && m_pSongList != NULL)
		m_pSongList->SetMarked(song->index, TRUE);
	SSongFormat *format = music->GetSongFormat();
	if (song == NULL || format == NULL || format->duration == 0 || format->duration >= 3600)
	{
		// Not known yet (or an hour or longer): try again next time.
		m_nSong = MUSIC_NO_SONG;
		m_nShownSong = MUSIC_NO_SONG;
		return;
	}
	m_nShownSong = song->index;
	m_nListSong = song->index;
	const wchar_t *channels;
	if (g_nPlayerSoundtrack == SOUNDTRACK_CD)
		channels = L"CD Stereo";
	else if (music->GetSongFormat()->channels == 1)
		channels = L"Mono";
	else
		channels = L"Stereo";
	wcscpy(m_szChannels, channels);
	swprintf(m_szBitRate, 10, L"%d KBPS", music->GetSongFormat()->bitRate / 1000);
	swprintf(m_szSampleRate, 10, L"%.1f KHz", (double)music->GetSongFormat()->sampleRate * 0.001);
	swprintf(m_szLength, 10, L"%02d:%02d", music->GetSongFormat()->duration / 60,
	         music->GetSongFormat()->duration % 60);
	if (song->index < g_playerTracks.size() && g_playerTracks[song->index].time.empty())
		g_playerTracks[song->index].time = m_szLength;
}

// 0x00133A27
void CAudioGUI::ShowMenu()
{
	m_menu.SetOptionEnabled(0, GetDiscType() == 4);   // an audio CD
	// Quirk: there is no option 5 (the menu has 0-4).
	m_menu.SetOptionEnabled(5, FALSE);
	m_menu.Show(0);
	m_bMenuOpen = TRUE;
}

// 0x00134225
void CAudioGUI::ShowTrackOptions()
{
	if (m_pTrackOptions == NULL)
	{
		m_pTrackOptions = new CAudioTrackOption;
		if (m_pTrackOptions != NULL)
			m_pTrackOptions->Init((CXBFontEx *)m_pFont);
	}
	if (m_pTrackOptions == NULL)
	{
		m_bTrackOptionsOpen = FALSE;
		return;
	}
	m_pTrackOptions->Show();
	m_bTrackOptionsOpen = TRUE;
}

// 0x00134741
void CAudioGUI::ShowBrowser(BOOL files)
{
	if (m_pBrowser == NULL)
	{
		m_pBrowser = new CFileBrowser;
		if (m_pBrowser != NULL)
			m_pBrowser->Create(m_pFont, NULL);
		if (m_pBrowser == NULL)
		{
			m_bBrowserOpen = FALSE;
			return;
		}
	}
	m_pBrowser->SetTitle(GetString(files ? STR_AUDIO_OPENFILE : STR_BROWSE_FOLDER));
	m_pBrowser->m_bFolderResult = !files;
	m_pBrowser->SetSelectFiles(files);
	if (files)
	{
		m_pBrowser->AddFilter("WMA");
		m_pBrowser->AddFilter("MP3");
		m_pBrowser->AddFilter("WAV");
	}
	m_pBrowser->m_bNewFolderItem = FALSE;
	m_pBrowser->SetStartPath(m_strBrowsePath.c_str());
	m_pBrowser->Show(FALSE);
	m_bBrowserOpen = TRUE;
}

// 0x00135A18
BOOL CAudioGUI::OpenFile(const char *path)
{
	CMusicManager *music = g_pMusicManager;
	if (music == NULL || !StrIsNotEmpty(path))
		return FALSE;
	if (m_bKeepMusic)
	{
		// Saved, but only restored when m_bKeepMusic is clear (never).
		m_nSavedState = music->GetState();
		m_nSavedSoundtrack = music->m_nSoundtrack;
		m_nSavedSong = music->GetCurrentSong();
	}
	music->m_bUserPaused = FALSE;
	if (!music->PlayFile(path, m_menu.GetSelection() == 4))
		return FALSE;
	m_bFileOpened = TRUE;
	return TRUE;
}
