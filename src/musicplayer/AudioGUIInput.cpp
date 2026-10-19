// CAudioGUI::ProcessInput (0x00135A99): the pop-ups first, then the
// player's buttons. See docs/MUSIC.md for the keys.
#include "AudioGUI.h"

#include "../audio/MusicManager.h"
#include "../common/FileUtil.h"
#include "../input/Gamepad.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../ui/FileBrowser.h"
#include "../ui/Menu.h"
#include "SongList.h"
#include "TrackOptions.h"

// The skin's button order: CSkin::m_nButtonPrev and on, one per
// PlayerButton (0xFF = not in the skin).
static UINT GetButtonPos(UINT button)
{
	return (UINT)(&g_skin.m_nButtonPrev)[button];
}

// Quirk: MoveFocus asks about 11 buttons, one past m_buttons (the
// original reads whatever follows); here the missing one is disabled.
static bool IsEnabled(const std::vector<SPlayerButton> &buttons, UINT button)
{
	return button < buttons.size() && buttons[button].enabled;
}

// Makes the music manager see WHITE held, so that an action becomes its
// WHITE + d-pad shortcut (previous/next song or soundtrack).
static void PressWhite(SGamepadState **pad)
{
	SGamepadState *gamepad = pad[1] != NULL ? pad[1] : pad[0];
	gamepad->bAnalogButtons[5] = 31;
}

// (inlined)
void CAudioGUI::MoveFocus(BOOL right)
{
	UINT savedButton = m_nButton;
	UINT pos = right ? m_nButtonPos + 1 : m_nButtonPos - 1;
	m_nButton = BUTTON_NONE;
	m_nButtonPos = pos;
	// The enabled button at that place...
	for (UINT i = 0; i < 11; i++)
	{
		if (GetButtonPos(i) == pos && IsEnabled(m_buttons, i))
		{
			m_nButton = i;
			break;
		}
	}
	// ...else the nearest one beyond it.
	if (m_nButton == BUTTON_NONE)
	{
		UINT best = right ? 10 : 0;
		for (UINT i = 0; i < 11; i++)
		{
			UINT other = GetButtonPos(i);
			if (other >= 11 || pos == 0 || !IsEnabled(m_buttons, i))
				continue;
			if (right ? (other > pos && other <= best) : (other < pos && other >= best))
			{
				best = other;
				m_nButton = i;
			}
		}
	}
	if (m_nButton == BUTTON_NONE)
	{
		// Nothing there: stay.
		m_nButtonPos = right ? pos - 1 : pos + 1;
		m_nButton = savedButton;
	}
	else
	{
		m_nButtonPos = GetButtonPos(m_nButton);
	}
}

// (inlined)
void CAudioGUI::ChangeVolume(BOOL up)
{
	int &volume = m_pSettings->m_audio.m_nMusicVolume;
	if (up)
	{
		if ((UINT)volume >= 100)
			return;
		volume++;
	}
	else
	{
		if ((UINT)volume == 0)
			return;
		volume--;
	}
	g_pMusicManager->SetVolume((float)m_pSettings->GetMusicVolume());
}

// (inlined)
void CAudioGUI::ToggleSongList()
{
	m_bSongListShown = !m_bSongListShown;
	if (!m_bSongListShown)
		return;
	if (m_pSongList == NULL)
	{
		m_pSongList = new CSongListControl;
		if (m_pSongList == NULL)
		{
			m_bSongListShown = FALSE;
			return;
		}
		m_pSongList->m_bMusicManager = TRUE;
		m_pSongList->Init(m_pFont, m_fListX, m_fListY, m_fListWidth, m_fListHeight);
	}
	SkinRectToFloats(g_skin.m_rcPlayList.left, g_skin.m_rcPlayList.top, g_skin.m_rcPlayList.width,
	                 g_skin.m_rcPlayList.height, &m_fListX, &m_fListY, &m_fListWidth, &m_fListHeight);
	m_pSongList->SetLayout(g_skin.m_pPlayListBackground, m_fListHeight, m_fListX, m_fListY, m_fListWidth,
	                       (float)(DWORD)g_skin.m_nPlayListOffsetX, (float)(DWORD)g_skin.m_nPlayListOffsetY,
	                       (float)(DWORD)g_skin.m_nPlayListOffsetX2, (float)(DWORD)g_skin.m_nPlayListOffsetY2);
	UpdateSongInfo();
	m_pSongList->SetSelection(m_nShownSong);
}

