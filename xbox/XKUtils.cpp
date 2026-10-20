// The parts of XKUtils (Team Assembly) UnleashX calls, for the Xbox build:
// the EEPROM read and getters as the binary has them, HMAC-SHA1 from the
// XKSHA1 in reference/thirdparty/xbmc4xbox (compiled as XbmcSHA1, see
// Makefile.xbox). Decrypt is a stand-in: the kernel has already decrypted
// the security section and exports the HDD key, so that is used instead of
// the RC4/HMAC key search.
#include "../src/thirdparty/XKUtils.h"
#include "../src/common/XboxKernel.h"

#include <stdlib.h>
#include <string.h>

XKEEPROM g_eeprom;   // 0x00277840

class XbmcSHA1
{
public:
	XbmcSHA1();
	~XbmcSHA1();
	void HMAC_SHA1(UCHAR *result, UCHAR *key, int keyLength, UCHAR *text1, int length1, UCHAR *text2, int length2);
	BYTE m_state[0x200];   // more than the real class needs
};

// 0x00091863
XKEEPROM::XKEEPROM()
{
	memset(m_data, 0, sizeof(m_data));
	m_dwXboxVersion = 0xA;
	m_bEncrypted = FALSE;
	m_bRead = FALSE;
}

// 0x0008F971: bytes first..end-1 over SMBus, 10 ms apart.
static void ReadEEPROMFromXBOX(BYTE *data, BYTE end, BYTE first)
{
	memset(data, 0, 0x100);
	for (BYTE i = first; i < end; i++)
	{
		// The kernel stores a ULONG: the last reads spill into +0x100
		// (m_dwXboxVersion).
		HalReadSMBusValue(0xA8, i, FALSE, (ULONG *)(data + i));
		Sleep(10);
	}
}

// 0x000915BF / 0x0009154C: lowercase hex, two digits a byte, with the
// separator between bytes if there is one.
static void BytesToHexStr(const BYTE *bytes, DWORD count, char *out, char separator)
{
	int stride = separator != 0 ? 3 : 2;
	for (DWORD i = 0; i < count; i++)
	{
		char *p = out + i * stride;
		if (bytes[i] <= 0xF)
			*p++ = '0';
		_itoa(bytes[i], p, 16);
	}
	if (separator != 0)
		for (DWORD i = 0; i + 1 < count; i++)
			out[i * 3 + 2] = separator;
}

// 0x0009171D. Quirk: byte 0xFF isn't read.
void XKEEPROM::ReadFromXBOX()
{
	ReadEEPROMFromXBOX(m_data, 0xFF, 0);
	m_bEncrypted = TRUE;
	m_bRead = TRUE;
}

// 0x00091AE6 (stand-in, see above)
void XKEEPROM::Decrypt()
{
	memcpy(m_data + 0x1C, XboxHDKey, 16);
	m_bEncrypted = FALSE;
}

// 0x00091D29
int XKEEPROM::GetDVDRegion()
{
	if (!m_bRead)
		return 0;
	if (m_bEncrypted)
		Decrypt();
	return m_data[0xBC];
}

// 0x00091DC1
void XKEEPROM::GetMACAddressString(char *mac, DWORD *length)
{
	if (!m_bRead)
		return;
	if (m_bEncrypted)
		Decrypt();
	BytesToHexStr(m_data + 0x40, 6, mac, '-');
	*length = 6;
}

// 0x00091DFD: not NUL-terminated by itself.
void XKEEPROM::GetSerialNumberString(char *serial, DWORD *length)
{
	if (!m_bRead)
		return;
	if (m_bEncrypted)
		Decrypt();
	strncpy(serial, (const char *)m_data + 0x34, 12);
	*length = 12;
}

// 0x00092007: the original encrypts the image again afterwards if it was
// encrypted; the stand-in Decrypt leaves nothing to undo.
void XKEEPROM::GetHDKeyString(char *key, DWORD *length)
{
	if (!m_bRead)
		return;
	if (m_bEncrypted)
		Decrypt();
	BytesToHexStr(m_data + 0x1C, 16, key, 0);
	*length = 16;
}

// 0x000900DB
void XKSHA1::HMAC_SHA1(BYTE *result, const BYTE *key, int keyLength, const BYTE *text1, int length1, const BYTE *text2,
                       int length2)
{
	XbmcSHA1 sha;
	sha.HMAC_SHA1(result, (UCHAR *)key, keyLength, (UCHAR *)text1, length1, (UCHAR *)text2, length2);
}
