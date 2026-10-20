// EEPROM settings (0x0008ECA2-0x0008EF83). The original calls the kernel
// through two thunks (0x0008B03A, 0x0008B054).
#include "XSettings.h"

#include <cstring>

#include "../common/XboxKernel.h"

#ifndef REG_DWORD
#define REG_DWORD 4
#endif

// The flags in the EEPROM are the XC_VIDEO_FLAGS_* shifted into the high word.
#define EEPROM_VIDEO_WIDESCREEN (XC_VIDEO_FLAGS_WIDESCREEN << 16)
#define EEPROM_VIDEO_720P (XC_VIDEO_FLAGS_HDTV_720p << 16)
#define EEPROM_VIDEO_1080I (XC_VIDEO_FLAGS_HDTV_1080i << 16)
#define EEPROM_VIDEO_480P (XC_VIDEO_FLAGS_HDTV_480p << 16)
#define EEPROM_VIDEO_LETTERBOX (XC_VIDEO_FLAGS_LETTERBOX << 16)
#define EEPROM_VIDEO_PAL60 (XC_VIDEO_FLAGS_PAL_60Hz << 16)

static HRESULT SaveXSetting(DWORD index, DWORD value)
{
	Sleep(10);
	return ExSaveNonVolatileSetting(index, REG_DWORD, &value, sizeof(value));
}

// Sets or clears one flag of a setting, writing only when it changes.
static HRESULT SetXSettingFlag(DWORD index, DWORD flag, BOOL enable)
{
	DWORD old = GetXSetting(index);
	DWORD value = enable ? old | flag : old & ~flag;
	if (old == value)
		return S_OK;
	return SaveXSetting(index, value);
}

// 0x0008ED12
DWORD GetXSetting(DWORD index)
{
	ULONG type;
	DWORD value = 0;
	ExQueryNonVolatileSetting(index, &type, &value, sizeof(value), NULL);
	Sleep(5);
	return value;
}

// 0x0008ECA2
HRESULT SetVideoStandard(DWORD region)
{
	DWORD standard = XGetVideoStandard();
	if ((standard == XC_VIDEO_STANDARD_NTSC_M && region == AV_REGION_NTSC_M) ||
	    (standard == XC_VIDEO_STANDARD_NTSC_J && region == AV_REGION_NTSC_J) ||
	    (standard == XC_VIDEO_STANDARD_PAL_I && region == AV_REGION_PAL_I))
		return S_OK;
	if (region != AV_REGION_NTSC_M && region != AV_REGION_NTSC_J && region != AV_REGION_PAL_I)
		return E_FAIL;
	return SaveXSetting(XC_FACTORY_AV_REGION, region);
}

// 0x0008ED94. Widescreen and letterbox exclude each other.
HRESULT SetVideoWidescreen(BOOL enable)
{
	DWORD old = GetXSetting(XC_VIDEO);
	DWORD value = old & ~(EEPROM_VIDEO_WIDESCREEN | EEPROM_VIDEO_LETTERBOX);
	if (enable)
		value |= EEPROM_VIDEO_WIDESCREEN;
	if (old != value)
		SaveXSetting(XC_VIDEO, value);
	return S_OK;
}

// 0x0008ED4C
HRESULT SetVideoLetterbox(BOOL enable)
{
	DWORD old = GetXSetting(XC_VIDEO);
	DWORD value = old & ~(EEPROM_VIDEO_WIDESCREEN | EEPROM_VIDEO_LETTERBOX);
	if (enable)
		value |= EEPROM_VIDEO_LETTERBOX;
	if (old == value)
		return S_OK;
	return SaveXSetting(XC_VIDEO, value);
}

// 0x0008EE20
HRESULT SetVideo480p(BOOL enable)
{
	return SetXSettingFlag(XC_VIDEO, EEPROM_VIDEO_480P, enable);
}

// 0x0008EE67
HRESULT SetVideo720p(BOOL enable)
{
	return SetXSettingFlag(XC_VIDEO, EEPROM_VIDEO_720P, enable);
}

// 0x0008EEAE
HRESULT SetVideo1080i(BOOL enable)
{
	return SetXSettingFlag(XC_VIDEO, EEPROM_VIDEO_1080I, enable);
}

// 0x0008EDD9
HRESULT SetVideoPAL60(BOOL enable)
{
	return SetXSettingFlag(XC_VIDEO, EEPROM_VIDEO_PAL60, enable);
}

// 0x0008EEF5
HRESULT SetAudioAC3(BOOL enable)
{
	return SetXSettingFlag(XC_AUDIO, AUDIO_FLAG_AC3, enable);
}

// 0x0008EF3C
HRESULT SetAudioDTS(BOOL enable)
{
	return SetXSettingFlag(XC_AUDIO, AUDIO_FLAG_DTS, enable);
}

