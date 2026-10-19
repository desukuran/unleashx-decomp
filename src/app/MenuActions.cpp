// What menu items do: <Item Action="..."> and <Command Action="...">.
//
// An Action is either the path of an XBE to launch, or one of the commands
// handled in CUnleashX::CMenuEvent::OnItemSelected. File commands take their
// operands from Arg1/Arg2 (Arg works for Arg1); "KBResult" or "$KBResult$"
// in an argument stands for the on-screen keyboard's last text.
#include "UnleashX.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../net/FtpServer.h"
#include "../net/Network.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/Storage.h"
#include "../system/SystemControl.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"

std::string g_strLoadingPreview;          // 0x00277F70
BYTE g_bLaunching;                        // 0x00276236
BYTE g_bShuttingDown;                     // 0x00276235
DWORD g_dwShutdownDeadline;               // 0x00276238

// Video standard switching (the EEPROM setting), used when launching games
// of another region and by the VideoMode action.
int GetVideoRegionSetting();                         // 0x00039507 TODO: decompile; 1 NTSC-M, 2 NTSC-J, 4 PAL
void SetVideoRegion(int standard, int mode);         // 0x000BCF48 TODO: decompile
// Shows the skin's loading screen for the XBE.
void ShowLaunchScreen(const char *path, const char *name);   // 0x000E3B65 TODO: decompile
// Maps D: to the XBE's folder and starts it; returns only on failure.
// Starts the network from the settings on a thread (unless one is running).

#ifndef _XBOX
static void WritePort(WORD, BYTE) {}
#else
static void WritePort(WORD port, BYTE value)
{
	_outp(port, value);
}
#endif

// 0x000E1988
// Moves (or adds) the XBE to the top of the recent list. Items on D: (the
// DVD) aren't remembered.
void AddRecentItem(const char *path, const char *name, int region)
{
	if (path[0] == 'd' || path[0] == 'D')
		return;

	SRecentItem item;
	strcpy(item.path, path);
	strcpy(item.name, name);
	item.launchCount = 1;
	item.region = region;

	SRecentItem items[10];
	memset(items, 0, sizeof(items));
	std::string file = g_pSettings->m_strDataPath;
	file += "recent.dat";
	FILE *fp = fopen(file.c_str(), "rb");
	if (fp != NULL)
	{
		fread(items, sizeof(items), 1, fp);
		fclose(fp);
	}

	// Already listed: count up and move it to the top. New: the last one
	// drops off.
	int slot = 9;
	for (int i = 0; i < 10; i++)
	{
		if (stricmp(items[i].path, path) == 0 && stricmp(items[i].name, name) == 0)
		{
			item.launchCount += items[i].launchCount;
			slot = i;
			break;
		}
	}
	for (int i = slot; i > 0; i--)
		items[i] = items[i - 1];
	items[0] = item;

	fp = fopen(file.c_str(), "wb");
	if (fp != NULL)
	{
		fwrite(items, sizeof(items), 1, fp);
		fclose(fp);
	}
}

// 0x000E2153
void CUnleashX::ShowMessage(const char *text, const wchar_t *title, int flags)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(&m_fonts[4], NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->SetButtonText(MSGBOX_OK, GetString(STR_OK));
			m_pMessageBox->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
			m_pMessageBox->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
			m_pMessageBox->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
			m_pMessageBox->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
			m_pMessageBox->SetButtonText(MSGBOX_YES, GetString(STR_YES));
			m_pMessageBox->SetButtonText(MSGBOX_NO, GetString(STR_NO));
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, flags);
		m_bMessageShown = TRUE;
	}
}

// 0x000E2853
BOOL CUnleashX::CreateTextEditor()
{
	if (m_pTextEditor == NULL)
	{
		m_pTextEditor = new CNotePad;
		if (m_pTextEditor != NULL)
			m_pTextEditor->Init(&m_fonts[4], NULL, NULL, NULL);
	}
	return m_pTextEditor != NULL;
}

