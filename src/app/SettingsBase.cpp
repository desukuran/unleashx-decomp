// The settings page base class: layout, the item list, scrolling and the
// "save changes?" question. The pages derive from it (Settings*.cpp).
#include "SettingsBase.h"

#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "../xml/XmlUtil.h"
#include "Screens.h"

// 0x0002DD70
BOOL IsYesW(const wchar_t *text)
{
	if (text == NULL)
		return FALSE;
	return _wcsicmp(L"Yes", text) == 0 || _wcsicmp(L"True", text) == 0 || _wcsicmp(L"1", text) == 0;
}

// 0x0002039A
BOOL IsNoW(const wchar_t *text)
{
	if (text == NULL)
		return FALSE;
	return _wcsicmp(L"No", text) == 0 || _wcsicmp(L"False", text) == 0 || _wcsicmp(L"0", text) == 0;
}

// 0x000C5E51
BOOL GetOrAddChildElement(const char *name, CXmlElement *parent, CXmlElement **element)
{
	if (parent == NULL)
		return FALSE;
	if (*element == NULL)
	{
		AddChildElement(parent, name, NULL, NULL);
		*element = parent->FindFirstChild(name);
	}
	return *element != NULL;
}

// The height of a line of text in the page's font.
static float LineHeight(const CXBFont *font)
{
	return font->m_fFontHeight * font->m_fScaleFactorY;
}

// 0x000203D8
CSettingItem::CSettingItem(std::wstring label, std::wstring value, int type, BOOL header, BOOL enabled,
                           BOOL visible, int value2, BOOL password)
	: m_strLabel(label), m_strValue(value)
{
	m_bHeader = header;
	m_bEnabled = enabled;
	m_nType = type;
	m_bVisible = visible;
	m_nValue = value2;
	m_strSavedValue = value;
	m_nSavedValue = m_nValue;
	m_bPassword = password;
	m_bChanged = FALSE;
	if (m_bHeader)
		m_bEnabled = FALSE;
}

// 0x000C00A2
void CSettingItem::SetValue(const wchar_t *text)
{
	if (text != NULL && m_strValue != text)
		m_strValue = text;
}

// 0x000C05DD
CSettingsBase::CSettingsBase()
{
	m_pFont = NULL;
	m_pSoundMove = NULL;
	m_pSoundSelect = NULL;
	m_nSelected = 0;
	m_nFirstVisible = 0;
	m_nVisibleCount = 0;
	m_nLastVisible = 0;
	m_bClosed = FALSE;
	m_bMessageBoxOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bAskSave = FALSE;
	m_pMessageBox = NULL;
	m_bModified = FALSE;
	m_pIPEditor = NULL;
	m_bIPEditorOpen = FALSE;
	m_pClockEditor = NULL;
	m_bClockEditorOpen = FALSE;
	m_bFileBrowserOpen = FALSE;
	m_pFileBrowser = NULL;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_bSelectionMoved = FALSE;
	m_fLabelWidth = 0.0f;
	m_pBackground = NULL;
	// m_szTitle is left as it is.
}

// 0x000C187F
CSettingsBase::~CSettingsBase()
{
	m_items.clear();
	FreePopups();
	m_items.clear();
}

// 0x000BEBB2
HRESULT CSettingsBase::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont == NULL)
		m_pFont = font;
	return S_OK;
}

// 0x000204BF
BOOL CSettingsBase::IsOpen()
{
	return !m_bClosed;
}

// 0x000204C8
void CSettingsBase::SetItemEnabled(int index, BOOL enabled)
{
	if (!m_items[index].m_bHeader)
		m_items[index].m_bEnabled = enabled;
}

// 0x00020499
void CSettingsBase::SetTitle(const wchar_t *title)
{
	memset(m_szTitle, 0, sizeof(m_szTitle));
	if (title != NULL)
		wcsncpy(m_szTitle, title, 29);
}

// 0x000C17F0
void CSettingsBase::LoadSounds()
{
	m_pSoundMove = g_soundEffects.Find("Scroll");
	m_pSoundSelect = g_soundEffects.Find("Select");
}

// 0x0002DDFC. Disabled lines can be selected; headers and hidden lines can't.
BOOL CSettingsBase::IsSelectedSelectable()
{
	CSettingItem item = m_items[m_nSelected];
	return !item.m_bHeader && item.m_bVisible;
}

// 0x00020455
BOOL CSettingsBase::IsSelectedDisabled()
{
	CSettingItem item = m_items[m_nSelected];
	return !item.m_bEnabled || item.m_bHeader || !item.m_bVisible;
}

