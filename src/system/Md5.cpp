// CMD5: an MD5 digest and its hex text (see SystemInfo.h).
#include <cstdio>
#include <cstring>

#include "../common/Platform.h"
#include "../thirdparty/Md5.h"
#include "SystemInfo.h"

CMD5 g_md5(NULL, -1);

// The digest as 32 lowercase hex digits. (inlined twice)
static void DigestToHex(const BYTE digest[16], char *hex)
{
	for (int i = 0; i < 16; i++)
		sprintf(hex + i * 2, "%02x", digest[i]);
}

// 0x0009845B (this in eax)
CMD5::CMD5(const void *data, int length)
{
	// (m_bValid is left uninitialized without data.)
	if (data != NULL)
		HashMemory(data, length);
}

// 0x00098307 (this in esi)
BOOL CMD5::HashMemory(const void *data, int length)
{
	m_bValid = false;
	if (data == NULL)
		return m_bValid;
	if (length == -1)
		length = (int)strlen((const char *)data);
	MD5_CTX context;
	MD5Init(&context);
	MD5Update(&context, (const unsigned char *)data, (unsigned int)length);
	MD5Final(m_digest, &context);
	DigestToHex(m_digest, m_szHex);
	m_bValid = true;
	return m_bValid;
}

// 0x0009837B (this in ebx)
BOOL CMD5::HashFile(const char *path)
{
	m_bValid = false;
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return m_bValid;
	BYTE *buffer = new BYTE[0x2000];
	if (buffer != NULL)
	{
		MD5_CTX context;
		MD5Init(&context);
		DWORD read = 0;
		if (!ReadFile(file, buffer, 0x2000, &read, NULL))
			read = 0;
		while (read > 0)
		{
			MD5Update(&context, buffer, read);
			if (!ReadFile(file, buffer, 0x2000, &read, NULL))
				read = 0;
		}
		MD5Final(m_digest, &context);
		DigestToHex(m_digest, m_szHex);
		m_bValid = true;
		delete[] buffer;
	}
	CloseHandle(file);
	return m_bValid;
}
