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
	// The DVD playback region (0 when the EEPROM wasn't read). (this in esi)
	int GetDVDRegion();                                       // 0x00091D29
	// Reads the EEPROM over SMBus. (this in esi)
	void ReadFromXBOX();                                      // 0x0009171D
	void Decrypt();                                           // 0x00091AE6
};

extern XKEEPROM g_eeprom;   // 0x00277840

class XKSHA1
{
public:
	// HMAC-SHA1 of text1 followed by text2 under key (20 bytes into result).
	// (key_length in edi)
	void HMAC_SHA1(BYTE *result, const BYTE *key, int keyLength, const BYTE *text1, int length1, const BYTE *text2,
	               int length2);   // 0x000900DB
};
