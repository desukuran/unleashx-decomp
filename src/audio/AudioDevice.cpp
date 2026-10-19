// CAudioDevice: the shared DirectSound device (0x0007FB84-0x0007FCDC).
// See AudioDevice.h.
#include "AudioDevice.h"

#include <cstring>

#include "../common/XboxKernel.h"

static CAudioDevice *s_pAudioDevice;   // 0x002776D8

// 0x0007FBA6
CAudioDevice::CAudioDevice()
{
	m_bStopping = FALSE;
	m_pDirectSound = NULL;
	m_hWorkThread = NULL;
}

// 0x0007FBB1
CAudioDevice *CAudioDevice::GetInstance()
{
	if (s_pAudioDevice == NULL)
		s_pAudioDevice = new CAudioDevice;
	return s_pAudioDevice;
}

// 0x00011472
IDirectSound *CAudioDevice::GetDirectSound()
{
	if (m_pDirectSound == NULL)
		Create(TRUE);
	return m_pDirectSound;
}

// 0x0007FB84
DWORD __stdcall CAudioDevice::WorkThread(void *param)
{
	const BYTE *stopping = (const BYTE *)param;
	if (stopping != NULL)
	{
		while (!*stopping)
		{
			DirectSoundDoWork();
			Sleep(100);
		}
	}
	return 0;
}

// 0x0007FC65
HRESULT CAudioDevice::Create(BOOL workThread)
{
	if (m_pDirectSound != NULL)
		return S_FALSE;
	XGetAudioFlags();
	HRESULT hr = DirectSoundCreate(NULL, &m_pDirectSound, NULL);
	if (FAILED(hr))
		return hr;
	hr = IDirectSound_SetMixBinHeadroom(m_pDirectSound, 0, 0);
	if ((BYTE)workThread)
	{
		DWORD threadId;
		m_hWorkThread = CreateThread(NULL, 0, WorkThread, this, 0, &threadId);
		if (m_hWorkThread == NULL)
		{
			DWORD error = GetLastError();
			return (int)error > 0 ? (HRESULT)((error & 0xFFFF) | 0x80070000) : (HRESULT)error;
		}
	}
	return hr;
}

// 0x0007FC2E
HRESULT CAudioDevice::Shutdown()
{
	m_bStopping = TRUE;
	if (m_hWorkThread != NULL)
		WaitForSingleObject(m_hWorkThread, INFINITE);
	if (m_pDirectSound != NULL)
	{
		IDirectSound_Release(m_pDirectSound);
		m_pDirectSound = NULL;
	}
	if (m_hWorkThread != NULL)
	{
		CloseHandle(m_hWorkThread);
		m_hWorkThread = NULL;
	}
	return S_OK;
}

// 0x0007FCDC
HRESULT CAudioDevice::CreateSoundBuffer(const WAVEFORMATEX *format, DWORD bytes, DWORD flags,
                                        IDirectSoundBuffer **buffer)
{
	*buffer = NULL;
	// Quirk: the device is (re)created only when it already exists (a
	// no-op); it's made at startup, so this doesn't matter in practice.
	if (m_pDirectSound != NULL)
	{
		HRESULT hr = Create(TRUE);
		if (FAILED(hr))
			return hr;
	}
	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = flags;
	desc.dwBufferBytes = bytes;
	desc.lpwfxFormat = (WAVEFORMATEX *)format;
	IDirectSoundBuffer *created = NULL;
	HRESULT hr = IDirectSound_CreateSoundBuffer(m_pDirectSound, &desc, &created, NULL);
	if (SUCCEEDED(hr))
		IDirectSoundBuffer_SetHeadroom(created, 1200);
	*buffer = created;
	return hr;
}

// 0x0007FBDA
void CAudioDevice::FillBuffer(IDirectSoundBuffer *buffer, DWORD offset, DWORD size, const void *data)
{
	void *locked;
	DWORD lockedSize;
	if (SUCCEEDED(IDirectSoundBuffer_Lock(buffer, offset, size, &locked, &lockedSize, NULL, NULL, 0)))
	{
		memcpy(locked, data, size);
		IDirectSoundBuffer_Unlock(buffer, locked, lockedSize, NULL, 0);
	}
}
