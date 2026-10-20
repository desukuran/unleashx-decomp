// Control Panel: Live Update. The settings (<Preference><LiveUpdate
// AutoCheck="">server</LiveUpdate>) plus the manual check and the download
// and install of a new build. See docs/NETWORK.md for Version.xml.
#include "SettingsPages.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../audio/MusicManager.h"
#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"
#include "../common/StrUtil.h"
#include "../net/Http.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../system/XbeLaunch.h"
#include "../thirdparty/Unrar.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"
#include "UnleashX.h"

// The MD5Hash of the release found by the last check (also used by the
// startup check). 0x0026C248
std::string g_strUpdateMD5;

// The install thread while it runs, else -1. 0x0026CDBC
static HANDLE s_hInstallThread = (HANDLE)-1;

#ifndef STILL_ACTIVE
#define STILL_ACTIVE 0x103
#endif

// 0x000C1C87
CLiveUpdate::CLiveUpdate()
{
	m_bFileBrowserOpen = FALSE;
	m_pSession = NULL;
	m_bDownloading = FALSE;
	m_bAsked = FALSE;
	m_bWIP = FALSE;
	m_bWIPAlternate = FALSE;
	m_bUpdateAvailable = FALSE;
	m_bBusy = FALSE;
	m_bUpdateReady = FALSE;
	m_pMirror = NULL;
	g_strUpdateMD5.clear();
}

// 0x000C1C39
CLiveUpdate::~CLiveUpdate()
{
	FreePopups();
	m_versionDoc.Clear();
	if (m_pSession != NULL)
	{
		delete m_pSession;
		m_pSession = NULL;
	}
}

// 0x000C422E
HRESULT CLiveUpdate::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Live Update");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	CAppSettings *settings = g_pSettings;
	AddItem(GetString(STR_AUTO_CHECK), settings->m_bLiveUpdateAutoCheck ? L"Yes" : L"No", ITEM_AUTO_CHECK, FALSE,
	        TRUE);
	// Not when running from DVD; the WIP line only with <LiveUpdate
	// EnableWIP="Yes">, and neither when UnleashX isn't what D: maps to.
	AddItem(GetString(STR_CHECK_UPDATE), L"", ITEM_CHECK, FALSE, !settings->m_bRunFromDVD);
	AddItem(L"Check WIP Update", L"", ITEM_CHECK_WIP, FALSE, !settings->m_bRunFromDVD && !settings->m_bHomeNotOnD,
	        settings->m_bLiveUpdateWIP && !settings->m_bHomeNotOnD);
	wchar_t text[160];
	// Quirk: the length passed excludes the '\0', so the text isn't
	// terminated by the conversion.
	MultiByteToWideChar(CP_ACP, 0, settings->m_strLiveUpdate.c_str(), (int)settings->m_strLiveUpdate.size(), text,
	                    80);
	AddItem(GetString(STR_SERVER), text, ITEM_SERVER, FALSE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	m_strServer = settings->m_strLiveUpdate;
	return S_OK;
}

// 0x000C1C20
void CLiveUpdate::Reset()
{
	CSettingsBase::Reset();
	if (m_pSession != NULL)
		m_pSession->Cancel();
}

// Starts the check: fetches <server>Version.xml.
static void StartCheck(CLiveUpdate *page)
{
	page->m_pMirror = NULL;
	g_strUpdateMD5.clear();
	if (page->m_pSession == NULL)
		page->m_pSession = new CInternetSession;
	if (page->m_pSession == NULL)
		return;
	page->m_pSession->Cancel();

	if (page->m_strServer.size() == 0)
	{
		// The default server, spelled out so it isn't a string in the XBE.
		StrFormat(page->m_strServer, "http://%c%c.%c%c%c%c.%c%c%c%c%c%c%c%c%c.com/", 'u', 'x', 'h', 'o', 'm', 'e', 'i',
		          'n', 's', 'i', 'g', 'h', 't', 'b', 'b');
		page->m_pSession->m_bHashCacheName = TRUE;
	}
	else
	{
		page->m_pSession->m_bHashCacheName = TRUE;
		if (StrCmpNoCase(StrLeft(page->m_strServer, 7), "http://") != 0)
			StrFormat(page->m_strServer, "%s%s", "http://", page->m_strServer.c_str());
		if (StrRight(page->m_strServer, 1).compare("/") != 0)
			page->m_strServer.append("/");
	}
	page->m_bUpdateAvailable = FALSE;
	page->m_bAsked = FALSE;
	std::string url;
	StrFormat(url, "%s%s", page->m_strServer.c_str(), "Version.xml");
	page->m_pSession->Get(url.c_str(), FALSE);
	page->m_bDownloading = FALSE;
	PlaySound(page->m_pSoundSelect);
	page->m_bBusy = TRUE;
}

