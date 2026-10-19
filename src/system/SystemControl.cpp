// Xbox only (SMBus); the host test build uses stand-ins.
#include "SystemControl.h"

#include <cstring>

#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"

#define SMBUS_SMC 0x20

// SMC registers
#define SMC_POWER 0x02           // 0x01 reset, 0x40 power cycle, 0x80 power off
#define SMC_FAN_MODE 0x05        // 1 = use the speed in SMC_FAN_SPEED_SET
#define SMC_FAN_SPEED_SET 0x06
#define SMC_LED_MODE 0x07        // 0 = automatic, 1 = custom sequence
#define SMC_LED_SEQUENCE 0x08
#define SMC_RESET_ON_EJECT 0x19  // 0 = enable, 1 = disable
#define SMC_FAN_SPEED 0x10

static void SmcWrite(UCHAR command, ULONG value)
{
	HalWriteSMBusValue(SMBUS_SMC, command, FALSE, value);
}

// 0x0009DD2F
// Quirk: the time is written up to three times, again when the year comes
// back past 2099 and when hour or minute didn't stick.
void SetXboxSystemTime(SYSTEMTIME st, BOOL local)
{
	if (st.wYear < 2000 || st.wYear > 2099)
	{
		st.wDay = 15;
		st.wMonth = 11;
		st.wYear = 2004;
		st.wHour = 12;
		st.wMinute = 0;
		st.wSecond = 0;
	}

	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	if (local)
		LocalFileTimeToFileTime(&ft, &ft);

	LARGE_INTEGER previous;
	KeQuerySystemTime(&previous);
	NtSetSystemTime((LARGE_INTEGER *)&ft, &previous);

	SYSTEMTIME now;
	GetSystemTime(&now);
	if (now.wYear > 2099)
	{
		KeQuerySystemTime(&previous);
		NtSetSystemTime((LARGE_INTEGER *)&ft, &previous);
	}
	if (st.wHour != now.wHour || st.wMinute != now.wMinute)
	{
		KeQuerySystemTime(&previous);
		NtSetSystemTime((LARGE_INTEGER *)&ft, &previous);
		GetSystemTime(&now);
	}
}

// 0x0009F552
void SetFanSpeed(int speed)
{
	if (speed <= 10)
		return;
	if (speed > 50)
		speed = 50;
	if (speed != GetFanSpeed())
	{
		SmcWrite(SMC_FAN_SPEED_SET, speed);
		Sleep(10);
		SmcWrite(SMC_FAN_MODE, 1);
	}
}

// 0x0009E65F
void SmcSetFanAuto()
{
	HalWriteSMBusValue(SMBUS_SMC, SMC_FAN_MODE, FALSE, 0);
}

// 0x0009E66D
int GetFanSpeed()
{
	ULONG value = 0;
	Sleep(10);
	HalReadSMBusValue(SMBUS_SMC, SMC_FAN_SPEED, FALSE, &value);
	return (signed char)value;
}

// 0x0008F9EF
void SmcPowerCycle()
{
	SmcWrite(SMC_POWER, 0x40);
	Sleep(10);
}

// 0x0008FA04
void SmcPowerOff()
{
	SmcWrite(SMC_POWER, 0x80);
	Sleep(10);
}

// 0x0008FA5C
void SmcReset()
{
	SmcWrite(SMC_POWER, 0x01);
}

// 0x0008FA1C
void SmcSetLedAuto()
{
	SmcWrite(SMC_LED_MODE, 0);
	Sleep(10);
}

// 0x0008FA31
void SmcSetLed(BYTE sequence)
{
	SmcWrite(SMC_LED_SEQUENCE, sequence);
	Sleep(5);
	SmcWrite(SMC_LED_MODE, 1);
	Sleep(5);
}

static BOOL s_bLedPatternActive;       // 0x00278D98
static int s_nLedPatternQueued;        // 0x00278D94 never set
int g_nLedPattern;                     // 0x00277658

// 0x0009D319
BOOL SetLedPattern(int pattern, BOOL force)
{
	if (!force)
	{
		if (pattern != 0)
		{
			if (s_bLedPatternActive && s_nLedPatternQueued != 0)
				return FALSE;
			goto apply;
		}
		if (!s_bLedPatternActive)
			return FALSE;
		if (s_nLedPatternQueued == 0)
			goto apply;
		pattern = s_nLedPatternQueued;
	}
	s_nLedPatternQueued = 0;

apply:
	s_bLedPatternActive = TRUE;
	g_nLedPattern = pattern;
	BYTE sequence;
	switch (pattern)
	{
	case LED_DEFAULT:
		// The settings pointer here is a separate copy (0x002788E4).
		if (CAppSettings::GetInstance()->m_prefs.m_nLEDDefault == 0)
			SmcSetLedAuto();
		else
			SetLedPattern(CAppSettings::GetInstance()->m_prefs.m_nLEDDefault, TRUE);
		s_bLedPatternActive = FALSE;
		return TRUE;
	case LED_RED: sequence = 0xF0; break;
	case LED_ORANGE: sequence = 0xFF; break;
	case LED_OFF: sequence = 0x00; break;
	case LED_GREEN: sequence = 0x0F; break;
	case LED_FLASHING_RED: sequence = 0xC0; break;
	case LED_FLASHING_ORANGE: sequence = 0xCC; break;
	case LED_FLASHING_GREEN: sequence = 0x0C; break;
	case LED_GREEN_ORANGE: sequence = 0x3F; break;
	case LED_ORANGE_RED: sequence = 0xF3; break;
	case LED_RED_GREEN: sequence = 0xC3; break;
	default:
		SmcSetLedAuto();
		return FALSE;
	}
	SmcSetLed(sequence);
	return TRUE;
}

// 0x0008F947 (enable) / 0x0008F95C (disable)
void SmcSetResetOnEject(BOOL reset)
{
	SmcWrite(SMC_RESET_ON_EJECT, reset ? 0 : 1);
	Sleep(10);
}

// 0x0009A240
HRESULT RebootToDashboard()
{
	LAUNCH_DATA launchData;
	memset(&launchData, 0, sizeof(launchData));
	XLaunchNewImageA(NULL, &launchData);
	return E_FAIL;
}

// 0x0008FA6A
void LaunchDefaultDashboard()
{
	XLaunchNewImageA(NULL, NULL);
}