// 0x000E4691
// Doesn't return when the launch works: the dashboard is torn down first and
// a failed launch reboots.
BOOL LaunchXbe(const char *path, const char *name, int region, const char *args)
{
	if (path == NULL || strlen(path) == 0 || !FileExists(path))
		return FALSE;

	EnterFileLock();
	SXbeInfo info;
	info.titleId = 0;
	memset(info.unknown20, 0, sizeof(info.unknown20));
	info.gameRegion = 0;
	std::string title = name;

	// <Preference AutoVideoMode>: read the game's region (and a missing
	// name) from the XBE.
	SPreferences &prefs = g_pSettings->m_prefs;
	if (prefs.m_bAutoVideoMode)
	{
		if (title.size() == 0 || region == -1)
		{
			ReadXbeInfo(path, &info);
			if (title.size() == 0)
				title = info.titleName;
		}
		if (region != -1)
			info.gameRegion = region;
	}

	if (g_hExitEvent != NULL)
		SetEvent(g_hExitEvent);
	g_bLaunching = TRUE;
	Sleep(5);
	ShowLaunchScreen(path, title.c_str());
	if (g_pApp != NULL)
		g_pApp->Cleanup();

	// Switch the video standard to the game's region: 1 North America,
	// 2 Japan, 4 Europe (PAL, 60 Hz with AutoVideoPAL60). Multi-region games
	// use <AutoVideoMode MultiRegion>.
	DWORD gameRegion = info.gameRegion;
	if (prefs.m_bAutoVideoMode && gameRegion != 0 && gameRegion <= 7)
	{
		int current = GetVideoRegionSetting();
		int target = 0;
		switch (gameRegion)
		{
		case 1:
		case 3:
			target = 1;
			break;
		case 2:
			target = 2;
			break;
		case 4:
			target = 4;
			break;
		default:
			switch (prefs.m_nMultiRegion)
			{
			case REGION_NTSC:
				target = 1;
				break;
			case REGION_NTSC_J:
				target = 2;
				break;
			case REGION_PAL:
				target = 4;
				break;
			}
			break;
		}
		if (target == 1 || target == 2)
		{
			if (current != target)
				SetVideoRegion(target, target);
		}
		else if (target == 4 && current != 4)
		{
			SetVideoRegion(3, prefs.m_bAutoVideoPAL60 ? 4 : 3);
		}
	}

	g_dwShutdownDeadline = GetTickCount() + 30000;
	g_bShuttingDown = TRUE;
	Sleep(100);
	LaunchXbeImage(path, args);

	// Still here: the launch failed.
	Sleep(500);
	if (!g_pSettings->m_bHomeNotOnD)
		RebootToDashboard();
	else
		SmcReset();
	LeaveFileLock();
	return FALSE;
}

// The original repeats this at every use.
void ApplyKeyboardResult(std::string &arg)
{
	if (StrCmpNoCase(arg.c_str(), "KBResult") == 0)
	{
		arg = g_virtualKeyboard.GetResult();
		StrTrimRight(StrTrimLeft(arg));
	}
	else
	{
		StrReplaceAll(arg, "$KBResult$", g_virtualKeyboard.GetResult().c_str());
	}
}

static void NormalizeSlashes(std::string &path)
{
	StrReplaceAll(path, "/", "\\");
	StrReplaceAll(path, "\\\\", "\\");
}

// The item's Arg1 (or Arg) and Arg2 for the file commands, resolved.
// Returns TRUE if both are set and differ.
static BOOL ResolveTwoPaths(std::string &arg1, std::string &arg2)
{
	ResolvePath(arg1.c_str(), arg1);
	ResolvePath(arg2.c_str(), arg2);
	return arg1.size() != 0 && arg2.size() != 0 && StrCmpNoCase(arg1, arg2.c_str()) != 0;
}

