// Hardware and system information shown by skins and the system info screen.
#pragma once

#include <string>

#include "../common/Platform.h"

// Number formatting with the dashboard language's separators.
wchar_t GetThousandsSeparator();                      // 0x00099F10
wchar_t GetDecimalSeparator();                        // 0x00099F3A
// n with thousands separators, e.g. "8,042,401,792". out holds 100 chars.
void FormatNumberW(wchar_t *out, ULONGLONG n);        // 0x000A1A7F
// v with the given number of decimals and separators.
std::string FormatDouble(int decimals, double v);     // 0x000A3A5A
std::string FormatIPAddress(DWORD addr);              // 0x000A385E
// Kernel version, "1.0.5838.1".
std::string GetKernelVersionString();                 // 0x000A38C8
// Name of the BIOS flash chip ("Unknown" if not recognized), with the chip
// ID in hex instead if withId.
std::string GetFlashChipName(BOOL withId);            // 0x000A3EA0
// Xbox revision from the SMC version: "V1.0" ... "V1.6", "DEBUG" ...
void GetXboxVersion(char *out);                       // 0x0008F7CB
// SMBus temperature, Celsius or Fahrenheit.
float ReadTemperature(BOOL cpu, BOOL celsius);        // 0x0009E69A
// Date and/or time (either can be NULL) of ft in the dashboard's format.
void FormatFileTime(const FILETIME *ft, wchar_t *date, wchar_t *time, BOOL seconds,
                    BOOL use24Hour);                  // 0x0009A0DA

// ATA IDENTIFY strings of the hard disk (hdd) or DVD drive, at most size chars.
void GetIdeModel(BOOL hdd, char *out, unsigned size);      // 0x000905A2
void GetIdeSerial(BOOL hdd, char *out, unsigned size);     // 0x00090567
void GetIdeFirmware(BOOL hdd, char *out, unsigned size);   // 0x0009052C
// HDD security bits: HDD_SECURITY_*.
WORD GetHddSecurityStatus();                               // 0x000904FC
#define HDD_SECURITY_SUPPORTED 0x01
#define HDD_SECURITY_ENABLED 0x02
#define HDD_SECURITY_FROZEN 0x08
#define HDD_SECURITY_COUNT_EXPIRED 0x10
// Locks the hard disk with the Xbox HDD key / removes the lock. TRUE on
// success. TODO: decompile
BOOL LockHdd();                                            // 0x00090D42
BOOL UnlockHdd();                                          // 0x00090EF8
// Hard disk details for the HDD settings page: 1 = geometry, 2 = the
// transfer mode ("Ultra DMA Mode 5", "PIO Mode 4"). TODO: decompile
void GetHddInfoText(char *out, int kind);                  // 0x0009048B
// SMART temperature attribute (0xC2) of the hard disk, 0 if unavailable.
// Also refreshes the attributes GetSmartAttribute returns.
BYTE GetHddTemperature();                                  // 0x00090AB1
// One of the (up to 11) SMART attributes kept by GetHddTemperature; any
// output can be NULL.
void GetSmartAttribute(unsigned index, DWORD *id, DWORD *value, DWORD *worst);   // 0x00090453

// MD5 of a block of memory or a file; 0x32 bytes.
class CMD5
{
public:
	CMD5(const void *data, int length);                 // 0x0009845B TODO: decompile; hashes data unless NULL, length -1 = strlen
	BOOL HashMemory(const void *data, int length);      // 0x00098307 TODO: decompile
	BOOL HashFile(const char *path);                    // 0x0009837B TODO: decompile

	BYTE m_digest[16];      // +0x00
	bool m_bValid;          // +0x10
	char m_szHex[33];       // +0x11
};
