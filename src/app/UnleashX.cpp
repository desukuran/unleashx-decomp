// CUnleashX: construction, screen switching, teardown and the skin/menu
// callbacks. FrameMove is in FrameMove.cpp, Render in AppRender.cpp;
// Initialize is TODO.
#include "UnleashX.h"

#include <cstring>

#include "../audio/AudioDevice.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../audio/MusicManager.h"
#include "../media/WMVideoPlayer.h"
#include "../musicplayer/AudioGUI.h"
#include "../render/Draw.h"
#include "../audio/Sound.h"
#include "../net/FtpServer.h"
#include "../net/Network.h"
#include "../render/D3D.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../ui/Language.h"
#include "../ui/MenuDialogs.h"
#include "../ui/StringIds.h"
#include "../filemgr/FileManager.h"
#include "SettingsPages.h"

CUnleashX *g_pApp;                         // 0x00276204
CAudioGUI *g_pAudioGUI;                    // 0x00276228
HANDLE g_hNetworkThread = INVALID_HANDLE_VALUE;   // 0x0026A630
CXBPackedResource *g_pResource;            // 0x00276200 m_resource of the app
IDirectSound *g_pDirectSound;               // 0x00276224
BOOL g_bFrameDrawn;                        // 0x00276218
float g_fGlobeSpin;                        // 0x0027621C
DWORD g_dwGlobeSpinTime;                   // 0x00276220
static BOOL s_bBackgroundSoundPlaying;     // 0x00278D48

// The XDK framework's resource/font teardown and DirectSound shutdown.
// Released at exit; nothing in the binary sets them.
IDirect3DTexture8 *g_pTexture270FB0;       // 0x00270FB0
IDirect3DTexture8 *g_pTexture270FB4;       // 0x00270FB4
HANDLE g_hExitEvent;                       // 0x0027624C

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
	m_pOrbVB = NULL;
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
	m_bDiscPresent = 0;
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

// 0x0002B81C (deleting 0x0002B8BB)
CUnleashX::~CUnleashX()
{
	Cleanup();
	// (Then the members: the meshes, the menu, the fonts, the resource.)
}

// 0x000DFF17
// The globe's swaying (<Main Globe>): 512 heights that speed up over 128
// steps, run evenly for 256 and slow down over 128.
void CUnleashX::BuildGlobePath()
{
	const float accel = 1.5978967e-05f;    // 0x0025C8E0
	const float speed = 0.0020453078f;     // 0x0025C8DC (128 * accel)
	float height = 0.0f;
	int i;
	for (i = 0; i < 128; i++)
	{
		m_globePath[i] = height;
		height = (float)i * accel + height;
	}
	for (i = 0; i < 256; i++)
	{
		m_globePath[128 + i] = height;
		height += speed;
	}
	for (i = 128; i > 0; i--)
	{
		m_globePath[384 + 128 - i] = height;
		height = (float)i * accel + height;
	}
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
	if (m_pOrbVB != NULL)
	{
		m_pOrbVB->Release();
		m_pOrbVB = NULL;
	}
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
		m_fonts[i].Destroy();
	ReleaseTexture(g_pTexture270FB4);
	ReleaseTexture(g_pTexture270FB0);
	m_resource.Destroy();

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

// 0x000E1838
void CUnleashX::CSkinEvent::DrawImage(int source, int left, int top, int width, int height, float opacity)
{
	IDirect3DTexture8 *texture;
	if (source == 1)
	{
		texture = m_pApp->m_menu.m_pIconTexture;
	}
	else if (source == 2)
	{
		if (g_skin.m_pPreviewVideo != NULL)
			g_skin.m_pPreviewVideo->Render();
		return;
	}
	else if (source == 3)
	{
		texture = g_pMusicManager != NULL ? g_pMusicManager->GetAlbumArt() : NULL;
	}
	else
	{
		return;
	}
	if (texture == NULL)
		return;
	float x, y, w, h;
	SkinRectToScreen(left, top, width, height, &x, &y, &w, &h);
	DrawTexture(texture, x, y, w, h, opacity, NULL, 0.0f, NULL);
}

// 0x000E040C
void CUnleashX::CSkinEvent::WaitForVerticalBlank()
{
	g_pd3dDevice->BlockUntilVerticalBlank();
}

// 0x000E0538
// The preview's sound replaces the music and the background sound.
void CUnleashX::CSkinEvent::OnPreviewSoundStart()
{
	if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
		g_pMusicManager->Pause();
	if (g_pApp != NULL)
		g_pApp->PlayBackgroundSound(FALSE);
}

// 0x000EA055
// The music resumes (the background sound only with soundtracks off), or
// without music the background sound comes back.
void CUnleashX::CSkinEvent::OnPreviewSoundEnd()
{
	if (m_pApp->m_bStartMusic && g_pMusicManager != NULL)
	{
		g_pApp->PlayBackgroundSound(g_pSettings->m_audio.m_nSoundTrackMode == 0);
		g_pMusicManager->Resume();
		return;
	}
	if (g_pApp != NULL)
		g_pApp->PlayBackgroundSound(TRUE);
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

// 0x000E2604
// The new build runs from D: (wherever the updater unpacked it); the
// launch data names the installed dashboard's XBE.
BOOL CUnleashX::InstallUpdate()
{
	m_bUpdatePending = FALSE;
	BOOL installed = FALSE;
	if (g_pSettings->m_bRunFromDVD || g_pSettings->m_bHomeNotOnD)
		return FALSE;
	if (g_pSettings->m_dwLaunchDataType != 4 || *(const DWORD *)g_pSettings->m_launchData != 0x9E115330)
		return FALSE;
	const char *installedXbe = (const char *)g_pSettings->m_launchData + 4;
	if (installedXbe[0] == '\0' || !FileExists(installedXbe))
		return FALSE;

	std::string newXbe;
	ResolvePath("D:\\", newXbe);
	StrAddBackslash(newXbe);
	newXbe.append(GetPathFileName(g_pSettings->m_strLaunchPath.c_str(), TRUE));
	if (!FileExists(newXbe.c_str()))
		return FALSE;

	// <old>.xbe -> <old>.bak, then the new one in its place (and back on
	// failure).
	std::string backup = installedXbe;
	backup = StrLeft(backup, (int)backup.size() - 4);
	backup.append(".bak");
	SetFileAttributesA(backup.c_str(), FILE_ATTRIBUTE_NORMAL);
	DeleteFileA(backup.c_str());
	BOOL moved = MoveFileA(installedXbe, backup.c_str());
	if (moved)
	{
		moved = MoveFileA(newXbe.c_str(), installedXbe);
		if (!moved)
			MoveFileA(backup.c_str(), installedXbe);
	}
	if (moved)
	{
		installed = TRUE;
		std::string message;
		StrFormat(message, GetStringA(STR_MSG_DASHUPDATE_SUCCESS), backup.c_str());
		ShowMessage(message.c_str(), GetString(STR_SUCCESS), 0);
	}
	else
	{
		ShowMessage(GetStringA(STR_MSG_DASHUPDATE_FAILED), GetString(STR_ERROR), 0);
	}
	m_nMode = MODE_REBOOT;
	return installed;
}
