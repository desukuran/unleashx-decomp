// CUnleashX::Render: draws a frame. The background and the skin, then the
// one thing that has the focus (the same order FrameMove gives input in),
// the pointer, and the file operation progress/errors over the menu.
#include <cwchar>

#include "../common/StrUtil.h"
#include "../net/FtpServer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../ui/Language.h"
#include "../ui/Menu.h"
#include "../ui/MenuDialogs.h"
#include "../ui/StringIds.h"
#include "FtpEvents.h"
#include "Screens.h"
#include "../filemgr/FileManager.h"
#include "SettingsPages.h"
#include "UnleashX.h"

// Over the menu: the progress of a running file operation, or the error of
// a failed one (then the operation is forgotten).
void CUnleashX::RenderFileOperation()
{
	if ((g_bFileOperationQueued || g_bFileOperationActive) && !g_bFileOperationPaused)
	{
		float x = 70.0f;
		float y = 180.0f;
		float width = 500.0f;
		float height = 150.0f;
		ScaleToScreen(&x, &width, &y, &height, TRUE);
		int type = g_nFileOperationType;
		if (type == 0 || type == 1)
		{
			// Copies and moves: the file, and overall (by bytes, else by count).
			unsigned filePercent = (unsigned)(LONGLONG)g_dFilePercent;
			if (filePercent > 100)
				filePercent = 100;
			unsigned total = (unsigned)(LONGLONG)g_dTotalPercent;
			if (total == 0)
			{
				if ((LONGLONG)g_ullOperationTotal > 0)
					total = (unsigned)(g_ullOperationDone * 100 / g_ullOperationTotal);
				if (total == 0 && g_nOperationFiles + g_nOperationFolders > 1)
					total = g_nOperationProgress * 100 / (g_nOperationFiles + g_nOperationFolders);
				if (total > 100)
					total = 100;
			}
			wchar_t from[0x10E];
			wchar_t to[0x10E];
			swprintf(from, 0x10E, L"From: %s", g_szOperationFile);
			swprintf(to, 0x10E, L"To: %s", g_szOperationTarget);
			DrawProgressBox(&m_fonts[4], x, y, width, height,
			                g_nFileOperationType == 0 ? L"Copying. Please Wait" : L"Moving. Please Wait", from, to,
			                (float)filePercent, (float)total, TRUE);
		}
		else if (type != 6)
		{
			unsigned percent = 0;
			unsigned items = g_nOperationFiles + g_nOperationFolders;
			if (items > 1)
			{
				percent = g_nOperationProgress * 100 / items;
				if (percent > 100)
					percent = 100;
			}
			const wchar_t *title = NULL;
			if (type == 2)
				title = L"Deleting. Please Wait";
			else if (type == 3)
				title = L"Compressing. Please Wait";
			else if (type == 4 || type == 5)
				title = L"Extracting. Please Wait";
			if (title != NULL)
				DrawProgressBox(&m_fonts[4], x, y, width, height, title, g_szOperationFile, NULL, (float)percent,
				                -1.0f, FALSE);
		}
		return;
	}

	if (!g_bOperationFailed)
		return;
	if (!g_bCancelOperation)
	{
		static const char *const formats[6] = {
			"An error occured while copying '%s'.\nError Code: %d",
			"An error occured while moving '%s'.\nError Code: %d",
			"An error occured while deleting '%s'.\nError Code: %d",
			"An error occured while zipping '%s'.\nError Code: %d",
			"An error occured while unzipping '%s'.\nError Code: %d",
			"An error occured while extacting '%s'.\nError Code: %d",
		};
		std::string message;
		if (g_nFileOperationType >= 0 && g_nFileOperationType <= 5)
		{
			DWORD error = g_dwOperationError != 0 ? g_dwOperationError : GetLastError();
			// Quirk: the file name is wide but formatted with %s.
			StrFormat(message, formats[g_nFileOperationType], (const char *)g_szOperationFile, error);
		}
		if (g_strOperationLog.size() == 0)
		{
			ShowMessage(message.c_str(), GetString(STR_ERROR), 0);
		}
		else
		{
			message.append("\nWould you like to view the log file?");
			ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_YESNO);
			if (m_bMessageShown)
			{
				m_pMessageBox->SetSelection(0);
				if (CreateTextEditor())
					m_bEditOnYes = m_pTextEditor->Load(g_strOperationLog.c_str());
			}
		}
	}
	g_bOperationFailed = FALSE;
	g_bFileOperationQueued = FALSE;
	g_nFileOperationType = 6;
}