// ExQueryNonVolatileSetting's start in the retail kernels: the search
// signature. 0x0026B950
static const BYTE s_queryCode[0x39] = {
	0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x01, 0x00, 0x00, 0x8B, 0x45, 0x08,
	0x3D, 0x04, 0x01, 0x00, 0x00, 0x53, 0x75, 0x32, 0x8B, 0x4D, 0x18, 0x85,
	0xC9, 0x6A, 0x04, 0x58, 0x74, 0x02, 0x89, 0x01, 0x39, 0x45, 0x14, 0x73,
	0x0A, 0xB8, 0x23, 0x00, 0x00, 0xC0, 0xE9, 0x59, 0x01, 0x00, 0x00, 0x8B,
	0x4D, 0x0C, 0x89, 0x01, 0x8B, 0x45, 0x10, 0x8B, 0x0D,
};
// The replacement: settings 0x104 (game region, the dword at +0x1F) and
// 0x103 (video standard, at +0x18) return fixed values, everything else
// goes to the original; the original's variable pointer is kept at +0x42.
// Its first 0x19 bytes find an earlier patch. 0x0026B990
static const BYTE s_queryPatch[0x46] = {
	0x55, 0x8B, 0xEC, 0xB9, 0x04, 0x01, 0x00, 0x00, 0x2B, 0xE1, 0x8B, 0x45,
	0x08, 0x53, 0x3B, 0xC1, 0x74, 0x0C, 0x49, 0x3B, 0xC1, 0x75, 0x2F, 0xB8,
	0x00, 0x03, 0x80, 0x00, 0xEB, 0x05, 0xB8, 0x04, 0x00, 0x00, 0x00, 0x50,
	0x8B, 0x4D, 0x18, 0x6A, 0x04, 0x58, 0x85, 0xC9, 0x74, 0x02, 0x89, 0x01,
	0x8B, 0x4D, 0x0C, 0x89, 0x01, 0x59, 0x8B, 0x45, 0x10, 0x89, 0x08, 0x33,
	0xC0, 0x5B, 0xC9, 0xC2, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Where the kernel's code is searched (0x80011000-0x80024000).
#define KERNEL_SEARCH_START 0x80011000
#define KERNEL_SEARCH_END 0x80024000

static BYTE *FindKernelCode(const BYTE *code, size_t length)
{
	for (size_t address = KERNEL_SEARCH_START; address < KERNEL_SEARCH_END; address++)
	{
		const BYTE *p = (const BYTE *)address;
		size_t i = 0;
		while (i < length && p[i] == code[i])
			i++;
		if (i == length)
			return (BYTE *)p;
	}
	return NULL;
}

// The kernel variable the code reads, from its embedded address.
static DWORD *ReadPointer(const BYTE *p)
{
	return (DWORD *)(size_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((DWORD)p[3] << 24));
}

// 0x000BCF48
// Quirk: the protection is set back to the variable's address instead of
// the old protection (which is queried and dropped).
void SetVideoRegion(int standard, int mode)
{
	// Game region bits, the standard's byte and the video flags' byte.
	static const BYTE regions[4] = {0, 1, 2, 4};
	static const BYTE standards[5] = {0, 1, 2, 3, 3};
	static const BYTE flags[5] = {0, 0x40, 0x40, 0x80, 0x40};

	if (standard == 0)
	{
		if (mode == 0)
			return;
		standard = mode == 1 ? 1 : mode == 2 ? 2 : 3;
	}
	if (mode == 0)
		mode = standard == 1 ? 1 : standard == 2 ? 2 : 3;

	BYTE *code = FindKernelCode(s_queryCode, sizeof(s_queryCode));
	DWORD *variable;
	if (code != NULL)
	{
		variable = ReadPointer(code + 0x39);
	}
	else
	{
		code = FindKernelCode(s_queryPatch, 0x19);
		if (code == NULL)
			return;
		variable = ReadPointer(code + 0x42);
	}
	MmQueryAddressProtect(code);
	MmSetAddressProtect(code, sizeof(s_queryPatch), PAGE_READWRITE);
	BYTE region = regions[standard];
	memcpy(code, s_queryPatch, sizeof(s_queryPatch));
	*variable = region;
	code[0x1F] = region;
	code[0x19] = standards[mode];
	code[0x1A] = flags[mode];
	DWORD pointer = (DWORD)(size_t)variable;
	code[0x42] = (BYTE)pointer;
	code[0x43] = (BYTE)(pointer >> 8);
	code[0x44] = (BYTE)(pointer >> 16);
	code[0x45] = (BYTE)(pointer >> 24);
	MmSetAddressProtect(code, sizeof(s_queryPatch), pointer);
}
