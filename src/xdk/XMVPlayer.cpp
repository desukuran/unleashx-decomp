// The XDK samples' xmvhelper CXMVPlayer as UnleashX modified it (sound on or
// off, looping, the music volume). The WMV player is in WmvPlayer.cpp. Xbox
// only. See media/WMVideoPlayer.h.
#include "../media/WMVideoPlayer.h"

#include <stdarg.h>
#include <stdio.h>
#include <xmv.h>

#include "../audio/DSound.h"
#include "../settings/AppSettings.h"

// 0x000930B9: printf to the debugger.
void XBUtil_DebugPrint(const char *format, ...)
{
	char text[512];
	va_list args;
	va_start(args, format);
	_vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	text[sizeof(text) - 1] = 0;
	OutputDebugStringA(text);
}

// The load context's fields the player itself touches (xmvhelper's
// LOAD_CONTEXT at +0x5C).
#define LOAD_FILE(player) (*(HANDLE *)((player)->m_loadContext + 0x10))       // +0x6C
#define LOAD_INPUT(player) (*(void **)((player)->m_loadContext + 0x28))       // +0x84

// 0x0008F777 (this in esi)
CXMVPlayer::CXMVPlayer()
{
	m_pXMVDecoder = NULL;
	m_dwWidth = 0;
	m_dwHeight = 0;
	m_dwFramesPerSecond = 0;
	m_dwAudioStreamCount = 0;
	memset(m_AudioDesc, 0, sizeof(m_AudioDesc));
	m_pTextures[0] = NULL;
	m_pTextures[1] = NULL;
	m_dwCurrentFrame = -1;
	LOAD_FILE(this) = INVALID_HANDLE_VALUE;
	m_bPlaying = FALSE;
	m_bOverlaysEnabled = FALSE;
	LOAD_INPUT(this) = NULL;
	m_physicalBuffer = NULL;
	m_bAudio = true;
	m_bLoop = true;
	m_bHasAudio = TRUE;
	m_pSettings = CAppSettings::GetInstance();
}

// (0x00031BDD deletes after this)
CXMVPlayer::~CXMVPlayer()
{
	Destroy();
}

// 0x0008F286
void CXMVPlayer::Destroy()
{
	if (m_bOverlaysEnabled)
	{
		D3DDevice_EnableOverlay(FALSE);
		m_bOverlaysEnabled = FALSE;
	}
	if (m_pXMVDecoder != NULL)
	{
		XMVDecoder_CloseDecoder((XMVDecoder *)m_pXMVDecoder);
		m_pXMVDecoder = NULL;
	}
	m_dwWidth = 0;
	m_dwHeight = 0;
	m_dwFramesPerSecond = 0;
	m_dwAudioStreamCount = 0;
	memset(m_AudioDesc, 0, sizeof(m_AudioDesc));
	for (int i = 0; i < 2; i++)
	{
		if (m_pTextures[i] != NULL)
			m_pTextures[i]->Release();
		m_pTextures[i] = NULL;
	}
	m_dwCurrentFrame = -1;
	m_bPlaying = FALSE;
	if (LOAD_FILE(this) != INVALID_HANDLE_VALUE)
	{
		CloseHandle(LOAD_FILE(this));
		LOAD_FILE(this) = INVALID_HANDLE_VALUE;
	}
	if (LOAD_INPUT(this) != NULL)
	{
		free(LOAD_INPUT(this));
		LOAD_INPUT(this) = NULL;
	}
	if (m_pAudioStream != NULL)
	{
		IDirectSoundStream *stream = (IDirectSoundStream *)m_pAudioStream;
		IDirectSoundStream_Pause(stream, DSSTREAMPAUSE_RESUME);
		IDirectSoundStream_FlushEx(stream, 0, 0);
		stream->Release();
		m_pAudioStream = NULL;
	}
	if (m_physicalBuffer != NULL)
	{
		XPhysicalFree(m_physicalBuffer);
		m_physicalBuffer = NULL;
	}
}

// 0x0008F34C: the video and audio descriptions, the audio streams, and two
// textures to decode into in turn.
static HRESULT FinishOpeningFile(CXMVPlayer *player, DWORD format, BOOL allocateTextures)
{
	XMVDecoder *decoder = (XMVDecoder *)player->m_pXMVDecoder;
	HRESULT hr = S_OK;
	XMVDecoder_GetVideoDescriptor(decoder, (XMVVIDEO_DESC *)&player->m_dwWidth);
	player->m_bHasAudio = player->m_dwAudioStreamCount != 0;
	if (player->m_dwHeight == 0 || player->m_dwWidth == 0)
		return E_FAIL;
	player->m_pAudioStream = NULL;
	// (The 5.1 mix bins are looked up but never passed to the decoder.)
	DSMIXBINS *mixBins = player->m_pSettings->m_audio.m_bEnable51 ? (DSMIXBINS *)&player->m_pSettings->m_audio.m_speakerMixBins
	                                                              : NULL;
	for (DWORD i = 0; i < player->m_dwAudioStreamCount; i++)
	{
		XMVAUDIO_DESC *audio = (XMVAUDIO_DESC *)player->m_AudioDesc;
		XMVDecoder_GetAudioDescriptor(decoder, i, audio);
		if (!player->m_bAudio)
		{
			XMVDecoder_DisableAudioStream(decoder, i);
			hr = S_OK;
		}
		else if (i == 0 && audio->ChannelCount <= 2 && mixBins != NULL)
		{
			// With 5.1 on, a mono or stereo movie plays at the music volume.
			hr = XMVDecoder_EnableAudioStream(decoder, 0, 0, NULL, (IDirectSoundStream **)&player->m_pAudioStream);
			if (player->m_pAudioStream != NULL)
			{
				IDirectSoundStream_SetVolume((IDirectSoundStream *)player->m_pAudioStream,
				                             player->m_pSettings->GetMusicVolume());
				IDirectSoundStream_SetHeadroom((IDirectSoundStream *)player->m_pAudioStream, 0x4B0);
			}
		}
		else
		{
			hr = XMVDecoder_EnableAudioStream(decoder, i, 0, NULL,
			                                  i == 0 ? (IDirectSoundStream **)&player->m_pAudioStream : NULL);
		}
		if (FAILED(hr))
		{
			XBUtil_DebugPrint("Unable to enable audio stream 0 (error %x)\n", hr);
			player->Destroy();
			return hr;
		}
	}
	for (DWORD i = 0; i < 2; i++)
	{
		player->m_pTextures[i] = NULL;
		if (allocateTextures)
		{
			player->m_pTextures[i] = (IDirect3DTexture8 *)D3DDevice_CreateTexture2(
				player->m_dwWidth, player->m_dwHeight, 1, 1, 0, (D3DFORMAT)format, D3DRTYPE_TEXTURE);
			hr = player->m_pTextures[i] != NULL ? S_OK : E_OUTOFMEMORY;
			if (FAILED(hr))
			{
				XBUtil_DebugPrint("Unable to create texture %d (error %x)\n", i, hr);
				player->Destroy();
				return hr;
			}
		}
	}
	player->m_pShowingTexture = player->m_pTextures[0];
	player->m_nDecodeTextureIndex = 0;
	player->m_pDecodingTexture = player->m_pTextures[1];
	player->m_pSubmittedTexture = NULL;
	player->m_bPlaying = TRUE;
	return hr;
}

