// CUnleashX::Initialize: startup. Sets up Direct3D, mounts the drives,
// loads the resources and Config.xml on threads behind the splash screen
// (or a recovery screen if resources are missing), picks the video mode,
// starts sound, the skin, the background threads and the startup password,
// plays the skin's intro movie, then fades the splash out and applies the
// screen calibration.
#include <cstdio>
#include <cwchar>

#include "../audio/AudioDevice.h"
#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../media/WMVideoPlayer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../system/Drives.h"
#include "../system/SystemControl.h"
#include "../system/XSettings.h"
#include "../system/XbeLaunch.h"
#include "../ui/Language.h"
#include "../ui/MenuDialogs.h"
#include "../ui/StringIds.h"
#include "FtpEvents.h"
#include "Screens.h"
#include "UnleashX.h"

// XRegisterThreadNotifyRoutine's record (the routine at +8).
struct SThreadNotification
{
	void *reserved[2];
	void(__stdcall *routine)(BOOL create);
};
static SThreadNotification s_threadNotify;          // 0x00270FA4
void XRegisterThreadNotifyRoutine(SThreadNotification *notification, BOOL registerIt);
// BACK plus a trigger at boot: not a real action, so the BACK handling below
// is skipped.
static const int ACTION_HANDLED_AT_BOOT = 0x30;

BOOL GetExitCodeThread(HANDLE thread, DWORD *exitCode);
DWORD ResumeThread(HANDLE thread);

// The device's default states for the 3D background.
static void SetDefaultRenderStates()
{
	// (The original writes most of these into D3D's state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	g_pd3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	g_pd3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	g_pd3dDevice->SetRenderState(D3DRS_ZBIAS, 0);
	g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	g_pd3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);
	// TODO: a few more deferred states are cleared (cache slots 0x00150B94-
	// 0x00150BAC) and one is set to -1 (0x00150BDC).
}

