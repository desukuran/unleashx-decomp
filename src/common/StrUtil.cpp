#include "StrUtil.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cwchar>
#include <locale>
#include <vector>

#ifndef va_copy
#define va_copy(dst, src) ((dst) = (src))  // MSVC 7.1 has no va_copy; x86 va_list is a pointer
#endif

// 0x00012A1D
std::string StrLeft(const std::string &s, int count)
{
	int n = (int)s.size() < count ? (int)s.size() : count;
	if (n <= 0)
		n = 0;
	return s.substr(0, n);
}

// 0x00011812
std::string StrRight(const std::string &s, int count)
{
	int n = (int)s.size() < count ? (int)s.size() : count;
	if (n <= 0)
		n = 0;
	return s.substr(s.size() - n);
}

// 0x00012A90
std::string StrMid(const std::string &s, int start, int count)
{
	if (start < 0)
		start = 0;
	if (count < 0)
		count = 0;
	if (start + count > (int)s.size())
		count = (int)s.size() - start;
	if (start > (int)s.size())
		return std::string();
	return s.substr(start, count);
}

// 0x0001F029
std::string &StrTrimLeft(std::string &s)
{
	std::locale loc;
	std::string::iterator it = s.begin();
	while (it != s.end() && std::isspace(*it, loc))
		++it;
	s.erase(s.begin(), it);
	return s;
}

// 0x0001F096
std::string &StrTrimRight(std::string &s)
{
	std::locale loc;
	std::string::size_type end = s.size();
	while (end > 0 && std::isspace(s[end - 1], loc))
		end--;
	s.erase(end);
	return s;
}

// 0x00019215
int StrReplaceAll(std::string &s, const char *find, const char *replacement)
{
	int count = 0;
	if (find == NULL)
		return 0;
	size_t findLen = strlen(find);
	if (findLen == 0)
		return 0;

	size_t replLen;
	if (replacement == NULL)
	{
		replLen = 0;
		replacement = "";
	}
	else
	{
		replLen = strlen(replacement);
	}

	std::string::size_type pos = s.find(find, 0);
	while (pos != std::string::npos)
	{
		s.replace(pos, findLen, replacement);
		count++;
		pos = s.find(find, pos + replLen);
	}
	return count;
}

// 0x000191DD
int StrRemoveChar(std::string &s, char ch)
{
	int count = 0;
	std::string::size_type pos;
	while ((pos = s.find(ch)) != std::string::npos)
	{
		s.erase(pos, 1);
		count++;
	}
	return count;
}

// 0x0001EFD9
int StrReplaceChar(std::string &s, char from, char to)
{
	int count = 0;
	for (std::string::iterator it = s.begin(); it != s.end(); ++it)
	{
		if (*it == from)
		{
			*it = to;
			count++;
		}
	}
	return count;
}

// 0x0001AA3C
int StrCmpNoCase(const char *a, const char *b)
{
	std::locale loc;
	const std::ctype<char> &ct = std::use_facet<std::ctype<char> >(loc);
	char ca, cb;
	do
	{
		ca = ct.tolower(*a++);
		cb = ct.tolower(*b++);
	} while (ca != 0 && ca == cb);
	return (int)ca - (int)cb;
}

// 0x00019113
std::string &StrFormatV(std::string &out, const char *fmt, va_list args)
{
	// The original formats into an alloca() buffer and retries with a bigger one
	// while _vsnprintf reports truncation: 2K, 6K, 12K, 20K, 30K, 42K. If even
	// that fails it keeps the truncated text.
	std::vector<char> buf;
	int size = 1, step = 0, len;
	for (;;)
	{
		size += step + 0x800;
		buf.resize(size + 1);
		va_list copy;
		va_copy(copy, args);
		// Original: _vsnprintf(buf, size - 1, ...), which returns -1 when the
		// text doesn't fit in size - 1 chars. C99 vsnprintf counts the NUL, so
		// pass size and accept the same lengths.
		len = vsnprintf(&buf[0], size, fmt, copy);
		va_end(copy);
		if (len >= 0 && len <= size - 1)
			break;
		if (step >= 0x2800)
		{
			len = size - 1;
			break;
		}
		step += 0x800;
	}

	out.assign(&buf[0], len);
	return out;
}

// 0x0001ADD6
std::string &StrFormat(std::string &out, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	StrFormatV(out, fmt, args);
	va_end(args);
	return out;
}

// 0x000191A9 / 0x000191C2
int StrFind(const std::string &str, const char *s, int pos)
{
	std::string::size_type found = str.find(s, pos);
	return found == std::string::npos ? -1 : (int)found;
}

// 0x00012AF5
int StrReverseFind(const std::string &str, const char *s, int pos)
{
	std::string::size_type found = str.rfind(s, pos == -1 ? std::string::npos : (std::string::size_type)pos);
	return found == std::string::npos ? -1 : (int)found;
}

// 0x0001EF9B
int StrFindChar(const std::string &str, char c)
{
	std::string::size_type found = str.find(c);
	return found == std::string::npos ? -1 : (int)found;
}

// 0x000117FD
void StrAppend(std::string &s, const char *text)
{
	s.append(text);
}

// 0x000192D4
std::string &StrTrimRightChars(std::string &s, const char *chars)
{
	std::string::size_type last = s.find_last_not_of(chars);
	s.erase(last == std::string::npos ? 0 : last + 1);
	return s;
}