// 0x0008F6DD: looping playback unless SetLoop(FALSE).
HRESULT CXMVPlayer::OpenFile(const char *path, DWORD format, BOOL allocateTextures)
{
	if (path == NULL)
	{
		XBUtil_DebugPrint("Bad parameter to OpenFile()\n");
		return E_FAIL;
	}
	DWORD flags = m_bLoop ? XMVFLAG_SYNC_ON_NEXT_VBLANK | XMVFLAG_FULL_LOOP : XMVFLAG_SYNC_ON_NEXT_VBLANK;
	HRESULT hr = XMVDecoder_CreateDecoderForFile(flags, path, (XMVDecoder **)&m_pXMVDecoder);
	if (FAILED(hr))
	{
		XBUtil_DebugPrint("Unable to create XMV Decoder for %s (error: %x)\n", path, hr);
		return hr;
	}
	return FinishOpeningFile(this, format, allocateTextures);
}

// 0x0008F15B
void CXMVPlayer::EnableOverlays()
{
	D3DDevice_EnableOverlay(TRUE);
	m_bOverlaysEnabled = TRUE;
}

// 0x0008F21C: the newest frame, or NULL before the first.
IDirect3DTexture8 *CXMVPlayer::AdvanceFrameForTexturing()
{
	IDirect3DSurface8 *surface = D3DTexture_GetSurfaceLevel2(m_pDecodingTexture, 0);
	XMVRESULT result = XMV_NOFRAME;
	XMVDecoder_GetNextFrame((XMVDecoder *)m_pXMVDecoder, surface, &result, NULL);
	if (result == XMV_NEWFRAME)
	{
		m_dwCurrentFrame++;
		m_pShowingTexture = m_pDecodingTexture;
		m_nDecodeTextureIndex = (m_nDecodeTextureIndex + 1) & 1;
		m_pDecodingTexture = m_pTextures[m_nDecodeTextureIndex];
	}
	else if (result == XMV_ENDOFFILE || result == XMV_FAIL)
	{
		m_bPlaying = FALSE;
	}
	if (surface != NULL)
		surface->Release();
	return m_dwCurrentFrame < 0 ? NULL : m_pShowingTexture;
}

// 0x0008F18D: a new frame to show on the overlay, or NULL. A looping movie
// restarts at the end.
IDirect3DTexture8 *CXMVPlayer::AdvanceFrameForOverlays()
{
	if (m_pSubmittedTexture != NULL && D3DDevice_GetOverlayUpdateStatus())
	{
		m_pDecodingTexture = m_pShowingTexture;
		m_pShowingTexture = m_pSubmittedTexture;
		m_pSubmittedTexture = NULL;
	}
	IDirect3DSurface8 *surface = NULL;
	if (m_pDecodingTexture != NULL)
		surface = D3DTexture_GetSurfaceLevel2(m_pDecodingTexture, 0);
	XMVRESULT result = XMV_NOFRAME;
	XMVDecoder_GetNextFrame((XMVDecoder *)m_pXMVDecoder, surface, &result, NULL);
	if (result == XMV_NEWFRAME)
	{
		m_dwCurrentFrame++;
		m_pSubmittedTexture = m_pDecodingTexture;
		m_pDecodingTexture = NULL;
	}
	else if (result == XMV_ENDOFFILE && m_bLoop)
	{
		XMVDecoder_Reset((XMVDecoder *)m_pXMVDecoder);
	}
	else if (result == XMV_ENDOFFILE || result == XMV_FAIL)
	{
		m_bPlaying = FALSE;
	}
	if (surface != NULL)
		surface->Release();
	return result == XMV_NEWFRAME ? m_pSubmittedTexture : NULL;
}

// 0x00031B85: turning looping off ends the current loop.
void CXMVPlayer::SetLoop(BOOL loop)
{
	if (m_bLoop == (loop != FALSE))
		return;
	m_bLoop = loop != FALSE;
	if (!m_bLoop && m_pXMVDecoder != NULL)
		XMVDecoder_TerminateLoop((XMVDecoder *)m_pXMVDecoder);
}
