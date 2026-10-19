#include "SkinColors.h"

#include <cstdlib>
#include <cstring>

#include "../common/StrUtil.h"
#include "../xml/XmlDocument.h"

SColorScheme g_colorScheme;   // 0x00278680

// 0x000AC201
SColorScheme::~SColorScheme()
{
	if (name != NULL)
	{
		delete[] name;
		name = NULL;
	}
}

// 0x000AC07A
void SColorScheme::SetDefaults()
{
	DIALOG_BACKCOLOR = 0xCD1E34F2;
	BUTTON_BACKCOLOR = 0xCD1E34F2;
	KB_KEY_PRESSED = 0xFF808080;
	KB_KEY_DISABLED = 0xFF808080;
	DIALOG_INNER_BORDERCOLOR = 0xFFFFE2B4;
	BUTTON_BORDERCOLOR = 0xFFFFE2B4;
	KB_KEY_ACTIVEBORDER = 0xFF0000FF;
	KB_KEY_TEXTBORDERCOLOR = 0xFF0000FF;
	DIALOG_BORDERCOLOR = 0xFFFFFFFF;
	DIALOG_TEXTCOLOR = 0xFFFFFFFF;
	OPTION_TEXTCOLOR = 0xFFFFFFFF;
	BUTTON_ACTIVETEXTCOLOR = 0xFFFFFFFF;
	KB_KEY_NORMAL = 0xFF000000;
	KB_KEY_TEXTCOLOR = 0xFF000000;
	KB_KEY_TEXTBACKCOLOR = 0xFFFFFFFF;
	WindowForeCOLOR = 0xFF000000;
	MMActiveForeColor = 0xFFFFFFFF;
	MENU_BORDERCOLOR = 0;
	MENU_BACKCOLOR = 0;
	MENU_ACTIVEBACKCOLOR = 0;
	MENU_ACTIVEBORDERCOLOR = 0;
	AMBIENT_FOGCOLOR = 0xFF000028;
	AMBIENT_SKYCOLOR = 0xFF000040;
	AMBIENT_CELLCOLOR = 0x065A79CE;
	CAPTION_TEXTCOLOR = 0xFFFFFF40;
	CAPTION_BACKCOLOR = 0xFF4274FF;
	DIALOG_INNER_BACKCOLOR = 0xE1003399;
	DIALOG_DISABLEDTEXT = 0x7F0000C0;
	OPTION_ACTIVETEXTCOLOR = 0xFF40FFFF;
	OPTION_ACTIVEBACKCOLOR = 0xFF000080;
	BUTTON_TEXTCOLOR = 0xFF993300;
	BUTTON_ACTIVEBACKCOLOR = 0xFF003399;
	KB_KEYBG_HIGHLIGHT = 0xCC0000C0;
	KB_KEYBG_DISABLED = 0xFFC0C0C0;
	MENU_TEXTCOLOR = 0x99FFFFFF;
	MENU_ACTIVETEXTCOLOR = 0xFFFBFA8F;
	WindowBackCOLOR = 0xFFECECEC;
	MM_BORDERCOLOR = 0xFF000080;
	MMLightBackColor = 0xFF3B5A92;
	MMDarkBackColor = 0xFF000080;
	MMLightBarColor = 0xFF3B5A92;
	MMDarkBarColor = 0xFF3B5A92;
	MMForeColor = 0xFFF2F2F2;
	MMOffForeColor = 0x19F2F2F2;
	MMBarBackColor = 0xFF000080;
}

// 0x000AC19C
void SColorScheme::SetName(const char *newName)
{
	if (name != NULL)
	{
		delete[] name;
		name = NULL;
	}
	if (newName == NULL || strlen(newName) == 0)
		return;
	int len = (int)strlen(newName);
	name = new wchar_t[len + 1];
	if (name != NULL)
		MultiByteToWideChar(CP_ACP, 0, newName, len, name, len);   // note: not terminated
}

