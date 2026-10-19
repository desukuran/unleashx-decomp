// CUnleashX::FrameMove: once per frame. Reads one input action and hands it
// to whatever has the focus (a message box, the startup password or
// passcode, the clock prompt, then the current screen), and runs the
// background chores: disc autostart, the FTP launch, shutdown countdowns,
// the soundtrack player and the globe animation.
#include "../audio/MusicManager.h"
#include "../input/Gamepad.h"
#include "../media/WMVideoPlayer.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinText.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../system/SystemControl.h"
#include "../ui/Language.h"
#include "../ui/Menu.h"
#include "../ui/MenuDialogs.h"
#include "../ui/StringIds.h"
#include "FtpEvents.h"
#include "Screens.h"
#include "SettingsPages.h"
#include "UnleashX.h"

// Extra actions FrameMove makes up.
enum
{
	INPUT_KEYBOARD_CHAR = 46,      // a character on the debug keyboard
	INPUT_OPERATION_DONE = 48,     // a file operation finished
};

static BOOL s_bDiscHandled;                      // 0x00278D40 the inserted disc was autostarted (or skipped)
static BOOL s_bUpdateMessageShown;               // 0x00278D3C
static DWORD s_dwNextMusicCheck;                 // 0x00278D38
static DWORD s_dwMusicIdleUntil = 1;             // 0x0026CBFC 1 = check the player thread
static BOOL s_bLockedOut;                        // 0x00278D44 too many wrong passwords: turn off after the message
static float s_fGlobeAngle;                      // 0x00278D34

