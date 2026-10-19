// The soundtrack tools' menus: the source list (CAudioTrackOption), the
// audio CD's menu (CAudioCDOption) and a soundtrack's menu
// (CSoundTrackOption). See TrackOptions.h.
#include "TrackOptions.h"

#include <cwchar>

#include "../audio/MusicManager.h"
#include "../audio/SoundtrackDB.h"
#include "../audio/AudioPlayer.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../system/Drives.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"

// 0x000F106D, 0x000F10DC, 0x000F114B, 0x000F11CB
void OpenTrackKeyboard(const wchar_t *title, const wchar_t *text, BOOL password, int maxLength, int owner,
                       BOOL *open)
{
	g_virtualKeyboard.m_nMaxLength = 64;
	if (maxLength != 0)
		g_virtualKeyboard.m_nMaxLength = maxLength;
	if (g_virtualKeyboard.m_bNumeric != owner)
		g_virtualKeyboard.Open();
	g_virtualKeyboard.m_bNumeric = owner;
	g_virtualKeyboard.SetTitle(title);
	g_virtualKeyboard.Open();
	g_virtualKeyboard.m_bPassword = password;
	if (text != NULL)
		g_virtualKeyboard.SetText(text);
	*open = TRUE;
}

// (Repeated for each class in the original: 0x000F2C07, 0x000F313C,
// 0x000F3280.)
void ShowTrackMessage(CMessageBox **messageBox, BOOL *shown, CXBFont *font, const char *text,
                             const wchar_t *title, int buttons)
{
	if (*messageBox == NULL)
	{
		*messageBox = new CMessageBox;
		if (*messageBox != NULL)
		{
			if (FAILED((*messageBox)->Create(font, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
			{
				delete *messageBox;
				*messageBox = NULL;
				return;
			}
			(*messageBox)->SetButtonText(MSGBOX_OK, GetString(STR_OK));
			(*messageBox)->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
			(*messageBox)->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
			(*messageBox)->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
			(*messageBox)->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
			(*messageBox)->SetButtonText(MSGBOX_YES, GetString(STR_YES));
			(*messageBox)->SetButtonText(MSGBOX_NO, GetString(STR_NO));
		}
	}
	if (*messageBox != NULL)
	{
		(*messageBox)->Show(text, title, buttons);
		*shown = TRUE;
	}
}

// ---- CAudioTrackOption ----

// 0x000F2A10
CAudioTrackOption::CAudioTrackOption()
{
	m_pSoundtrackOptions = NULL;
	m_bOpen = FALSE;
	m_pFont = NULL;
	m_pCDOptions = NULL;
	m_bInitialized = FALSE;
	m_bCDOptionsOpen = FALSE;
	m_bSoundtrackOptionsOpen = FALSE;
}

// 0x000F0E67
CAudioTrackOption::~CAudioTrackOption()
{
	if (m_pCDOptions != NULL)
	{
		delete m_pCDOptions;
		m_pCDOptions = NULL;
	}
	if (m_pSoundtrackOptions != NULL)
	{
		delete m_pSoundtrackOptions;
		m_pSoundtrackOptions = NULL;
	}
	m_pFont = NULL;
}

// 0x000F2F98
BOOL CAudioTrackOption::Init(CXBFontEx *font)
{
	if (!m_bInitialized)
	{
		m_pFont = (CXBFont *)font;
		m_menu.Create((CXBFont *)font, GetString(STR_AUDIO_SELECTOPTION), 0.0f, 0.0f, 0.0f, 0.0f, 10);
		FillList();
		Show();
		m_bInitialized = TRUE;
	}
	return m_bInitialized ? S_OK : E_FAIL;
}

// 0x000F2DE5
void CAudioTrackOption::FillList()
{
	m_menu.ClearOptions();
	m_menu.AddOption(GetString(STR_AUDIO_CD), 0);
	XSOUNDTRACK_DATA data;
	HANDLE find = XFindFirstSoundtrack(&data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			m_menu.AddOption(data.szName, (int)data.uSoundtrackId);
		} while (XFindNextSoundtrack(find, &data));
		XFindClose(find);
	}
	if (g_pMusicManager != NULL)
	{
		for (UINT i = 0; i < g_pMusicManager->GetUserSoundtrackCount(); i++)
		{
			SSoundtrack *soundtrack = g_pMusicManager->GetUserSoundtrack(i);
			if (soundtrack != NULL && soundtrack->isFolder)
				m_menu.AddOption(soundtrack->name, (int)(soundtrack->index + MUSIC_FOLDER_VALUE));
		}
	}
}

// 0x000F148C
void CAudioTrackOption::Show()
{
	m_menu.SetOptionEnabled(0, GetDiscType() == 4 && g_cdToc.hDevice != INVALID_HANDLE_VALUE);   // an audio CD
	m_bOpen = TRUE;
	m_menu.Show(0);
}

// 0x000F091F
BOOL CAudioTrackOption::Render()
{
	if (m_bSoundtrackOptionsOpen)
		m_pSoundtrackOptions->Render();
	else if (m_bCDOptionsOpen)
		m_pCDOptions->Render();
	else
		m_menu.Render();
	return TRUE;
}

// 0x000F2974
void CAudioTrackOption::ShowCDOptions()
{
	if (m_pCDOptions == NULL)
	{
		m_pCDOptions = new CAudioCDOption;
		if (m_pCDOptions != NULL)
			m_pCDOptions->Create(m_pFont, L" ", 0.0f, 0.0f, 0.0f, 0.0f, 10);
	}
	m_bCDOptionsOpen = m_pCDOptions != NULL;
	if (m_pCDOptions == NULL)
		return;
	m_pCDOptions->m_strTitle = m_menu.m_options[m_menu.GetSelection()].text;
	m_pCDOptions->Reopen();
}

// 0x000F28BF
void CAudioTrackOption::ShowSoundtrackOptions()
{
	if (m_pSoundtrackOptions == NULL)
	{
		m_pSoundtrackOptions = new CSoundTrackOption;
		if (m_pSoundtrackOptions != NULL)
			m_pSoundtrackOptions->Create(m_pFont, L" ", 0.0f, 0.0f, 0.0f, 0.0f, 10);
	}
	m_bSoundtrackOptionsOpen = m_pSoundtrackOptions != NULL;
	if (m_pSoundtrackOptions == NULL)
		return;
	m_pSoundtrackOptions->m_strTitle = m_menu.m_options[m_menu.GetSelection()].text;
	DWORD value = (DWORD)m_menu.GetSelectedValue();
	if (m_pSoundtrackOptions->m_dwSoundtrack != value)
		m_pSoundtrackOptions->m_dwSoundtrack = value;
	m_pSoundtrackOptions->Reopen();
}

// 0x000F2EC6
// Takes all input while open.
HRESULT CAudioTrackOption::ProcessInput(int *action, SGamepadState **pad)
{
	if (!m_bOpen)
		return S_OK;
	if (m_bSoundtrackOptionsOpen)
	{
		m_pSoundtrackOptions->ProcessInput(action, pad);
		m_bSoundtrackOptionsOpen = m_pSoundtrackOptions->IsOpen();
		// (Checked even while it's still open.)
		if (m_pSoundtrackOptions->m_bChanged)
			FillList();
	}
	else if (m_bCDOptionsOpen)
	{
		m_pCDOptions->ProcessInput(action, pad);
		m_bCDOptionsOpen = m_pCDOptions->IsOpen();
	}
	else
	{
		m_menu.ProcessInput(action, pad);
		if (!m_menu.IsOpen())
		{
			if (m_menu.m_bChosen)
			{
				// Quirk: an Xbox soundtrack with ID 0 has the CD's value.
				if (m_menu.GetSelectedValue() == 0)
					ShowCDOptions();
				else
					ShowSoundtrackOptions();
				m_menu.Show(0);
				m_menu.m_bSlideIn = FALSE;
			}
			else
			{
				m_bOpen = FALSE;
			}
		}
	}
	*action = 0;
	return S_OK;
}

// ---- CAudioCDOption ----

// 0x000F2796
CAudioCDOption::CAudioCDOption()
{
	m_bKeyboardOpen = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_unknownB0 = 0;
	m_pTrackCopy = NULL;
	m_bTrackCopyOpen = FALSE;
	m_unknown98 = 0;
}

// 0x000F1361
CAudioCDOption::~CAudioCDOption()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (m_pTrackCopy != NULL)
	{
		delete m_pTrackCopy;
		m_pTrackCopy = NULL;
	}
}

// 0x000F2B99
HRESULT CAudioCDOption::Create(CXBFont *font, const wchar_t *title, float x, float y, float width, float height,
                               int maxVisible)
{
	HRESULT hr = COptionMenu::Create(font, title, x, y, width, height, maxVisible);
	if (SUCCEEDED(hr))
	{
		AddOption(GetString(STR_AUDIO_PLAY), 0);
		AddOption(GetString(STR_COPY), 1);
		// Ripping is disabled.
		SetOptionEnabled(1, FALSE);
	}
	return hr;
}

// 0x000F1312
void CAudioCDOption::Reopen()
{
	Show(0);
	m_unknownB0 = 0;
	m_unknown98 = 0;
}

// 0x000F132C
void CAudioCDOption::Render()
{
	COptionMenu::Render();
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bTrackCopyOpen)
		m_pTrackCopy->Render();
}

