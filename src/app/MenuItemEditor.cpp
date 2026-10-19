// CMenuItemEditor (0x0010986F-0x0010CBF9): the menu's item editor. See
// MenuItemEditor.h.
#include "MenuItemEditor.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../net/Http.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../system/Storage.h"
#include "../ui/FileBrowser.h"
#include "../ui/Language.h"
#include "../ui/Menu.h"
#include "../ui/MenuDialogs.h"
#include "../ui/MenuItem.h"
#include "../ui/VirtualKeyboard.h"
#include "Screens.h"

HANDLE g_hItemInstallThread = INVALID_HANDLE_VALUE;   // 0x0026C728
BOOL g_bItemInstalling;                               // 0x00278D04

// A UTF-8 attribute or text as a shown value. Quirk: the original
// converts into a reused buffer without a terminator, so a value shorter
// than the one converted before it keeps that one's tail.
static std::wstring ToWide(const std::string &text)
{
	wchar_t buffer[260];
	int count = MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), buffer, 260);
	return std::wstring(buffer, count > 0 ? count : 0);
}

static float LineHeight(const CXBFont *font)
{
	return font->m_fFontHeight * font->m_fScaleFactorY;
}

// Checks a typed value: only the title (line 0) must not be empty.
static BOOL ValidateItemValue(int index, const wchar_t *text, char *message)   // 0x0010987A
{
	if (index == CMenuItemEditor::ITEM_TITLE && (text == NULL || wcslen(text) == 0))
	{
		sprintf(message, "Title not be empty.");
		return FALSE;
	}
	return TRUE;
}

