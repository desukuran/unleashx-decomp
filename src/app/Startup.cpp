// The other startup pieces: the thread counter, the startup thread (menu
// and ambient sound), boot shortcuts, the hardware monitor thread and the
// new hard disk wizard.
#include <cwchar>

#include "../audio/MusicManager.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../net/FtpServer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/SystemControl.h"
#include "../system/SystemInfo.h"
#include "../thirdparty/XKUtils.h"
#include "../ui/Language.h"
#include "../ui/MenuScan.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "UnleashX.h"

extern int g_nThreadCount;   // 0x00276248 ($Thread$)

// 0x000DFF8C
// Counts the running threads (registered with XRegisterThreadNotifyRoutine).
void __stdcall ThreadNotifyRoutine(BOOL create)
{
	if (create)
		g_nThreadCount++;
	else
		g_nThreadCount--;
}

// 0x000EA4AA
// Builds the menu (on its own thread) and loads the ambient sound, which
// only plays without soundtracks.
unsigned __stdcall StartupThread(void *)
{
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, MenuLoaderThread, NULL, 0, NULL);
	if (thread != NULL && thread != INVALID_HANDLE_VALUE)
		SetThreadPriority(thread, 1);
	if (g_pApp != NULL && g_pSettings->m_audio.m_nSoundTrackMode == 0 && g_pSettings->m_audio.m_bSoundEffects)
	{
		const std::string &ambient = g_pSettings->m_audio.m_strSound[SOUND_AMBIENT];
		if (FileExists(ambient.c_str()))
			g_soundEffects.Load("Ambient", ambient.c_str(), &g_pApp->m_pBackgroundSound);
	}
	WaitForSingleObject(thread, 120000);
	CloseHandle(thread);
	_endthreadex(0);
	return 0;
}

// 0x000E87BD
// A button held at boot runs its shortcut, <Settings><Shortcut><GP_A>...
// (A, START, B, X, Y, WHITE, BLACK; not BACK). The result is LaunchXbe's.
BOOL RunBootShortcut(int action)
{
	if (action < INPUT_A || action > INPUT_BLACK || action == INPUT_BACK)
		return FALSE;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
		return FALSE;
	CXmlElement *shortcuts = settings->FindFirstChild("Shortcut");
	if (shortcuts == NULL)
		return FALSE;
	std::string path;
	switch (action)
	{
	case INPUT_A:
		path = "GP_A";
		break;
	case INPUT_START:
		path = "GP_START";
		break;
	case INPUT_B:
		path = "GP_B";
		break;
	case INPUT_X:
		path = "GP_X";
		break;
	case INPUT_Y:
		path = "GP_Y";
		break;
	case INPUT_WHITE:
		path = "GP_WHITE";
		break;
	case INPUT_BLACK:
		path = "GP_BLACK";
		break;
	}
	if (path.size() == 0)
		return FALSE;
	CXmlElement *shortcut = shortcuts->FindFirstChild(path.c_str());
	if (shortcut == NULL)
		return FALSE;
	path = shortcut->m_strValue;
	if (path.size() == 0)
		return FALSE;
	// TODO: why an XBE marks the first frame as drawn.
	if (StrCmpNoCase(StrRight(path, 4), ".xbe") == 0)
		g_bFrameDrawn = TRUE;
	return LaunchXbe(path.c_str(), NULL, -1, NULL);
}

