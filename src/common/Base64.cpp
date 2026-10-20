// Base64 for the FTP site passwords. See Base64.h.
#include "Base64.h"

#include <cstring>

static const char s_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";   // 0x00269E98

// 0x0001ECC5: three bytes (count of them real) into four characters
// (0x0001ED46, splitting them into 6-bit values, folded in).
static void EncodeGroup(const unsigned char *in, int count, char *out)
{
	unsigned char b0 = in[0], b1 = count > 1 ? in[1] : 0, b2 = count > 2 ? in[2] : 0;
	out[0] = s_alphabet[b0 >> 2];
	out[1] = s_alphabet[((b0 & 3) << 4) | (b1 >> 4)];
	out[2] = s_alphabet[((b1 & 0xF) << 2) | (b2 >> 6)];
	out[3] = s_alphabet[b2 & 0x3F];
	if (count == 1)
		out[2] = '=';
	if (count == 1 || count == 2)
		out[3] = '=';
}

// 0x0001E977 (with the class's encoder, 0x0001EAE4, folded in). The original
// is a small class with an input and an output buffer: 0x0001EC66 copies the
// input in, 0x0001EBE3 and 0x0001EB9D grow the input and output buffers in
// 4 KB steps, and 0x0001E955 frees both (its destructor); a std::string does
// all of that here.
BOOL Base64Encode(const char *text, std::string &out)
{
	out.clear();
	if (text == NULL)
		return FALSE;
	const unsigned char *in = (const unsigned char *)text;
	size_t length = strlen(text);
	size_t i = 0;
	char group[4];
	for (; i + 3 <= length; i += 3)
	{
		EncodeGroup(in + i, 3, group);
		out.append(group, 4);
	}
	if (i < length)
	{
		EncodeGroup(in + i, (int)(length - i), group);
		out.append(group, 4);
	}
	return TRUE;
}

// 0x0001ED83: 0xFE for anything not in the alphabet, 0xFF for '='; the
// high-bit copies of the letters decode too.
static void BuildDecodeTable(unsigned char *table)
{
	memset(table, 0xFE, 256);
	for (int i = 0; i < 64; i++)
	{
		table[(unsigned char)s_alphabet[i]] = (unsigned char)i;
		table[(unsigned char)s_alphabet[i] | 0x80] = (unsigned char)i;
	}
	table['='] = 0xFF;
	table['=' | 0x80] = 0xFF;
}

// 0x0001ED07 / 0x0001EC93: four values into three bytes. TODO: verify how
// the 0xFE of a stray character combines.
static void DecodeGroup(const unsigned char *v, std::string &out)
{
	char bytes[3];
	bytes[0] = (char)((v[0] << 2) | (v[1] >> 4));
	bytes[1] = (char)((v[1] << 4) | (v[2] >> 2));
	bytes[2] = (char)((v[2] << 6) | v[3]);
	out.append(bytes, 3);
}

// 0x0001E9B4 (with the class's decoder, 0x0001E9F0, folded in)
BOOL Base64Decode(const char *text, std::string &out)
{
	out.clear();
	if (text == NULL)
		return FALSE;
	unsigned char table[256];
	BuildDecodeTable(table);
	// 0x0001EC26: drop tabs, line breaks and spaces (7..13 and ' ').
	std::string clean;
	for (const unsigned char *p = (const unsigned char *)text; *p != 0; p++)
	{
		if (*p < 7 || (*p > 13 && *p != ' '))
			clean.append(1, (char)*p);
	}
	size_t i = 0;
	unsigned char v[4];
	for (; i + 4 <= clean.size(); i += 4)
	{
		for (int k = 0; k < 4; k++)
			v[k] = table[(unsigned char)clean[i + k]];
		// Padding counts as 0 bits (the bytes are kept: they end the
		// text as NULs).
		if (v[2] == 0xFF)
			v[2] = 0;
		if (v[3] == 0xFF)
			v[3] = 0;
		DecodeGroup(v, out);
	}
	if (i < clean.size())
	{
		memset(v, 0, sizeof(v));
		for (size_t k = 0; i + k < clean.size(); k++)
		{
			v[k] = table[(unsigned char)clean[i + k]];
			if (v[k] == 0xFF)
				v[k] = 0;
		}
		DecodeGroup(v, out);
	}
	// What the callers use is the text up to the first NUL.
	out = std::string(out.c_str());
	return TRUE;
}