// 0x000F42FE
void CAudioCDOption::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_bMessageShown || m_bKeyboardOpen)
		return;
	if (m_bTrackCopyOpen)
	{
		m_pTrackCopy->ProcessInput(action, pad);
		m_bTrackCopyOpen = m_pTrackCopy->IsOpen();
		return;
	}
	COptionMenu::ProcessInput(action, pad);
	if (IsOpen() || !m_bChosen)
		return;
	m_nAction = GetSelection();
	std::string unused;
	if (m_nAction == 0)
		g_pMusicManager->PlayAudioCD();
	// (Copy does nothing: it's disabled.)
	Show(0);
	m_bSlideIn = FALSE;
}

// ---- CSoundTrackOption ----

// 0x000F2862
CSoundTrackOption::CSoundTrackOption()
{
	m_bKeyboardOpen = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bChanged = FALSE;
	m_pEditor = NULL;
	m_bEditorOpen = FALSE;
	m_pTrackCopy = NULL;
	m_bTrackCopyOpen = FALSE;
}

// 0x000F1438
CSoundTrackOption::~CSoundTrackOption()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (m_pTrackCopy != NULL)
	{
		delete m_pTrackCopy;
		m_pTrackCopy = NULL;
	}
	if (m_pEditor != NULL)
	{
		delete m_pEditor;
		m_pEditor = NULL;
	}
}