// 0x000D4F97
HRESULT CLiveUpdate::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	BOOL dispatch = FALSE;
	if (m_bUpdateReady && m_bMessageBoxOpen)
	{
		// "Update successfully downloaded": closing the box starts
		// D:\UXUpdate.xbe, telling it which XBE to replace.
		m_pMessageBox->ProcessInput(action);
		m_bMessageBoxOpen = m_pMessageBox->IsOpen();
		if (m_bMessageBoxOpen)
		{
			dispatch = TRUE;   // Quirk: the action is still handled below
		}
		else
		{
			std::string updater;
			ResolvePath("D:\\", updater);
			StrAddBackslash(updater);
			std::string target = updater;
			updater.append("UXUpdate.xbe");
			target.append(GetPathFileName(g_pSettings->m_strLaunchPath.c_str(), TRUE));

			SXbeLaunchData launch;
			memset(&launch, 0, sizeof(launch));
			launch.magic = 0x9E115330;   // UnleashX's title ID
			strcpy(launch.path, target.c_str());
			strcpy(launch.name, GetPathFileName(g_pSettings->m_strLaunchPath.c_str(), TRUE).c_str());
			launch.type = XBE_LAUNCH_FROM_UPDATE;
			LaunchXbeWithData(updater.c_str(), (LAUNCH_DATA *)&launch);
			return S_OK;
		}
	}
	else if (m_bBusy && m_bAsked && m_bMessageBoxOpen)
	{
		// "Do you want to download this version?"
		m_pMessageBox->ProcessInput(action);
		m_bMessageBoxOpen = m_pMessageBox->IsOpen();
		if (!m_bMessageBoxOpen)
		{
			if (m_pMessageBox->GetResult() == MSGBOX_YES)
			{
				std::string url;
				StrFormat(url, "%s%s", m_strServer.c_str(), m_bWIP ? "UXUWIP2.dat" : "UXUpdate2.bin");
				m_pSession->Get(url.c_str(), FALSE);
				m_bDownloading = TRUE;
			}
			else
			{
				m_bBusy = FALSE;
				m_bDownloading = FALSE;
			}
		}
		*action = INPUT_NONE;
		return S_OK;
	}
	else if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_virtualKeyboard.m_bCancelled == 0)
		{
			wchar_t text[300];
			wcscpy(text, g_virtualKeyboard.GetText());
			SetItemValue(&m_items[m_nSelected], text, TRUE);
		}
		if (g_virtualKeyboard.m_bNumeric != 0)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = 0;
		return S_OK;
	}

	if (!dispatch)
	{
		if (FAILED(CSettingsBase::HandleInput(action, pad)))
			return S_OK;
		if (m_bModified && m_bAskSave && Save())
		{
			m_bClosed = TRUE;
			m_bAskSave = FALSE;
			return S_OK;
		}
	}

	CSettingItem item = m_items[m_nSelected];
	if (m_bBusy)
	{
		// B/BACK cancels a running check or download.
		if (*action == INPUT_B || *action == INPUT_BACK)
		{
			*action = INPUT_NONE;
			m_bDownloading = FALSE;
			m_bBusy = FALSE;
			if (m_pSession != NULL)
				m_pSession->Cancel();
		}
		return S_OK;
	}

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		if (IsSelectedDisabled())
			return S_OK;
		switch (item.m_nType)
		{
		case ITEM_AUTO_CHECK:
			SetItemValue(&m_items[m_nSelected], item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
			break;
		case ITEM_CHECK:
		case ITEM_CHECK_WIP:
			m_bWIP = item.m_nType == ITEM_CHECK_WIP;
			if (!m_bBusy)
				StartCheck(this);
			break;
		case ITEM_SERVER:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 80, 0);
			break;
		case ITEM_SAVE:
			if (m_bModified)
				Save();
			else
				m_bClosed = TRUE;
			break;
		case ITEM_RESET:
			RevertChanges();
			break;
		case ITEM_EXIT:
			AskToSave();
			break;
		}
		PlaySound(m_pSoundSelect);
		break;

	case INPUT_B:
	case INPUT_BACK:
		// (Unreachable: the base class has used these.)
		if (m_bDownloading && m_pSession != NULL)
			m_pSession->Cancel();
		else
			AskToSave();
		*action = INPUT_NONE;
		break;

	default:
		// (Up/down are handled here too, the same as in the base class,
		// which has always used them.)
		break;
	}
	return S_OK;
}

