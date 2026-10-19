// The loading screen shown before another program starts (the skin's
// <LoadingScreen>), and starting the disc in the drive (CUnleashX::
// LaunchDVD: the LaunchDVD action and the AutoLaunch settings).
#include "UnleashX.h"

#include <cstring>
#include <cwchar>

#include "../audio/MusicManager.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../filemgr/FileManager.h"
#include "../input/Gamepad.h"
#include "../media/WMVideoPlayer.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/Storage.h"
#include "../system/XbeLaunch.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"

// The picture fades in over 80 steps of at least 20 ms.
#define LAUNCH_FADE_STEPS 80
#define LAUNCH_FADE_STEP_MS 20

// "Loading..." at a third of the screen's height and the title below it,
// half as large again, both centered. (Inlined twice.)
static void DrawLaunchInfo(const wchar_t *title, const D3DDISPLAYMODE &mode, float scaleX, float scaleY)
{
	CXBFont *font = g_pSettings->m_pDialogFont;
	if (!g_pSettings->m_bLoadingShowInfo || font == NULL)
		return;
	font->SetScaleFactors(scaleX, scaleY);
	font->DrawText((float)mode.Width * 0.5f, (float)mode.Height * (1.0f / 3.0f), 0xFFFFFFFF,
	               GetString(STR_MSG_LOADING), XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
	font->SetScaleFactors(scaleX * 1.5f, scaleY * 1.5f);
	font->DrawText((float)mode.Width * 0.5f, (float)mode.Height * 0.5f, 0xFFFFFFFF, title,
	               XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
}

// 0x000E3B65
void ShowLaunchScreen(const char *path, const char *name)
{
	wchar_t title[0x104];
	memset(title, 0, sizeof(title));
	if (g_pMusicManager != NULL)
		g_pMusicManager->Stop();
	Sleep(50);
	g_pd3dDevice->SetSoftDisplayFilter(TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);

	if (path != NULL && strlen(path) != 0 && g_pSettings->m_bLoadingShowInfo)
	{
		std::string text;
		if (name == NULL || strlen(name) == 0)
			text = GetXbeTitleName(path);
		else
			text = name;
		if (text.size() != 0)
			MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size() + 1, title, 0x104);
	}

	D3DDISPLAYMODE mode;
	g_pd3dDevice->GetDisplayMode(&mode);
	// The fonts grow with the screen, never shrink.
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, NULL, &scaleY, FALSE);
	if (scaleX < 1.0f)
		scaleX = 1.0f;
	if (scaleY < 1.0f)
		scaleY = 1.0f;

	if (g_skin.GetLoadingBackground() != NULL)
		g_pSettings->m_pLoadingIcon = g_skin.GetLoadingBackground();

	std::string preview = g_skin.m_strLoadingPreview;
	if (FileExists(preview.c_str()))
	{
		// The preview video, once (A or START skips it).
		CWMVideoPlayer *player = new CWMVideoPlayer;
		if (player == NULL)
			return;
		player->m_bFullScreen = TRUE;
		player->m_bModal = FALSE;
		player->SetLoop(FALSE);
		player->SetFont(g_pSettings->m_pDialogFont);
		player->Open(g_skin.m_strLoadingPreview.c_str());
		if (player->IsValid())
		{
			while (player->IsOpen())
			{
				g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
				if (g_pApp != NULL)
					UpdateGamepads(g_pApp);
				int action = g_gamepadInput.GetAction();
				player->ProcessInput(&action, NULL);
				player->Render();
				DrawLaunchInfo(title, mode, scaleX, scaleY);
				g_pd3dDevice->Swap(0);
			}
		}
		delete player;
		// Quirk: unlike after the picture, the soften setting comes back
		// only on SD screens.
		if (g_pSettings->m_viewport.Width <= 640)
		{
			g_pd3dDevice->SetSoftDisplayFilter(g_pSettings->m_video.m_bSoften);
			g_pd3dDevice->PersistDisplay();
		}
	}
	else if (g_pSettings->m_pLoadingIcon != NULL)
	{
		// The picture fades in over black (A or START cuts it short).
		int step = 0;
		Sleep(5);
		DWORD lastStep = 0;
		do
		{
			g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
			DrawTextureFullScreen(NULL, 1.0f);
			if (g_pSettings->m_pLoadingIcon != NULL)
				DrawTextureFullScreen(g_pSettings->m_pLoadingIcon, (float)step * 0.0125f);
			DrawLaunchInfo(title, mode, scaleX, scaleY);
			if (g_pApp != NULL)
				UpdateGamepads(g_pApp);
			int action = g_gamepadInput.GetAction();
			g_pd3dDevice->Swap(0);
			if (action == INPUT_A || action == INPUT_START)
				break;
			if (GetTickCount() - lastStep > LAUNCH_FADE_STEP_MS)
			{
				step++;
				lastStep = GetTickCount();
			}
		} while (step < LAUNCH_FADE_STEPS);
		g_pd3dDevice->SetSoftDisplayFilter(g_pSettings->m_video.m_bSoften);
		if (g_pSettings->m_viewport.Width <= 640)
			g_pd3dDevice->PersistDisplay();
	}
}

