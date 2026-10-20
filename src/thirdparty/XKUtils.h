// XKUtils (Team Assembly's Xbox hardware library: XKEEPROM, XKHDD, XKRC4,
// XKSHA1, XKCRC), linked into UnleashX. Library code, so only what UnleashX
// calls is declared here; see the public XKUtils sources for the rest. The
// Xbox build links xbox/XKUtils.cpp in its place.
#pragma once

#include "../common/Platform.h"

class XKEEPROM
{
public:
	XKEEPROM();                                               // 0x00091863 (this in edx)

	// Hex strings (MAC as "00-50-F2-..."); length receives the count.
	void GetMACAddressString(char *mac, DWORD *length);       // 0x00091DC1
	void GetSerialNumberString(char *serial, DWORD *length);  // 0x00091DFD
	void GetHDKeyString(char *key, DWORD *length);            // 0x00092007
	// The DVD playback region (0 when the EEPROM wasn't read). (this in esi)
	int GetDVDRegion();                                       // 0x00091D29
	// Reads the EEPROM over SMBus. (this in esi)
	void ReadFromXBOX();                                      // 0x0009171D
	void Decrypt();                                           // 0x00091AE6

	// The EEPROM image: the security section (0x14 HMAC, confounder +0x14,
	// HDD key +0x1C, XBE region +0x2C) is RC4-encrypted until Decrypt; the
	// serial number is at +0x34, the MAC at +0x40, the DVD region at +0xBC.
	BYTE m_data[0x100];       // +0x000
	DWORD m_dwXboxVersion;    // +0x100 0xA from the constructor
	BOOL m_bEncrypted;        // +0x104
	BOOL m_bRead;             // +0x108
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