// 0x000C004C
void CSettingsBase::SetItemValue(CSettingItem *item, const wchar_t *text, BOOL changed)
{
	if (item == NULL || text == NULL || item->m_strValue == text)
		return;
	// Quirk: the original assigns a _wcsdup() copy of the text and never
	// frees it.
	if (wcslen(text) != 0)
		item->m_strValue = text;
	else
		item->m_strValue.clear();
	if (changed)
	{
		item->m_bChanged = TRUE;
		m_bModified = TRUE;
	}
	else
	{
		item->m_bChanged = FALSE;
	}
}

// 0x000BF231. A line that wasn't changed takes its current value as the
// saved one; a changed line goes back to the saved value.
void CSettingsBase::RevertChanges()
{
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		if (!it->m_bChanged)
		{
			it->m_nSavedValue = it->m_nValue;
			if (it->m_strSavedValue != it->m_strValue)
				it->m_strSavedValue = it->m_strValue;
		}
		else
		{
			it->m_strValue = it->m_strSavedValue;
			it->m_bChanged = FALSE;
			it->m_nValue = it->m_nSavedValue;
		}
	}
	m_bModified = FALSE;
}

// 0x000BEEFA
void CSettingsBase::ClearChanged()
{
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end(); ++it)
	{
		if (it->m_bChanged)
			it->m_bChanged = FALSE;
	}
}

// 0x000C1816
void CSettingsBase::FreePopups()
{
	if (m_pClockEditor != NULL)
	{
		delete m_pClockEditor;
		m_pClockEditor = NULL;
	}
	if (m_pFileBrowser != NULL)
	{
		delete m_pFileBrowser;
		m_pFileBrowser = NULL;
	}
	if (m_pIPEditor != NULL)
	{
		delete m_pIPEditor;
		m_pIPEditor = NULL;
	}
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
}

// 0x000BF2B5. The page's icon, faint, behind the list.
void CSettingsBase::DrawBackground()
{
	DrawTexture(m_pBackground, m_fX + 12.0f, m_fY + 12.0f, m_fWidth - 24.0f, m_fHeight - 24.0f, 0.12f, NULL,
	            0.0f, NULL);
}

// 0x000C16C8
void CSettingsBase::Reset()
{
	m_fLabelWidth = 0.0f;
	m_fY = 30.0f;
	m_bModified = FALSE;
	m_bMessageBoxOpen = FALSE;
	m_bKeyboardOpen = FALSE;
	m_bAskSave = FALSE;
	m_bClosed = FALSE;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_pFont->SetScaleFactors(g_pSettings->m_fUnknown131C, g_pSettings->m_fUnknown131C);

	// Shrink the box to the list: caption, the lines and some margin.
	float lineHeight = LineHeight(m_pFont);
	float height = (float)(int)(m_items.size() + 3) * (lineHeight + 2.0f) + lineHeight * 0.5f +
	               lineHeight * 0.5f + lineHeight + 24.0f;
	if (height < m_fHeight)
		m_fHeight = height;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);

	m_bClosed = FALSE;
	RevertChanges();
	LoadSounds();
}

// 0x000C5358
void CSettingsBase::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->AddButton(GetString(STR_OK));
			m_pMessageBox->AddButton(GetString(STR_CANCEL));
			m_pMessageBox->AddButton(GetString(STR_ABORT));
			m_pMessageBox->AddButton(GetString(STR_RETRY));
			m_pMessageBox->AddButton(GetString(STR_IGNORE));
			m_pMessageBox->AddButton(GetString(STR_YES));
			m_pMessageBox->AddButton(GetString(STR_NO));
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageBoxOpen = TRUE;
	}
}

