// The main menu: the <Menu> tree from Config.xml, drawn by the skin. Only
// what other modules use so far. TODO: decompile (0x0010CBF9-0x001123B9).
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

class CXmlElement;
struct IDirect3DTexture8;

// Callbacks from CMenu (CUnleashX::CMenuEvent implements them).
class CMenuEventSink
{
public:
	virtual ~CMenuEventSink() {}                      // slot 0, 0x0002C65F
	// An <Item> (or a <Command>) was chosen. Returns FALSE when the
	// action failed or left the menu (launch, error message).
	virtual BOOL OnItemSelected(CXmlElement *item) { (void)item; return TRUE; }   // slot 1
	virtual BOOL Unknown2() { return TRUE; }          // slot 2, 0x0002B65C
	virtual void Unknown3() {}                        // slot 3, 0x0002B8DA
	virtual void Unknown4() {}                        // slot 4, 0x0002B8DA
};

// Recently launched XBEs, kept in <data path>\recent.dat as 10 of these.
// 0x138 bytes.
struct SRecentItem
{
	char name[0x2B];         // +0x000
	char path[0x105];        // +0x02B
	int launchCount;         // +0x130
	int region;              // +0x134
};

// 0xF44 bytes
class CMenu
{
public:
	CMenu();                                          // 0x00110B3B
	virtual ~CMenu();                                 // 0x0010CF0B

	BYTE m_unknown004[0xDF4];
	IDirect3DTexture8 *m_pIconTexture;                // +0xDF8 icon of the selected item
	BYTE m_unknownDFC[0xF44 - 0xDFC];
};