// 0x00135A99
HRESULT CAudioGUI::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_NONE)
	{
		// Idle: once a second, the song info; a disc arriving opens the
		// menu (with "Play Disc").
		static DWORD s_dwNextUpdate = GetTickCount() + 1000;   // 0x0027A464
		if (GetTickCount() <= s_dwNextUpdate)
			return S_OK;
		UpdateSongInfo();
		s_dwNextUpdate = GetTickCount() + 1000;
		if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
		{
			m_bNoDisc = TRUE;
		}
		else if (m_bNoDisc)
		{
			m_bNoDisc = FALSE;
			ShowMenu();
			m_menu.m_nSelection = 0;
		}
		return S_OK;
	}

	if (m_bMenuOpen)
	{
		m_menu.ProcessInput(action, pad);
		m_bMenuOpen = m_menu.IsOpen();
		if (!m_bMenuOpen && m_menu.m_bChosen)
		{
			int choice = m_menu.GetSelection();
			if (choice == 0)
			{
				// Play Disc
				g_pMusicManager->m_bUserPaused = FALSE;
				g_nPlayerSoundtrack = MUSIC_NO_SONG;
				g_pMusicManager->PlayAudioCD();
			}
			else if (choice == 1)
			{
				ShowTrackOptions();
			}
			else if (choice == 3 || choice == 4)
			{
				// Open File, Browse Folder
				ShowBrowser(m_menu.GetSelection() == 3);
			}
		}
		*action = INPUT_NONE;
		return S_OK;
	}
	if (m_bTrackOptionsOpen)
	{
		m_pTrackOptions->ProcessInput(action, pad);
		m_bTrackOptionsOpen = m_pTrackOptions->IsOpen();
		*action = INPUT_NONE;
		return S_OK;
	}
	if (m_bDialogOpen)
	{
		m_pDialog->ProcessInput(action, pad);
		m_bDialogOpen = m_pDialog->IsOpen();
		*action = INPUT_NONE;
		return S_OK;
	}
	if (m_bBrowserOpen)
	{
		CFileBrowser *browser = m_pBrowser;
		browser->ProcessInput(action, pad);
		if (!browser->IsOpen())
		{
			m_bBrowserOpen = FALSE;
			if (!browser->m_bCancelled)
			{
				m_strBrowsePath = browser->GetSelectedPath();
				if (!m_strBrowsePath.empty())
				{
					OpenFile(m_strBrowsePath.c_str());
					// A file: next time start in its folder.
					if (m_menu.GetSelection() == 3)
						m_strBrowsePath = GetPathDirectory(m_strBrowsePath.c_str());
				}
			}
		}
		*action = INPUT_NONE;
		return S_OK;
	}

	// WHITE held and the remote's transport keys are the music manager's.
	SGamepadState *gamepad = pad[1] != NULL ? pad[1] : pad[0];
	if (gamepad->bAnalogButtons[5] > 30 || (*action > INPUT_IR_DISPLAY && *action < INPUT_IR_TITLE))
		return g_pMusicManager->HandleInput(action, pad);

	if (*action == INPUT_LTHUMB && pad[0] != NULL)
	{
		if (pad[0]->fY1 == 1.0f)
			*action = INPUT_UP;
		else if (pad[0]->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	if (*action == INPUT_POINTER_CLICK)
	{
		// A click on a button shape is A on that button.
		if (!g_bPointerClicked)
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		std::vector<SSkinDrawItem>::iterator it = g_skin.m_musicOrder.begin();
		for (; it != g_skin.m_musicOrder.end(); ++it)
		{
			if (it->kind != 1)
				continue;
			SSkinShape &shape = g_skin.m_musicShapes[it->index];
			if (shape.type > 0x13 && shape.type < 0x1F && PointInRect(&shape.left, g_nPointerX, g_nPointerY))
				break;
		}
		if (it == g_skin.m_musicOrder.end())
		{
			*action = INPUT_NONE;
			return S_OK;
		}
		m_nButton = g_skin.m_musicShapes[it->index].type - SHAPE_PREV;
		*action = INPUT_A;
	}

	CMusicManager *music = g_pMusicManager;
	switch (*action)
	{
	case INPUT_A:
		// (The original tests enabled first, even for BUTTON_NONE.)
		if (!IsEnabled(m_buttons, m_nButton) || m_nButton > BUTTON_MUTE)
			break;
		switch (m_nButton)
		{
		case BUTTON_PREV:
			if (music->GetSongCount() <= 1)
				break;
			PressWhite(pad);
			*action = INPUT_LEFT;
			music->HandleInput(action, pad);
			break;
		case BUTTON_PLAY:
			// With the list shown: the selected song (if it isn't the
			// shown one).
			if (m_bSongListShown && m_pSongList->m_nSelected != m_nShownSong &&
			    music->GetSong(m_pSongList->m_nSelected) != NULL)
			{
				if (FAILED(music->PlaySongAt(m_pSongList->m_nSelected)))
					return S_OK;   // (the action stays)
				m_nListSong = m_pSongList->m_nSelected;
				music = g_pMusicManager;
			}
			if (music != NULL && (m_nListSong != m_nShownSong || music->GetState() != MUSIC_PLAYING))
			{
				music->m_bUserPaused = FALSE;
				music->m_bLastSong = FALSE;
				g_nPlayerSoundtrack = MUSIC_NO_SONG;
				music->Play();
			}
			break;
		case BUTTON_PAUSE:
			*action = INPUT_IR_PAUSE;
			music->HandleInput(action, pad);
			break;
		case BUTTON_STOP:
			music->Stop();
			break;
		case BUTTON_NEXT:
			if (music->GetSongCount() <= 1)
				break;
			PressWhite(pad);
			*action = INPUT_RIGHT;
			music->HandleInput(action, pad);
			break;
		case BUTTON_PREVTRACK:
			if (music->GetUserSoundtrackCount() <= 1)
				break;
			PressWhite(pad);
			*action = INPUT_UP;
			music->HandleInput(action, pad);
			break;
		case BUTTON_NEXTTRACK:
			if (music->GetUserSoundtrackCount() <= 1)
				break;
			PressWhite(pad);
			*action = INPUT_DOWN;
			music->HandleInput(action, pad);
			break;
		case BUTTON_RANDOM:
			m_buttons[BUTTON_RANDOM].on = !m_buttons[BUTTON_RANDOM].on;
			music->SetRandom(m_buttons[BUTTON_RANDOM].on);
			break;
		case BUTTON_MUTE:
			m_buttons[BUTTON_MUTE].on = !m_buttons[BUTTON_MUTE].on;
			music->SetMute(m_buttons[BUTTON_MUTE].on);
			break;
		}
		break;

	case INPUT_START:
		ShowMenu();
		break;

	case INPUT_B:
	case INPUT_BACK:
		if (!m_bKeepMusic)
		{
			// Dead code (m_bKeepMusic is always set): back to the music
			// from before OpenFile.
			BOOL random = music->IsRandom();
			music->SetRandom(FALSE);
			music->LoadSoundtrack(m_nSavedSoundtrack, FALSE);
			g_pMusicManager->PlaySongAt(m_nSavedSong);
			music = g_pMusicManager;
			music->SetRandom(random);
			if (m_nSavedState == MUSIC_STOPPED)
			{
				music->Stop();
			}
			else if (m_nSavedState == MUSIC_PAUSED)
			{
				music->Pause();
			}
			else if (m_nSavedState == MUSIC_PLAYING)
			{
				music->m_bUserPaused = FALSE;
				music->Play();
			}
		}
		m_bOpen = FALSE;
		break;

	case INPUT_BLACK:
		m_bOpen = FALSE;
		break;

	case INPUT_UP:
	case INPUT_IR_UP:
		if (m_nButton == BUTTON_VOLUME || *action == INPUT_IR_UP)
		{
			ChangeVolume(TRUE);
			break;
		}
		// fall through
	case INPUT_LTRIGGER:
		if (m_bSongListShown)
		{
			m_pSongList->ProcessInput(action, pad);
			if (m_nListSong > 0)
				m_nListSong--;
			// (The list moves the focus to PLAY.)
			m_nButton = BUTTON_PLAY;
			m_nButtonPos = g_skin.m_nButtonPlay;
		}
		else if (music != NULL)
		{
			int state = music->GetState();
			music->PrevSoundtrack();
			if (state == MUSIC_PLAYING)
				music->Play();
		}
		break;

	case INPUT_DOWN:
	case INPUT_IR_DOWN:
		if (m_nButton == BUTTON_VOLUME || *action == INPUT_IR_DOWN)
		{
			ChangeVolume(FALSE);
			break;
		}
		// fall through
	case INPUT_RTRIGGER:
		if (m_bSongListShown)
		{
			m_pSongList->ProcessInput(action, pad);
			if (m_nListSong < music->GetSongCount() - 1)
				m_nListSong++;
			m_nButton = BUTTON_PLAY;
			m_nButtonPos = g_skin.m_nButtonPlay;
		}
		else if (music != NULL)
		{
			int state = music->GetState();
			music->NextSoundtrack();
			if (state == MUSIC_PLAYING)
				music->Play();
		}
		break;

	case INPUT_LEFT:
		if (m_nButtonPos != 0)
			MoveFocus(FALSE);
		break;

	case INPUT_RIGHT:
		if (m_nButtonPos < 10)
			MoveFocus(TRUE);
		break;

	case INPUT_RTHUMB:
		// The right stick: up or right is louder, down or left quieter.
		// (Only the first gamepad's state is read here.)
		if (pad[0] == NULL)
			break;
		if (pad[0]->fX2 == 1.0f || pad[0]->fY2 == 1.0f)
			ChangeVolume(TRUE);
		else if (pad[0]->fX2 == -1.0f || pad[0]->fY2 == -1.0f)
			ChangeVolume(FALSE);
		break;

	case INPUT_RTHUMB_CLICK:
		ToggleSongList();
		break;

	default:
		// X, Y, WHITE, the stick clicks, the remote's other keys...
		music->HandleInput(action, pad);
		break;
	}
	*action = INPUT_NONE;
	return S_OK;
}