// Reads Version.xml and asks whether to download a newer build.
static void CheckVersion(CLiveUpdate *page)
{
	std::string message;
	CInternetSession *session = page->m_pSession;
	if (session->m_nStatus != 200)
	{
		StrFormat(message, GetStringA(STR_MSG_HTTP_FETCH_ERROR), session->m_nStatus);
		page->ShowMessage(message.c_str(), GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
		delete page->m_pSession;
		page->m_pSession = NULL;
		page->m_bBusy = FALSE;
		return;
	}

	std::string text;
	if (!session->m_bSavedToFile)
		text = _strdup(session->GetData());   // (the copy is never freed)
	else
		text = ReadTextFile(session->m_szFile);
	if (text.size() == 0 || text.size() > 0x1400)
	{
		page->ShowMessage("Can't find Live Update Script.", GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
		page->m_bBusy = FALSE;
		return;
	}

	CXmlElement *release = NULL;
	if (page->m_versionDoc.LoadFromString(text.c_str()))
	{
		const char *name = page->m_bWIP ? (page->m_bWIPAlternate ? "WIP_A" : "WIP") : "Release";
		page->m_versionDoc.m_pCurrent = &page->m_versionDoc.m_root;
		release = page->m_versionDoc.FindElement(&page->m_versionDoc.m_root, name);
	}
	if (release == NULL)
	{
		if (!page->m_bAsked)
		{
			page->ShowMessage("Unable to retrieve update information from the server.", GetString(STR_ERROR),
			                  MSGBOX_BUTTONS_OK);
			page->m_bBusy = FALSE;
			page->m_bUpdateAvailable = FALSE;
		}
		return;
	}

	unsigned major = (unsigned)atoi(release->GetAttribute("Major").c_str());
	unsigned minor = (unsigned)atoi(release->GetAttribute("Minor").c_str());
	unsigned revision = (unsigned)atoi(release->GetAttribute("Revision").c_str());
	g_strUpdateMD5 = release->GetAttribute("MD5Hash");
	// Quirk: "newer" is any major version, minor above 39 or revision above
	// 584, each on its own (see docs/NETWORK.md).
	if (g_strUpdateMD5.size() != 0 && (major > 0 || minor > 39 || revision > 584))
	{
		StrFormat(message,
		          "UnleashX V%u.%02u %s Build %u is available for download.\nDo you want to download this version?",
		          major, minor, release->GetAttribute("Type").c_str(), revision);
		page->m_bUpdateAvailable = TRUE;
		page->ShowMessage(message.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
		page->m_bAsked = TRUE;
	}
	else
	{
		page->ShowMessage("Your version is currently up to date.", GetString(STR_INFORMATION), MSGBOX_BUTTONS_OK);
		page->m_bBusy = FALSE;
		page->m_bUpdateAvailable = FALSE;
	}
}

// After the update was downloaded: saves it if it's still in memory, then
// unpacks it on a thread while showing "Rebuilding. Please Wait".
static void InstallDownload(CLiveUpdate *page)
{
	CInternetSession *session = page->m_pSession;
	std::string message;
	if (!session->m_bSavedToFile && session->m_dwDataSize > 0x100000 && !FileExists(session->m_szFile))
		WriteTextFile(session->m_szFile, session->GetData(), FALSE);
	if (!session->m_bSavedToFile && !FileExists(session->m_szFile))
	{
		page->ShowMessage("Unable to retrieve data from the server.", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		page->m_bBusy = FALSE;
		page->m_bUpdateAvailable = FALSE;
		page->m_bDownloading = FALSE;
		return;
	}

	if (s_hInstallThread == (HANDLE)-1)
	{
		s_hInstallThread = (HANDLE)_beginthreadex(NULL, 0, CLiveUpdate::InstallThread, page, 0, NULL);
		if (s_hInstallThread != NULL && s_hInstallThread != (HANDLE)-1)
		{
			Sleep(5);
		}
		else
		{
			page->m_bDownloading = FALSE;
			StrFormat(message, "Unspecified Error: %d", GetLastError());
			page->ShowMessage(message.c_str(), GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
		}
		return;
	}

	DrawBusyBox(page->m_pFont, L"Rebuilding. Please Wait");
	DWORD exitCode;
	if (WaitForSingleObject(s_hInstallThread, 5) != 0)
		return;
	GetExitCodeThread(s_hInstallThread, &exitCode);
	if (exitCode == STILL_ACTIVE)
		return;
	CloseHandle(s_hInstallThread);
	s_hInstallThread = (HANDLE)-1;
	page->m_bDownloading = FALSE;
	page->m_bBusy = FALSE;
	if (exitCode != 0)
	{
		page->ShowMessage("Unexpected error occured while installing update.\nFile may be corrupt or damaged.",
		                  GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
	}
	else
	{
		page->m_bUpdateReady = TRUE;
		page->ShowMessage("Update successfully downloaded.\nPress OK to reboot and finish the upgrade process.",
		                  GetString(STR_INFORMATION), MSGBOX_BUTTONS_OK);
	}
	delete page->m_pSession;
	page->m_pSession = NULL;
}

// 0x000D65CD
HRESULT CLiveUpdate::Render()
{
	CSettingsBase::Render();
	if (!m_bBusy)
		return S_OK;

	if (!m_pSession->IsDone())
	{
		CInternetSession::DrawProgress(m_pSession, 0, m_pFont);
		return S_OK;
	}
	if (!m_bDownloading)
	{
		if (!m_bAsked)
			CheckVersion(this);
		return S_OK;
	}

	if (m_pSession->m_nStatus != 200)
	{
		// On 404 try the <URL> mirrors listed in Version.xml (non-empty ones).
		if (m_pSession->m_nStatus == 404)
		{
			m_versionDoc.m_pCurrent = &m_versionDoc.m_root;
			if (m_pMirror == NULL)
			{
				m_pMirror = m_versionDoc.FindElement(&m_versionDoc.m_root, "URL");
			}
			else
			{
				do
				{
					m_versionDoc.m_pCurrent = &m_versionDoc.m_root;
					m_pMirror = m_versionDoc.FindNextElement(&m_versionDoc.m_root);
				} while (m_pMirror != NULL && m_pMirror->m_strValue.size() == 0);
			}
		}
		else
		{
			m_pMirror = NULL;
		}
		if (m_pMirror == NULL)
		{
			std::string message;
			StrFormat(message, "Unable to retrieve file: HTTP STATUS %d", m_pSession->m_nStatus);
			ShowMessage(message.c_str(), GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
			delete m_pSession;
			m_pSession = NULL;
			m_bBusy = FALSE;
			m_bDownloading = FALSE;
			return S_OK;
		}
		std::string url = m_pMirror->m_strValue;
		if (StrRight(url, 1).compare("/") != 0)
			url.append("/");
		url.append(m_bWIP ? "UXUWIP2.dat" : "UXUpdate2.bin");
		m_pSession->Get(url.c_str(), FALSE);
		return S_OK;
	}

	InstallDownload(this);
	return S_OK;
}

// 0x000D5EF9
unsigned __stdcall CLiveUpdate::InstallThread(void *param)
{
	CLiveUpdate *page = (CLiveUpdate *)param;
	if (page == NULL)
		return 1;
	if (g_pMusicManager != NULL)
		g_pMusicManager->Stop();   // (in a try/catch(...) in the original)
	Sleep(500);

	std::string download = _strdup(page->m_pSession->m_szFile);   // (never freed)
	Sleep(100);
	// Anything under 1 MB isn't an update.
	WIN32_FILE_ATTRIBUTE_DATA attributes;
	if (GetFileAttributesExA(download.c_str(), GetFileExInfoStandard, &attributes) &&
	    attributes.nFileSizeLow < 0x100000)
	{
		DeleteFileA(download.c_str());
		Sleep(1000);
		return 1;
	}

	std::string home;
	ResolvePath("D:\\", home);
	StrAddBackslash(home);
	std::string archive = home;
	archive.append("UXUpdate.dat");
	if (FileExists(archive.c_str()))
		DeleteFileA(archive.c_str());
	Sleep(500);
	if (!CopyFileA(download.c_str(), archive.c_str(), FALSE))
	{
		DeleteFileA(download.c_str());
		return 1;
	}
	DeleteFileA(download.c_str());
	Sleep(500);

	// Put back the RAR signature the server copy has blanked out.
	BYTE signature[10];
	memset(signature, 0, sizeof(signature));
	HexToBytes("526172211A0700", signature);
	FILE *file = fopen(archive.c_str(), "r+b");
	fseek(file, 0, SEEK_SET);
	fwrite(signature, 1, 7, file);
	fflush(file);
	fclose(file);
	Sleep(1000);

	std::string rarFile = archive;
	ArchiveList_struct *list = NULL;
	unsigned result = 1;
	Sleep(500);
	// The archive passwords, built a character at a time.
	char releasePassword[16] = { 'U', 'h', 's', 'a', 'e', 'l', 'n', 'U', 'n', 'l', 'e', 'a', 's', 'h', 'U', 0 };
	char wipPassword[16] = { 'W', 'h', 's', 'a', 'e', 'l', 'n', 'U', 'n', 'l', 'e', 'a', 's', 'h', 'W', 0 };
	int count = urarlib_list(rarFile.c_str(), &list, page->m_bWIP ? wipPassword : releasePassword);
	if (list != NULL && count > 0)
	{
		std::string root;   // (never set, so nothing is stripped)
		for (ArchiveList_struct *entry = list; entry != NULL; entry = entry->next)
		{
			if (entry->attributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			// Into UnleashX's folder; default.xbe is unpacked as UXUpdate.xbe.
			std::string name = entry->name;
			if (StrCmpNoCase(StrRight(name, 11), "default.xbe") == 0)
			{
				name = StrLeft(name, (int)name.size() - 11);
				name.append("UXUpdate.xbe");
			}
			StrReplaceAll(name, ":", "_");
			if (root.size() != 0)
				StrReplaceAll(name, root.c_str(), "");
			std::string relative = name;
			StrFormat(name, "%s%s", home.c_str(), relative.c_str());
			StrReplaceAll(name, "\\\\", "\\");
			StrReplaceAll(name, "/", "\\");
			DeleteFileA(name.c_str());
			std::string folder = GetPathDirectory(name.c_str());
			if ((int)folder.size() > 3)
				CreateDirectoryTree(folder.c_str());
			Sleep(500);
			if (urarlib_get(rarFile.c_str(), GetPathDirectory(name.c_str()).c_str(), NULL,
			                page->m_bWIP ? wipPassword : releasePassword, NULL) == 0)
			{
				result = 1;
				break;
			}
			if (result != 0 && StrFind(name, "UXUpdate") != -1 && FileExists(name.c_str()))
				result = 0;
			SetFileAttributesA(name.c_str(), FILE_ATTRIBUTE_NORMAL);
		}
		urarlib_freelist(list);
	}
	DeleteFileA(rarFile.c_str());
	DeleteFileA(archive.c_str());
	Sleep(1000);
	return result;
}

// 0x000CA280
BOOL CLiveUpdate::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	std::string text;
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		if (it->m_nType != ITEM_AUTO_CHECK && it->m_nType != ITEM_SERVER)
			continue;

		CXmlElement *preference = g_config.FindElement(settings, "Preference");
		CXmlElement *element = NULL;
		if (GetOrAddChildElement("Preference", settings, &preference))
		{
			element = preference->FindFirstChild("LiveUpdate");
			if (!GetOrAddChildElement("LiveUpdate", preference, &element))
				element = NULL;
		}
		if (element == NULL)
		{
			sprintf(message, "Unable to set automatic check enable. Error Code: %d", GetLastError());
			ok = FALSE;
			continue;
		}

		if (it->m_nType == ITEM_SERVER)
		{
			m_strServer = StrFromWide(it->m_strValue.c_str());
			element->SetValue(m_strServer.c_str());
			g_pSettings->m_strLiveUpdate = m_strServer;
		}
		else
		{
			g_pSettings->m_bLiveUpdateAutoCheck = !IsNoW(it->m_strValue.c_str());
			text = g_pSettings->m_bLiveUpdateAutoCheck ? "Yes" : "No";
			element->SetAttribute("AutoCheck", text.c_str());
		}
		changed = TRUE;
	}
	return FinishSave(ok, message, changed, FALSE);
}
