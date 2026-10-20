// The parts of XKUtils (Team Assembly) UnleashX calls, for the Xbox build:
// the EEPROM read and getters as the binary has them, and HMAC-SHA1
// (XKSHA1, used for the hard disk lock password). Decrypt is a stand-in: the kernel has already decrypted
// the security section and exports the HDD key, so that is used instead of
// the RC4/HMAC key search.
#include "../src/thirdparty/XKUtils.h"
#include "../src/common/XboxKernel.h"

#include <stdlib.h>
#include <string.h>

XKEEPROM g_eeprom;   // 0x00277840

// ---- SHA-1 (FIPS 180-1) ----

struct SSha1
{
	DWORD state[5];
	DWORD count;        // bytes so far
	BYTE block[64];
};

static DWORD Rol(DWORD x, int n)
{
	return (x << n) | (x >> (32 - n));
}

static void Sha1Block(SSha1 *sha, const BYTE *p)
{
	DWORD w[80];
	for (int i = 0; i < 16; i++)
		w[i] = ((DWORD)p[i * 4] << 24) | ((DWORD)p[i * 4 + 1] << 16) | ((DWORD)p[i * 4 + 2] << 8) | p[i * 4 + 3];
	for (int i = 16; i < 80; i++)
		w[i] = Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	DWORD a = sha->state[0], b = sha->state[1], c = sha->state[2], d = sha->state[3], e = sha->state[4];
	for (int i = 0; i < 80; i++)
	{
		DWORD f, k;
		if (i < 20)
			f = (b & c) | (~b & d), k = 0x5A827999;
		else if (i < 40)
			f = b ^ c ^ d, k = 0x6ED9EBA1;
		else if (i < 60)
			f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
		else
			f = b ^ c ^ d, k = 0xCA62C1D6;
		DWORD t = Rol(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = Rol(b, 30);
		b = a;
		a = t;
	}
	sha->state[0] += a;
	sha->state[1] += b;
	sha->state[2] += c;
	sha->state[3] += d;
	sha->state[4] += e;
}

static void Sha1Init(SSha1 *sha)
{
	sha->state[0] = 0x67452301;
	sha->state[1] = 0xEFCDAB89;
	sha->state[2] = 0x98BADCFE;
	sha->state[3] = 0x10325476;
	sha->state[4] = 0xC3D2E1F0;
	sha->count = 0;
}

static void Sha1Update(SSha1 *sha, const BYTE *data, int length)
{
	for (int i = 0; i < length; i++)
	{
		sha->block[sha->count % 64] = data[i];
		sha->count++;
		if (sha->count % 64 == 0)
			Sha1Block(sha, sha->block);
	}
}

static void Sha1Final(SSha1 *sha, BYTE *digest)
{
	DWORD bits = sha->count * 8;
	BYTE pad = 0x80;
	Sha1Update(sha, &pad, 1);
	pad = 0;
	while (sha->count % 64 != 56)
		Sha1Update(sha, &pad, 1);
	BYTE length[8] = {0, 0, 0, 0, (BYTE)(bits >> 24), (BYTE)(bits >> 16), (BYTE)(bits >> 8), (BYTE)bits};
	Sha1Update(sha, length, 8);
	for (int i = 0; i < 20; i++)
		digest[i] = (BYTE)(sha->state[i / 4] >> (24 - (i % 4) * 8));
}

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

// 0x000900DB: the standard HMAC (keys up to 64 bytes) of text1 + text2.
void XKSHA1::HMAC_SHA1(BYTE *result, const BYTE *key, int keyLength, const BYTE *text1, int length1, const BYTE *text2,
                       int length2)
{
	BYTE pad[64];
	BYTE inner[20];
	SSha1 sha;
	for (int i = 0; i < 64; i++)
		pad[i] = (BYTE)((i < keyLength ? key[i] : 0) ^ 0x36);
	Sha1Init(&sha);
	Sha1Update(&sha, pad, 64);
	if (text1 != NULL)
		Sha1Update(&sha, text1, length1);
	if (text2 != NULL)
		Sha1Update(&sha, text2, length2);
	Sha1Final(&sha, inner);
	for (int i = 0; i < 64; i++)
		pad[i] = (BYTE)((i < keyLength ? key[i] : 0) ^ 0x5C);
	Sha1Init(&sha);
	Sha1Update(&sha, pad, 64);
	Sha1Update(&sha, inner, 20);
	Sha1Final(&sha, result);
}