static void SetIdentity(float *m)
{
	for (int i = 0; i < 16; i++)
		m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

void CUnleashX::DrawSplash(float alpha, const D3DDISPLAYMODE &mode)
{
	if (m_bSplashIsScreen)
		DrawSavedScreen(m_pSplash, mode.Width, mode.Height, alpha);
	else
		DrawTexture(m_pSplash, 0.0f, 0.0f, (float)mode.Width, (float)mode.Height, alpha, NULL, 0.0f, NULL);
}

// 0x000EC94A
HRESULT CUnleashX::Initialize()
{
	g_hExitEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	g_pSettings->SetDefaults();
	g_bFrameDrawn = FALSE;

	// The 3D background: world, view (from 2 in front, slightly above),
	// a white material.
	SetIdentity(m_worldMatrix);
	g_pd3dDevice->SetTransform(D3DTS_WORLD, m_worldMatrix);
	SetIdentity(m_viewMatrix);
	float eye[3] = {0.0f, 0.01f, -2.0f};
	float at[3] = {0.0f, 0.1f, 0.0f};
	float up[3] = {0.0f, 1.0f, 0.0f};
	D3DXMatrixLookAtLH(m_viewMatrix, eye, at, up);
	g_pd3dDevice->SetTransform(D3DTS_VIEW, m_viewMatrix);
	D3DMATERIAL8 material;
	XBUtil_InitMaterial(&material, 1.0f, 1.0f, 1.0f, 1.0f);
	g_pd3dDevice->SetMaterial(&material);
	s_threadNotify.routine = ThreadNotifyRoutine;
	XRegisterThreadNotifyRoutine(&s_threadNotify, TRUE);

	// The drives: the one we run from (F: or G: turn those on), C:, and
	// when run from DVD, E: and the Z: cache.
	if (g_pSettings->m_strHomePath.size() != 0)
	{
		char drive = (char)toupper(StrLeft(g_pSettings->m_strHomePath, 1)[0]);
		if (drive == 'F')
		{
			g_pSettings->m_prefs.m_bEnableDriveF = TRUE;
			SetDriveFMapped(TRUE);
		}
		else if (drive == 'G')
		{
			g_pSettings->m_prefs.m_bEnableDriveF = TRUE;
			g_pSettings->m_prefs.m_bEnableDriveG = TRUE;
			SetDriveFMapped(TRUE);
			SetDriveGMapped(TRUE);
			MountDrive('F');
		}
		MountDrive(drive);
	}
	MountDrive('C');
	if (g_pSettings->m_bRunFromDVD)
	{
		MountDrive('E');
		Sleep(5);
		if (GetFileAttributesA("C:\\") == INVALID_FILE_ATTRIBUTES && GetFileAttributesA("E:\\") == INVALID_FILE_ATTRIBUTES)
			m_bNoHardDisk = TRUE;
		MountDrive('Z');
	}
	g_pd3dDevice->GetViewport((D3DVIEWPORT8 *)&g_pSettings->m_viewport);

	// The resources and Config.xml load on threads.
	HANDLE resourceThread = (HANDLE)_beginthreadex(NULL, 0, LoadResources, this, 0, NULL);
	SetThreadPriority(resourceThread, 1);
	WaitForSingleObject(resourceThread, m_bNoHardDisk ? (DWORD)-1 : 5);
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ConfigLoaderThread, NULL, 0, NULL);
	SetThreadPriority(thread, 1);
	Sleep(1);

	// The splash: what the previous program left on screen, else
	// Splash.jpg (or UXSplash.jpg) in UnleashX's E:\UDATA folder. BACK held
	// skips it.
	m_pSplash = NULL;
	CaptureSavedScreen(&m_pSplash, m_pd3dDevice);
	SetDefaultRenderStates();
	BOOL waitResources = FALSE;   // (never set: the config thread is already running)
	BOOL waitConfig = TRUE;
	BOOL waitStartup = FALSE;
	D3DDISPLAYMODE mode;
	g_pd3dDevice->GetDisplayMode(&mode);
	UpdateGamepads(this);
	int action = g_gamepadInput.GetAction();
	if (m_pSplash == NULL && action != INPUT_BACK)
	{
		MountDrive('E');
		try
		{
			std::string path = "E:\\UDATA\\9e115330\\0064122817A8\\Splash.jpg";
			if (!FileExists(path.c_str()))
				path = StrLeft(path, (int)path.size() - 10) + "UXSplash.jpg";
			if (D3DXCreateTextureFromFileA(m_pd3dDevice, path.c_str(), &m_pSplash) != S_OK)
				m_pSplash = NULL;
			else
				m_bSplashIsScreen = FALSE;
		}
		catch (...)
		{
			m_pSplash = NULL;
		}
	}
	else
	{
		m_bSplashIsScreen = TRUE;
	}

	// Show the splash (fading out over 80 frames) until the threads are
	// done: the config loader, then the startup thread.
	int fade = 80;
	m_fSplashAlpha = 1.0f;
	for (;;)
	{
		try
		{
			g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
			if (m_pSplash == NULL)
			{
				Sleep(1);
			}
			else if (fade != 0)
			{
				m_fSplashAlpha = (float)fade * 0.0125f;
				DrawSplash(m_fSplashAlpha, mode);
			}
			g_pd3dDevice->Swap(0);
		}
		catch (...)
		{
		}
		if (fade != 0)
			fade--;

		if (waitResources)
		{
			if (WaitForSingleObject(resourceThread, 40) != 0)
			{
				Sleep(10);
				continue;
			}
			CloseHandle(resourceThread);
			waitResources = FALSE;
			waitConfig = TRUE;
			thread = (HANDLE)_beginthreadex(NULL, 0, ConfigLoaderThread, NULL, 0, NULL);
			if (thread == NULL || thread == INVALID_HANDLE_VALUE)
			{
				ConfigLoaderThread(this);
				thread = NULL;
			}
		}
		else if (waitConfig)
		{
			if (WaitForSingleObject(thread, 30) != 0)
			{
				Sleep(10);
				continue;
			}
			waitConfig = FALSE;
			waitStartup = TRUE;
			CloseHandle(thread);
			// From DVD: a Z: cache with under 5MB free is formatted.
			if (g_pSettings->m_bRunFromDVD)
			{
				SDriveInfo *z = GetDriveInfo('Z');
				if (z != NULL && z->freeBytes < 5000000)
					FormatDrive('Z');
			}
			thread = (HANDLE)_beginthreadex(NULL, 0, StartupThread, NULL, 0, NULL);
			if (thread == NULL || thread == INVALID_HANDLE_VALUE)
			{
				StartupThread(this);
				thread = NULL;
			}
			else
			{
				SetThreadPriority(thread, 1);
			}
		}
		else if (waitStartup)
		{
			if (thread != NULL)
			{
				if (WaitForSingleObject(thread, 30) != 0)
				{
					Sleep(10);
					continue;
				}
				CloseHandle(thread);
			}
			break;
		}
		else
		{
			Sleep(10);
		}
	}

	if (action == INPUT_NONE)
	{
		UpdateGamepads(this);
		action = g_gamepadInput.GetAction();
	}
	WaitForSingleObject(resourceThread, 60000);
	DWORD missing = 0;
	GetExitCodeThread(resourceThread, &missing);
	CloseHandle(resourceThread);

	// Required resources missing: delete the unpacked "Media" save (U:,
	// then X:) and try again on a fresh utility drive; if
	// that fails too, a recovery screen with FTP on 192.168.0.100 for 5
	// minutes, then the MS dashboard.
	if (missing != 0)
	{
		if (XDeleteSaveGame("U:\\", L"Media"))
			XDeleteSaveGame("X:\\", L"Media");
		XMountUtilityDrive(TRUE);
		if (LoadResources(this) != 0)
		{
			MountAllDrives();
			SNetworkSettings &network = g_pSettings->m_network;
			network.m_bEnable = TRUE;
			network.m_bDHCP = FALSE;
			network.m_strIP = "192.168.0.100";
			network.m_strSubnet = "255.255.255.0";
			g_pSettings->m_prefs.m_bSNTPSync = FALSE;
			g_pSettings->m_ftp.m_bEnable = TRUE;
			unsigned threadId;
			_beginthreadex(NULL, 0x10000, NetworkThread, NULL, 0, &threadId);
			DWORD deadline = GetTickCount() + 300000;
			CXBFont *font = &m_fonts[1];
			for (;;)
			{
				g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f,
				                    0);
				DrawTextCentered(font, 320.0f, 240.0f, L"Not all required resources can be loaded.", 0xFFFFFFFF,
				                 XBFONT_CENTER_X | XBFONT_CENTER_Y);
				DrawTextCentered(font, 320.0f, 280.0f, L"This is usually caused by lack of HD space on E:\\ or X:\\.",
				                 0xFFFFFFFF, XBFONT_CENTER_X | XBFONT_CENTER_Y);
				DrawTextCentered(font, 320.0f, 320.0f,
				                 L"You can try FTPing (192.168.0.100:21) and correct the problem.", 0xFFFFFFFF,
				                 XBFONT_CENTER_X | XBFONT_CENTER_Y);
				g_pd3dDevice->Swap(0);
				Sleep(500);
				if (deadline < GetTickCount())
					break;
				if (g_bFtpLaunchPending)
				{
					LaunchXbeImage(g_strFtpLaunchPath.c_str(), NULL);
					g_bFtpLaunchPending = FALSE;
				}
			}
			LaunchXbeImage("C:\\XboxBoxDash.xbe", NULL);
			return E_FAIL;
		}
	}

	MountAllDrives();
	XSetFileCacheSize(0x100000);
	// Input is read from the merged gamepad (all ports) and remote.
	g_gamepadInput.m_pFixedGamepad = &m_defaultGamepad;
	g_gamepadInput.m_pFixedRemote = &m_defaultIR;
	g_pd3dDevice->GetViewport((D3DVIEWPORT8 *)&g_pSettings->m_viewport);
	g_pSettings->m_nSkinWidth = g_pSettings->m_viewport.Width;
	g_pSettings->m_nSkinHeight = g_pSettings->m_viewport.Height;
	if (action == INPUT_NONE)
	{
		UpdateGamepads(this);
		action = g_gamepadInput.GetAction();
	}
	if (RunBootShortcut(action))
		RebootToDashboard();

	// The video mode. BACK held at boot: with a trigger, switch the Xbox
	// to NTSC (right) or PAL (left) and reboot; anyway, start in 640x480
	// with the default settings.
	BOOL reset = FALSE;
	if (action == INPUT_BACK)
	{
		DWORD standard = XGetVideoStandard();
		if (m_defaultGamepad.bAnalogButtons[7] != 0)
		{
			if (standard != XC_VIDEO_STANDARD_NTSC_M)
			{
				SetVideoStandard(0x400100);
				RebootToDashboard();
				g_pSettings->m_video.m_bPAL = FALSE;
				reset = TRUE;
			}
			action = ACTION_HANDLED_AT_BOOT;
		}
		else if (m_defaultGamepad.bAnalogButtons[6] != 0)
		{
			if (standard != XC_VIDEO_STANDARD_PAL_I)
			{
				SetVideoStandard(0x800300);
				RebootToDashboard();
				g_pSettings->m_video.m_bPAL = TRUE;
				reset = TRUE;
			}
			action = ACTION_HANDLED_AT_BOOT;
		}
	}
	SVideoSettings &video = g_pSettings->m_video;
	if (video.m_bFastRefresh)
	{
		m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	}
	else
	{
		m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE;
		reset = TRUE;
	}
	if (video.m_bPAL)
	{
		reset = TRUE;
		m_dwScreenHeight = 576;
		m_dwScreenWidth = 640;
		if ((g_dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN) && video.m_bWidescreen)
		{
			m_dwScreenWidth = 720;
			m_d3dpp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
		}
		m_d3dpp.FullScreen_RefreshRateInHz = 0;
		if (action != INPUT_BACK && (g_dwVideoFlags & XC_VIDEO_FLAGS_PAL_60Hz) && video.m_bUsePAL60)
		{
			m_dwScreenHeight = 480;
			m_d3dpp.FullScreen_RefreshRateInHz = 60;
		}
	}
	else if (action != INPUT_BACK)
	{
		if ((g_dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN) && video.m_bWidescreen)
		{
			reset = TRUE;
			m_dwScreenWidth = 720;
			m_d3dpp.Flags |= D3DPRESENTFLAG_WIDESCREEN;
		}
		// HDTV modes need the HD AV pack.
		if (XGetAVPack() == XC_AV_PACK_HDTV)
		{
			BYTE flags = (BYTE)g_dwVideoFlags;
			if ((flags & XC_VIDEO_FLAGS_HDTV_480p) && video.m_bUse480p)
			{
				m_dwScreenWidth = 720;
				m_dwScreenHeight = 480;
				reset = TRUE;
				m_d3dpp.Flags |= D3DPRESENTFLAG_PROGRESSIVE;
			}
			if ((flags & XC_VIDEO_FLAGS_HDTV_1080i) && video.m_bUse1080i)
			{
				if (m_d3dpp.Flags & D3DPRESENTFLAG_PROGRESSIVE)
					m_d3dpp.Flags &= ~D3DPRESENTFLAG_PROGRESSIVE;
				m_d3dpp.Flags = D3DPRESENTFLAG_WIDESCREEN | D3DPRESENTFLAG_INTERLACED;
				m_dwScreenWidth = 1920;
				m_dwScreenHeight = 1080;
				m_d3dpp.FullScreen_PresentationInterval = 0;
				m_d3dpp.FullScreen_RefreshRateInHz = 0;
				reset = TRUE;
			}
			else if ((flags & XC_VIDEO_FLAGS_HDTV_720p) && video.m_bUse720p)
			{
				m_d3dpp.Flags = D3DPRESENTFLAG_WIDESCREEN | D3DPRESENTFLAG_PROGRESSIVE;
				m_dwScreenWidth = 1280;
				m_dwScreenHeight = 720;
				reset = TRUE;
			}
		}
	}
	BOOL defaults = FALSE;
	if (action == INPUT_LTHUMB_CLICK)
	{
		// Left stick held: no vsync this run.
		m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		reset = TRUE;
	}
	if (action == INPUT_BACK)
	{
		defaults = TRUE;
		g_pSettings->SetDefaults();
		reset = TRUE;
		m_d3dpp.BackBufferWidth = 640;
		m_d3dpp.BackBufferHeight = 480;
		m_d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;
	}
	else if (video.m_bUse16bit)
	{
		m_d3dpp.BackBufferFormat = D3DFMT_LIN_X1R5G5B5;
		reset = TRUE;
	}
	D3DXMatrixPerspectiveFovLH(m_projMatrix, 1.1423974f, 4.0f / 3.0f, 1.0f, 200.0f);
	g_pd3dDevice->SetTransform(D3DTS_PROJECTION, m_projMatrix);

	// Sound: 5.1 if the Xbox is on stereo, the HRTF and the effects image
	// (dsstdfx.bin in the data folder, or one level up).
	if (g_pSettings->m_audio.m_bEnable51 && (XGetAudioFlags() & 0xFFFF) == 0)
		DirectSoundOverrideSpeakerConfig(2);
	g_pDirectSound = CAudioDevice::GetInstance()->GetDirectSound();
	if (g_pDirectSound != NULL)
	{
		DirectSoundUseLightHRTF();
		std::string effects = g_pSettings->m_strDataPath;
		if (StrRight(effects, 1).compare("\\") != 0)
			effects.append("\\");
		effects.append("dsstdfx.bin");
		if (!FileExists(effects.c_str()))
		{
			StrReplaceAll(effects, "9e115330\\", "");
			if (!FileExists(effects.c_str()))
				effects.clear();
		}
		if (effects.size() != 0)
		{
			DWORD loc[2] = {3, 4};
			XAudioDownloadEffectsImage(effects.c_str(), loc, 0, NULL);
		}
	}
	m_bStartMusic = g_pDirectSound != NULL;
	CreateOrbBuffer();
	InitOrb();

	// The skin. skincheck.dat is there while one loads: if it's still there
	// at the next start, that skin locked up and the default is used.
	BOOL skinLoading = FALSE;
	BOOL skinOK = TRUE;
	std::string check;
	std::string skin;
	ResolvePath("skincheck.dat", check);
	const std::string &name = g_pSettings->m_prefs.m_strSkinName;
	if (FileExists(check.c_str()) && !g_pSettings->m_bRunFromDVD)
	{
		skinOK = FALSE;
		g_skin.SetDefaultMainLayout();
		g_colorScheme.Load(&g_config);
	}
	else if (name.size() == 0 || StrCmpNoCase(name, "[Default]") == 0)
	{
		g_skin.SetDefaultMainLayout();
		g_colorScheme.Load(&g_config);
	}
	else
	{
		if (!g_pSettings->m_bRunFromDVD)
			WriteTextFile(check.c_str(), " ", FALSE);
		// A full path, a built-in skin, or a folder under the skins path:
		// its skin.xml, else the first .zip in it.
		int internal = -1;
		BOOL folder;
		if (StrFind(name, ":\\") != -1)
		{
			skin = name;
			folder = TRUE;
		}
		else
		{
			internal = FindInternalSkin(name.c_str());
			if (internal == -1)
			{
				ResolvePath(g_pSettings->m_prefs.m_strSkinPath.c_str(), skin);
				if (skin.size() != 0)
					StrAddBackslash(skin);
				skin.append(name);
			}
			else
			{
				skin = name;
			}
			folder = internal == -1;
		}
		if (folder)
		{
			StrAddBackslash(skin);
			skin.append("skin.xml");
			if (!FileExists(skin.c_str()))
			{
				CFileFinder finder;
				StrReplaceAll(skin, "\\skin.xml", "");
				const char *zip = finder.FindFirst(skin.c_str(), "*.zip");
				if (zip != NULL)
				{
					skin.append("\\");
					skin.append(zip);
				}
				else
				{
					skin.clear();
				}
			}
		}
		if (skin.size() == 0)
		{
			g_skin.SetDefaultMainLayout();
			g_colorScheme.Load(&g_config);
		}
		else
		{
			g_skin.BeginLoad(skin.c_str());
			skinLoading = TRUE;
			if (!g_skin.m_bColorScheme)
				g_colorScheme.Load(&g_config);
		}
	}
	if (wcslen(g_pSettings->m_szDiscTitle) == 0)
		wcscpy(g_pSettings->m_szDiscTitle, L"UnleashX Xbox Launcher");

	// The clock prompt after a cold boot.
	if (g_bColdBoot)
	{
		m_pClockEditor = new CClockEditor;
		if (m_pClockEditor != NULL && FAILED(m_pClockEditor->Init(&m_fonts[1])))
		{
			delete m_pClockEditor;
			m_pClockEditor = NULL;
		}
		g_bColdBoot = m_pClockEditor != NULL;
	}
	thread = (HANDLE)_beginthreadex(NULL, 0x4000, MonitorThread, this, 4 /* CREATE_SUSPENDED */, NULL);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
	{
		SetThreadPriority(thread, -1);
		ResumeThread(thread);
		CloseHandle(thread);
	}
	if (g_pSettings->m_network.m_bEnable)
		StartNetworkThread();

	// The keyboard (with its key texture), and the startup password or
	// passcode.
	IDirect3DTexture8 *key = m_resource.GetTexture("Keyboard");
	if (key != NULL)
		g_textures.Add("Keyboard", key, FALSE);
	CXBFont *font = &m_fonts[4];
	g_virtualKeyboard.Create(font, font, key, 0, 0.0f, 0.0f, 0);
	g_virtualKeyboard.m_pIR = &m_defaultIR;
	m_bPasswordShown = FALSE;
	if (!g_pSettings->m_bUnlocked)
	{
		if (g_pSettings->m_bUsePasscode)
		{
			m_pPassCode = new CPassCode;
			if (m_pPassCode != NULL)
			{
				m_pPassCode->SetCode(g_pSettings->m_strPassword.c_str());
				m_pPassCode->Init(font, &m_fonts[5]);
			}
		}
		else
		{
			g_virtualKeyboard.SetTitle(L"Enter System Password");
			g_virtualKeyboard.Open();
			g_virtualKeyboard.m_bPassword = TRUE;
			m_bPasswordShown = TRUE;
		}
	}

	// Wait for the skin, fading the splash out meanwhile.
	while (!g_skin.m_bLoaded)
	{
		if (fade != 0 && m_pSplash != NULL)
		{
			g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
			m_fSplashAlpha = (float)fade * 0.0125f;
			DrawSplash(m_fSplashAlpha, mode);
			fade--;
			g_pd3dDevice->Swap(0);
		}
		Sleep(10);
	}
	m_skinEvent.OnSkinLoaded();
	g_skin.m_pEventSink = &m_skinEvent;

	// The skin's intro movie (A or B skips it).
	if (skinLoading && g_skin.m_strIntroMovie.size() != 0 && FileExists(g_skin.m_strIntroMovie.c_str()) &&
	    g_pDirectSound != NULL)
	{
		if (m_pSplash != NULL)
		{
			m_pSplash->Release();
			m_pSplash = NULL;
		}
		m_pSplash = NULL;
		CWMVideoPlayer *movie = NULL;
		try
		{
			movie = new CWMVideoPlayer;
			if (movie != NULL)
			{
				movie->SetFullScreen(TRUE);
				movie->m_bModal = FALSE;
				movie->SetLoop(FALSE);
				movie->SetFont(font);
				movie->Open(g_skin.m_strIntroMovie.c_str());
				while (movie->IsOpen())
				{
					g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000,
					                    1.0f, 0);
					UpdateGamepads(this);
					int key2 = g_gamepadInput.GetAction();
					if (movie->ProcessInput(&key2, NULL) >= 0)
						movie->Render();
					g_pd3dDevice->Swap(0);
				}
				Sleep(5);
			}
		}
		catch (...)
		{
		}
		delete movie;
	}
	if (!g_pSettings->m_bRunFromDVD)
		DeleteFileA(check.c_str());
	BuildGlobePath();
	if (!g_bConfigLoaded)
		ShowMessage(GetStringA(STR_MSG_BAD_CONFIGFILE), GetString(STR_WARNING), 0);
	else if (!skinOK)
		ShowMessage(GetStringA(STR_MSG_BAD_SKINFILE), GetString(STR_WARNING), 0);
	m_timer.Resume();
	if (g_pSettings->m_prefs.m_nLEDDefault != LED_DEFAULT)
		SetLedPattern(LED_DEFAULT, TRUE);

	// The globe's starting transform, and an ambient light.
	float rotation[16];
	SetIdentity(rotation);
	D3DXMatrixRotationYawPitchRoll(rotation, 3.1415927f, 1.5707964f, 0.0f);
	SetIdentity(m_globeMatrix);
	BuildWorldMatrix(m_globeMatrix, 0.0f, 0.0f, 48.0f, 1.0f, 1.0f, 1.0f, 1.5707964f, 3.1415927f, 0.0f);
	g_pd3dDevice->SetTransform(D3DTS_WORLD, m_globeMatrix);
	g_pd3dDevice->SetRenderState(D3DRS_AMBIENT, 0xFF000000);

	// Started by the Live Update installer (launch data with its path):
	// validate this build.
	if (!g_pSettings->m_bRunFromDVD && !g_pSettings->m_bHomeNotOnD && g_pSettings->m_dwLaunchDataType == 4)
	{
		const BYTE *data = g_pSettings->m_launchData;
		if (*(const DWORD *)data == 0x9E115330 && data[4] != 0 && FileExists((const char *)data + 4))
		{
			m_bUpdatePending = TRUE;
			g_bAutoLaunchPending = TRUE;
			ShowMessage(GetStringA(STR_MSG_DASHUPDATE_CHECK), GetString(STR_INFORMATION), 0);
		}
	}

	// The rest of the splash fade.
	if (m_pSplash != NULL)
	{
		while (fade != 0)
		{
			g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
			m_fSplashAlpha = (float)fade * 0.0125f;
			DrawSplash(m_fSplashAlpha, mode);
			fade--;
			g_pd3dDevice->Swap(0);
			g_pd3dDevice->BlockUntilVerticalBlank();
			Sleep(3);
		}
		if (m_pSplash != NULL)
		{
			m_pSplash->Release();
			m_pSplash = NULL;
		}
		m_pSplash = NULL;
	}

	// The display filters.
	if (!defaults && (video.m_bSoften || video.m_nFilterLevel != 5))
	{
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
		g_pd3dDevice->Swap(0);
		g_pd3dDevice->SetSoftDisplayFilter(video.m_bSoften);
		g_pd3dDevice->SetFlickerFilter(video.m_nFilterLevel);
		if (!video.m_bSoften)
			g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
	}

	if (!reset && action == INPUT_BACK)
	{
		g_pd3dDevice->GetViewport((D3DVIEWPORT8 *)&g_pSettings->m_viewport);
	}
	else
	{
		if (reset)
		{
			// The new mode.
			m_d3dpp.BackBufferWidth = m_dwScreenWidth;
			m_d3dpp.BackBufferHeight = m_dwScreenHeight;
			try
			{
				g_pd3dDevice->Reset(&m_d3dpp);
			}
			catch (...)
			{
			}
			g_pd3dDevice->GetDisplayMode(&mode);
			if (m_pSplash != NULL)
			{
				DrawSplash(m_fSplashAlpha, mode);
				g_pd3dDevice->Swap(0);
			}
		}

		// The screen calibration (<data>\videoset.dat), where it fits the
		// display: the corner within 10%, the far corner past 90%.
		std::string path = g_pSettings->m_strDataPath;
		StrAppend(path, "videoset.dat");
		FILE *file = fopen(path.c_str(), "rb");
		if (file != NULL)
		{
			fread(video.m_screenAdjust, sizeof(SScreenAdjust) * SCREEN_MODE_COUNT, 1, file);
			fclose(file);
		}
		float width = 0.0f;
		float height = 0.0f;
		GetDisplaySize(&width, &height, TRUE);
		SScreenAdjust *adjust = g_pSettings->GetScreenAdjust();
		if (adjust != NULL)
		{
			SViewport &viewport = g_pSettings->m_viewport;
			if ((unsigned)adjust->x <= (unsigned)(LONGLONG)(width * 0.1f))
				viewport.X = adjust->x;
			if ((unsigned)adjust->y <= (unsigned)(LONGLONG)(height * 0.1f))
				viewport.Y = adjust->y;
			if ((unsigned)(adjust->x + adjust->width) >= (unsigned)(LONGLONG)(width * 0.9f) &&
			    (unsigned)adjust->width <= (unsigned)(LONGLONG)width)
				viewport.Width = adjust->width;
			if ((unsigned)(adjust->y + adjust->height) >= (unsigned)(LONGLONG)(height * 0.9f) &&
			    (unsigned)adjust->height <= (unsigned)(LONGLONG)height)
				viewport.Height = adjust->height;
			if (adjust->scale > 0.8f)
				g_pSettings->m_fFontScale = adjust->scale;
		}
	}
	g_pSettings->m_nDisplayWidth = m_dwScreenWidth;
	g_pSettings->m_nDisplayHeight = m_dwScreenHeight;
	return S_OK;
}