// 0x000AD398
void SColorScheme::Load(CXmlDocument *doc)
{
	static const struct
	{
		const char *key;
		DWORD SColorScheme::*color;
	} kColors[] = {
		{"AMBIENT_FOGCOLOR", &SColorScheme::AMBIENT_FOGCOLOR},
		{"AMBIENT_SKYCOLOR", &SColorScheme::AMBIENT_SKYCOLOR},
		{"CAPTION_TEXTCOLOR", &SColorScheme::CAPTION_TEXTCOLOR},
		{"CAPTION_BACKCOLOR", &SColorScheme::CAPTION_BACKCOLOR},
		{"DIALOG_BORDERCOLOR", &SColorScheme::DIALOG_BORDERCOLOR},
		{"DIALOG_BACKCOLOR", &SColorScheme::DIALOG_BACKCOLOR},
		{"DIALOG_INNER_BORDERCOLOR", &SColorScheme::DIALOG_INNER_BORDERCOLOR},
		{"DIALOG_INNER_BACKCOLOR", &SColorScheme::DIALOG_INNER_BACKCOLOR},
		{"DIALOG_TEXTCOLOR", &SColorScheme::DIALOG_TEXTCOLOR},
		{"DIALOG_DISABLEDTEXT", &SColorScheme::DIALOG_DISABLEDTEXT},
		{"OPTION_TEXTCOLOR", &SColorScheme::OPTION_TEXTCOLOR},
		{"OPTION_ACTIVETEXTCOLOR", &SColorScheme::OPTION_ACTIVETEXTCOLOR},
		{"OPTION_ACTIVEBACKCOLOR", &SColorScheme::OPTION_ACTIVEBACKCOLOR},
		{"BUTTON_BORDERCOLOR", &SColorScheme::BUTTON_BORDERCOLOR},
		{"BUTTON_TEXTCOLOR", &SColorScheme::BUTTON_TEXTCOLOR},
		{"BUTTON_BACKCOLOR", &SColorScheme::BUTTON_BACKCOLOR},
		{"BUTTON_ACTIVETEXTCOLOR", &SColorScheme::BUTTON_ACTIVETEXTCOLOR},
		{"BUTTON_ACTIVEBACKCOLOR", &SColorScheme::BUTTON_ACTIVEBACKCOLOR},
		{"KB_KEYBG_HIGHLIGHT", &SColorScheme::KB_KEYBG_HIGHLIGHT},
		{"KB_KEYBG_DISABLED", &SColorScheme::KB_KEYBG_DISABLED},
		{"KB_KEY_PRESSED", &SColorScheme::KB_KEY_PRESSED},
		{"KB_KEY_NORMAL", &SColorScheme::KB_KEY_NORMAL},
		{"KB_KEY_DISABLED", &SColorScheme::KB_KEY_DISABLED},
		{"KB_KEY_ACTIVEBORDER", &SColorScheme::KB_KEY_ACTIVEBORDER},
		{"KB_KEY_TEXTCOLOR", &SColorScheme::KB_KEY_TEXTCOLOR},
		{"KB_KEY_TEXTBACKCOLOR", &SColorScheme::KB_KEY_TEXTBACKCOLOR},
		{"KB_KEY_TEXTBORDERCOLOR", &SColorScheme::KB_KEY_TEXTBORDERCOLOR},
		{"MENU_TEXTCOLOR", &SColorScheme::MENU_TEXTCOLOR},
		{"MENU_ACTIVETEXTCOLOR", &SColorScheme::MENU_ACTIVETEXTCOLOR},
		{"MENU_BORDERCOLOR", &SColorScheme::MENU_BORDERCOLOR},
		{"MENU_BACKCOLOR", &SColorScheme::MENU_BACKCOLOR},
		{"MENU_ACTIVEBACKCOLOR", &SColorScheme::MENU_ACTIVEBACKCOLOR},
		{"MENU_ACTIVEBORDERCOLOR", &SColorScheme::MENU_ACTIVEBORDERCOLOR},
		{"AMBIENT_CELLCOLOR", &SColorScheme::AMBIENT_CELLCOLOR},
		{"WindowBackCOLOR", &SColorScheme::WindowBackCOLOR},
		{"WindowForeCOLOR", &SColorScheme::WindowForeCOLOR},
		{"MM_BORDERCOLOR", &SColorScheme::MM_BORDERCOLOR},
		{"MMLightBackColor", &SColorScheme::MMLightBackColor},
		{"MMDarkBackColor", &SColorScheme::MMDarkBackColor},
		{"MMLightBarColor", &SColorScheme::MMLightBarColor},
		{"MMBarBackColor", &SColorScheme::MMBarBackColor},
		{"MMDarkBarColor", &SColorScheme::MMDarkBarColor},
		{"MMForeColor", &SColorScheme::MMForeColor},
		{"MMOffForeColor", &SColorScheme::MMOffForeColor},
		{"MMActiveForeColor", &SColorScheme::MMActiveForeColor},
	};
	// TODO: verify the order of the original's if-chain matches this table
	// (it only matters for duplicate element names).

	if (doc == NULL)
		return;

	SetDefaults();
	doc->m_pCurrent = &doc->m_root;
	CXmlElement *scheme = doc->FindElementRecursive("ColorScheme", &doc->m_root);
	if (scheme == NULL)
		return;

	SetName(scheme->GetAttribute("Name").c_str());
	for (CXmlElement *child = scheme->GetFirstChild(); child != NULL; child = scheme->GetNextChild())
	{
		char *end;
		DWORD color = strtoul(child->m_strValue.c_str(), &end, 16);
		if (color == 0)
			continue;   // 0 can't be set from a skin
		for (size_t i = 0; i < sizeof(kColors) / sizeof(kColors[0]); i++)
		{
			if (StrCmpNoCase(child->m_strName, kColors[i].key) == 0)
			{
				this->*kColors[i].color = color;
				break;
			}
		}
	}

	// Keep a few pairs distinguishable.
	if (KB_KEYBG_HIGHLIGHT == KB_KEYBG_DISABLED)
		KB_KEYBG_DISABLED = KB_KEYBG_DISABLED == 0xFFFFFFFF ? KB_KEYBG_DISABLED - 1 : KB_KEYBG_DISABLED + 1;
	if (DIALOG_TEXTCOLOR == OPTION_ACTIVETEXTCOLOR)
		OPTION_ACTIVETEXTCOLOR = DIALOG_TEXTCOLOR - (DWORD)((double)(DIALOG_TEXTCOLOR & 0xFF000000) * 0.05);
}