// 0x0010A4E5
CMenuItemEditor::CMenuItemEditor() : m_md5(NULL, -1)
{
	m_pItem = NULL;
	m_pSession = NULL;
	m_bDownloading = FALSE;
	m_nDownloadStep = DOWNLOAD_IDLE;
	m_pPassCode = NULL;
	m_bPassCodeOpen = FALSE;
	m_pDropDown = NULL;
	m_bDropDownOpen = FALSE;
	m_bIsXbe = FALSE;
	m_strTitleId = "";
	m_strTitle = "";
	m_strAction = "";
	m_unknown24C = 0;
	m_nValueX = 0;
	m_nValueY = 0;
	m_pLogo = NULL;
	m_pSink = NULL;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x001098D2
CMenuItemEditor::~CMenuItemEditor()
{
	FreePopups();
	m_info.Clear();
	if (m_pLogo != NULL)
	{
		m_pLogo->Release();
		m_pLogo = NULL;
	}
	if (m_pSession != NULL)
	{
		delete m_pSession;
		m_pSession = NULL;
	}
	if (m_pPassCode != NULL)
	{
		delete m_pPassCode;
		m_pPassCode = NULL;
	}
	if (m_pDropDown != NULL)
	{
		delete m_pDropDown;
		m_pDropDown = NULL;
	}
}

// 0x0010A05B
HRESULT CMenuItemEditor::Init(CXBFont *font, CMenuItem *item)
{
	if (font == NULL)
		return E_FAIL;
	if (item != NULL)
	{
		m_pItem = item;
		SetTitle(item->m_pszText);
	}
	if (m_pFont != NULL)
		return S_OK;

	m_pFont = font;
	LoadSounds();
	m_fY = 80.0f;
	m_fX = 100.0f;
	m_fWidth = 440.0f;
	m_fHeight = 320.0f;
	m_items.clear();

	// The lines start with the item's values (Reset loads them again).
	CXmlElement *element = NULL;
	std::wstring icon;
	if (m_pItem != NULL)
	{
		element = m_pItem->m_pElement;
		icon = ToWide(m_pItem->m_strIcon);
	}
	AddItem(L"Title", m_pItem != NULL ? m_pItem->m_pszText : L"", ITEM_TITLE, FALSE, TRUE);
	AddItem(L"Icon", icon.c_str(), ITEM_ICON, FALSE, TRUE);
	AddItem(L"Preview", element != NULL ? ToWide(element->GetAttribute("Preview")).c_str() : L"", ITEM_PREVIEW,
	        FALSE, TRUE);
	AddItem(L"Password", element != NULL ? ToWide(element->GetAttribute("Password")).c_str() : L"",
	        ITEM_PASSWORD, FALSE, TRUE);
	AddItem(L"Passcode", element != NULL ? ToWide(element->GetAttribute("Passcode")).c_str() : L"",
	        ITEM_PASSCODE, FALSE, TRUE);
	AddItem(L"Save", L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(L"Save", L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(L"Reset", L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(L"Exit", L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x00109CE7
void CMenuItemEditor::Reset()
{
	CSettingsBase::Reset();
	m_bIsXbe = FALSE;
	m_strTitleId.clear();
	m_strTitle.clear();
	m_strAction.clear();

	BOOL haveItem = FALSE;
	if (m_pItem != NULL)
	{
		SetTitle(m_pItem->m_pszText);
		m_strTitle = StrFromWide(m_pItem->m_pszText);
		haveItem = TRUE;
		CXmlElement *element = m_pItem->m_pElement;
		if (element != NULL)
		{
			m_strAction = element->GetAttribute("Action");
			m_bIsXbe = StrCmpNoCase(StrRight(m_strAction, 4), ".xbe") == 0;
			if (m_bIsXbe)
			{
				// The title ID: <Item ID=...>, else from the XBE.
				m_strTitleId = element->GetAttribute("ID");
				if (m_strTitleId.size() == 0)
				{
					SXbeInfo info;
					EnterFileLock();
					if (ReadXbeInfo(m_strAction.c_str(), &info))
						StrFormat(m_strTitleId, "%08x", info.titleId);
					LeaveFileLock();
				}
			}
			SetItemValue(&m_items[ITEM_TITLE], m_pItem->m_pszText, FALSE);
			SetItemValue(&m_items[ITEM_ICON], ToWide(m_pItem->m_strIcon).c_str(), FALSE);
			SetItemValue(&m_items[ITEM_PREVIEW], ToWide(element->GetAttribute("Preview")).c_str(), FALSE);
			SetItemValue(&m_items[ITEM_PASSWORD], ToWide(element->GetAttribute("Password")).c_str(), FALSE);
			SetItemValue(&m_items[ITEM_PASSCODE], ToWide(element->GetAttribute("Passcode")).c_str(), FALSE);
		}
	}
	// Without an item there's nothing to edit but Reset and Exit.
	m_items[ITEM_TITLE].m_bEnabled = haveItem;
	m_items[ITEM_ICON].m_bEnabled = m_bIsXbe || haveItem;
	m_items[ITEM_PREVIEW].m_bEnabled = m_bIsXbe || haveItem;
	m_items[ITEM_PASSWORD].m_bEnabled = haveItem;
	m_items[ITEM_PASSCODE].m_bEnabled = haveItem;
	m_items[ITEM_SAVE].m_bEnabled = haveItem;
	if (m_pSession != NULL)
		m_pSession->Cancel();
	m_bDownloading = FALSE;
	m_nDownloadStep = DOWNLOAD_IDLE;
}

// Sets or (when empty) removes an attribute of both copies of the item.
static void SetBothAttributes(CXmlElement *saved, CXmlElement *shown, const char *name, const std::string &value)
{
	if (value.size() == 0)
	{
		saved->RemoveAttribute(name);
		shown->RemoveAttribute(name);
	}
	else
	{
		saved->SetAttribute(name, value.c_str());
		shown->SetAttribute(name, value.c_str());
	}
}

// 0x0010A61C
BOOL CMenuItemEditor::Save()
{
	if (!m_bModified)
		return TRUE;
	BOOL restart = FALSE;
	CXmlDocument *doc = NULL;
	BOOL ownDoc = FALSE;
	std::string message;
	std::string path;
	// The menu's own copy of the item, and the one in the file that's saved.
	CXmlElement *element = m_pItem->m_pElement;
	CXmlElement *target = NULL;
	if (element == NULL)
		return FALSE;

	BOOL found = FALSE;
	if (m_bIsXbe)
	{
		// A scanned game: its entry in items.xml (by Action).
		ResolvePath("d:\\items.xml", path);
		doc = new CXmlDocument;
		ownDoc = doc != NULL;
		if (doc != NULL && doc->Load(path.c_str()))
		{
			for (CXmlElement *folder = doc->GetFirstChild(NULL); folder != NULL && !found;
			     folder = doc->GetNextChild(NULL))
			{
				for (CXmlElement *child = folder->GetFirstChild(); child != NULL; child = folder->GetNextChild())
				{
					if (StrCmpNoCase(m_strAction, child->GetAttribute("Action").c_str()) == 0)
					{
						found = TRUE;
						target = child;
						break;
					}
				}
			}
			if (!found)
				doc->Clear();
		}
	}

	BOOL isList = FALSE;
	if (!found)
	{
		// Config.xml: the item under <Menu>.
		doc = &g_config;
		isList = StrCmpNoCase(element->m_strName, "List") == 0;
		// The <List> texts from the item up, each followed by '|'.
		std::string lists;
		for (CXmlElement *parent = element->m_pParent;
		     parent != NULL && StrCmpNoCase(parent->m_strName, "List") == 0; parent = parent->m_pParent)
		{
			lists.append(parent->GetAttribute("Text"));
			lists.append("|");
		}
		CXmlElement *container = g_config.FindElement(&g_config.m_root, "Menu");
		if (container != NULL)
		{
			// Quirk: only one level is followed. The texts run from the
			// nearest list up, the first match ends the walk, and the
			// later pieces keep their leading '|' so never match: an item
			// two lists deep is looked for in the wrong (or no) list.
			int pos = (int)lists.find("|");
			std::string piece;
			if (pos != -1)
			{
				while (container != NULL)
				{
					piece = StrLeft(lists, pos);
					lists = StrMid(lists, pos, (int)lists.size() - pos);
					if (piece.size() != 0)
					{
						for (CXmlElement *child = container->GetFirstChild(); child != NULL;
						     child = container->GetNextChild())
						{
							if (StrCmpNoCase(piece, child->GetAttribute("Text").c_str()) == 0)
							{
								container = child;
								lists.clear();
								break;
							}
						}
					}
					int next = (int)lists.find("|", 1);
					if (next == -1)
						break;
					pos = next;
				}
			}
			if (container != NULL)
			{
				// An <Item> by its Action (case-sensitive), a <List> by its Text.
				if (!isList || m_bIsXbe)
				{
					std::string action = element->GetAttribute("Action");
					for (target = container->FindFirstChild("Item"); target != NULL;
					     target = container->FindNextChild())
					{
						if (action.compare(target->GetAttribute("Action")) == 0)
							break;
					}
				}
				else
				{
					std::string text = element->GetAttribute("Text");
					for (target = container->FindFirstChild("List"); target != NULL;
					     target = container->FindNextChild())
					{
						if (text.compare(target->GetAttribute("Text")) == 0)
							break;
					}
				}
			}
		}
	}

	BOOL ok = FALSE;
	if (target != NULL)
	{
		std::string value;
		if (m_items[ITEM_TITLE].m_bChanged)
		{
			// An <Item>'s title is its text, a <List>'s its Text attribute.
			value = StrFromWide(m_items[ITEM_TITLE].m_strValue.c_str());
			if (!isList)
			{
				target->SetValue(value.c_str());
				element->SetValue(value.c_str());
			}
			else
			{
				target->SetAttribute("Text", value.c_str());
				element->SetAttribute("Text", value.c_str());
			}
			// Quirk: copied over the menu entry's text without a length
			// check.
			wcscpy(m_pItem->m_pszText, m_items[ITEM_TITLE].m_strValue.c_str());
		}
		if (m_items[ITEM_ICON].m_bChanged)
			SetBothAttributes(target, element, "AltIcon", StrFromWide(m_items[ITEM_ICON].m_strValue.c_str()));
		if (m_items[ITEM_PREVIEW].m_bChanged)
			SetBothAttributes(target, element, "Preview", StrFromWide(m_items[ITEM_PREVIEW].m_strValue.c_str()));
		if (m_items[ITEM_PASSWORD].m_bChanged)
		{
			SetBothAttributes(target, element, "Password",
			                  StrFromWide(m_items[ITEM_PASSWORD].m_strValue.c_str()));
			restart = TRUE;
		}
		if (m_items[ITEM_PASSCODE].m_bChanged)
		{
			SetBothAttributes(target, element, "Passcode",
			                  StrFromWide(m_items[ITEM_PASSCODE].m_strValue.c_str()));
			restart = TRUE;
		}
		ok = doc->Save(FALSE);
	}
	if (ownDoc && doc != NULL)
		delete doc;

	if (!ok)
	{
		StrFormat(message, "Unable to Save settings. Error Code: %d", GetLastError());
		ShowMessage(message.c_str(), L"Save Error", MSGBOX_BUTTONS_OK);
	}
	else
	{
		m_bModified = FALSE;
		if (!restart)
			ShowSavedMessage();
		else
			ShowRestartMessage();
		// A game's new title also goes into its XBE.
		if (m_bIsXbe && m_items[ITEM_TITLE].m_bChanged)
			SetXbeTitleName(m_strAction.c_str(), m_items[ITEM_TITLE].m_strValue.c_str());
	}
	return ok;
}

// 0x0010AE75
HRESULT CMenuItemEditor::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->ProcessInput(action, pad);
		if (m_pFileBrowser->IsOpen())
			return S_OK;
		m_bFileBrowserOpen = FALSE;
		if (m_pFileBrowser->m_bCancelled)
			return S_OK;
		std::string path = m_pFileBrowser->GetSelectedPath();
		if (path.size() != 0)
		{
			wchar_t text[260];
			MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 260);
			SetItemValue(&m_items[m_nSelected], text, TRUE);
			m_bModified = TRUE;
		}
		return S_OK;
	}
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_virtualKeyboard.m_bCancelled == 0)
		{
			wchar_t text[260];
			char message[260];
			wcscpy(text, g_virtualKeyboard.GetText());
			if (!ValidateItemValue(m_nSelected, text, message))
			{
				ShowMessage(message, L"Invalid Value", MSGBOX_BUTTONS_OK);
			}
			else
			{
				SetItemValue(&m_items[m_nSelected], g_virtualKeyboard.GetText(), TRUE);
				m_bModified = TRUE;
			}
		}
		if (g_virtualKeyboard.m_bNumeric != 0)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = 0;
		return S_OK;
	}
	if (m_bPassCodeOpen)
	{
		m_pPassCode->ProcessInput(action, pad);
		m_bPassCodeOpen = !m_pPassCode->IsClosed();
		if (m_bPassCodeOpen || m_pPassCode->m_bCancelled)
			return S_OK;
		std::wstring code = m_pPassCode->m_strInput;
		SetItemValue(&m_items[m_nSelected], code.c_str(), TRUE);
		m_bModified = TRUE;
		return S_OK;
	}
	if (m_bDropDownOpen)
	{
		m_pDropDown->ProcessInput(action);
		m_bDropDownOpen = m_pDropDown->IsOpen();
		if (m_bDropDownOpen || m_pDropDown->m_bCancelled || m_pDropDown->m_nSelection == -1)
			return S_OK;
		int choice = m_pDropDown->m_bCancelled ? -1 : m_pDropDown->m_nSelection;
		if (choice == 0)
		{
			// Clear Entry
			SetItemValue(&m_items[m_nSelected], L"", TRUE);
			m_items[m_nSelected].m_strValue.clear();
			return S_OK;
		}
		if (choice != 1)
			return S_OK;
		// Download Online...: first the title's info file.
		if (m_pSession == NULL)
		{
			m_pSession = new CInternetSession;
			std::string logo = m_pSettings->m_strDataPath;
			logo.append("\\xboxskins.jpg");
			if (FAILED(D3DXCreateTextureFromFileA(g_pd3dDevice, logo.c_str(), &m_pLogo)))
				m_pLogo = NULL;
			if (m_pSession == NULL)
				return S_OK;
		}
		std::string url;
		StrFormat(url, "http://www.xbox-skins.net/games/xml/%s.xml", m_strTitleId.c_str());
		m_pSession->Get(url.c_str(), TRUE);
		m_nDownloadStep = DOWNLOAD_INFO;
		m_bDownloading = TRUE;
		PlaySound(m_pSoundSelect);
		return S_OK;
	}
	if (m_bDownloading)
	{
		// B/BACK cancels.
		if (*action != INPUT_B && *action != INPUT_BACK)
			return S_OK;
		*action = INPUT_NONE;
		if (m_pSession != NULL)
			m_pSession->Cancel();
		m_bDownloading = FALSE;
		return S_OK;
	}

	if (FAILED(CSettingsBase::HandleInput(action, pad)))
		return S_OK;
	if (m_bModified && m_bAskSave && Save())
	{
		m_bClosed = TRUE;
		m_bAskSave = FALSE;
		return S_OK;
	}

	CSettingItem item = m_items[m_nSelected];
	// (A one-second timer is set up here but never used.)
	static DWORD s_dwNextUpdate = GetTickCount() + 1000;   // 0x0027A454
	(void)s_dwNextUpdate;
	std::string path;
	int type = item.m_nType;

	switch (*action)
	{
	case INPUT_A:
		if (IsSelectedDisabled())
			break;
		switch (type)
		{
		case ITEM_TITLE:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 40, 0);
			break;
		case ITEM_ICON:
		case ITEM_PREVIEW:
			// The browser starts in the current file's folder.
			path = StrFromWide(item.m_strValue.c_str());
			if (path.size() != 0)
			{
				ResolvePath(path.c_str(), path);
				path = GetPathDirectory(path.c_str());
			}
			ShowFileBrowser(item.m_strLabel.c_str(), FALSE, TRUE, FALSE, path.c_str(),
			                type == ITEM_ICON ? "png|jpg|bmp|xbx" : "xmv|wmv");
			break;
		case ITEM_PASSWORD:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), TRUE, 15, 0);
			break;
		case ITEM_PASSCODE:
			if (m_pPassCode == NULL)
			{
				m_pPassCode = new CPassCode;
				if (m_pPassCode != NULL)
				{
					m_pPassCode->SetTitle(L"Passcode");
					m_pPassCode->Init(m_pFont, NULL);
				}
				if (m_pPassCode == NULL)
					break;
			}
			m_pPassCode->SetCode(StrFromWide(item.m_strValue.c_str()).c_str());
			m_pPassCode->Show();
			m_bPassCodeOpen = TRUE;
			break;
		case ITEM_SAVE:
			Save();
			break;
		case ITEM_RESET:
			RevertChanges();
			break;
		case ITEM_EXIT:
			if (m_bModified)
			{
				// Quirk: the message warns that the changes will be lost,
				// but Yes is CSettingsBase's "save" answer, so they're saved.
				m_bAskSave = TRUE;
				ShowMessage("You have made changes to your settings.\nIf you exit, your changes will be lost.\n\n"
				            "Are you sure you want to continue?",
				            L"Settings", MSGBOX_BUTTONS_YESNO);
			}
			else
			{
				m_bClosed = TRUE;
			}
			break;
		}
		break;

	case INPUT_START:
		// The icon, preview, password and passcode lines: Clear Entry,
		// and (icon and preview) Download Online...
		if (type <= ITEM_TITLE || type >= ITEM_SAVE_HEADER)
			break;
		if (m_pDropDown == NULL)
		{
			m_pDropDown = new CDropDown;
			if (m_pDropDown != NULL)
			{
				m_pDropDown->AddItem(L"Clear Entry");
				m_pDropDown->AddItem(L"Download Online...");
				m_pDropDown->Create(m_pFont, (float)(m_nValueX + 5), (float)(m_nValueY + 5));
			}
		}
		m_pDropDown->SetEnabled(1, type == ITEM_ICON || type == ITEM_PREVIEW);
		m_pDropDown->Show((float)(m_nValueX + 10), (float)(m_nValueY + 5));
		m_bDropDownOpen = TRUE;
		break;

	case INPUT_B:
	case INPUT_BACK:
		AskToSave();
		*action = INPUT_NONE;
		break;

	case INPUT_X:
		// Clears the icon, preview, password or passcode.
		if (!IsSelectedDisabled() && type > ITEM_TITLE && type < ITEM_SAVE_HEADER)
			SetItemValue(&m_items[m_nSelected], L"", TRUE);
		break;
	}
	return S_OK;
}