// 0x000F2D4E
HRESULT CSoundTrackOption::Create(CXBFont *font, const wchar_t *title, float x, float y, float width,
                                  float height, int maxVisible)
{
	HRESULT hr = COptionMenu::Create(font, title, x, y, width, height, maxVisible);
	if (SUCCEEDED(hr))
	{
		AddOption(GetString(STR_AUDIO_PLAY), 0);
		AddOption(GetString(STR_COPY), 1);
		AddOption(GetString(STR_EDIT), 2);
		AddOption(GetString(STR_RENAME), 3);
		AddOption(GetString(STR_DELETE), 4);
	}
	return hr;
}

// 0x000F13A0
void CSoundTrackOption::Reopen()
{
	Show(0);
	m_bChanged = FALSE;
	BOOL xboxSoundtrack = m_dwSoundtrack < MUSIC_FOLDER_VALUE;
	SetOptionEnabled(1, xboxSoundtrack);
	SetOptionEnabled(2, xboxSoundtrack);
	SetOptionEnabled(4, xboxSoundtrack);
}

// 0x000F2C07
void CSoundTrackOption::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	ShowTrackMessage(&m_pMessageBox, &m_bMessageShown, m_pFont, text, title, buttons);
}

// 0x000F1D97
void CSoundTrackOption::ShowEditor(DWORD soundtrack)
{
	if (m_pEditor == NULL)
	{
		m_pEditor = new CTrackEditor;
		if (m_pEditor != NULL && FAILED(m_pEditor->Init((CXBFontEx *)m_pFont)))
		{
			delete m_pEditor;
			m_pEditor = NULL;
		}
	}
	if (m_pEditor == NULL)
	{
		m_bEditorOpen = FALSE;
		return;
	}
	m_pEditor->SetSoundtrack(soundtrack);
	m_pEditor->Show();
	m_bEditorOpen = TRUE;
}

// 0x000F27D7
void CSoundTrackOption::ShowTrackCopy(DWORD soundtrack)
{
	if (m_pTrackCopy == NULL)
	{
		m_pTrackCopy = new CTrackCopy;
		if (m_pTrackCopy != NULL && FAILED(m_pTrackCopy->Init((CXBFontEx *)m_pFont)))
		{
			delete m_pTrackCopy;
			m_pTrackCopy = NULL;
		}
	}
	if (m_pTrackCopy == NULL)
	{
		m_bTrackCopyOpen = FALSE;
		return;
	}
	m_pTrackCopy->SetSoundtrack(soundtrack);
	m_pTrackCopy->Show();
	m_bTrackCopyOpen = TRUE;
}