// 0x000EB93B
HRESULT CUnleashX::FrameMove()
{
	g_fFrameRate = m_fFPS;
	DWORD now = 0;
	now = GetTickCount();
	int action = g_gamepadInput.GetAction();

	// The debug keyboard, if no gamepad/remote input.
	if (action == INPUT_NONE)
	{
		const SKeystroke *key = m_pKeystroke;
		if (key->ascii > 0)
		{
			action = INPUT_KEYBOARD_CHAR;
		}
		else
		{
			switch (key->virtualKey)
			{
			case 0:
				if (g_bPointerEvent)
					action = INPUT_POINTER_CLICK;
				break;
			case 0x0D:   // Enter
			case 0x5B:   // Windows keys
			case 0x5C:
				action = INPUT_START;
				break;
			case 0x1B:   // Escape
				action = INPUT_B;
				break;
			case 0x25:
				action = INPUT_LEFT;
				break;
			case 0x26:
				action = INPUT_UP;
				break;
			case 0x27:
				action = INPUT_RIGHT;
				break;
			case 0x28:
				action = INPUT_DOWN;
				break;
			case 0x5D:   // Menu key
				action = INPUT_WHITE;
				break;
			case 0x73:   // Alt+F4
				if (key->flags & 4)
					action = INPUT_BACK;
				break;
			default:
				action = INPUT_KEYBOARD_CHAR;
				break;
			}
		}
	}

	if (m_nMode == 0)
	{
		// (Nothing in the code sets g_bMenuPending.)
		if (g_bMenuPending)
		{
			IDirect3DTexture8 *texture = m_pTexture7F0 != NULL ? m_pTexture7F0 : m_pTexture7D4;
			CXBFont *font = g_skin.m_pMenuFont != NULL ? g_skin.m_pMenuFont : &m_fonts[0];
			m_menu.Create(&g_defaultMenu, font, &m_fonts[4], &m_menuEvent, !g_skin.m_bShowIcon,
			              g_skin.m_nBorderWidth, texture, m_pTexture7DC);
			action = INPUT_NONE;
			InitOrb();
			SetMode(MODE_MENU);
		}
	}
	else
	{
		// Start a disc when one goes in (unless a trigger is held).
		if (!m_bAutoLaunchDisc || !g_bMusicReady)
		{
			s_bDiscHandled = FALSE;
		}
		else if (g_pSettings->m_bUnlocked && !s_bDiscHandled)
		{
			if (m_defaultGamepad.bAnalogButtons[7] != 0 || m_defaultGamepad.bAnalogButtons[6] != 0)
			{
				s_bDiscHandled = TRUE;
			}
			else if (GetDiscType() == DISC_GAME || GetDiscType() == DISC_VIDEO || GetDiscType() == DISC_AUDIO ||
			         GetDiscType() == DISC_DATA || GetDiscType() == DISC_DATA2)
			{
				s_bDiscHandled = TRUE;
				if (m_nMode == MODE_MUSIC && GetDiscType() == DISC_AUDIO)
					g_pMusicManager->PlayAudioCD();
				else
					LaunchDVD();
			}
		}

		// SITE EXEC from FTP.
		if (g_bFtpLaunchPending)
		{
			g_bFtpLaunchPending = FALSE;
			LaunchXbe(g_strFtpLaunchPath.c_str(), NULL, -1, NULL);
		}

		// Started by the Live Update installer: once it has run for 5s, say
		// the update works, show the new Release.nfo, then install it.
		if (m_bUpdatePending && g_bMusicReady && m_fAppTime > 5.0f)
		{
			if (!s_bUpdateMessageShown)
			{
				s_bUpdateMessageShown = TRUE;
				ShowMessage(GetStringA(STR_MSG_DASHUPDATE_PASS), GetString(STR_INFORMATION), 0);
			}
			else if (!m_bMessageShown)
			{
				if (m_bShowReleaseNfo)
				{
					m_bShowReleaseNfo = FALSE;
					std::string path;
					ResolvePath("D:\\", path);
					StrAddBackslash(path);
					path.append("Release.nfo");
					if (FileExists(path.c_str()) && CreateTextEditor() && m_pTextEditor != NULL &&
					    m_pTextEditor->Open(path.c_str()))
						SetMode(MODE_TEXTEDITOR);
				}
				if (m_nMode == MODE_TEXTEDITOR && m_pTextEditor != NULL)
				{
					m_pTextEditor->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
					if (!m_pTextEditor->IsOpen())
					{
						delete m_pTextEditor;
						m_pTextEditor = NULL;
						SetMode(MODE_MENU);
					}
					else
					{
						m_bKeepAwake = TRUE;
					}
				}
				else
				{
					InstallUpdate();
					m_bUpdatePending = FALSE;
				}
			}
		}
	}

	// A shutdown that waits for file operations; the message counts down.
	if (g_bShuttingDown && !g_bFileOperationQueued && !g_bFileOperationActive)
	{
		DWORD deadline = g_dwShutdownDeadline;
		if (deadline == 0 || deadline < now)
		{
			g_bShuttingDown = FALSE;
			Sleep(1000);
			g_trayMonitor.PowerOff();
		}
		else
		{
			if (deadline - now > 30000)
			{
				deadline = now + 30000;
				g_dwShutdownDeadline = deadline;
			}
			if (m_pMessageBox != NULL)
			{
				std::string text;
				StrFormat(text, "Xbox will shutdown in... %d seconds", (deadline - now) / 1000);
				m_pMessageBox->SetText(text.c_str());
			}
			m_bKeepAwake = TRUE;
		}
	}

	// Soundtracks: create the player, keep it going, pause it for a preview
	// with sound.
	if (g_bMusicReady && m_bStartMusic)
	{
		if (g_pMusicManager == NULL)
		{
			m_hThread2248 = (HANDLE)_beginthreadex(NULL, 0, MusicManagerThread, this, 0, NULL);
			if (m_hThread2248 == NULL || m_hThread2248 == INVALID_HANDLE_VALUE)
			{
				CloseHandle(m_hThread2248);
				m_hThread2248 = NULL;
				MusicManagerThread(this);
			}
			else
			{
				Sleep(40);
			}
			s_dwNextMusicCheck = now + 5000;
		}
		else if (g_pMusicManager->IsPlaying() && s_dwNextMusicCheck < now)
		{
			if (g_pMusicManager->GetState() != MUSIC_STOPPED)
			{
				g_pMusicManager->Update();
				s_dwMusicIdleUntil = 0;
			}
			else if (s_dwMusicIdleUntil == 1)
			{
				if (m_hThread2248 != NULL && m_hThread2248 != INVALID_HANDLE_VALUE &&
				    WaitForSingleObject(m_hThread2248, 200) == 0)
				{
					CloseHandle(m_hThread2248);
					m_hThread2248 = NULL;
				}
				if (m_hThread2248 == NULL)
				{
					if (g_pSettings->m_audio.m_nSoundTrackMode != 0)
					{
						g_pMusicManager->Play();
						if (g_skin.m_bPreviewAudio && g_skin.IsPreviewPlaying() &&
						    g_skin.m_pPreviewVideo->HasAudio() && g_skin.m_bPreviewPlaying &&
						    !g_skin.m_bPreviewMusic)
							g_pMusicManager->Pause();
					}
					s_dwNextMusicCheck = now + 100;
					s_dwMusicIdleUntil = 0;
				}
			}
			else if (s_dwMusicIdleUntil == 0)
			{
				s_dwMusicIdleUntil = now + 5000;
				s_dwNextMusicCheck = s_dwMusicIdleUntil;
			}
		}
	}

	// A file operation ended: report it once.
	if (action == INPUT_NONE && !g_bFileOperationQueued && g_nFileOperationType != 6)
	{
		action = INPUT_OPERATION_DONE;
		g_nFileOperationType = 6;
	}
	g_skin.PollLoad(&action);
	UpdateScreenSaver(&action);

	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(&action);
		m_bMessageShown = m_pMessageBox->IsOpen();
		if (!m_bMessageShown)
		{
			if (m_nMode == 10)
			{
				RebootToDashboard();
			}
			else if (s_bLockedOut)
			{
				g_bShuttingDown = TRUE;
				g_dwShutdownDeadline = 0;
			}
			else if (g_dwShutdownDeadline != 0)
			{
				// Any answer cancels a pending shutdown.
				g_bShuttingDown = FALSE;
				g_dwShutdownDeadline = 0;
			}
			if (m_bEditOnYes && m_pMessageBox->GetResult() == MSGBOX_YES)
				SetMode(MODE_TEXTEDITOR);
			m_bEditOnYes = FALSE;
			goto done;
		}
	}

	// A file operation runs: BACK asks to cancel it.
	if (g_bFileOperationActive && m_nMode != MODE_FILEMANAGER)
	{
		if (action == INPUT_BACK)
		{
			g_bCancelOperation = TRUE;
			if (g_nFileOperationType == 0)
				ShowMessage(GetStringA(STR_MSG_COPY_CANCELED), GetString(STR_INFORMATION), 0);
		}
		Sleep(5);
		g_bFileOperationActive = FALSE;
		goto done;
	}

	// The startup password. (Quirk: a password of "MicrosoftXBOX" accepts
	// anything.)
	if (m_bPasswordShown)
	{
		g_virtualKeyboard.ProcessInput(&action);
		m_bPasswordShown = !g_virtualKeyboard.IsDone();
		if (m_bPasswordShown || g_pSettings->m_bUnlocked)
			goto done;
		std::string typed = g_virtualKeyboard.GetResult();
		g_pSettings->m_bUnlocked = g_pSettings->m_strPassword.compare(typed.c_str()) == 0 ||
		                             g_pSettings->m_strPassword.compare("MicrosoftXBOX") == 0;
		if (!g_pSettings->m_bUnlocked)
		{
			m_bPasswordShown = TRUE;
			g_pSettings->m_nPasswordFailures++;
			if (g_pSettings->m_nPasswordMaxTries != 0 &&
			    (unsigned)g_pSettings->m_nPasswordMaxTries <= (unsigned)g_pSettings->m_nPasswordFailures)
			{
				s_bLockedOut = TRUE;
				std::string text;
				StrFormat(text, "%s\n\n%s", GetStringA(STR_MSG_INVALID_PASSWORD), GetStringA(STR_MSG_MAX_INVALID_PASSWORD));
				ShowMessage(text.c_str(), GetString(STR_INVALID_PASSWORD), 0);
			}
			else
			{
				ShowMessage(GetStringA(STR_MSG_INVALID_PASSWORD), GetString(STR_INVALID_PASSWORD), 0);
			}
		}
		g_virtualKeyboard.Open();
		g_bKeyboardPassword = !g_pSettings->m_bUnlocked;
		goto done;
	}

	// The startup passcode (controller buttons).
	if (!g_pSettings->m_bUnlocked && m_pPassCode != NULL)
	{
		m_pPassCode->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
		if (m_pPassCode->IsClosed() && !g_pSettings->m_bUnlocked)
		{
			g_pSettings->m_bUnlocked = m_pPassCode->IsCorrect();
			if (!g_pSettings->m_bUnlocked)
			{
				m_pPassCode->Show();
				g_pSettings->m_nPasswordFailures++;
				if (g_pSettings->m_nPasswordMaxTries != 0 &&
				    (unsigned)g_pSettings->m_nPasswordMaxTries <= (unsigned)g_pSettings->m_nPasswordFailures)
				{
					s_bLockedOut = TRUE;
					std::string text;
					StrFormat(text, "%s\n\n%s", GetStringA(STR_MSG_INVALID_PASSCODE), GetStringA(STR_MSG_MAX_INVALID_PASSCODE));
					ShowMessage(text.c_str(), GetString(STR_INVALID_PASSCODE), 0);
				}
				else
				{
					ShowMessage(GetStringA(STR_MSG_INVALID_PASSCODE), GetString(STR_INVALID_PASSCODE), 0);
				}
			}
			else
			{
				delete m_pPassCode;
				m_pPassCode = NULL;
			}
		}
		goto done;
	}

	// The clock prompt after a cold boot.
	if (g_bColdBoot && m_pClockEditor != NULL)
	{
		m_pClockEditor->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
		g_bColdBoot = m_pClockEditor->m_bOpen;
		if (!g_bColdBoot)
		{
			delete m_pClockEditor;
			m_pClockEditor = NULL;
		}
		goto done;
	}
	if (!g_pSettings->m_bUnlocked)
		goto done;

	// The current screen.
	switch (m_nMode)
	{
	case MODE_MENU:
		if (g_bUpdateAvailable && !m_bMessageShown)
		{
			ShowMessage(g_strUpdateMessage.c_str(), GetString(STR_INFORMATION), 0);
			g_bUpdateAvailable = FALSE;
		}
		if (m_menu.ProcessInput(&action, (SGamepadState **)&g_gamepadInput) >= 0)
		{
			// The globe spins the way the menu moved.
			if (action == INPUT_UP || action == INPUT_LTRIGGER)
			{
				g_fGlobeSpin = 1.0f;
				g_dwGlobeSpinTime = now;
			}
			else if (action == INPUT_DOWN || action == INPUT_RTRIGGER)
			{
				g_fGlobeSpin = -1.0f;
				g_dwGlobeSpinTime = now;
			}
			break;
		}
		if (action == INPUT_Y || action == INPUT_IR_INFO)
		{
			SetMode(MODE_SYSINFO);
			break;
		}
		if (action == INPUT_BLACK)
		{
			g_skin.SetPreviewPlaying(!g_skin.m_bPreviewPlaying);
			break;
		}
		{
			CMusicManager *music = g_pMusicManager;
			if (g_bMusicReady && music == NULL && (action == INPUT_RTHUMB_CLICK || action == INPUT_IR_PLAY))
			{
				m_bStartMusic = TRUE;
				break;
			}
			// BLACK held + right: the next news item.
			if (m_defaultGamepad.bAnalogButtons[4] > 30)
			{
				if (g_pSettings->m_bNewsfeedEnable && (m_defaultGamepad.wPressedButtons & 0x08))
				{
					InvalidateSkinText(TEXT_NEWSFEED);
					goto done;
				}
				break;
			}
			if (music == NULL || action == INPUT_NONE)
				break;
			if (action == INPUT_RTHUMB_CLICK || action == INPUT_IR_DISPLAY)
			{
				if (g_skin.IsPreviewPlaying())
				{
					g_skin.TogglePreviewFullScreen();
					action = INPUT_NONE;
					music->HandleInput(&action, (SGamepadState **)&g_gamepadInput);
					break;
				}
				if (m_nMode == MODE_MUSIC)
				{
					SetMode(MODE_MENU);
					music = g_pMusicManager;
					action = INPUT_NONE;
				}
			}
			if (action == INPUT_WHITE)
			{
				if (music != NULL && music->IsPlaying())
					SetMode(MODE_MUSIC);
				break;
			}
			music->HandleInput(&action, (SGamepadState **)&g_gamepadInput);
		}
		break;

	case MODE_FILEMANAGER:
		if (m_pFileManager == NULL)
		{
			SetMode(MODE_MENU);
			break;
		}
		m_pFileManager->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
		if (!m_pFileManager->m_bOpen)
			SetMode(MODE_MENU);
		break;

	case MODE_GAMEMANAGER:
		if (m_pGameManager == NULL)
		{
			SetMode(MODE_MENU);
			break;
		}
		m_pGameManager->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
		if (!m_pGameManager->m_bOpen)
			SetMode(MODE_MENU);
		break;

	case MODE_TEXTEDITOR:
		if (m_pTextEditor != NULL)
		{
			m_pTextEditor->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
			if (!m_pTextEditor->IsOpen())
			{
				delete m_pTextEditor;
				m_pTextEditor = NULL;
				SetMode(MODE_MENU);
			}
		}
		// (Then the settings screen too, if there is one.)
		// fall through
	case MODE_SETTINGS:
		if (m_pSettingsMenu != NULL)
		{
			m_pSettingsMenu->HandleInput(&action, (SGamepadState **)&g_gamepadInput);
			if (m_pSettingsMenu->m_bClosed)
			{
				delete m_pSettingsMenu;
				m_pSettingsMenu = NULL;
				SetMode(MODE_MENU);
			}
		}
		break;

	case MODE_SKINS:
		if (m_pSkinBrowser != NULL)
		{
			m_pSkinBrowser->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
			if (!m_pSkinBrowser->IsOpen())
			{
				m_unknown1964 = 0;
				delete m_pSkinBrowser;
				m_pSkinBrowser = NULL;
				SetMode(MODE_MENU);
			}
		}
		break;

	case MODE_SYSINFO:
		if (m_pSystemInfo != NULL)
		{
			m_pSystemInfo->ProcessInput((SGamepadState **)&g_gamepadInput, &action);
			if (m_pSystemInfo->m_bClosed)
			{
				delete m_pSystemInfo;
				m_pSystemInfo = NULL;
				SetMode(MODE_MENU);
			}
		}
		break;

	case MODE_MUSIC:
		if (g_pAudioGUI != NULL)
		{
			g_pAudioGUI->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
			if (!g_pAudioGUI->IsOpen())
				SetMode(MODE_MENU);
		}
		break;
	}

	// The screen saver takes the input while it runs.
	if (m_bScreenSaverActive && m_pScreenSaver != NULL)
	{
		if (action == INPUT_OPERATION_DONE)
			UpdateScreenSaver(&action);
		else
			m_pScreenSaver->ProcessInput(&action, (SGamepadState **)&g_gamepadInput);
	}

