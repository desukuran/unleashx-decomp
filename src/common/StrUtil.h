// std::string helpers used throughout UnleashX. The originals were
// specialized by link-time code generation into register calling conventions;
// they're expressed here as plain functions. Names are reconstructed.
#pragma once

#include <string>

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