// 0x000E99FD
HRESULT CUnleashX::LaunchDVD(BOOL manual)
{
	if (!IsDiscPresent() || m_bUpdatePending)
		return E_FAIL;
	SPreferences &prefs = g_pSettings->m_prefs;

	// A game. Inserted, a disc of UnleashX itself (or the one it was
	// started from) isn't started again.
	if ((manual || prefs.m_bGamesAutoLaunch) && GetDiscType() == DISC_GAME)
	{
		if (!manual)
		{
			if (_wcsicmp(GetDiscTitle(), L"UnleashX Xbox Launcher") == 0)
				return S_OK;
			if (_wcsicmp(GetDiscTitle(), g_pSettings->m_szDiscTitle) == 0)
				return S_OK;
		}
		std::string name = StrFromWide(GetDiscTitle());
		return LaunchXbe("D:\\default.xbe", name.c_str(), -1, NULL);
	}

	// A video DVD: the DVD player, when there is one.
	if ((manual || prefs.m_bDVDAutoLaunch) && prefs.m_strDVDPlayer.size() != 0 && GetDiscType() == DISC_VIDEO)
		return LaunchXbe(prefs.m_strDVDPlayer.c_str(), NULL, -1, NULL);

	// An audio CD: the MS dashboard's music screen, the music player, or
	// another program.
	if ((manual || prefs.m_bAudioCDAutoLaunch) && GetDiscType() == DISC_AUDIO)
	{
		// Quirk: with both settings empty, this is the branch taken (an
		// empty path starts the default dashboard), not the music player.
		if (StrCmpNoCase(prefs.m_strAudioCDPlayer, g_pSettings->m_strMSDashboard.c_str()) == 0)
		{
			ShowLaunchScreen(prefs.m_strAudioCDPlayer.c_str(), NULL);
			return LaunchDashboardMusic(g_pSettings->m_strMSDashboard.c_str());
		}
		if (prefs.m_strAudioCDPlayer.size() == 0)
		{
			if (g_pMusicManager == NULL)
				return E_FAIL;
			g_pMusicManager->PlayAudioCD();
			return S_OK;
		}
		return LaunchXbe(prefs.m_strAudioCDPlayer.c_str(), NULL, -1, NULL);
	}

	// A data disc: the data launcher, else the file manager on D:.
	if ((manual || prefs.m_bDataAutoLaunch) && (GetDiscType() == DISC_DATA || GetDiscType() == DISC_DATA2))
	{
		if (prefs.m_strDataLauncher.size() != 0)
		{
			LaunchXbe(prefs.m_strDataLauncher.c_str(), NULL, -1, NULL);
			return S_OK;
		}
		if (m_pFileManager == NULL)
			m_pFileManager = new CFileManager;
		if (m_pFileManager != NULL)
		{
			if (!m_pFileManager->m_bInitialized)
			{
				if (m_pFileIcon == NULL)
					m_pFileIcon = m_resource.GetTexture("File.png");
				if (m_pFolderIcon == NULL)
					m_pFolderIcon = m_resource.GetTexture("Folder.png");
				m_pFileManager->Init(&m_fonts[4], 0, 0, NULL, m_pFolderIcon, m_pFileIcon);
			}
			m_pFileManager->OpenPath("D:\\");
			SetMode(MODE_FILEMANAGER);
		}
	}
	return S_OK;
}