done:
	// BACK (when no keyboard is up) and the remote's MENU go back to the
	// menu.
	if (action == INPUT_BACK && !g_virtualKeyboard.IsDone())
	{
		g_nKeyboardMode = 1;
	}
	else if (action == INPUT_BACK || action == INPUT_IR_MENU)
	{
		if (m_nMode > MODE_MENU)
		{
			SetMode(MODE_MENU);
			action = INPUT_NONE;
		}
	}

	// (Unused.)
	static float s_unused[3] = {0.0f, 0.0f, 0.0f};   // 0x0027A5DC
	static float s_fStartTime = m_fTime;             // 0x0027A5D8
	(void)s_unused;
	(void)s_fStartTime;

	// The globe sways along <Main Globe> path, back and forth.
	if (!g_skin.m_bGlobe || !(m_fAppTime > 5.0f))
		return S_OK;
	float angle;
	if (m_nGlobePos >= 0x200)
	{
		m_nGlobeStep = -1;
		m_nGlobePos = 0x1FF;
		angle = m_globePath[0x1FF];
	}
	else if (m_nGlobePos < 0)
	{
		m_nGlobePos = 0;
		m_nGlobeStep = 1;
		angle = m_globePath[0];
	}
	else
	{
		angle = s_fGlobeAngle;
	}
	int next = m_nGlobeStep + m_nGlobePos;
	float target = m_globePath[next];
	angle = (target - m_globePath[m_nGlobePos]) / ((m_fFPS + 1.0f) * 0.15f) + angle;
	s_fGlobeAngle = angle;
	if (m_nGlobeStep == 1 ? (0.0f > angle || !(angle < target)) : !(target < angle))
		m_nGlobePos = next;
	BuildWorldMatrix(m_globeMatrix, 0.0f, 0.0f, 48.0f, 1.0f, 1.0f, 1.0f, 1.5707964f, angle + 3.1415927f, 0.0f);
	return S_OK;
}