// Launches an <Item Action="...xbe">.
static BOOL LaunchItem(CUnleashX *app, CXmlElement *item, std::string &path)
{
	// The loading screen shows the item's preview video or its icon.
	if (StrCmpNoCase(g_skin.m_strLoadingPreview, "GamePreview") == 0)
	{
		g_strLoadingPreview = item->GetAttribute("Preview");
	}
	else if (StrCmpNoCase(g_skin.m_strLoadingBackground, "GameIcon") == 0)
	{
		if (app->m_menu.m_pIconTexture != NULL)
			g_pSettings->m_pLoadingIcon = app->m_menu.m_pIconTexture;
	}
	else if (g_skin.GetLoadingBackground() != NULL)
	{
		g_pSettings->m_pLoadingIcon = g_skin.GetLoadingBackground();
	}

	NormalizeSlashes(path);
	std::string message;
	if (!FileExists(path.c_str()))
	{
		StrFormat(message, "Unable to launch \"%s\".\nFile does not exists.", path.c_str());
		app->ShowMessage(message.c_str(), GetString(STR_ERROR));
		return FALSE;
	}

	// Locked items stay out of the recent list.
	if (item->GetAttribute("Password").size() == 0 && item->GetAttribute("Passcode").size() == 0)
		AddRecentItem(path.c_str(), item->GetValue().c_str(), atol(item->GetAttribute("Region").c_str()));

	if (!LaunchXbe(path.c_str(), item->GetValue().c_str(), atol(item->GetAttribute("Region").c_str()),
	               item->GetAttribute("Arg1").c_str()))
	{
		StrFormat(message, "Unable to launch \"%s\".", path.c_str());
		app->ShowMessage(message.c_str(), GetString(STR_ERROR));
	}
	return FALSE;
}

// VideoMode Arg1="NTSC M|NTSC|NTSC J|PAL50|PAL60|PAL" [Arg2="60"]
static BOOL SetVideoModeAction(const std::string &mode, const std::string &arg2)
{
	int current = GetVideoRegionSetting();
	BOOL pal60 = FALSE;
	int target;
	if (StrCmpNoCase(mode, "NTSC M") == 0 || StrCmpNoCase(mode, "NTSC") == 0)
		target = 1;
	else if (StrCmpNoCase(mode, "NTSC J") == 0)
		target = 2;
	else if (StrCmpNoCase(mode, "PAL50") == 0)
		target = 4;
	else if (StrCmpNoCase(mode, "PAL60") == 0)
	{
		target = 4;
		pal60 = TRUE;
	}
	else if (StrCmpNoCase(mode, "PAL") == 0)
	{
		target = 4;
		pal60 = StrCmpNoCase(arg2, "60") == 0;
	}
	else
		return FALSE;

	// Quirk: already PAL means no change, 50 or 60 Hz.
	if (target == current)
		return TRUE;
	if (target == 4)
		SetVideoRegion(3, 3 + pal60);
	else
		SetVideoRegion(target, target);
	return TRUE;
}

