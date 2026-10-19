// One line of the menu: an <Item> or a <List> (sub-menu) with its text and
// icon. 0x48 bytes.
#pragma once

#include <string>

#include "../common/Platform.h"

class CXmlElement;
struct IDirect3DTexture8;

class CMenuItem
{
public:
	// text is UTF-8; iconPath may be NULL.
	CMenuItem(const char *text, const char *iconPath, BOOL isList);   // 0x000220C2
	~CMenuItem();                                                     // 0x00022183

	// Loads m_strIcon into m_pTexture (once), through the shared icon cache.
	void LoadIcon();                                                  // 0x0002220D
	// A text-only entry: neither a sub-menu nor something to run.
	BOOL IsLabel() const { return !m_bIsList && m_pElement == NULL; }   // 0x000221FA

	std::string m_strIcon;          // +0x00 lowercased path, cleared if it can't be loaded
	int m_unknown1C;                // +0x1C
	int m_unknown20;                // +0x20
	BOOL m_unknown24;               // +0x24 1
	BOOL m_bIsList;                 // +0x28
	CXmlElement *m_pElement;        // +0x2C the <Item>/<List>; NULL for labels
	BYTE m_unknown30[0x10];         // +0x30
	wchar_t *m_pszText;             // +0x40 owned
	IDirect3DTexture8 *m_pTexture;  // +0x44 one reference, the cache holds another
};

// Releases the icon cache (CMenu's destructor).
void FreeIconCache();