// 0x000F13F3
void CSoundTrackOption::Render()
{
	COptionMenu::Render();
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bEditorOpen)
		m_pEditor->Render();
	else if (m_bTrackCopyOpen)
		m_pTrackCopy->Render();
}

// 0x000F43B4
void CSoundTrackOption::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_bMessageShown)
	{
		// The delete question.
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		if (m_bMessageShown || m_nAction != 4 || m_pMessageBox->GetResult() != MSGBOX_YES)
			return;
		if (!OpenSoundtrackDB())
			return;
		if (DeleteSoundtrack(m_dwSoundtrack))
		{
			// Back to the list, which is filled again.
			m_bChanged = TRUE;
			m_bCancelled = TRUE;
		}
		else
		{
			std::string text = GetStringA(STR_MSG_DELETE_ERROR);
			StrFormat(text, text.c_str(), GetLastError());
			ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		}
		CloseSoundtrackDB();
		return;
	}

	if (m_bKeyboardOpen)
	{
		// The new name.
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return;
		if (!g_virtualKeyboard.m_bCancelled)
		{
			wchar_t name[260];
			wcscpy(name, g_virtualKeyboard.GetText());
			if (m_nAction == 3)
			{
				if (m_dwSoundtrack > MUSIC_FOLDER_VALUE)
				{
					// A music folder: its Info.xml title.
					SSoundtrack *folder = g_pMusicManager->GetUserSoundtrack(m_dwSoundtrack - MUSIC_FOLDER_VALUE - 2);
					if (folder != NULL && folder->isFolder)
					{
						std::string title;
						StrFormat(title, "%S", name);
						if (g_pMusicManager->SetSoundtrackTitle(folder->index, title.c_str()))
							m_bChanged = TRUE;
					}
				}
				else if (OpenSoundtrackDB())
				{
					if (RenameSoundtrack(m_dwSoundtrack, name))
						m_bChanged = TRUE;
					CloseSoundtrackDB();
				}
				else
				{
					// Quirk: the format has "%s to %s ... %d" and gets the
					// old name (a wide string) and the new one only.
					std::string text = GetStringA(STR_MSG_RENAME_ERROR);
					std::string newName = g_virtualKeyboard.GetResult();
					StrFormat(text, text.c_str(), (const char *)m_strTitle.c_str(), newName.c_str());
					ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
				}
			}
		}
		if (g_virtualKeyboard.m_bNumeric != 0)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = 0;
		return;
	}

	if (m_bEditorOpen)
	{
		m_pEditor->ProcessInput(action, pad);
		m_bEditorOpen = m_pEditor->IsOpen();
		return;
	}
	if (m_bTrackCopyOpen)
	{
		m_pTrackCopy->ProcessInput(action, pad);
		m_bTrackCopyOpen = m_pTrackCopy->IsOpen();
		return;
	}

	COptionMenu::ProcessInput(action, pad);
	if (IsOpen() || !m_bChosen)
		return;
	m_nAction = GetSelection();
	std::string text;
	switch (m_nAction)
	{
	case 0:
	{
		// Play.
		HRESULT hr;
		if (m_dwSoundtrack < MUSIC_FOLDER_VALUE)
		{
			hr = g_pMusicManager->SelectSoundtrack(m_dwSoundtrack);
		}
		else
		{
			SSoundtrack *folder = g_pMusicManager->GetUserSoundtrack(m_dwSoundtrack - MUSIC_FOLDER_VALUE - 2);
			if (folder == NULL || !folder->isFolder)
				break;
			hr = g_pMusicManager->LoadSoundtrack(folder->index, FALSE);
		}
		if (SUCCEEDED(hr))
			g_pMusicManager->Play();
		break;
	}
	case 1:
		ShowTrackCopy(m_dwSoundtrack);
		break;
	case 2:
		ShowEditor(m_dwSoundtrack);
		break;
	case 3:
		OpenTrackKeyboard(GetString(STR_RENAME), m_strTitle.c_str(), FALSE, 0x20, 0, &m_bKeyboardOpen);
		break;
	case 4:
	{
		char name[257];
		StrWideToUtf8(m_strTitle.c_str(), name, 0x100);
		StrFormat(text, GetStringA(STR_MSG_DELETEST_ASK), name);
		ShowMessage(text.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
		if (m_pMessageBox != NULL)
			m_pMessageBox->SetSelection(1);
		break;
	}
	}
	Show(0);
	m_bSlideIn = FALSE;
}

// ---- CTrackManager ----

// 0x000F0E52
CTrackManager::~CTrackManager()
{
}
