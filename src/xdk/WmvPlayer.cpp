// The WMV sample's player (CWmvSamplePlayer, 0x0009B53E-0x0009BB70) on the
// XDK's WMV decoder XMO. The decoder library (wmvdec.lib) and its header
// (wmvxmo.h) only came with XDK 4xxx releases; UnleashX linked 1.0.4627.
// Makefile.xbox defines UX_WMVDEC when WMVDEC= points at them; without them
// .wmv files fail to open (XMV still plays). Xbox only. See
// media/WMVideoPlayer.h.
#include "../media/WMVideoPlayer.h"

#include <string.h>

#include "../audio/DSound.h"
#include "../render/D3D.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"

#ifdef UX_WMVDEC

#include <wmvxmo.h>

static IWMVDecoder *Decoder(CWmvSamplePlayer *player)
{
	return (IWMVDecoder *)player->m_pDecoder;
}

static LPDIRECTSOUNDSTREAM Stream(CWmvSamplePlayer *player)
{
	return (LPDIRECTSOUNDSTREAM)player->m_pStream;
}

// 0x0009BB31
CWmvSamplePlayer::CWmvSamplePlayer()
{
	m_nLoops = -1;
	m_nLoop = 0;
	m_pDecoder = NULL;
	m_pDirectSound = NULL;
	m_pStream = NULL;
	m_pAudioBuffer = NULL;
	m_bHasAudio = false;
	m_bAudio = true;
	m_pDecodeTexture = NULL;
	m_pTexture = NULL;
	m_bFrameReady = TRUE;   // (before anything is decoded)
	m_pSettings = CAppSettings::GetInstance();
}

// (the deleting destructor 0x00031BBC)
CWmvSamplePlayer::~CWmvSamplePlayer()
{
	Destroy();
}

// 0x0009B819
void CWmvSamplePlayer::Initialize(IDirectSound *directSound)
{
	m_pDirectSound = directSound;
	IDirectSound_AddRef(directSound);
	m_pAudioBuffer = new BYTE[WMV_AUDIO_PACKETS * WMV_AUDIO_PACKET_SIZE];
	memset(m_packetStatus, 0, sizeof(m_packetStatus));
}

// 0x0009B840
// (Quirk: the sound device is released but the pointer kept.)
void CWmvSamplePlayer::Destroy()
{
	CloseFile();
	if (m_pDirectSound != NULL)
		IDirectSound_Release(m_pDirectSound);
	if (m_pAudioBuffer != NULL)
	{
		delete[] m_pAudioBuffer;
		m_pAudioBuffer = NULL;
	}
}

// 0x0009B7C2
HRESULT CWmvSamplePlayer::CloseFile()
{
	if (m_pDecoder != NULL)
	{
		Decoder(this)->Release();
		m_pDecoder = NULL;
	}
	if (m_pTexture != NULL)
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}
	if (m_pDecodeTexture != NULL)
	{
		m_pDecodeTexture->Release();
		m_pDecodeTexture = NULL;
	}
	if (m_pStream != NULL)
	{
		IDirectSoundStream_Pause(Stream(this), DSSTREAMPAUSE_RESUME);
		IDirectSoundStream_Release(Stream(this));
		m_pStream = NULL;
	}
	return S_OK;
}

// 0x0009B564
HRESULT CWmvSamplePlayer::GetVideoInfo(SWmvVideoInfo *info)
{
	*info = m_videoInfo;
	return S_OK;
}

// 0x0009B53E (this in eax): the first audio packet the stream is done
// with. FALSE (and *index untouched) if all are still queued.
static BOOL FindReadyPacket(CWmvSamplePlayer *player, DWORD *index)
{
	for (DWORD i = 0; i < WMV_AUDIO_PACKETS; i++)
	{
		if (player->m_packetStatus[i] != XMEDIAPACKET_STATUS_PENDING)
		{
			*index = i;
			return TRUE;
		}
	}
	return FALSE;
}