// 0x000C54E0
void CSettingsBase::ShowSavedMessage()
{
	ShowMessage(GetStringA(STR_MSG_SETTING_SUCCESS), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
}

// 0x000C54FD
void CSettingsBase::ShowRestartMessage()
{
	ShowMessage(GetStringA(STR_MSG_SETTING_RESTART), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
}

// 0x000C5499
void CSettingsBase::AskToSave()
{
	if (!m_bModified)
	{
		m_bClosed = TRUE;
		return;
	}
	m_bAskSave = TRUE;
	ShowMessage(GetStringA(STR_MSG_ASK_SAVE), L"Settings", MSGBOX_BUTTONS_YESNOCANCEL);
	if (m_pMessageBox != NULL)
		m_pMessageBox->SetSelection(1);
}

// 0x000C5BB0. Returns S_FALSE when the page is closed or "Yes, save" was
// chosen (m_bAskSave is then still set), E_FAIL when the input was used
// here, and S_OK when the page should handle it.
HRESULT CSettingsBase::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	// The left stick, when pushed all the way, works like the d-pad.
	if (*action == INPUT_LTHUMB && pad != NULL)
	{
		if (*pad != NULL)
		{
			if ((*pad)->fY1 == 1.0f)
				*action = INPUT_UP;
			else if ((*pad)->fY1 == -1.0f)
				*action = INPUT_DOWN;
		}
		if (*pad != NULL)
		{
			if ((*pad)->fX1 == 1.0f)
				*action = INPUT_RIGHT;
			else if ((*pad)->fX1 == -1.0f)
				*action = INPUT_LEFT;
		}
	}

	if (m_bMessageBoxOpen)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageBoxOpen = m_pMessageBox->IsOpen();
		*action = INPUT_NONE;
		if (!m_bAskSave || m_bMessageBoxOpen)
			return E_FAIL;
		if (m_pMessageBox->GetResult() == MSGBOX_YES)
		{
			m_bClosed = FALSE;
			m_bAskSave = TRUE;
			PlaySound(m_pSoundSelect);
			return S_FALSE;
		}
		int result = m_pMessageBox->GetResult();
		m_bAskSave = FALSE;
		if (result == MSGBOX_NO)
		{
			// Close without saving.
			m_bClosed = TRUE;
			m_bModified = FALSE;
			PlaySound(m_pSoundSelect);
			return E_FAIL;
		}
		m_bClosed = FALSE;
		return E_FAIL;
	}

	BOOL moved = FALSE;
	switch (*action)
	{
	case INPUT_B:
	case INPUT_BACK:
		AskToSave();
		*action = INPUT_NONE;
		return E_FAIL;

	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (m_items.size() == 0)
			break;
		if (m_nSelected > 0)
		{
			// Up to the previous line that can be selected (or the first).
			int tries = 0;
			do
			{
				if (m_nSelected != 0)
					m_nSelected--;
				if ((int)m_items.size() < tries)
					break;
				tries++;
			} while (!IsSelectedSelectable());
			moved = TRUE;
			PlaySound(m_pSoundMove);
		}
		*action = INPUT_NONE;
		break;

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (m_items.size() == 0)
			break;
		if (m_nSelected < (int)m_items.size() - 1)
		{
			int tries = 0;
			do
			{
				if (m_nSelected == (int)m_items.size() - 1)
					m_nSelected = (int)m_items.size() - 1;
				else
					m_nSelected++;
				if ((int)m_items.size() < tries)
					break;
				tries++;
			} while (!IsSelectedSelectable());
			moved = TRUE;
			PlaySound(m_pSoundMove);
		}
		*action = INPUT_NONE;
		break;
	}

	if (!moved)
	{
		// A and START, the other buttons, and up/down at the ends: the page's.
		// Quirk: up/down at the ends was used here (action cleared) but still
		// returns S_OK.
		m_bSelectionMoved = FALSE;
		return S_OK;
	}

	// Scroll the list so the selection is shown.
	int selected = m_nSelected;
	if (selected < m_nFirstVisible)
	{
		m_nFirstVisible = selected;
		m_nLastVisible = m_nVisibleCount - 1 + selected;
		if (m_nLastVisible > (int)m_items.size() - 1)
		{
			m_nLastVisible = (int)m_items.size() - 1;
			m_nFirstVisible = m_nLastVisible - m_nVisibleCount + 1;
		}
	}
	if (m_nLastVisible < selected)
	{
		m_nLastVisible = selected;
		m_nFirstVisible = selected - m_nVisibleCount + 1;
		if (m_nFirstVisible < 0)
		{
			m_nFirstVisible = 0;
			m_nLastVisible = m_nVisibleCount - 1;
		}
	}
	m_bSelectionMoved = TRUE;
	return E_FAIL;
}