// 0x000F0097
HRESULT CUnleashX::Render()
{
	D3DVIEWPORT8 viewport;
	viewport.X = 0;
	viewport.Y = 0;
	viewport.Width = m_d3dpp.BackBufferWidth;
	viewport.Height = m_d3dpp.BackBufferHeight;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = 1.0f;
	g_pd3dDevice->SetViewport(&viewport);
	g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
	                    g_skin.m_bGlobe ? g_colorScheme.AMBIENT_SKYCOLOR : 0xFF000000, 1.0f, 0);
	g_pd3dDevice->SetViewport((const D3DVIEWPORT8 *)&g_pSettings->m_viewport);

	// A video or slideshow screen saver takes the whole screen.
	// (CFTPServer +0x124, m_bPaused: also checked by MonitorThread.)
	if (m_bScreenSaverActive && !m_pScreenSaver->IsTextOnly() &&
	    (g_pSettings->m_prefs.m_nScreenSaverType != 0 || g_ftpServer.m_bPaused))
	{
		m_pScreenSaver->Render();
		goto present;
	}

	// (At 1080 lines the busy screens skip the background.)
	if (m_d3dpp.BackBufferHeight != 1080 ||
	    (m_nMode != MODE_FILEMANAGER && m_nMode != MODE_SETTINGS && m_nMode != MODE_SYSINFO &&
	     m_nMode != MODE_TEXTEDITOR))
		RenderBackground();
	g_bFtpActive = FALSE;
	if (m_nMode == MODE_MENU || m_nMode == MODE_GAMEMANAGER || m_nMode == MODE_SETTINGS || m_nMode == MODE_SKINS ||
	    m_nMode == MODE_SYSINFO || m_nMode == MODE_MUSIC)
		g_skin.Render();

	if (m_bMessageShown && m_fAppTime >= 1.0f)
	{
		m_pMessageBox->Render();
		goto pointer;
	}
	if (m_bPasswordShown && m_fAppTime >= 1.0f)
	{
		g_virtualKeyboard.Render();
		goto pointer;
	}
	if (!g_pSettings->m_bUnlocked && m_pPassCode != NULL)
	{
		m_pPassCode->Render();
		goto pointer;
	}

	switch (m_nMode)
	{
	case MODE_MENU:
		m_menu.Render();
		RenderFileOperation();
		break;
	case MODE_FILEMANAGER:
		m_pFileManager->Render();
		break;
	case MODE_GAMEMANAGER:
		m_pGameManager->Render();
		break;
	case MODE_SETTINGS:
		m_pSettingsMenu->Render();
		break;
	case MODE_SKINS:
		m_pSkinBrowser->Render();
		break;
	case MODE_SYSINFO:
		m_pSystemInfo->Render();
		break;
	case MODE_TEXTEDITOR:
		m_pTextEditor->Render();
		break;
	case MODE_MUSIC:
		m_menu.Render();
		g_pAudioGUI->Render();
		break;
	}

	if (m_bScreenSaverActive && m_pScreenSaver != NULL && !g_ftpServer.m_bPaused)
		m_pScreenSaver->Render();
	else if (g_bColdBoot && m_pClockEditor != NULL)
		m_pClockEditor->Render();

pointer:
	// The pointer: shown for 5s after it moved, then fading out by 10s.
	if (g_bFrameDrawn && !m_bScreenSaverActive && g_bPointerActive)
	{
		DWORD idle = GetTickCount() - g_dwPointerMoveTime;
		if (idle < 10000)
		{
			float alpha = idle > 5000 ? 2.0f - (float)idle * 0.0002f : 1.0f;
			DrawPointer(g_nPointerX, g_nPointerY, alpha);
		}
	}

present:
	// Back to the states the rest of the code expects. (The original writes
	// most of these into D3D's state cache.)
	g_pd3dDevice->SetTexture(0, NULL);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
	// TODO: two more deferred render states are cleared (cache slots
	// 0x00150BA8 and 0x00150BAC).
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	if (m_bTakeScreenshot)
	{
		std::string path = GetScreenshotPath();
		if (path.size() != 0)
		{
			IDirect3DSurface8 *surface = NULL;
			g_pd3dDevice->GetBackBuffer(0, 0, &surface);
			SaveSurfaceToBmp(surface, path.c_str());
			surface->Release();
		}
		m_bTakeScreenshot = FALSE;
	}
	try
	{
		g_pd3dDevice->Swap(0);
	}
	catch (...)
	{
	}
	g_bFrameDrawn = TRUE;
	return S_OK;
}
