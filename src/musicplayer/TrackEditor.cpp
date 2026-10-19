// CTrackEditor, the song editor of an Xbox soundtrack, and the button row
// it shares with CTrackCopy. See TrackOptions.h.
#include "TrackOptions.h"

#include <cwchar>

#include "../audio/SoundtrackDB.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../ui/VirtualKeyboard.h"

// The song list's box in skin coordinates (CTrackEditor and CTrackCopy).
#define TRACK_LIST_X 120.0f
#define TRACK_LIST_Y 60.0f
#define TRACK_LIST_WIDTH 400.0f
#define TRACK_LIST_HEIGHT 300.0f

// The frame and the row of buttons under the song list (inlined in each
// Render). (Quirk: an unselected label is drawn in the dialog text color,
// not the button text color.)
void DrawTrackButtons(float x, float y, float width, float height, float scaleY, CXBFont *font,
                      IDirect3DTexture8 *panel, const wchar_t *const *labels, UINT count, UINT selected)
{
	float rowHeight = font->m_fScaleFactorY * font->m_fFontHeight + scaleY * 28.0f;
	DrawBevelBox(x, y, width, rowHeight + height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 2);
	float rowY = y + height;
	DrawPanelTexture(panel, x, rowY, width, rowHeight);
	float buttonWidth = ((width - 20.0f) - (float)(10 * (count - 1))) * (1.0f / (float)count);
	float buttonHeight = rowHeight - scaleY * 20.0f;
	float textWidth = buttonWidth - 4.0f;
	float buttonY = scaleY * 10.0f + rowY;
	float textY = buttonHeight * 0.5f + buttonY;
	float buttonX = x + 10.0f;
	for (UINT i = 0; i < count; i++)
	{
		BOOL isSelected = selected == i;
		DrawButtonBox(buttonX, buttonY, buttonWidth, buttonHeight, g_colorScheme.BUTTON_BORDERCOLOR,
		              isSelected ? g_colorScheme.BUTTON_ACTIVEBACKCOLOR : g_colorScheme.BUTTON_BACKCOLOR, 1);
		font->DrawText(buttonWidth * 0.5f + buttonX, textY,
		               isSelected ? g_colorScheme.BUTTON_ACTIVETEXTCOLOR : g_colorScheme.DIALOG_TEXTCOLOR, labels[i],
		               XBFONT_CENTER_X | XBFONT_CENTER_Y | XBFONT_TRUNCATED, textWidth);
		buttonX = buttonWidth + 10.0f + buttonX;
	}
}

// The song list over the screen, laid out on the skin's dialog background
// (or the built-in panel). (Inlined in each Show.)
void LayoutTrackScreen(CSongListControl *list, float *x, float *y, float *width, float *height, float *scaleX,
                       float *scaleY)
{
	if (g_skin.m_pDialogBackground != NULL)
		list->SetLayout(g_skin.m_pDialogBackground, 0.0f, 0.0f, 0.0f, 0.0f, (float)(DWORD)g_skin.m_nDialogOffsetX,
		                (float)(DWORD)g_skin.m_nDialogOffsetY, (float)(DWORD)g_skin.m_nDialogOffsetX2,
		                (float)(DWORD)g_skin.m_nDialogOffsetY2);
	else
		list->SetLayout(NULL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f);
	*x = TRACK_LIST_X;
	*y = TRACK_LIST_Y;
	*width = TRACK_LIST_WIDTH;
	*height = TRACK_LIST_HEIGHT;
	*scaleY = 1.0f;
	*scaleX = 1.0f;
	ScaleToScreen(x, scaleX, y, scaleY, FALSE);
	*width = *scaleX * *width;
	*height = *height * *scaleY;
}

// (The original's Init, inlined in each: the list's box and the button
// row's texture, then Show.)
void InitTrackScreen(CSongListControl *list, CXBFont *font, IDirect3DTexture8 **panel)
{
	list->Init(font, TRACK_LIST_X, TRACK_LIST_Y, TRACK_LIST_WIDTH, TRACK_LIST_HEIGHT);
	list->m_bMusicManager = FALSE;
	if (*panel == NULL)
		g_textures.Find("Keyboard", panel);
}

// 0x000F1B8C
CTrackEditor::CTrackEditor()
{
	m_pFont = NULL;
	m_pPanelTexture = NULL;
	m_nButton = 0;
	m_dwSoundtrack = 0;
	m_bKeyboardOpen = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bDeletePending = FALSE;
}

// 0x000F1B5A
CTrackEditor::~CTrackEditor()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
}

// 0x000F21D6
BOOL CTrackEditor::Init(CXBFontEx *font)
{
	m_pFont = (CXBFont *)font;
	InitTrackScreen(&m_songList, m_pFont, &m_pPanelTexture);
	Show();
	return S_OK;
}