// 0x000E4188
// Watches the hardware once the first frame is drawn: reads the EEPROM,
// then (until a launch) polls the DVD drive and the CPU temperature while
// no file operation or FTP transfer runs.
unsigned __stdcall MonitorThread(void *param)
{
	CUnleashX *app = (CUnleashX *)param;
	try
	{
		SmcSetResetOnEject(FALSE);
	}
	catch (...)
	{
	}
	Sleep(500);
	while (!g_bFrameDrawn)
	{
		if (WaitForSingleObject(g_hExitEvent, 1000) == WAIT_OBJECT_0)
			return 0;
	}
	g_eeprom.ReadFromXBOX();
	Sleep(500);
	g_eeprom.Decrypt();
	if (g_pSettings->m_audio.m_nSoundTrackMode != 0)
	{
		// Give the soundtrack up to 5 seconds to start.
		for (unsigned i = 0; i < 5; i++)
		{
			if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
				break;
			if (WaitForSingleObject(g_hExitEvent, 1000) == WAIT_OBJECT_0)
				return 0;
		}
		Sleep(1000);
	}
	try
	{
		SmcSetResetOnEject(FALSE);
	}
	catch (...)
	{
	}
	if (app == NULL)
		return 0;

	// The fan speed setting is 10..50 (tenths of full speed).
	int &fanSetting = g_pSettings->m_prefs.m_nFanSpeed;
	if ((unsigned)fanSetting > 50)
		fanSetting = 50;
	else if ((unsigned)fanSetting < 10)
		fanSetting = 10;
	app->m_bDiscPresent = FALSE;
	DWORD nextCheck = GetTickCount() + 60000;
	unsigned fan = fanSetting;
	BOOL cooling = FALSE;
	int media = 0;
	while (!g_bLaunching)
	{
		if (g_nFileOperationType != 6 || g_ftpServer.m_nActiveTransfers != 0 || g_ftpServer.m_bPaused)
		{
			Sleep(1000);
			continue;
		}
		// A changed disc wakes the screen saver.
		RefreshDrives(FALSE);
		int current = CheckDvdMedia(g_pSettings->m_prefs.m_bDVDMediaCheckExtended);
		if (media != current)
		{
			app->m_bKeepAwake = TRUE;
			media = current;
		}
		if (WaitForSingleObject(g_hExitEvent, 300) == WAIT_OBJECT_0)
			break;
		// (Reset on eject is turned off again whenever the tray state
		// changes.)
		if (IsDiscPresent())
		{
			if (!app->m_bDiscPresent)
				SmcSetResetOnEject(FALSE);
			app->m_bDiscPresent = TRUE;
		}
		else
		{
			if (app->m_bDiscPresent)
				SmcSetResetOnEject(FALSE);
			app->m_bDiscPresent = FALSE;
		}

		// Over the temperature threshold the fan speeds up (by 5, then by
		// 1 every 30 s, up to 40); back under it, it returns to the
		// setting (10 = automatic). Checked every 15 s.
		unsigned threshold = (unsigned)g_pSettings->m_prefs.m_nTempThreshold;
		if (threshold == 0 || GetTickCount() <= nextCheck)
			continue;
		nextCheck = GetTickCount() + 15000;
		unsigned temperature = (unsigned)(LONGLONG)ReadTemperature(TRUE, TRUE);
		if (temperature > (unsigned)g_pSettings->m_prefs.m_nTempThreshold)
		{
			unsigned speed = fan + (cooling ? 1 : 5);
			fan = speed < 40 ? speed : 40;
			if (GetFanSpeed() < (int)fan)
				SetFanSpeed(fan);
			nextCheck = GetTickCount() + 30000;
			cooling = TRUE;
		}
		else if (cooling)
		{
			int setting = g_pSettings->m_prefs.m_nFanSpeed;
			if (GetFanSpeed() > setting + 1)
			{
				if (setting == 10)
					SmcSetFanAuto();
				else
					SetFanSpeed(setting);
				fan = g_pSettings->m_prefs.m_nFanSpeed;
				cooling = FALSE;
			}
		}
	}
	return 0;
}

// Both triggers held and START (0x000DFEF4) or BACK (0x000DFED7) pressed.
static BOOL IsFormatConfirmed(const XBGAMEPAD *pad)
{
	return pad != NULL && pad->bAnalogButtons[6] != 0 && pad->bAnalogButtons[7] != 0 &&
	       (pad->wPressedButtons & XINPUT_GAMEPAD_START) != 0;
}

static BOOL IsFormatSkipped(const XBGAMEPAD *pad)
{
	return pad != NULL && pad->bAnalogButtons[6] != 0 && pad->bAnalogButtons[7] != 0 &&
	       (pad->wPressedButtons & XINPUT_GAMEPAD_BACK) != 0;
}

// The wizard's screen: a box over the whole screen with centered text.
static void DrawWizardText(CXBFont *font, float width, float height, const wchar_t *text, D3DCOLOR color)
{
	g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
	DrawBevelBox(50.0f, 50.0f, width, height, 0xFFFFFFFF, 0xCD1E34F2, 1.0f, 1);
	DrawTextCentered(font, 320.0f, 240.0f, text, color, XBFONT_CENTER_X | XBFONT_CENTER_Y);
	g_pd3dDevice->Swap(0);
}

