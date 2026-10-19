// The DirectSound device, created on first use (with a thread that runs
// DirectSoundDoWork), from the MS dashboard's audio code. See
// AudioDevice.cpp.
#pragma once

#include "../common/Platform.h"
#include "DSound.h"

// 0x0C bytes; one instance (0x002776D8).
class CAudioDevice
{
public:
	CAudioDevice();                       // 0x0007FBA6 (this in eax)
	static CAudioDevice *GetInstance();   // 0x0007FBB1
	// Creates the device on first use. (this in eax)
	IDirectSound *GetDirectSound();       // 0x00011472
	// Stops the work thread and releases the device. (this in esi)
	HRESULT Shutdown();                   // 0x0007FC2E
	// The device and, with workThread, the thread that runs
	// DirectSoundDoWork every 100 ms. S_FALSE if it exists. (this in edi)
	HRESULT Create(BOOL workThread);      // 0x0007FC65
	// A sound buffer with 12 dB of headroom. (this in eax)
	HRESULT CreateSoundBuffer(const WAVEFORMATEX *format, DWORD bytes, DWORD flags,
	                          IDirectSoundBuffer **buffer);   // 0x0007FCDC
	// Copies size bytes of data into the buffer at offset.
	static void FillBuffer(IDirectSoundBuffer *buffer, DWORD offset, DWORD size, const void *data);   // 0x0007FBDA

private:
	static DWORD __stdcall WorkThread(void *param);       // 0x0007FB84
public:

	BYTE m_bStopping;                     // +0x00
	IDirectSound *m_pDirectSound;         // +0x04
	HANDLE m_hWorkThread;                 // +0x08
};

// The device from startup; NULL without sound.
extern IDirectSound *g_pDirectSound;      // 0x00276224

// DirectSound (XDK).
HRESULT DirectSoundOverrideSpeakerConfig(DWORD config);
HRESULT DirectSoundUseLightHRTF();
#ifndef _XBOX
// The output meters (XDK).
struct DSOUTPUTLEVELS
{
	DWORD dwAnalogLeftTotalPeak, dwAnalogRightTotalPeak;
	DWORD dwAnalogLeftTotalRMS, dwAnalogRightTotalRMS;
	DWORD dwDigitalFrontLeftPeak, dwDigitalFrontCenterPeak, dwDigitalFrontRightPeak;
	DWORD dwDigitalBackLeftPeak, dwDigitalBackRightPeak, dwDigitalLowFrequencyPeak;
	DWORD dwDigitalFrontLeftRMS, dwDigitalFrontCenterRMS, dwDigitalFrontRightRMS;
	DWORD dwDigitalBackLeftRMS, dwDigitalBackRightRMS, dwDigitalLowFrequencyRMS;
};
HRESULT IDirectSound_GetOutputLevels(IDirectSound *sound, DSOUTPUTLEVELS *levels, BOOL resetPeakValues);   // 0x00193223
#endif
// Loads the effects image (dsstdfx.bin): 3 effects at index 4. TODO: verify the layout
HRESULT XAudioDownloadEffectsImage(const char *file, const void *loc, DWORD flags, void **desc);   // 0x00193538