// 0x0002FC7D
std::string &StrTrimRightChar(std::string &s, char c)
{
	std::string::size_type last = s.find_last_not_of(c);
	s.erase(last == std::string::npos ? 0 : last + 1);
	return s;
}

// 0x0001EFBD
void StrMakeUpper(std::string &s)
{
	std::locale loc;
	const std::ctype<char> &ct = std::use_facet<std::ctype<char> >(loc);
	for (std::string::iterator it = s.begin(); it != s.end(); ++it)
		*it = ct.toupper(*it);
}

// 0x000226F6 (0x0002DAEB makes the locale)
void StrMakeLower(std::string &s)
{
	std::locale loc;
	const std::ctype<char> &ct = std::use_facet<std::ctype<char> >(loc);
	for (std::string::iterator it = s.begin(); it != s.end(); ++it)
		*it = ct.tolower(*it);
}

// 0x000A41DF
void StrAddBackslash(std::string &s)
{
	if (StrRight(s, 1).compare("\\") != 0)
		s += "\\";
}

std::string StrToLower(const char *s)
{
	std::string out(s != NULL ? s : "");
	for (std::string::iterator it = out.begin(); it != out.end(); ++it)
		*it = (char)tolower((unsigned char)*it);
	return out;
}

// _mbsicmp() in the original; ASCII is all that matters for these keywords.
static int AsciiCompareNoCase(const char *a, const char *b)
{
	int ca, cb;
	do
	{
		ca = tolower((unsigned char)*a++);
		cb = tolower((unsigned char)*b++);
	} while (ca != 0 && ca == cb);
	return ca - cb;
}

// 0x00021E1A
BOOL IsTrueString(const char *s)
{
	if (s == NULL)
		return FALSE;
	return AsciiCompareNoCase("Yes", s) == 0 || AsciiCompareNoCase("True", s) == 0 ||
	       AsciiCompareNoCase("1", s) == 0;
}

// 0x0009DF0D (s in eax)
BOOL StrIsNotEmpty(const char *s)
{
	return s != NULL && strlen(s) != 0;
}

// 0x0001C918
BOOL IsFalseString(const char *s)
{
	if (s == NULL)
		return FALSE;
	return AsciiCompareNoCase("No", s) == 0 || AsciiCompareNoCase("False", s) == 0 ||
	       AsciiCompareNoCase("0", s) == 0;
}

// 0x0009DF6F
void StrUtf8ToWide(const char *src, wchar_t *dst, unsigned int size)
{
	if (dst == NULL)
		return;
	if (src == NULL || strlen(src) == 0)
	{
		memset(dst, 0, size);
		*(char *)dst = 0;
		return;
	}

	memset(dst, 0, size);
	unsigned int len = (unsigned int)strlen(src);
	if (len > size)
		len = size;

	int n = MultiByteToWideChar(CP_UTF8, 8 /* MB_ERR_INVALID_CHARS */, src, (int)len, dst, (int)size);
	if (n > 0)
	{
		dst[n] = 0;
		// Done if every character is Latin-1; otherwise redo it by hand.
		unsigned int i = 0;
		if (wcslen(dst) == 0)
			return;
		while (dst[i] < 0x100)
		{
			i++;
			if (wcslen(dst) <= i)
				return;
		}
	}

	// Fallback: a lead byte plus continuation byte is folded into one
	// character with the original's arithmetic (not real UTF-8 decoding);
	// anything else is taken as Latin-1.
	int out = 0;
	for (unsigned int i = 0; i < len; i++)
	{
		signed char c = (signed char)src[i];
		unsigned short w;
		if (c < 0)
		{
			w = (unsigned short)(c + 0x100);
			if ((signed char)src[i + 1] < -0x40)
			{
				unsigned short folded = (unsigned short)(c + 0x17D + (signed char)src[i + 1]);
				if (folded < 0x101)
				{
					i++;
					if (folded != 0x100)
						w = folded;
				}
			}
		}
		else
		{
			w = (unsigned short)c;
		}
		dst[out++] = w;
	}
	dst[out] = 0;
}

// 0x00020B69
// TODO: verify; the original narrows through the locale's codecvt facet.
std::string StrFromWide(const wchar_t *s)
{
	std::string out;
	if (s == NULL || wcslen(s) == 0)
		return out;
	for (; *s != 0; s++)
		out += (char)*s;
	return out;
}

// 0x0009DF2C
void StrWideToUtf8(const wchar_t *src, char *dst, unsigned int size)
{
	if (src == NULL || wcslen(src) == 0 || dst == NULL)
		return;
	unsigned int length = (unsigned int)wcslen(src);
	if (length > size)
		length = size;
	int written = WideCharToMultiByte(65001 /* CP_UTF8 */, 0x40, src, (int)length, dst, (int)size, NULL, NULL);
	if (written > 0)
		dst[written] = '\0';
}

// 0x0009E6DB
int HexToBytes(const char *hex, BYTE *out)
{
	if (strlen(hex) & 1)
		return 1;
	int count = (int)(strlen(hex) / 2);
	for (int i = 0; i < count; i++)
	{
		char pair[100];
		sprintf(pair, "%c%c", hex[i * 2], hex[i * 2 + 1]);
		out[i] = (BYTE)strtoul(pair, NULL, 16);
	}
	return 0;
}