// 0x0010B6F2
BOOL CMenuItemEditor::InstallDownload()
{
	BOOL ok = FALSE;
	std::string message;
	// Next to the XBE: the file, and a backup of the one it replaces.
	std::string target = GetPathDirectory(m_strAction.c_str());
	StrAddBackslash(target);
	std::string backup = target;
	while (g_bItemInstalling)
		Sleep(100);
	g_bItemInstalling = TRUE;

	if (m_nDownloadStep == DOWNLOAD_ICON)
	{
		backup.append("icon.bak");
		target.append("icon.png");
		CopyFileA(target.c_str(), backup.c_str(), TRUE);
		if (MoveFileExA(m_pSession->GetCachePath(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
		{
			message = "Game Icon successfully installed.";
			ok = TRUE;
			m_pItem->m_strIcon = target;
			m_items[m_nSelected].m_strValue.clear();
			SetItemValue(&m_items[m_nSelected], ToWide(target).c_str(), TRUE);
		}
		else
		{
			MoveFileExA(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
			StrFormat(message, "Unable to install file. Error code: %d", GetLastError());
		}
	}
	else
	{
		// The preview, as described by the info file: Video 2 is an .xmv
		// (else .wmv); VC 0 a plain file, 1 a .rar, else a .zip.
		m_info.m_pCurrent = &m_info.m_root;
		CXmlElement *xbg = m_info.FindElement(&m_info.m_root, "XBG");
		if (xbg == NULL)
			goto done;
		long video = atol(xbg->GetAttribute("Video").c_str());
		long packing = atol(xbg->GetAttribute("VC").c_str());
		// Stop the preview that's playing.
		if (m_pSink != NULL)
			m_pSink->OnPreview(NULL);
		if (packing == 0)
		{
			target.append(video == 2 ? "preview.xmv" : "preview.wmv");
			backup.append("preview.bak");
			CopyFileA(target.c_str(), backup.c_str(), FALSE);
			DeleteFileA(target.c_str());
			if (!MoveFileExA(m_pSession->GetCachePath(), target.c_str(),
			                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
			{
				MoveFileExA(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
				StrFormat(message, "Unable to install file. Error code: %d", GetLastError());
				goto done;
			}
			message = "Game Preview successfully installed.";
			ok = TRUE;
			m_items[m_nSelected].m_strValue.clear();
			SetItemValue(&m_items[m_nSelected], ToWide(target).c_str(), TRUE);
		}
		else
		{
			// Unpacked into "ziptmp" next to the cache file.
			std::string folder = GetPathDirectory(m_pSession->GetCachePath());
			StrAddBackslash(folder);
			std::string archive = folder;
			folder.append("ziptmp\\");
			archive.append(packing == 1 ? "preview.rar" : "preview.zip");
			if (CopyFileA(m_pSession->GetCachePath(), archive.c_str(), FALSE))
			{
				if (packing == 1)
					UnRarFile(archive.c_str(), folder.c_str());
				else
					UnzipFile(archive.c_str(), folder.c_str());
				Sleep(40);
				while (g_bFileOperationQueued)
					Sleep(500);
				if (!g_bOperationFailed)
				{
					std::vector<SFileListItem> files;
					CollectFiles(folder.c_str(), &files);
					for (std::vector<SFileListItem>::iterator it = files.begin(); it != files.end(); ++it)
					{
						if (it->isFolder)
							continue;
						// Quirk: meant to pick the .xmv or .wmv, but the test
						// takes the first file whatever its type.
						std::string name = it->path;
						BOOL isXmv = StrCmpNoCase(StrRight(name, 4), ".xmv") == 0;
						BOOL pick = !isXmv || StrCmpNoCase(StrRight(name, 4), ".wmv") != 0;
						if (!pick)
							continue;
						target.append("preview");
						target.append(StrRight(name, 4));
						backup.append("preview.bak");
						CopyFileA(target.c_str(), backup.c_str(), FALSE);
						DeleteFileA(target.c_str());
						if (!MoveFileExA(name.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
						{
							MoveFileExA(backup.c_str(), target.c_str(),
							            MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
							StrFormat(message, "Unable to install file. Error code: %d", GetLastError());
						}
						else
						{
							message = "Game Preview successfully installed.";
							ok = TRUE;
						}
						break;
					}
					files.clear();
					if (ok)
					{
						m_items[m_nSelected].m_strValue.clear();
						SetItemValue(&m_items[m_nSelected], ToWide(target).c_str(), TRUE);
						DeleteFolder(folder.c_str());
						Sleep(40);
						while (g_bFileOperationQueued)
							Sleep(500);
					}
				}
				else
				{
					message = "An error occured while during file extraction.\nUnable to install file.";
				}
			}
			if (!ok)
				goto done;
		}
		// Play the new preview.
		if (m_pSink != NULL && FileExists(target.c_str()))
			m_pSink->OnPreview(target.c_str());
	}

done:
	if (message.size() != 0)
		ShowMessage(message.c_str(), ok ? L"Success" : L"Error", MSGBOX_BUTTONS_OK);
	g_bItemInstalling = FALSE;
	return ok;
}

// 0x0010BE83
unsigned __stdcall CMenuItemEditor::InstallThread(void *param)
{
	if (param == NULL)
		return 2;
	CMenuItemEditor *editor = (CMenuItemEditor *)param;
	BOOL ok = editor->InstallDownload();
	editor->m_bDownloading = FALSE;
	return !ok;
}

// 0x0010BEA5
HRESULT CMenuItemEditor::Render()
{
	if (m_bClosed)
		return S_OK;
	// The page, as CSettingsBase draws it (without the page icon).
	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	DimScreen(0.3f);
	DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	             1.0f, 1);
	DrawRect(m_fX + 10.0f, LineHeight(m_pFont) + m_fY + 10.0f, m_fWidth - 20.0f,
	         m_fHeight - 20.0f - LineHeight(m_pFont), g_colorScheme.DIALOG_INNER_BORDERCOLOR,
	         g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f, 1);
	float captionHeight = LineHeight(m_pFont) + 10.0f;
	DrawGradientRect(m_fX + 10.0f, m_fY + 2.0f, m_fWidth - 20.0f, captionHeight - 4.0f,
	                 g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE, 0);
	m_pFont->DrawText(m_fX + 12.0f, captionHeight * 0.5f + m_fY + 2.0f, g_colorScheme.CAPTION_TEXTCOLOR, m_szTitle,
	                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 30.0f);

	if (m_fLabelWidth == 0.0f)
	{
		for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end(); ++it)
		{
			float width = m_pFont->GetTextWidth(it->m_strLabel.c_str());
			if (width > m_fLabelWidth)
				m_fLabelWidth = width;
		}
		m_fLabelWidth += 20.0f;
	}
	float x = m_fX + 12.0f;
	float y = LineHeight(m_pFont) * 0.5f + captionHeight + m_fY + 4.0f;
	float rowWidth = m_fWidth - 24.0f;
	if (m_fLabelWidth > rowWidth * 0.5f)
		m_fLabelWidth = rowWidth * 0.5f - 2.0f;
	float valueWidth = rowWidth - m_fLabelWidth;

	m_pFont->Begin();
	if (m_nFirstVisible > (int)m_items.size())
		m_nFirstVisible = 0;
	float bottom = m_fHeight + m_fY - 20.0f - LineHeight(m_pFont);
	float lineHeight = LineHeight(m_pFont);
	int index = m_nFirstVisible;
	for (; index < (int)m_items.size(); index++)
	{
		const CSettingItem &item = m_items[index];
		if (!item.m_bVisible)
			continue;
		if (index == m_nSelected)
		{
			m_pFont->End();
			DrawGradientRect(x - 1.0f, y - LineHeight(m_pFont) * 0.5f, rowWidth, LineHeight(m_pFont),
			                 g_colorScheme.OPTION_ACTIVEBACKCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE,
			                 g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			m_pFont->Begin();
		}

		if (item.m_bHeader)
		{
			m_pFont->DrawText(m_fWidth * 0.5f + m_fX, y, g_colorScheme.CAPTION_TEXTCOLOR | 0xFF000000,
			                  item.m_strLabel.c_str(), XBFONT_CENTER_X | XBFONT_CENTER_Y, rowWidth);
		}
		else if (item.m_bEnabled)
		{
			BOOL selected = index == m_nSelected;
			DWORD color = selected ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.OPTION_TEXTCOLOR;
			if (selected)
			{
				// Where the START pop-up opens.
				m_nValueX = (UINT)(int)(x + m_fLabelWidth + 5.0f);
				m_nValueY = (UINT)(int)(y - lineHeight * 0.5f);
			}
			m_pFont->DrawText(x, y, color, item.m_strLabel.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fLabelWidth);
			if (index == ITEM_PASSWORD || index == ITEM_PASSCODE)
			{
				// Masked. Quirk: the value is copied into a 16-character
				// buffer first, unchecked.
				if (item.m_strValue.size() != 0)
				{
					wchar_t masked[16];
					memset(masked, 0, sizeof(masked));
					size_t count = item.m_strValue.size() < 15 ? item.m_strValue.size() : 15;
					for (size_t i = 0; i < count; i++)
						masked[i] = L'*';
					m_pFont->DrawText(m_fLabelWidth + x, y, color, masked, XBFONT_CENTER_Y, valueWidth);
				}
			}
			else
			{
				// The selected line's value isn't cut off.
				m_pFont->DrawText(m_fLabelWidth + x, y, color, item.m_strValue.c_str(),
				                  selected ? XBFONT_CENTER_Y : XBFONT_CENTER_Y | XBFONT_TRUNCATED, valueWidth);
			}
		}
		else
		{
			DWORD color = g_colorScheme.DIALOG_DISABLEDTEXT | 0xFFC0C0C0;
			m_pFont->DrawText(x, y, color, item.m_strLabel.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fLabelWidth);
			m_pFont->DrawText(m_fLabelWidth + x, y, color, item.m_strValue.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED,
			                  valueWidth);
		}

		y = lineHeight + 2.0f + y;
		if (y > bottom)
		{
			m_nLastVisible = index;
			m_nVisibleCount = index - m_nFirstVisible + 1;
			break;
		}
	}
	if (m_nLastVisible == 0)
		m_nLastVisible = index;
	m_pFont->End();

	// The open pop-up, if any.
	if (m_bMessageBoxOpen)
	{
		m_pMessageBox->Render();
		return S_OK;
	}
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.Render();
		return S_OK;
	}
	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->Render();
		return S_OK;
	}
	if (m_bPassCodeOpen)
	{
		m_pPassCode->Render();
		return S_OK;
	}
	if (m_bDropDownOpen)
	{
		m_pDropDown->Render();
		return S_OK;
	}
	if (!m_bDownloading)
		return S_OK;

	std::string text;
	if (m_nDownloadStep != DOWNLOAD_INFO)
	{
		// The icon or preview itself, then its installation.
		if (g_hItemInstallThread != INVALID_HANDLE_VALUE)
		{
			DrawBusyBox(m_pFont, L"Installing. Please Wait");
			if (WaitForSingleObject(g_hItemInstallThread, 5) != WAIT_OBJECT_0)
				return S_OK;
			CloseHandle(g_hItemInstallThread);
			g_hItemInstallThread = INVALID_HANDLE_VALUE;
			delete m_pSession;
			m_pSession = NULL;
			m_bDownloading = FALSE;
			m_nDownloadStep = DOWNLOAD_IDLE;
			return S_OK;
		}
		if (!m_pSession->IsDone())
		{
			CInternetSession::DrawProgress(m_pSession, m_pLogo, m_pFont);
			m_unknown24C = 0;
			return S_OK;
		}
		int status = m_pSession->m_nStatus;
		if (status == 200)
		{
			if (!m_pSession->m_bSavedToFile)
			{
				ShowMessage("Downloaded file is not in the correct format.\nYou may have reached your quota for "
				            "the day.\nPlease try again later.",
				            L"Download Error", MSGBOX_BUTTONS_OK);
			}
			else
			{
				// Installed on a thread; without one, right here.
				g_hItemInstallThread = (HANDLE)_beginthreadex(NULL, 0, InstallThread, this, 0, NULL);
				if (g_hItemInstallThread != NULL && g_hItemInstallThread != INVALID_HANDLE_VALUE)
					return S_OK;
				if (InstallDownload())
				{
					ShowMessage("File successfully installed.", L"Success", MSGBOX_BUTTONS_OK);
				}
				else
				{
					StrFormat(text, "Unable to install file. Error code: %d", GetLastError());
					ShowMessage(text.c_str(), L"Error", MSGBOX_BUTTONS_OK);
				}
			}
		}
		else
		{
			StrFormat(text, "Unable to retrieve file: HTTP STATUS %d", status);
			ShowMessage(text.c_str(), L"HTTP Error", MSGBOX_BUTTONS_OK);
		}
		delete m_pSession;
		m_pSession = NULL;
		m_bDownloading = FALSE;
		m_nDownloadStep = DOWNLOAD_IDLE;
		return S_OK;
	}

	// The info file.
	if (!m_pSession->IsDone())
	{
		wchar_t state[100];
		memset(state, 0, sizeof(state));
		switch (m_pSession->m_nState)
		{
		case 1:
			wcscpy(state, GetString(STR_HTTP_OPENING));
			break;
		case 2:
			wcscpy(state, GetString(STR_HTTP_CONNECTING));
			break;
		case 3:
			wcscpy(state, GetString(STR_HTTP_CONNECTED));
			break;
		case 4:
			swprintf(state, 100, L"Retrieving %d", m_pSession->GetPercent());
			wcscat(state, L"%");
			break;
		case 5:
			wcscpy(state, GetString(STR_HTTP_FINISHED));
			break;
		}
		DrawBusyBox(m_pFont, state);
		return S_OK;
	}
	std::string url;
	if (m_pSession->m_nStatus != 200)
	{
		// Nothing for this game yet: ask the site for it.
		StrFormat(url, "http://www.xbox-skins.net/games/request.php?t=%s&h=%s&f=", m_strTitle.c_str(),
		          m_strTitleId.c_str());
		if (m_nSelected == ITEM_ICON)
		{
			url.append("thumb");
			text = "Game Icon is not available.\nA request for this title is being sent to the server.\nPlease "
			       "check back some other time for the availability of the icon.";
		}
		else
		{
			url.append("wmv");
			text = "Game Preview is not available.\nA request for this title is being sent to the server.\n"
			       "Please check back some other time for the availability of the video.";
		}
		ShowMessage(text.c_str(), L"Info", MSGBOX_BUTTONS_OK);
		m_pSession->Get(url.c_str(), TRUE);
		m_bDownloading = FALSE;
		m_nDownloadStep = DOWNLOAD_IDLE;
		return S_OK;
	}

	m_info.Clear();
	BOOL loaded = m_info.LoadFromString(m_pSession->GetData());
	if (!loaded)
		loaded = m_info.Load(m_pSession->GetCachePath());
	BOOL failed = TRUE;
	if (loaded)
	{
		m_info.m_pCurrent = &m_info.m_root;
		CXmlElement *xbg = m_info.FindElement(&m_info.m_root, "XBG");
		if (xbg != NULL)
		{
			if (m_nSelected == ITEM_ICON)
			{
				// <XBG Thumb="1">: thumbs/<id>.png
				if (IsTrueString(xbg->GetAttribute("Thumb").c_str()))
				{
					m_nDownloadStep = DOWNLOAD_ICON;
					StrFormat(url, "http://www.xbox-skins.net/games/thumbs/%s.png", m_strTitleId.c_str());
					m_pSession->Get(url.c_str(), FALSE);
					failed = FALSE;
				}
			}
			else if (m_nSelected == ITEM_PREVIEW)
			{
				// <XBG Video="n" VidID="...">
				if (atol(xbg->GetAttribute("Video").c_str()) != 0 && xbg->GetAttribute("VidID").size() != 0)
				{
					StrFormat(url, "http://www.xbox-skins.net/games/sendvid.php?sid=%s",
					          xbg->GetAttribute("VidID").c_str());
					m_pSession->Get(url.c_str(), FALSE);
					failed = FALSE;
					m_nDownloadStep = DOWNLOAD_PREVIEW;
				}
			}
		}
	}
	if (failed)
	{
		StrFormat(text, "Can't find downloadable content for '%s'", m_strTitle.c_str());
		ShowMessage(text.c_str(), L"HTTP Error", MSGBOX_BUTTONS_OK);
		m_bDownloading = FALSE;
		m_nDownloadStep = DOWNLOAD_IDLE;
	}
	return S_OK;
}