// 0x0009B589
// Asks the decoder for video and audio until it has a frame and then runs
// out of data. Each round fills one free audio packet; at the end of the
// file it rewinds while loops are left (pausing the sound until the next
// frame is shown). Quirk: without a free packet the audio still decodes
// into packet 0's buffer, which the stream may be playing.
HRESULT CWmvSamplePlayer::DecodeNext()
{
	IWMVDecoder *decoder = Decoder(this);
	if (decoder == NULL)
		return E_FAIL;

	BOOL gotFrame = FALSE;
	HRESULT hr;
	do
	{
		XMEDIAPACKET video;
		XMEDIAPACKET audio;
		memset(&video, 0, sizeof(video));
		memset(&audio, 0, sizeof(audio));
		DWORD videoSize = 0;
		DWORD videoStatus;
		REFERENCE_TIME videoTime;
		DWORD audioSize = 0;
		DWORD audioStatus;
		REFERENCE_TIME audioTime;
		DWORD packet = 0;
		BOOL freePacket = FALSE;
		XMEDIAPACKET *audioOut = NULL;
		if (m_bHasAudio)
		{
			freePacket = FindReadyPacket(this, &packet);
			audio.pvBuffer = m_pAudioBuffer + packet * WMV_AUDIO_PACKET_SIZE;
			audio.dwMaxSize = WMV_AUDIO_PACKET_SIZE;
			audio.pdwCompletedSize = &audioSize;
			audio.pdwStatus = &audioStatus;
			audio.prtTimestamp = &audioTime;
			audioOut = &audio;
		}
		video.pvBuffer = m_pFrameBits;
		video.dwMaxSize = (m_videoInfo.height * m_videoInfo.bitsPerPixel * m_videoInfo.width) >> 3;
		video.pdwCompletedSize = &videoSize;
		video.pdwStatus = &videoStatus;
		video.prtTimestamp = &videoTime;
		// One frame per call: after it, only audio.
		XMEDIAPACKET *videoOut = gotFrame ? NULL : &video;

		hr = decoder->ProcessMultiple(videoOut, audioOut);
		if (hr == S_FALSE)
		{
			// The end of the file.
			m_nLoop++;
			if (m_nLoops == -1 || m_nLoop < m_nLoops)
			{
				decoder->Flush();
				if (m_pStream != NULL)
					IDirectSoundStream_Pause(Stream(this), DSSTREAMPAUSE_PAUSE);
				m_bRestartClock = TRUE;
				m_llStartTime = 0;
				hr = decoder->ProcessMultiple(videoOut, m_bHasAudio ? &audio : NULL);
			}
			else if (m_pStream != NULL)
			{
				IDirectSoundStream_Discontinuity(Stream(this));
			}
		}

		if (m_bHasAudio && m_bAudio && freePacket && audioSize != 0)
		{
			// Hand the packet to the stream, which sets its status.
			audio.dwMaxSize = audioSize;
			audio.pdwStatus = &m_packetStatus[packet];
			audio.pdwCompletedSize = NULL;
			audio.prtTimestamp = NULL;
			if (m_pStream != NULL)
				IDirectSoundStream_Process(Stream(this), &audio, NULL);
		}
		if (videoSize != 0)
		{
			m_llFrameTime = videoTime;
			gotFrame = TRUE;
			m_bFrameReady = TRUE;
		}
	} while (hr == S_OK);
	return hr != XMEDIAPACKET_STATUS_PENDING;
}

// 0x0009B737
BOOL CWmvSamplePlayer::IsFrameDue()
{
	if (m_pDecoder == NULL)
		return FALSE;
	REFERENCE_TIME now;
	IDirectSound_GetTime(m_pDirectSound, &now);
	return now - m_llStartTime >= m_llFrameTime;
}

// 0x0009B86D
// (The texture isn't unlocked, and the copy assumes both have its pitch.)
HRESULT CWmvSamplePlayer::CopyFrame(IDirect3DTexture8 *texture)
{
	D3DLOCKED_RECT locked;
	texture->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
	memcpy(locked.pBits, m_pFrameBits, m_videoInfo.height * locked.Pitch);
	if (m_bRestartClock)
	{
		// The first frame (of each loop): the clock and the sound start.
		m_bRestartClock = FALSE;
		if (m_pStream != NULL)
			IDirectSoundStream_Pause(Stream(this), DSSTREAMPAUSE_RESUME);
		IDirectSound_GetTime(m_pDirectSound, &m_llStartTime);
	}
	m_bFrameReady = FALSE;
	return S_OK;
}

// 0x0009BAEC
IDirect3DTexture8 *CWmvSamplePlayer::GetTexture()
{
	if (m_pTexture == NULL)
		g_pd3dDevice->CreateTexture(m_videoInfo.width, m_videoInfo.height, 1, 0, D3DFMT_YUY2, 0, &m_pTexture);
	if (m_bFrameReady)
		CopyFrame(m_pTexture);
	return m_pTexture;
}