// 0x000F2094
void CTrackEditor::Show()
{
	m_bOpen = TRUE;
	LayoutTrackScreen(&m_songList, &m_fX, &m_fY, &m_fWidth, &m_fHeight, &m_fScaleX, &m_fScaleY);
}

// 0x000F123A
void CTrackEditor::SetSoundtrack(DWORD soundtrack)
{
	m_dwSoundtrack = soundtrack;
	m_songList.SetSoundtrack((int)soundtrack);
}

// 0x000F3280
void CTrackEditor::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	ShowTrackMessage(&m_pMessageBox, &m_bMessageShown, m_pFont, text, title, buttons);
}

// 0x000F4B99
BOOL CTrackEditor::Render()
{
	const wchar_t *labels[4] = {GetString(STR_MOVE_UP), GetString(STR_MOVE_DOWN), GetString(STR_RENAME),
	                            GetString(STR_DELETE)};
	DrawTrackButtons(m_fX, m_fY, m_fWidth, m_fHeight, m_fScaleY, m_pFont, m_pPanelTexture, labels, 4, m_nButton);
	m_songList.Render();
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bMessageShown)
		m_pMessageBox->Render();
	return S_OK;
}

// 0x000F33C4
HRESULT CTrackEditor::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_bKeyboardOpen)
	{
		// The song's new name.
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen || g_virtualKeyboard.m_bCancelled)
			return S_OK;
		wchar_t name[260];
		wcscpy(name, g_virtualKeyboard.GetText());
		if (!OpenSoundtrackDB())
			return S_OK;
		if (RenameSong(m_dwSoundtrack, m_songList.m_nSelected, name))
		{
			CloseSoundtrackDB();
			m_songList.Reload();
			return S_OK;
		}
		// Quirk: both names are wide strings for "%s".
		std::string text = StrFromWide(GetString(STR_MSG_RENAME_ERROR));
		DWORD error = GetLastError();
		StrFormat(text, text.c_str(), (const char *)m_songList.GetSelectedName(), (const char *)name, error);
		ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		CloseSoundtrackDB();
		return S_OK;
	}

	if (m_bMessageShown)
	{
		// The delete question.
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		if (m_bMessageShown || !m_bDeletePending || m_pMessageBox->GetResult() != MSGBOX_YES)
			return S_OK;
		m_bDeletePending = FALSE;
		if (!OpenSoundtrackDB())
			return S_OK;
		if (DeleteSong(m_dwSoundtrack, m_songList.m_nSelected))
		{
			CloseSoundtrackDB();
			UINT selected = m_songList.m_nSelected;
			m_songList.SetSelection(selected != 0 ? selected - 1 : 0);
			m_songList.Reload();
			return S_OK;
		}
		std::string text = GetStringA(STR_MSG_DELETE_ERROR);
		StrFormat(text, text.c_str(), GetLastError());
		ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		CloseSoundtrackDB();
		return S_OK;
	}

	m_songList.ProcessInput(action, pad);
	switch (*action)
	{
	case INPUT_NONE:
		return S_OK;
	case INPUT_A:
		switch (m_nButton)
		{
		case 0:
		case 1:
		{
			// Move Up / Move Down: swap with the song above or below.
			UINT selected = m_songList.m_nSelected;
			UINT other;
			if (m_nButton == 0)
			{
				if (selected == 0 || !OpenSoundtrackDB())
					break;
				other = selected - 1;
			}
			else
			{
				if (selected + 1 >= m_songList.m_items.size() || !OpenSoundtrackDB())
					break;
				other = selected + 1;
			}
			if (SwapSongs(m_dwSoundtrack, selected, other))
			{
				m_songList.SetSelection(other);
				m_songList.Reload();
			}
			CloseSoundtrackDB();
			break;
		}
		case 2:
			OpenTrackKeyboard(GetString(STR_RENAME), m_songList.GetSelectedName(), FALSE, 0x1F, 0, &m_bKeyboardOpen);
			break;
		case 3:
		{
			std::string text;
			char name[257];
			StrWideToUtf8(m_songList.GetSelectedName(), name, 0x100);
			StrFormat(text, GetStringA(STR_MSG_DELETEST_ASK), name);
			ShowMessage(text.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
			if (m_pMessageBox != NULL)
			{
				m_bDeletePending = TRUE;
				m_pMessageBox->SetSelection(1);
			}
			break;
		}
		}
		break;
	case INPUT_B:
	case INPUT_BACK:
		m_bOpen = FALSE;
		break;
	case INPUT_LEFT:
		if (m_nButton > 0)
			m_nButton--;
		break;
	case INPUT_RIGHT:
		if (m_nButton < 3)
			m_nButton++;
		break;
	}
	*action = INPUT_NONE;
	return S_OK;
}