// Runs the file commands (and VideoMode). Returns 1 when done, 0 when an
// argument is missing (silently), -1 for an invalid command.
static int RunFileCommand(CUnleashX *app, CXmlElement *item, const std::string &action)
{
	(void)app;
	std::string arg1 = item->GetAttribute("Arg1");
	if (arg1.size() == 0 && item->GetAttribute("Arg").size() == 0)
	{
		if (StrCmpNoCase(action.c_str(), "CopyDVD") == 0)
		{
			CopyDVD(NULL);
			return 1;
		}
		return -1;
	}

	// The original strdup()s the action to lowercase it and never frees it.
	std::string command = StrToLower(action);
	arg1 = item->GetAttribute("Arg1");
	if (arg1.size() == 0)
		arg1 = item->GetAttribute("Arg");
	StrTrimRight(StrTrimLeft(arg1));
	if (arg1.size() == 0)
		return 0;
	ApplyKeyboardResult(arg1);
	NormalizeSlashes(arg1);

	std::string arg2 = item->GetAttribute("Arg2");
	if (arg2.size() != 0)
	{
		ApplyKeyboardResult(arg2);
		NormalizeSlashes(arg2);
	}

	if (StrCmpNoCase(command, "format") == 0)
	{
		FormatDrive(StrLeft(arg1, 1)[0]);
		return 1;
	}
	if (StrCmpNoCase(command, "copydvd") == 0)
	{
		ResolvePath(arg1.c_str(), arg1);
		CopyDVD(arg1.size() != 0 ? arg1.c_str() : NULL);
		return 1;
	}
	if (StrCmpNoCase(command, "copy") == 0)
	{
		if (!ResolveTwoPaths(arg1, arg2))
			return -1;
		CopyFiles(arg1.c_str(), arg2.c_str());
		return 1;
	}
	if (StrCmpNoCase(command, "Move") == 0)
	{
		if (!ResolveTwoPaths(arg1, arg2))
			return -1;
		MoveFiles(arg1.c_str(), arg2.c_str());
		return 1;
	}
	if (StrCmpNoCase(command, "Rename") == 0)
	{
		if (!ResolveTwoPaths(arg1, arg2))
			return -1;
		return MoveFileA(arg1.c_str(), arg2.c_str()) ? 1 : 0;
	}
	if (StrCmpNoCase(command, "Delete") == 0)
	{
		ResolvePath(arg1.c_str(), arg1);
		if (DirectoryExists(arg1.c_str()))
		{
			DeleteFolder(arg1.c_str());
			return 1;
		}
		if (FileExists(arg1.c_str()))
			return DeleteFileA(arg1.c_str()) ? 1 : 0;
		return -1;
	}
	if (StrCmpNoCase(command, "zip") == 0)
	{
		if (!ResolveTwoPaths(arg1, arg2))
			return -1;
		ZipFiles(arg1.c_str(), arg2.c_str(), 0);
		return 1;
	}
	if (StrCmpNoCase(command, "unzip") == 0 || StrCmpNoCase(command, "UnRar") == 0)
	{
		// Either command handles both formats, by extension.
		if (!ResolveTwoPaths(arg1, arg2))
			return -1;
		if (StrCmpNoCase(StrRight(arg1, 4), ".rar") == 0)
			UnRarFile(arg1.c_str(), arg2.c_str());
		else
			UnzipFile(arg1.c_str(), arg2.c_str());
		return 1;
	}
	if (StrCmpNoCase(command, "VideoMode") == 0)
		return SetVideoModeAction(arg1, arg2) ? 1 : -1;
	return -1;
}

static void OpenFileManager(CUnleashX *app, CXmlElement *item)
{
	if (app->m_pFileManager == NULL)
	{
		app->m_pFileManager = new CFileManager;
		Sleep(5);
	}
	CFileManager *fm = app->m_pFileManager;
	if (fm == NULL)
		return;

	if (!fm->m_bInitialized)
	{
		if (app->m_pFileIcon == NULL)
			app->m_pFileIcon = app->m_resource.GetTexture("File.png");
		if (app->m_pFolderIcon == NULL)
			app->m_pFolderIcon = app->m_resource.GetTexture("Folder.png");
		fm->Init(0, 0, app->m_pFolderIcon, app->m_pFileIcon);
		Sleep(5);
	}

	std::string path = item->GetAttribute("Arg1");
	if (path.size() != 0)
	{
		ApplyKeyboardResult(path);
		fm->OpenPath(path.c_str());
	}
	app->SetMode(MODE_FILEMANAGER);
}

