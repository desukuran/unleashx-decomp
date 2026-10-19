// CUnleashX: construction, screen switching, teardown and the skin/menu
// callbacks. FrameMove is in FrameMove.cpp, Render in AppRender.cpp;
// Initialize is TODO.
#include "UnleashX.h"

#include <cstring>

#include "../audio/AudioDevice.h"
#include "../audio/MusicManager.h"
#include "../audio/Sound.h"
#include "../net/FtpServer.h"
#include "../net/Network.h"
#include "../render/D3D.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../ui/MenuDialogs.h"
#include "SettingsPages.h"

CUnleashX *g_pApp;                         // 0x00276204
CAudioGUI *g_pAudioGUI;                    // 0x00276228
HANDLE g_hNetworkThread = INVALID_HANDLE_VALUE;   // 0x0026A630
CXBPackedResource *g_pResource;            // 0x00276200 m_resource of the app
IDirectSound *g_pDirectSound;               // 0x00276224
BOOL g_bMenuPending;                       // 0x0027623C
BOOL g_bFrameDrawn;                        // 0x00276218
float g_fGlobeSpin;                        // 0x0027621C
DWORD g_dwGlobeSpinTime;                   // 0x00276220
static BOOL s_bBackgroundSoundPlaying;     // 0x00278D48

// The XDK framework's resource/font teardown and DirectSound shutdown.
void CXBFontDestroy(CXBFontEx *font);      // 0x00098FFE CXBFont::Destroy
void CXBPackedResourceDestroy(CXBPackedResource *resource);   // 0x00097104
extern IDirect3DTexture8 *g_pTexture270FB0;   // 0x00270FB0
extern IDirect3DTexture8 *g_pTexture270FB4;   // 0x00270FB4

// 0x000E229C
CUnleashX::CUnleashX()
{
	m_nMode = 0;
	m_nPrevMode = 1;
	g_pSettings = CAppSettings::GetInstance();
	m_pDefaultGamepad = &m_defaultGamepad;
	m_pDefaultIR = &m_defaultIR;
	m_nGlobePos = 0;
	m_nGlobeStep = 1;
	g_pResource = &m_resource;
	m_bStartMusic = 0;
	m_bNoHardDisk = FALSE;
	g_pDirectSound = NULL;
	g_pMusicManager = NULL;
	m_unknown9CC = 0;
	m_pSplash = NULL;
	m_unknown9D4 = 0;
	m_pTexture9D8 = NULL;
	m_unknown9C0 = 0;
	m_d3dpp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN;
	m_unknown804 = 0;
	m_unknown878 = 0.2f;
	m_menuEvent.m_pApp = this;
	m_skinEvent.m_pApp = this;
	m_pTexture9E4 = NULL;
	m_pCellWall = NULL;
	m_pEarthEnvMap = NULL;
	m_pOrb = NULL;
	m_pTexture7D8 = NULL;
	m_pDefaultLoadingIcon = NULL;
	m_pDashCellWall = NULL;
	m_pDashOrb = NULL;
	m_pDashLogo = NULL;
	m_pFileIcon = NULL;
	m_pFolderIcon = NULL;
	m_pGameManager = NULL;
	m_pFileManager = NULL;
	m_pMessageBox = NULL;
	m_pSkinBrowser = NULL;
	m_pSystemInfo = NULL;
	m_pTextEditor = NULL;
	m_bMessageShown = FALSE;
	m_unknown1964 = 0;
	m_bTakeScreenshot = FALSE;
	m_pBackgroundSound = NULL;
	m_pScreenSaver = NULL;
	m_bScreenSaverActive = 0;
	m_bKeepAwake = 0;
	m_bAutoLaunchDisc = 0;
	m_pSettingsMenu = NULL;
	m_pClockEditor = NULL;
	m_pPassCode = NULL;
	m_hThread2248 = NULL;
	m_bEditOnYes = 0;
	m_bShowReleaseNfo = 1;
	m_bUpdatePending = FALSE;
}