// 0x000C551A
HRESULT CSettingsBase::Render()
{
	if (m_bClosed)
		return S_OK;

	m_pFont->SetScaleFactors(g_pSettings->m_fUnknown131C, g_pSettings->m_fUnknown131C);
	DimScreen(0.3f);
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 1);
	DrawRect(m_fX + 10.0f, LineHeight(m_pFont) + m_fY + 10.0f, m_fWidth - 20.0f,
	         m_fHeight - 20.0f - LineHeight(m_pFont), g_colorScheme.DIALOG_INNER_BORDERCOLOR,
	         g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f, 1);
	DrawBackground();

	// The caption bar and the title.
	float captionHeight = LineHeight(m_pFont) + 10.0f;
	DrawGradientRect(m_fX + 10.0f, m_fY + 2.0f, m_fWidth - 20.0f, captionHeight - 4.0f,
	                 g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE, 0);
	m_pFont->DrawText(m_fX + 12.0f, captionHeight * 0.5f + m_fY + 2.0f, g_colorScheme.CAPTION_TEXTCOLOR, m_szTitle,
	                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 30.0f);

	// The label column is as wide as the widest label (worked out once),
	// at most half the list.
	if (m_fLabelWidth == 0.0f)
	{
		for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end(); ++it)
		{
			float width = m_pFont->GetTextWidth(it->m_strLabel.c_str());
			if (width > m_fLabelWidth)
				m_fLabelWidth = width;
		}
		m_fLabelWidth += 20.0f;
	}
	float x = m_fX + 12.0f;
	float y = LineHeight(m_pFont) * 0.5f + captionHeight + m_fY + 4.0f;
	float rowWidth = m_fWidth - 24.0f;
	if (m_fLabelWidth > rowWidth * 0.5f)
		m_fLabelWidth = rowWidth * 0.5f - 2.0f;
	float valueWidth = rowWidth - m_fLabelWidth;

	m_pFont->Begin();
	if (m_nFirstVisible > (int)m_items.size())
		m_nFirstVisible = 0;
	float bottom = m_fHeight + m_fY - 20.0f - LineHeight(m_pFont);
	float lineHeight = LineHeight(m_pFont);
	// Quirk: one buffer for all the masked values, filled with as many '*'s
	// as the value has characters (at most 30, then unterminated). A shorter
	// password after a longer one shows the longer one's length.
	wchar_t masked[30];
	memset(masked, 0, sizeof(masked));

	int index = m_nFirstVisible;
	for (; index < (int)m_items.size(); index++)
	{
		const CSettingItem &item = m_items[index];
		if (!item.m_bVisible)
			continue;

		if (index == m_nSelected)
		{
			m_pFont->End();
			DrawGradientRect(x - 1.0f, y - LineHeight(m_pFont) * 0.5f, rowWidth, LineHeight(m_pFont),
			                 g_colorScheme.OPTION_ACTIVEBACKCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE,
			                 g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			m_pFont->Begin();
		}

		if (item.m_bHeader)
		{
			m_pFont->DrawText(m_fWidth * 0.5f + m_fX, y, g_colorScheme.CAPTION_TEXTCOLOR | 0xFF000000,
			                  item.m_strLabel.c_str(), XBFONT_CENTER_X | XBFONT_CENTER_Y, rowWidth);
		}
		else if (item.m_bEnabled)
		{
			DWORD color = index == m_nSelected ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.OPTION_TEXTCOLOR;
			m_pFont->DrawText(x, y, color, item.m_strLabel.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fLabelWidth);
			if (item.m_bPassword)
			{
				size_t count = item.m_strValue.size() < 30 ? item.m_strValue.size() : 30;
				for (size_t i = 0; i < count; i++)
					masked[i] = L'*';
				m_pFont->DrawText(m_fLabelWidth + x, y, color, masked, XBFONT_CENTER_Y, valueWidth);
			}
			else
			{
				m_pFont->DrawText(m_fLabelWidth + x, y, color, item.m_strValue.c_str(),
				                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, valueWidth);
			}
		}
		else
		{
			DWORD color = g_colorScheme.DIALOG_DISABLEDTEXT | 0xFFC0C0C0;
			m_pFont->DrawText(x, y, color, item.m_strLabel.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fLabelWidth);
			m_pFont->DrawText(m_fLabelWidth + x, y, color, item.m_strValue.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED,
			                  valueWidth);
		}

		y = lineHeight + 2.0f + y;
		if (y > bottom)
		{
			m_nLastVisible = index;
			m_nVisibleCount = index - m_nFirstVisible + 1;
			break;
		}
	}
	if (m_nLastVisible == 0)
		m_nLastVisible = index;
	m_pFont->End();

	// The open pop-up, if any.
	if (m_bMessageBoxOpen)
		m_pMessageBox->Render();
	else if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bIPEditorOpen)
		m_pIPEditor->Render();
	else if (m_bClockEditorOpen)
		m_pClockEditor->Render();
	else if (m_bFileBrowserOpen)
		m_pFileBrowser->Render();
	return S_OK;
}
