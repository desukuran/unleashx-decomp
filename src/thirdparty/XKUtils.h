// XKUtils (Team Assembly's Xbox hardware library: XKEEPROM, XKHDD, XKRC4,
// XKSHA1, XKCRC), linked into UnleashX. Library code, so only what UnleashX
// calls is declared here; see the public XKUtils sources for the rest.
#pragma once

#include "../common/Platform.h"

class XKEEPROM
{
public:
	// Hex strings (MAC as "00-50-F2-..."); length receives the count.
	void GetMACAddressString(char *mac, DWORD *length);       // 0x00091DC1
	void GetSerialNumberString(char *serial, DWORD *length);  // 0x00091DFD
	void GetHDKeyString(char *key, DWORD *length);            // 0x00092007
};

extern XKEEPROM g_eeprom;   // 0x00277840