// 0x000E045B
// Never with SoundTrackMode set (the soundtrack plays instead).
void CUnleashX::PlayBackgroundSound(BOOL play)
{
	if (g_pSettings->m_audio.m_nSoundTrackMode != 0)
		play = FALSE;
	if (m_pBackgroundSound == NULL || m_pBackgroundSound->m_pBuffer == NULL)
		return;
	IDirectSoundBuffer *buffer = m_pBackgroundSound->m_pBuffer;
	if (!play)
	{
		if (s_bBackgroundSoundPlaying)
		{
			buffer->SetCurrentPosition(0);
			buffer->Stop();
			s_bBackgroundSoundPlaying = FALSE;
		}
	}
	else if (!s_bBackgroundSoundPlaying)
	{
		buffer->SetVolume(g_pSettings->GetSoundVolume());
		m_pBackgroundSound->Play(DSBPLAY_LOOPING);
		s_bBackgroundSoundPlaying = TRUE;
	}
}

// 0x000E568F
void CUnleashX::SetMode(int mode)
{
	int previous = m_nMode;
	if (previous == mode)
		return;
	m_nMode = mode;
	m_nPrevMode = previous;

	if (mode == MODE_MENU)
	{
		g_bOperationFailed = FALSE;
	}
	else if (mode == MODE_FILEMANAGER)
	{
		if (m_pFileManager == NULL)
		{
			SetMode(MODE_MENU);
		}
		else
		{
			m_pFileManager->m_bDrawBorder = g_skin.m_bFileManagerDrawBorder;
			m_pFileManager->m_bSafeBorder = g_skin.m_bFileManagerSafeBorder;
			m_pFileManager->Reset();
			m_pFileManager->SetBackground(g_skin.GetFileManagerBackground());
		}
	}
	else if (mode == MODE_SYSINFO)
	{
		if (m_pSystemInfo == NULL)
		{
			m_pSystemInfo = new CXboxInfo;
			m_pSystemInfo->Init(&m_fonts[4]);
		}
		m_pSystemInfo->Show();
	}
	else if (mode == MODE_MUSIC)
	{
		if (g_pAudioGUI == NULL)
		{
			g_pAudioGUI = new CAudioGUI;
			if (g_pAudioGUI == NULL)
			{
				SetMode(MODE_MENU);
				goto sound;
			}
			g_pAudioGUI->Init(&m_fonts[4]);
			if (g_pAudioGUI == NULL)
			{
				SetMode(MODE_MENU);
				goto sound;
			}
		}
		g_pAudioGUI->Show();
	}

sound:
	// The background sound plays in the menu screens, unless music or a
	// preview video plays.
	if (m_pBackgroundSound == NULL || m_pBackgroundSound->m_pBuffer == NULL)
		return;
	BOOL play = FALSE;
	if (m_nMode != MODE_FILEMANAGER && m_nMode != MODE_MUSIC)
	{
		if (g_pMusicManager == NULL || g_pMusicManager->GetState() != MUSIC_PLAYING)
			play = !g_skin.IsPreviewPlaying() && g_pSettings->m_audio.m_nSoundTrackMode == 0;
	}
	PlayBackgroundSound(play);
}

