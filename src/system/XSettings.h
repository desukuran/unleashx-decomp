// The Xbox's own settings in the EEPROM: video standard, video flags (the
// AV modes the dashboard allows) and audio flags. CXSettings (vtable
// 0x0025B764) has no data; UnleashX uses these helpers directly.
#pragma once

#include "../common/Platform.h"

// EEPROM setting indexes (ExQueryNonVolatileSetting)
#define XC_VIDEO 0x008                  // the XC_VIDEO_FLAGS_* in the high word
#define XC_AUDIO 0x009
#define XC_FACTORY_AV_REGION 0x103      // video standard

// XC_FACTORY_AV_REGION values
#define AV_REGION_NTSC_M 0x00400100
#define AV_REGION_NTSC_J 0x00400200
#define AV_REGION_PAL_I 0x00800300

// XC_AUDIO flags
#define AUDIO_FLAG_AC3 0x00010000
#define AUDIO_FLAG_DTS 0x00020000

DWORD GetXSetting(DWORD index);                     // 0x0008ED12
// Only writes when the standard differs from the current one; E_FAIL for an
// unknown value. The original takes region in esi.
HRESULT SetVideoStandard(DWORD region);             // 0x0008ECA2
// These only write when the flag changes; they return the save's status.
HRESULT SetVideoWidescreen(BOOL enable);            // 0x0008ED94 (also clears letterbox; always returns 0)
HRESULT SetVideoLetterbox(BOOL enable);             // 0x0008ED4C (also clears widescreen)
HRESULT SetVideo480p(BOOL enable);                  // 0x0008EE20
HRESULT SetVideo720p(BOOL enable);                  // 0x0008EE67
HRESULT SetVideo1080i(BOOL enable);                 // 0x0008EEAE
HRESULT SetVideoPAL60(BOOL enable);                 // 0x0008EDD9
HRESULT SetAudioAC3(BOOL enable);                   // 0x0008EEF5
HRESULT SetAudioDTS(BOOL enable);                   // 0x0008EF3C
