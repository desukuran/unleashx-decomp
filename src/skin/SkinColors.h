// Color scheme: the colors the dashboard's own UI (dialogs, buttons, the
// virtual keyboard, the menu, the music player) is drawn with. A skin can
// override them with <ColorScheme Name="..."><MENU_TEXTCOLOR>0xAARRGGBB</...>.
#pragma once

#include "../common/Platform.h"

class CXmlDocument;

// size 0xB8. Member names are the <ColorScheme> element names.
struct SColorScheme
{
	wchar_t *name;                        // +0x00 <ColorScheme Name=...>
	DWORD AMBIENT_FOGCOLOR;               // +0x04
	DWORD AMBIENT_SKYCOLOR;               // +0x08
	DWORD AMBIENT_CELLCOLOR;              // +0x0C
	DWORD CAPTION_TEXTCOLOR;              // +0x10
	DWORD CAPTION_BACKCOLOR;              // +0x14
	DWORD DIALOG_BORDERCOLOR;             // +0x18
	DWORD DIALOG_BACKCOLOR;               // +0x1C
	DWORD DIALOG_INNER_BORDERCOLOR;       // +0x20
	DWORD DIALOG_INNER_BACKCOLOR;         // +0x24
	DWORD DIALOG_TEXTCOLOR;               // +0x28
	DWORD DIALOG_DISABLEDTEXT;            // +0x2C
	DWORD OPTION_TEXTCOLOR;               // +0x30
	DWORD OPTION_ACTIVETEXTCOLOR;         // +0x34
	DWORD OPTION_ACTIVEBACKCOLOR;         // +0x38
	DWORD BUTTON_BORDERCOLOR;             // +0x3C
	DWORD BUTTON_TEXTCOLOR;               // +0x40
	DWORD BUTTON_BACKCOLOR;               // +0x44
	DWORD BUTTON_ACTIVETEXTCOLOR;         // +0x48
	DWORD BUTTON_ACTIVEBACKCOLOR;         // +0x4C
	DWORD KB_KEYBG_HIGHLIGHT;             // +0x50
	DWORD KB_KEYBG_DISABLED;              // +0x54
	DWORD KB_KEY_PRESSED;                 // +0x58
	DWORD KB_KEY_NORMAL;                  // +0x5C
	DWORD KB_KEY_DISABLED;                // +0x60
	DWORD KB_KEY_ACTIVEBORDER;            // +0x64
	DWORD KB_KEY_TEXTCOLOR;               // +0x68
	DWORD KB_KEY_TEXTBACKCOLOR;           // +0x6C
	DWORD KB_KEY_TEXTBORDERCOLOR;         // +0x70
	DWORD MENU_TEXTCOLOR;                 // +0x74
	DWORD MENU_ACTIVETEXTCOLOR;           // +0x78
	DWORD MENU_BORDERCOLOR;               // +0x7C
	DWORD MENU_BACKCOLOR;                 // +0x80
	DWORD MENU_ACTIVEBACKCOLOR;           // +0x84
	DWORD MENU_ACTIVEBORDERCOLOR;         // +0x88
	DWORD WindowBackCOLOR;                // +0x8C
	DWORD WindowForeCOLOR;                // +0x90
	DWORD MM_BORDERCOLOR;                 // +0x94 MM* = music manager / player
	DWORD MMLightBackColor;               // +0x98
	DWORD MMDarkBackColor;                // +0x9C
	DWORD MMLightBarColor;                // +0xA0
	DWORD MMBarBackColor;                 // +0xA4
	DWORD MMDarkBarColor;                 // +0xA8
	DWORD MMForeColor;                    // +0xAC
	DWORD MMOffForeColor;                 // +0xB0
	DWORD MMActiveForeColor;              // +0xB4

	void SetDefaults();                       // 0x000AC07A
	void SetName(const char *name);           // 0x000AC19C
	void Load(CXmlDocument *doc);             // 0x000AD398
	~SColorScheme();                          // 0x000AC201
};

extern SColorScheme g_colorScheme;          // 0x00278680
extern BOOL g_bSkinGameIcon;                // 0x00277E70 <Main><GameIcon Enable=...>

inline void LoadSkinColorScheme(CXmlDocument *doc)
{
	g_colorScheme.Load(doc);
}