// 0x000E1B1E
HRESULT CUnleashX::Cleanup()
{
	if (IsNetworkReady())
	{
		StopFtpServer();
		StopNetwork();
	}
	if (m_pBackgroundSound != NULL)
	{
		PlayBackgroundSound(FALSE);
		delete m_pBackgroundSound;
		m_pBackgroundSound = NULL;
	}
	delete m_pClockEditor;
	m_pClockEditor = NULL;
	ReleaseTexture(m_pTexture9D8);
	ReleaseTexture(m_pTexture9E4);
	delete m_pFileManager;
	m_pFileManager = NULL;
	delete m_pGameManager;
	m_pGameManager = NULL;
	delete m_pMessageBox;
	m_pMessageBox = NULL;
	delete m_pSkinBrowser;
	m_pSkinBrowser = NULL;
	delete m_pSystemInfo;
	m_pSystemInfo = NULL;
	delete m_pScreenSaver;
	m_pScreenSaver = NULL;
	delete m_pSettingsMenu;
	m_pSettingsMenu = NULL;
	delete m_pTextEditor;
	m_pTextEditor = NULL;
	delete m_pPassCode;
	m_pPassCode = NULL;
	ReleaseTexture(m_pCellWall);
	ReleaseTexture(m_pOrb);
	ReleaseTexture(m_pTexture7D8);
	ReleaseTexture(m_pEarthEnvMap);
	ReleaseTexture(m_pFileIcon);
	ReleaseTexture(m_pFolderIcon);
	ReleaseTexture(m_pDashCellWall);
	ReleaseTexture(m_pDashOrb);
	for (int i = 0; i < 6; i++)
		CXBFontDestroy(&m_fonts[i]);
	ReleaseTexture(g_pTexture270FB4);
	ReleaseTexture(g_pTexture270FB0);
	CXBPackedResourceDestroy(&m_resource);

	if (m_hThread2248 != NULL && m_hThread2248 != INVALID_HANDLE_VALUE)
		WaitForSingleObject(m_hThread2248, 200);
	delete g_pAudioGUI;
	g_pAudioGUI = NULL;
	if (g_pMusicManager != NULL)
	{
		g_pMusicManager->Stop();
		delete g_pMusicManager;
		g_pMusicManager = NULL;
	}
	CAudioDevice::GetInstance()->Shutdown();
	if (g_hNetworkThread != NULL && g_hNetworkThread != INVALID_HANDLE_VALUE)
		WaitForSingleObject(g_hNetworkThread, 200);
	return S_OK;
}

// 0x000E4173
void CUnleashX::CMenuEvent::OnPreview(const char *file)
{
	g_skin.PlayPreview(file);
}

// 0x000E5824
// Applies the skin's <Menu> settings to the menu.
void CUnleashX::CSkinEvent::OnSkinLoaded()
{
	CMenu &menu = m_pApp->m_menu;
	if (g_skin.m_bRotate)
	{
		menu.m_bFixed = false;
		menu.m_bWrap = true;
	}
	else
	{
		menu.m_bWrap = false;
		if (g_skin.m_bFixed)
		{
			menu.m_bFixed = true;
			menu.m_nFixAt = g_skin.m_nFixAt;
		}
		else
		{
			menu.m_bFixed = false;
		}
	}
	menu.m_bScrollText = g_skin.m_bScroll;
	menu.m_nMaxVisibleItems = g_skin.m_nMaxItem;
	menu.m_bSmoothScroll = g_skin.m_bSmooth;
	if (g_skin.m_nAlign == 1)
		menu.m_dwTextAlign = 0x0E;   // centered
	else if (g_skin.m_nAlign == 2)
		menu.m_dwTextAlign = 0x0D;   // right
	else
		menu.m_dwTextAlign = 0x0C;   // left
	menu.m_bNoIcons = !g_skin.m_bShowIcon;
	menu.m_bShowArrows = g_skin.m_bShowArrows;
	menu.m_nBorderWidth = g_skin.m_nBorderWidth;
	menu.SetFont(g_skin.m_pMenuFont);
	menu.SetArea((float)g_skin.m_rcMenu.left, (float)g_skin.m_rcMenu.top, (float)g_skin.m_rcMenu.width,
	             (float)g_skin.m_rcMenu.height);
	menu.SetHorizontal(g_skin.m_bHorizontal);
	menu.SetItemPositions(g_skin.m_nItemPosCount, g_skin.m_itemPos);
	menu.m_bShowPreviews = g_skin.m_bPreview;

	// No <LoadingScreen> preview setting: launches show the default image.
	if (g_skin.m_strLoadingPreview.size() == 0)
		g_pSettings->m_pLoadingIcon = m_pApp->m_pDefaultLoadingIcon;
	g_pSettings->m_bLoadingShowInfo = g_skin.m_bLoadingShowInfo;
	if (!g_skin.m_bColorScheme)
		g_colorScheme.Load(&g_config);
	if (g_skin.m_bOrb)
		m_pApp->InitOrb();
	menu.ReloadList();
}
