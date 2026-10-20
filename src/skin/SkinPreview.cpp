// The skin's preview video (<Main><Menu><Preview>) and switching skins.
// Xbox only (the video player, Direct3D).
#include <cstdlib>
#include <cstring>

#include "../audio/MusicManager.h"
#include "../common/StrUtil.h"
#include "../media/WMVideoPlayer.h"
#include "../render/D3D.h"
#include "../render/Texture.h"
#include "Skin.h"

// 0x000F90E6 (this in ecx)
void CSkin::TogglePreviewFullScreen()
{
	if (m_pPreviewVideo == NULL)
		return;
	m_bPreviewToggled = !m_bPreviewToggled;
	m_pPreviewVideo->SetFullScreen(!m_bPreviewToggled);
}

// 0x000FFDAA (playing in eax, this in ecx)
void CSkin::SetPreviewPlaying(BOOL playing)
{
	if (!m_bPreview)
		return;
	m_bPreviewPlaying = playing;
	if (m_strPreviewFile.size() == 0 || m_pPreviewVideo == NULL)
		return;
	if (m_pEventSink != NULL)
	{
		// The preview's own sound takes over from the music, or gives it
		// back.
		if (m_pPreviewVideo->HasAudio() && playing && !m_bPreviewMusic && m_bPreviewAudio)
			m_pEventSink->OnPreviewSoundStart();
		else
			m_pEventSink->OnPreviewSoundEnd();
	}
	if (!m_bPreviewPlaying)
		PlayPreview(NULL);
}

// 0x000FD563 (this in esi, file in ebx)
// Quirks: the name is kept through a strdup that's never freed, and a
// name of 4 characters or less stops the preview.
void CSkin::PlayPreview(const char *file)
{
	if (m_bPreview && file != NULL && strlen(file) >= 5)
	{
		m_dwPreviewTick = 0;
		if (StrCmpNoCase(m_strPreviewFile, file) == 0 || m_pPreviewVideo == NULL)
			return;
		if (!m_pPreviewVideo->Open(file))
		{
			m_strPreviewFile.clear();
			return;
		}
		m_strPreviewFile = strdup(file);
		if (m_pPreviewVideo->HasAudio() && m_bPreviewPlaying && m_bPreviewAudio && !m_bPreviewMusic &&
		    g_pMusicManager != NULL && m_pEventSink != NULL)
			m_pEventSink->OnPreviewSoundStart();
		return;
	}

	if (m_pPreviewVideo != NULL)
		m_pPreviewVideo->Close();
	if (m_strPreviewFile.size() != 0)
	{
		// The sink hears about the end when the timer runs out (PollLoad).
		m_strPreviewFile.clear();
		m_dwPreviewTick = GetTickCount();
	}
}

// 0x001017DE (this in eax)
// Quirk: the picture the last program left (CaptureSavedScreen) is made
// first and then replaced by the screen's picture without being released.
void CSkin::ChangeSkin(const char *path)
{
	if (m_pEventSink != NULL)
		m_pEventSink->WaitForVerticalBlank();
	if (m_pBackgroundVideo != NULL)
	{
		m_pBackgroundVideo->Close();
		delete m_pBackgroundVideo;
		m_pBackgroundVideo = NULL;
	}
	if (m_pPreviewVideo != NULL)
	{
		m_pPreviewVideo->Close();
		delete m_pPreviewVideo;
		m_pPreviewVideo = NULL;
	}
	if (m_pLoadingTexture != NULL)
	{
		m_pLoadingTexture->Release();
		m_pLoadingTexture = NULL;
	}
	CaptureSavedScreen(&m_pLoadingTexture, NULL);

	// The screen as it is now, shown while the new skin loads.
	IDirect3DSurface8 *front = NULL;
	g_pd3dDevice->GetBackBuffer(-1, 0, &front);
	D3DSURFACE_DESC desc;
	front->GetDesc(&desc);
	if (desc.Width != 0 && desc.Height != 0)
	{
		D3DDISPLAYMODE mode;
		g_pd3dDevice->GetDisplayMode(&mode);
		g_pd3dDevice->CreateTexture(640, 480, 1, 0, mode.Format, 0, &m_pLoadingTexture);
		IDirect3DSurface8 *level = NULL;
		m_pLoadingTexture->GetSurfaceLevel(0, &level);
		if (level != NULL)
		{
			D3DXLoadSurfaceFromSurface(level, NULL, NULL, front, NULL, NULL, D3DX_FILTER_NONE, 0);
			level->Release();
		}
	}
	front->Release();
	BeginLoad(path);
}
