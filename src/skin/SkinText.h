// Values of skin text sources ($IP$, $CPUTemp$, $Newsfeed$, ...; see the
// SkinTextSource IDs in Skin.h) and the labelled lines of the system info
// screen. Values are cached in a map; most are read once, the rest refresh on
// their own timers (drive space 2 s, network 2.5 s, clock 1 s, temperatures
// 5 s, HDD temperature and fan 15 s, memory 2.5 s, song 1 s, news per
// <Newsfeed Interval>).
#pragma once

#include "../common/Platform.h"

// Text-source IDs only used internally (system info screen, related sources).
enum
{
	TEXT_HOMEPATH = 0x0A,         // directory UnleashX runs from
	TEXT_LAUNCHPATH = 0x0B,       // device path of the XBE
	TEXT_APPVERSIONBUILD = 0x10,  // "0.39.0528A Build 584"
	TEXT_KERNELVERSION = 0x11,
	TEXT_SKINNAME = 0x1A,
	TEXT_DVDSERIAL = 0x25,        // "<serial>  FW: <firmware>"
	TEXT_BIOSNAME = 0x36,         // always "Unknown"
	TEXT_SOURCE_COUNT = 0x5A,
};

// The current text for a source, never NULL. The pointer stays valid until
// the source is refreshed.
const wchar_t *GetSkinTextValue(int source);       // 0x001146ED
// Forgets the cached value so it's read again.
void InvalidateSkinText(int source);               // 0x001146A3
// One line of the system info screen, e.g. "IP: 192.168.0.5 DHCP".
// (source in edx)
const wchar_t *GetInfoText(int source);            // 0x00117FB9
// All headlines of the current feed (<Newsfeed Continuous>), or "".
const wchar_t *GetNewsText();                      // 0x00114655

// Set once the news ticker has started (the network thread starts
// downloading feeds then).
extern BOOL g_bNewsStarted;                        // 0x00276244

// Unused (nothing makes one): its constructor and destructor empty the
// text cache; the destructor also frees the news text. (SkinText.cpp)
class CInfoCenter
{
public:
	CInfoCenter();             // 0x0011468E (this in esi)
	virtual ~CInfoCenter();    // 0x00114664 (deleting 0x00021474)
};