// 0x000EE514 (CUnleashX::CMenuEvent vtable slot 1)
BOOL CUnleashX::CMenuEvent::OnItemSelected(CXmlElement *item)
{
	CUnleashX *app = m_pApp;
	if (item == NULL)
		return FALSE;

	if (StrCmpNoCase(item->m_strName, "Command") == 0)
	{
		// <Command Action="filemanager" Arg1="..."> opens the file manager
		// like the equivalent <Item>.
		std::string action = item->GetAttribute("Action");
		if (StrCmpNoCase(action.c_str(), "filemanager") == 0)
		{
			CXmlElement *fake = new CXmlElement;
			if (fake != NULL)
			{
				fake->m_strName = "Item";
				fake->SetAttribute("Action", "filemanager");
				fake->SetAttribute("Arg1", item->GetAttribute("Arg1").c_str());
				OnItemSelected(fake);
				delete fake;
			}
		}
		return TRUE;
	}
	if (StrCmpNoCase(item->m_strName, "Item") != 0)
		return TRUE;

	std::string action = item->GetAttribute("Action");
	const char *cmd = action.c_str();
	std::string message;

	if (StrCmpNoCase(StrRight(action, 4), ".xbe") == 0)
		return LaunchItem(app, item, action);

	if (StrCmpNoCase(cmd, "LaunchDVD") == 0)
	{
		app->LaunchDVD();
		return FALSE;
	}
	if (StrCmpNoCase(cmd, "filemanager") == 0)
	{
		OpenFileManager(app, item);
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "SavesManager") == 0 || StrCmpNoCase(cmd, "GameManager") == 0)
	{
		if (g_pSettings->m_bRunFromDVD)
		{
			StrFormat(message, "'%s(%s)' Action command is not available when in DVD mode",
			          item->GetValue().c_str(), item->GetAttribute("Action").c_str());
			app->ShowMessage(message.c_str(), L"Invalid Command");
			return FALSE;
		}
		if (app->m_pGameManager == NULL)
		{
			app->m_pGameManager = new CGameManager;
			if (app->m_pGameManager != NULL && FAILED(app->m_pGameManager->Init(&app->m_fonts[4])))
			{
				delete app->m_pGameManager;
				app->m_pGameManager = NULL;
			}
			Sleep(5);
		}
		if (app->m_pGameManager != NULL)
		{
			app->m_pGameManager->Show();
			app->SetMode(MODE_GAMEMANAGER);
		}
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "Settings") == 0)
	{
		if (app->m_pSettingsMenu == NULL)
		{
			app->m_pSettingsMenu = new CSettingsMenu;
			if (app->m_pSettingsMenu != NULL)
				app->m_pSettingsMenu->Init(&app->m_fonts[4]);
		}
		if (app->m_pSettingsMenu != NULL)
		{
			app->m_pSettingsMenu->Show();
			app->SetMode(MODE_SETTINGS);
		}
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "Skins") == 0 || StrCmpNoCase(cmd, "Skin") == 0)
	{
		if (app->m_pSkinBrowser == NULL)
		{
			app->m_pSkinBrowser = new CSkinBrowser;
			if (app->m_pSkinBrowser != NULL)
			{
				Sleep(5);
				app->m_pSkinBrowser->m_strSkinPath = g_pSettings->m_prefs.m_strSkinPath.c_str();
				IDirect3DTexture8 *checked = app->m_resource.GetTexture("CheckedBox.png");
				IDirect3DTexture8 *unchecked = app->m_resource.GetTexture("UncheckedBox.png");
				app->m_pSkinBrowser->Init(checked, unchecked, &app->m_fonts[4]);
				Sleep(5);
			}
		}
		if (app->m_pSkinBrowser != NULL)
		{
			app->m_pSkinBrowser->m_strSkinPath = g_pSettings->m_prefs.m_strSkinPath.c_str();
			app->m_pSkinBrowser->Show();
			app->SetMode(MODE_SKINS);
		}
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "reboot") == 0 || StrCmpNoCase(cmd, "restart") == 0)
	{
		g_bLaunching = TRUE;
		Sleep(50);
		if (g_pSettings->m_bHomeNotOnD)
			SmcReset();
		else
			RebootToDashboard();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "reset") == 0)
	{
		RebootToDashboard();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "powercycle") == 0)
	{
		SmcPowerCycle();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "shutdown") == 0)
	{
		// The main loop powers off once g_bShuttingDown is seen.
		g_bShuttingDown = TRUE;
		g_dwShutdownDeadline = 0;
		return FALSE;
	}
	if (StrCmpNoCase(cmd, "ftpreset") == 0)
	{
		g_bFtpEnabled = FALSE;
		if (IsFtpServerRunning())
			StopFtpServer();
		g_bFtpEnabled = TRUE;
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "ftpstop") == 0)
	{
		g_bFtpEnabled = FALSE;
		if (IsFtpServerRunning())
			StopFtpServer();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "ftpstart") == 0)
	{
		if (!IsFtpServerRunning())
			StartFtpServer();
		g_bFtpEnabled = TRUE;
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "netreset") == 0)
	{
		g_bFtpEnabled = FALSE;
		StopFtpServer();
		g_pSettings->m_network.m_bEnable = TRUE;
		StopNetwork();
		Sleep(100);
		StartNetworkThread();
		g_bFtpEnabled = TRUE;
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "trayclose") == 0)
	{
		g_trayMonitor.CloseTray();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "trayopen") == 0)
	{
		g_trayMonitor.OpenTray();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "PrepareHD") == 0)
	{
		app->PrepareHardDisk();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "SetClock") == 0)
	{
		if (app->m_pClockEditor == NULL)
		{
			app->m_pClockEditor = new CClockEditor;
			if (app->m_pClockEditor != NULL && FAILED(app->m_pClockEditor->Init(&app->m_fonts[1])))
			{
				delete app->m_pClockEditor;
				app->m_pClockEditor = NULL;
			}
		}
		// The clock screen is the one shown after a cold boot.
		g_bColdBoot = app->m_pClockEditor != NULL;
		if (g_bColdBoot)
			app->m_pClockEditor->Show();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "TextEditor") == 0)
	{
		if (app->CreateTextEditor() && app->m_pTextEditor != NULL)
		{
			std::string path = item->GetAttribute("Arg1");
			if (path.size() == 0)
			{
				app->m_pTextEditor->NewFile();
			}
			else
			{
				ApplyKeyboardResult(path);
				app->m_pTextEditor->Open(path.c_str());
			}
			app->SetMode(MODE_TEXTEDITOR);
		}
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "Xenium-OS") == 0 || StrCmpNoCase(cmd, "XeniumOS") == 0)
	{
		// Tells a Xenium modchip to boot its own menu on the next reset.
		WritePort(0xEF, 1);
		Sleep(5);
		SmcReset();
		return TRUE;
	}
	// The X2 modchip's in-game reset is off while this file exists.
	if (StrCmpNoCase(cmd, "X2IGR_OFF") == 0)
	{
		if (FileExists("E:\\x2_IGR_disabled"))
			return TRUE;
		if (!WriteTextFile("E:\\x2_IGR_disabled", " ", FALSE))
			return FALSE;
		SmcReset();
		return TRUE;
	}
	if (StrCmpNoCase(cmd, "X2IGR_ON") == 0)
	{
		if (!FileExists("E:\\x2_IGR_disabled"))
			return TRUE;
		SetFileAttributesA("E:\\x2_IGR_disabled", FILE_ATTRIBUTE_NORMAL);
		if (!DeleteFileA("E:\\x2_IGR_disabled"))
			return FALSE;
		SmcReset();
		return TRUE;
	}

	int result = RunFileCommand(app, item, action);
	if (result >= 0)
		return result;

	StrFormat(message, "Unable to execute command.\nItem: %s\nAction: %s", item->GetValue().c_str(),
	          item->GetAttribute("Action").c_str());
	app->ShowMessage(message.c_str(), L"Invalid Command");
	return FALSE;
}
