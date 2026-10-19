// EEPROM settings (0x0008ECA2-0x0008EF83). The original calls the kernel
// through two thunks (0x0008B03A, 0x0008B054).
#include "XSettings.h"

#include "../common/XboxKernel.h"

#define REG_DWORD 4

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
