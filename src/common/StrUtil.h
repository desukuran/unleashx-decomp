// std::string helpers used throughout UnleashX. The originals were
// specialized by link-time code generation into register calling conventions;
// they're expressed here as plain functions. Names are reconstructed.
#pragma once

#include <cstdarg>
#include <string>

#include "Platform.h"

std::string StrLeft(const std::string &s, int count);                // 0x00012A1D
std::string StrRight(const std::string &s, int count);               // 0x00011812
std::string StrMid(const std::string &s, int start, int count);      // 0x00012A90
std::string &StrTrimLeft(std::string &s);                            // 0x0001F029
std::string &StrTrimRight(std::string &s);                           // 0x0001F096

// Replaces every occurrence of find with replacement (NULL = ""), scanning
// forward past each replacement. Returns the number of replacements.
int StrReplaceAll(std::string &s, const char *find, const char *replacement);  // 0x00019215
int StrRemoveChar(std::string &s, char ch);                                    // 0x000191DD
int StrReplaceChar(std::string &s, char from, char to);                        // 0x0001EFD9

// Case-insensitive compare using the C++ locale's ctype<char>::tolower.
int StrCmpNoCase(const char *a, const char *b);                               // 0x0001AA3C
inline int StrCmpNoCase(const std::string &a, const char *b)                  // 0x0001918F
{
	return StrCmpNoCase(a.c_str(), b);
}

// sprintf into a std::string (tries buffers of 2K, 4K, ... 12K).
std::string &StrFormat(std::string &out, const char *fmt, ...);              // 0x0001ADD6
std::string &StrFormatV(std::string &out, const char *fmt, va_list args);    // 0x00019113

// Position of s in str at or after pos, or -1.                    (0x000191A9 / 0x000191C2)
int StrFind(const std::string &str, const char *s, int pos = 0);
// Last position of s in str at or before pos (-1 = end), or -1.   (0x00012AF5)
int StrReverseFind(const std::string &str, const char *s, int pos = -1);
// Position of c in str, or -1.                                    (0x0001EF9B)
int StrFindChar(const std::string &str, char c);
void StrAppend(std::string &s, const char *text);                // 0x000117FD
// Removes trailing characters that are in chars (all of them if the
// string consists only of those).                                 (0x000192D4)
std::string &StrTrimRightChars(std::string &s, const char *chars);
std::string &StrTrimRightChar(std::string &s, char c);           // 0x0002FC7D
// Uppercases with the C++ locale's ctype<char>::toupper.            (0x0001EFBD)
void StrMakeUpper(std::string &s);
// Lowercases with the C++ locale's ctype<char>::tolower.  (0x000226F6, 0x0002DAEB)
void StrMakeLower(std::string &s);
// Appends '\' unless the string already ends with one.            (0x000A41DF)
void StrAddBackslash(std::string &s);
// Lowercased copy (the original uses _strdup + _mbslwr and leaks the copy).
std::string StrToLower(const char *s);
inline std::string StrToLower(const std::string &s)
{
	return StrToLower(s.c_str());
}

// Not NULL and not "".                                          (0x0009DF0D)
BOOL StrIsNotEmpty(const char *s);

// "Yes", "True" or "1" (any case).                                (0x00021E1A)
BOOL IsTrueString(const char *s);
// "No", "False" or "0" (any case).                                (0x0001C918)
BOOL IsFalseString(const char *s);

// UTF-8 to wide into a buffer of size bytes, with the original's fallback
// for text that doesn't convert cleanly.                          (0x0009DF6F)
void StrUtf8ToWide(const char *src, wchar_t *dst, unsigned int size);
// Wide to UTF-8 into a buffer: at most size characters, then a NUL (so the
// buffer needs size + 1 bytes). Nothing for an empty or NULL string. (src in
// edi, dst in esi)                                                (0x0009DF2C)
void StrWideToUtf8(const wchar_t *src, char *dst, unsigned int size);
// Wide to narrow string.                                          (0x00020B69)
std::string StrFromWide(const wchar_t *s);

// "0A1B..." to bytes; 1 if the length is odd (nothing written), else 0.
// Pairs that aren't hex give 0 (or what strtoul makes of them). The
// original takes hex in edi.
int HexToBytes(const char *hex, BYTE *out);                       // 0x0009E6DB