// Copies D:\<letter>Drive (or D:\<letter>Backup) to the drive, showing the
// file being copied, until the copy ends.
static void CopyDriveImage(CUnleashX *app, char letter, const char *target, float screenWidth, float screenHeight)
{
	std::string source;
	StrFormat(source, "D:\\%cDrive", letter);
	if (!DirectoryExists(source.c_str()))
	{
		StrFormat(source, "D:\\%cBackup", letter);
		if (!DirectoryExists(source.c_str()))
			source = "";
	}
	if (source.size() == 0)
	{
		// Without an E: image, E: gets the empty save folders.
		if (letter == 'E')
		{
			CreateDirectoryA("E:\\TData", NULL);
			CreateDirectoryA("E:\\UData", NULL);
		}
		return;
	}
	CopyFiles(source.c_str(), target);
	std::string caption;
	StrFormat(caption, "Copying files to Drive %c:\\...\n", letter);
	std::wstring text(caption.begin(), caption.end());
	for (;;)
	{
		Sleep(100);
		if (g_bFileOperationPaused || g_bOperationFailed || g_bCancelOperation)
			break;
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
		DrawBevelBox(50.0f, screenHeight * 0.5f - 50.0f, screenWidth, 150.0f, g_colorScheme.DIALOG_BORDERCOLOR,
		             g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
		DrawTextCentered(&app->m_fonts[1], 320.0f, 220.0f, text.c_str(), 0xFFFFFFFF,
		                 XBFONT_CENTER_X | XBFONT_CENTER_Y);
		DrawTextCentered(&app->m_fonts[1], 320.0f, 260.0f, g_szOperationFile, 0xFFFFFFFF,
		                 XBFONT_CENTER_X | XBFONT_CENTER_Y);
		g_pd3dDevice->Swap(0);
	}
}

// 0x000E48B4
// Run from DVD with no hard disk content: formats C:, E:, X:, Y:, Z: (and
// F:/G: if wanted), copies D:\CDrive, D:\EDrive, ... onto them and puts
// this XBE in place as C:\evoxdash.xbe and C:\xboxdash.xbe. Every step
// waits for both triggers and START.
void CUnleashX::PrepareHardDisk()
{
	// (The original takes this on the stack and returns if it's NULL.)
	float width = 0.0f;
	float height = 0.0f;
	GetScreenSize(&width, &height);
	Sleep(500);
	float boxWidth = width - 100.0f;
	float boxHeight = height - 100.0f;
	CXBFont *font = &m_fonts[1];
	static const char s_drives[] = "CEXYZ";

	do
	{
		Sleep(5);
		UpdateGamepads(this);
		DrawWizardText(font, boxWidth, boxHeight, GetString(STR_MSG_NEWHARDDISK), 0xFFFFFFFF);
	} while (!IsFormatConfirmed(&m_defaultGamepad));
	do
	{
		Sleep(5);
		UpdateGamepads(this);
		DrawWizardText(font, boxWidth, boxHeight, GetString(STR_MSG_NEWHD_WARNING), 0xFFFF0000);
	} while (!IsFormatConfirmed(&m_defaultGamepad));

	for (int i = 0; s_drives[i] != '\0'; i++)
	{
		wchar_t text[32];
		swprintf(text, 32, L"Formatting Drive %c:\\...", s_drives[i]);
		DrawWizardText(font, boxWidth, boxHeight, text, 0xFFFFFFFF);
		FormatDrive(s_drives[i]);
		Sleep(500);
	}

	// F: and G: (on big drives): format or skip (both triggers + BACK).
	BOOL formattedF = FALSE;
	BOOL formattedG = FALSE;
	for (;;)
	{
		Sleep(5);
		UpdateGamepads(this);
		DrawWizardText(font, boxWidth, boxHeight,
		               L"Do you have Drive F:\\?\n\nPress both Triggers and Start Button to format it...\nboth "
		               L"Triggers and BACK Button to skip",
		               0xFFFFFFFF);
		if (IsFormatConfirmed(&m_defaultGamepad))
		{
			DrawWizardText(font, boxWidth, boxHeight, L"Formatting Drive F:\\...", 0xFFFFFFFF);
			if (FormatDrive('F') < 0)
				break;
			formattedF = TRUE;
			SetDriveFMapped(TRUE);
			Sleep(500);
			MountDrive('F');
			// G: only after an F: of 0x1F40000000 bytes (~134 GB) or more.
			SDriveInfo *f = GetDriveInfo('F');
			if (f == NULL || f->totalBytes <= 0x1F40000000ULL)
				break;
			while (!IsFormatSkipped(&m_defaultGamepad))
			{
				Sleep(5);
				UpdateGamepads(this);
				DrawWizardText(font, boxWidth, boxHeight,
				               L"Do you have Drive G:\\?\n\nPress both Triggers and Start Button to format "
				               L"it...\nboth Triggers and BACK Button to skip",
				               0xFFFFFFFF);
				if (IsFormatConfirmed(&m_defaultGamepad))
				{
					DrawWizardText(font, boxWidth, boxHeight, L"Formatting Drive G:\\...", 0xFFFFFFFF);
					if (FormatDrive('G') >= 0)
					{
						formattedG = TRUE;
						SetDriveGMapped(TRUE);
					}
					break;
				}
			}
			break;
		}
		if (IsFormatSkipped(&m_defaultGamepad))
		{
			SetDriveFMapped(FALSE);
			break;
		}
	}

	MountAllDrives();
	CopyDriveImage(this, 'C', "C:\\", boxWidth, height);
	CopyDriveImage(this, 'E', "E:\\", boxWidth, height);
	if (formattedF)
	{
		CopyDriveImage(this, 'F', "F:\\", boxWidth, height);
		if (formattedG)
			CopyDriveImage(this, 'G', "G:\\", boxWidth, height);
	}
	if (!FileExists("C:\\evoxdash.xbe"))
	{
		CopyFileA("D:\\default.xbe", "C:\\evoxdash.xbe", TRUE);
		SetFileAttributesA("C:\\evoxdash.xbe", FILE_ATTRIBUTE_NORMAL);
	}
	if (!FileExists("C:\\xboxdash.xbe"))
	{
		CopyFileA("D:\\default.xbe", "C:\\xboxdash.xbe", TRUE);
		SetFileAttributesA("C:\\xboxdash.xbe", FILE_ATTRIBUTE_NORMAL);
	}
	do
	{
		Sleep(5);
		UpdateGamepads(this);
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
		DrawBevelBox(50.0f, height * 0.5f - 50.0f, boxWidth, 150.0f, g_colorScheme.DIALOG_BORDERCOLOR,
		             g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
		DrawTextCentered(font, 320.0f, 240.0f, GetString(STR_MSG_NEWHD_FINISHED), 0xFFFFFFFF,
		                 XBFONT_CENTER_X | XBFONT_CENTER_Y);
		g_pd3dDevice->Swap(0);
	} while (!IsFormatConfirmed(&m_defaultGamepad));
	Sleep(500);
	g_bFileOperationPaused = TRUE;
	g_bOperationFailed = FALSE;
	g_bCancelOperation = FALSE;
	g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
}

// 0x000E85B3
// Creates g_pMusicManager (from FrameMove, on a thread): soundtracks for
// <SoundTrack> modes 1 and 3, the music folder for modes 2 and 3; then
// random/global play, 5.1, the volume and the menu's background sound.
// Exit code 1 if Init failed.
unsigned __stdcall MusicManagerThread(void *param)
{
	CUnleashX *app = (CUnleashX *)param;
	unsigned result = 0;
	if (app == NULL)
		return result;
	app->m_bStartMusic = FALSE;
	if (g_bLaunching || g_pMusicManager != NULL)
		return result;

	SAudioSettings &audio = g_pSettings->m_audio;
	int mode = audio.m_nSoundTrackMode;
	g_pMusicManager = new CMusicManager(mode == 1 || mode == 3, 1 < (unsigned)mode, audio.m_strMusicPath.c_str());
	if (g_pMusicManager == NULL)
		return result;
	// Quirk: "global" only takes effect with random play.
	if (audio.m_bRandom)
	{
		g_pMusicManager->SetRandom(TRUE);
		g_pMusicManager->SetGlobal(audio.m_bGlobal);
	}
	else
	{
		g_pMusicManager->SetGlobal(FALSE);
	}
	// The players get the data folder's sample.xsb.
	std::string path = g_pSettings->m_strDataPath;
	StrReplaceAll(path, "U:\\", "E:\\UDATA\\");
	path.append("\\sample.xsb");
	StrReplaceAll(path, "\\\\", "\\");
	if (g_pMusicManager->Init(path.c_str()) < 0)
	{
		delete g_pMusicManager;
		g_pMusicManager = NULL;
		return 1;
	}
	try
	{
		if (audio.m_bEnable51)
		{
			audio.SetEnable51(TRUE);
			if (audio.m_bEnable51)
				g_pMusicManager->SetSpeakerConfig(audio.m_bEnable51 ? g_pSettings->m_speakerMixBins : NULL);
		}
	}
	catch (...)
	{
	}
	g_pMusicManager->SetVolume((float)g_pSettings->GetMusicVolume());
	Sleep(500);
	app->PlayBackgroundSound(audio.m_nSoundTrackMode == 0);
	app->m_bStartMusic = TRUE;
	return result;
}
