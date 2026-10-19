// Formatting helpers of SystemInfo.h. The hardware readers are in Hardware.cpp.
#include "SystemInfo.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"

// XC_LANGUAGE_* values
enum
{
	LANGUAGE_ENGLISH = 1,
	LANGUAGE_JAPANESE = 2,
	LANGUAGE_GERMAN = 3,
	LANGUAGE_FRENCH = 4,
	LANGUAGE_SPANISH = 5,
	LANGUAGE_ITALIAN = 6,
	LANGUAGE_KOREAN = 7,
	LANGUAGE_TCHINESE = 8,
	LANGUAGE_PORTUGUESE = 9,
};

// 0x00099F10
wchar_t GetThousandsSeparator()
{
	DWORD language = XGetLanguage();
	if (language == LANGUAGE_GERMAN)
		return L'.';
	if (language == LANGUAGE_FRENCH)
		return L' ';
	if (language < LANGUAGE_SPANISH || (language > LANGUAGE_ITALIAN && language != LANGUAGE_PORTUGUESE))
		return L',';
	return L'.';
}

// 0x00099F3A
wchar_t GetDecimalSeparator()
{
	DWORD language = XGetLanguage();
	if (language > LANGUAGE_JAPANESE && (language < LANGUAGE_KOREAN || language == LANGUAGE_PORTUGUESE))
		return L',';
	return L'.';
}

// 0x00099F58
// v with decimals digits after the separator, thousands grouped if group.
void FormatDoubleW(wchar_t *out, int decimals, BOOL group, double v)
{
	int decimalPos, negative;
	const char *digits = _fcvt(v, decimals < 0 ? 0 : decimals, &decimalPos, &negative);
	wchar_t decimalSeparator = GetDecimalSeparator();
	wchar_t thousandsSeparator = GetThousandsSeparator();

	if (negative)
		*out++ = L'-';
	wchar_t *p = out;
	if (decimalPos < 1)
	{
		*p++ = L'0';
		if (decimals > 0)
		{
			*p++ = decimalSeparator;
			for (int i = 0; i < -decimalPos; i++)
				*p++ = L'0';
		}
	}

	for (int i = 1; *digits != 0; i++, digits++)
	{
		*p++ = (wchar_t)*digits;
		if (group && decimalPos > 3 && i - 1 < decimalPos - 1 && ((1 - (decimalPos + 1) % 3) + i) % 3 == 0)
			*p++ = thousandsSeparator;
		if (i == decimalPos && decimals > 0)
			*p++ = decimalSeparator;
	}
	*p = 0;
}

// 0x000A1A7F
void FormatNumberW(wchar_t *out, ULONGLONG n)
{
	// _ui64tow(n, out, 10)
	wchar_t digits[24];
	int len = 0;
	do
	{
		digits[len++] = (wchar_t)(L'0' + n % 10);
		n /= 10;
	} while (n != 0);
	for (int i = 0; i < len; i++)
		out[i] = digits[len - 1 - i];
	out[len] = 0;

	int separators = (len - 1) / 3;
	if (separators <= 0)
		return;
	wchar_t separator = GetThousandsSeparator();
	// Move the digits right from the end, a separator before every third.
	wchar_t *src = out + len;
	wchar_t *dst = out + len + separators + 1;
	*dst = 0;   // sic: one past the new terminator
	for (int i = 0; i < len; i++)
	{
		*--dst = *src--;
		if (i != 0 && i % 3 == 0)
			*--dst = separator;
	}
}

// 0x0009A060
void FormatIntW(wchar_t *out, int n)
{
	swprintf(out, 12, L"%d", n);   // _itow(n, out, 10) TODO: verify (_itow or _ultow)
	int len = (int)wcslen(out);
	int separators = (len - 1) / 3;
	if (separators <= 0)
		return;
	wchar_t separator = GetThousandsSeparator();
	// Like FormatNumberW, but the terminator lands in the right place.
	wchar_t *src = out + len;
	wchar_t *dst = out + len + separators;
	for (int i = 0; i < len; i++)
	{
		*dst = *src;
		if (i != 0 && i % 3 == 0)
			*--dst = separator;
		dst--;
		src--;
	}
}

// 0x000A3A5A
// 0x000A3B09
std::string FormatInt(int n)
{
	wchar_t text[100];
	FormatIntW(text, n);
	return StrFromWide(text);
}

// 0x000A2C2B
std::string FormatNumberA(ULONGLONG n)
{
	// The digits, then a separator before every third from the right.
	char digits[0x64];
	memset(digits, 0, sizeof(digits));
	int length = 0;
	do
	{
		digits[length++] = (char)('0' + (int)(n % 10));
		n /= 10;
	} while (n != 0);
	char text[0x64];
	int out = 0;
	char separator = (char)GetThousandsSeparator();
	for (int i = length - 1; i >= 0; i--)
	{
		text[out++] = digits[i];
		if (i != 0 && i % 3 == 0)
			text[out++] = separator;
	}
	text[out] = 0;
	return std::string(text);
}

std::string FormatDouble(int decimals, double v)
{
	wchar_t text[102];
	FormatDoubleW(text, decimals, TRUE, v);
	return StrFromWide(text);
}

// 0x000A385E
std::string FormatIPAddress(DWORD addr)
{
	std::string str;
	StrFormat(str, "%ld.%ld.%ld.%ld", addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, addr >> 24);
	return str;
}

// 0x000A38C8
std::string GetKernelVersionString()
{
	std::string str;
	StrFormat(str, "%d.%d.%d.%d", XboxKrnlVersion.Major, XboxKrnlVersion.Minor, XboxKrnlVersion.Build,
	          XboxKrnlVersion.Qfe & 0x7FFF);
	return str;
}

// 0x0009A0DA
void FormatFileTime(const FILETIME *ft, wchar_t *date, wchar_t *time, BOOL seconds, BOOL use24Hour)
{
	if (ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0)
	{
		if (date != NULL)
			date[0] = 0;
		if (time != NULL)
			time[0] = 0;
		return;
	}

	FILETIME local;
	SYSTEMTIME st;
	FileTimeToLocalFileTime(ft, &local);
	FileTimeToSystemTime(&local, &st);

	// The order follows the dashboard language (the DateFormat preference
	// isn't used here).
	if (date != NULL)
	{
		switch (XGetLanguage())
		{
		case LANGUAGE_ENGLISH:
			swprintf(date, 100, L"%d/%d/%d", st.wMonth, st.wDay, st.wYear);
			break;
		case LANGUAGE_JAPANESE:
		case LANGUAGE_KOREAN:
		case LANGUAGE_TCHINESE:
			swprintf(date, 100, L"%d/%d/%d", st.wYear, st.wMonth, st.wDay);
			break;
		case LANGUAGE_GERMAN:
		case LANGUAGE_ITALIAN:
			swprintf(date, 100, L"%d.%d.%d", st.wDay, st.wMonth, st.wYear);
			break;
		default:
			swprintf(date, 100, L"%d/%d/%d", st.wDay, st.wMonth, st.wYear);
			break;
		}
	}

	if (time != NULL)
	{
		WORD hour = st.wHour;
		if (!use24Hour)
		{
			if (hour >= 12)
				hour -= 12;
			if (hour == 0)
				hour = 12;
		}
		swprintf(time, 15, L"%d:%2.2d", hour, st.wMinute);
		if (seconds)
		{
			wchar_t sec[8];
			swprintf(sec, 8, L":%2.2d", st.wSecond);
			wcscat(time, sec);
		}
		if (!use24Hour)
			wcscat(time, st.wHour >= 12 ? L" PM" : L" AM");
	}
}