// 0x0009B8D6
// The decoder outputs YUY2; format is the frame texture's. With 5.1 on, a
// mono or stereo sound track goes through the speaker mix bins (as
// WAVEFORMATEXTENSIBLE if they have more than two). Quirk: a file without a
// picture size fails with the decoder left open.
HRESULT CWmvSamplePlayer::OpenFile(const char *path, DWORD format)
{
	CloseFile();
	HRESULT hr = WmvCreateDecoder(path, NULL, WMVVIDEOFORMAT_YUY2, NULL, (LPWMVDECODER *)&m_pDecoder);
	if (FAILED(hr))
		return hr;
	IWMVDecoder *decoder = Decoder(this);
	m_nLoop = 0;
	m_bHasAudio = true;
	decoder->GetVideoInfo((LPWMVVIDEOINFO)&m_videoInfo);
	if (m_videoInfo.height == 0 || m_videoInfo.width == 0)
		return E_FAIL;
	if (decoder->GetAudioInfo((LPWAVEFORMATEX)m_waveFormat) != S_OK)
		m_bHasAudio = false;
	REFERENCE_TIME preroll;
	decoder->GetPlayDuration((PREFERENCE_TIME)&m_llDuration, &preroll);
	m_llDuration -= preroll;

	m_pDecodeTexture = NULL;
	g_pd3dDevice->CreateTexture(m_videoInfo.width, m_videoInfo.height, 1, 0, (D3DFORMAT)format, 0, &m_pDecodeTexture);
	D3DLOCKED_RECT locked;
	m_pDecodeTexture->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
	m_pFrameBits = (BYTE *)locked.pBits;
	m_llFrameTime = 0;

	BOOL started = FALSE;
	if (!m_bHasAudio)
	{
		m_pStream = NULL;
	}
	else
	{
		DSSTREAMDESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.dwMaxAttachedPackets = WMV_AUDIO_PACKETS;
		desc.lpwfxFormat = (LPWAVEFORMATEX)m_waveFormat;
		memset(m_packetStatus, 0, sizeof(m_packetStatus));
		LPDIRECTSOUNDSTREAM stream = NULL;
		if (SUCCEEDED(DirectSoundCreateStream(&desc, &stream)) && stream != NULL)
		{
			m_pStream = stream;
			IDirectSoundStream_Pause(stream, DSSTREAMPAUSE_PAUSE);
			if (m_bAudio)
			{
				const WAVEFORMATEX *wfx = (const WAVEFORMATEX *)m_waveFormat;
				LPCDSMIXBINS mixBins = NULL;
				if (wfx->nChannels <= 2)
					mixBins = XDK_CAST(LPCDSMIXBINS, m_pSettings->m_audio.GetMixBins());
				if (mixBins != NULL)
				{
					DSVOICEPROPS props;
					IDirectSoundStream_GetVoiceProperties(stream, &props);
					if (props.dwMixBinCount != mixBins->dwMixBinCount)
					{
						IDirectSoundStream_SetMixBins(stream, mixBins);
						if (mixBins->dwMixBinCount > 2)
						{
							WAVEFORMATEXTENSIBLE extensible;
							memset(&extensible, 0, sizeof(extensible));
							memcpy(&extensible.Format, wfx, sizeof(WAVEFORMATEX));
							extensible.Samples.wValidBitsPerSample = 0;
							extensible.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
							extensible.Format.cbSize = 0x16;
							extensible.dwChannelMask = 0x3F;   // 5.1
							extensible.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
							IDirectSoundStream_SetFormat(stream, (LPCWAVEFORMATEX)&extensible);
						}
					}
				}
				IDirectSoundStream_SetHeadroom(stream, 1200);
				IDirectSoundStream_Pause(stream, DSSTREAMPAUSE_RESUME);
				IDirectSoundStream_SetVolume(stream, m_pSettings->GetMusicVolume());
			}
			started = TRUE;
		}
		else
		{
			m_pStream = stream;
			m_bHasAudio = false;
		}
	}
	if (!started)
		m_bAudio = false;

	m_llStartTime = 0;
	m_bRestartClock = TRUE;
	DecodeNext();
	return S_OK;
}

#else   // no WMV decoder library: .wmv files fail to open

CWmvSamplePlayer::CWmvSamplePlayer()
{
	memset(this, 0, sizeof(*this));
	m_nLoops = -1;
}
CWmvSamplePlayer::~CWmvSamplePlayer() {}
void CWmvSamplePlayer::Initialize(IDirectSound *) {}
void CWmvSamplePlayer::Destroy() {}
HRESULT CWmvSamplePlayer::OpenFile(const char *, DWORD) { return E_NOTIMPL; }
HRESULT CWmvSamplePlayer::CloseFile() { return S_OK; }
HRESULT CWmvSamplePlayer::GetVideoInfo(SWmvVideoInfo *info) { memset(info, 0, sizeof(*info)); return E_FAIL; }
HRESULT CWmvSamplePlayer::DecodeNext() { return 1; }
BOOL CWmvSamplePlayer::IsFrameDue() { return FALSE; }
HRESULT CWmvSamplePlayer::CopyFrame(IDirect3DTexture8 *) { return E_FAIL; }
IDirect3DTexture8 *CWmvSamplePlayer::GetTexture() { return NULL; }

#endif
