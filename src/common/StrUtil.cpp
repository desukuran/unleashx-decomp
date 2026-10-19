#include "StrUtil.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
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

// 0x0001ADD6 (body at 0x00019113)
std::string &StrFormat(std::string &out, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

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
	va_end(args);

	out.assign(&buf[0], len);
	return out;
}
