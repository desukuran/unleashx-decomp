#include "Menu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../app/Screens.h"
#include "../app/UnleashX.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../settings/ConfigLoader.h"
#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/D3D.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../system/FileOps.h"
#include "../settings/AppSettings.h"
#include "../system/Storage.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "MenuDialogs.h"
#include "MenuItem.h"
#include "MenuScan.h"

std::vector<SUnlockedItem> g_unlockedItems;   // 0x00277958
CXmlDocument *g_pItemCache;                   // 0x002761F4

// The default line height when the skin doesn't set one.
static const int kDefaultItemHeight = 34;

// 0x00110B3B
CMenu::CMenu() : m_md5(NULL, -1)
{
	m_fAreaX = 50.0f;
	m_fAreaY = 50.0f;
	m_fAreaWidth = 540.0f;
	m_fAreaHeight = 380.0f;
	m_fUnknownF20 = 1.0f;
	m_fUnknownF24 = 1.0f;
	m_pSink = NULL;
	m_nItemSpacing = 2;
	m_nBorderWidth = 0;
	m_pFont = NULL;
	m_pSelectedFont = NULL;
	m_pMenuDoc = NULL;
	m_bShowArrows = TRUE;
	m_bMessageShown = FALSE;
	m_nSelected = 0;
	m_nFirstVisible = 0;
	m_nLastVisible = 0;
	m_nItemCount = 0;
	m_pIconTexture = NULL;
	m_bReady = FALSE;
	m_unknown064 = 0;
	m_bLoadedInline = FALSE;
	m_pBootSurface = NULL;
	m_pBootTexture = NULL;
	m_nDepth = 0;
	memset(m_selectedStack, 0, sizeof(m_selectedStack));
	memset(m_firstVisibleStack, 0, sizeof(m_firstVisibleStack));
	m_pTexture70 = NULL;
	m_pTexture74 = NULL;
	m_nMaxVisibleItems = 0;
	m_bWrap = false;
	m_bFixed = false;
	m_nFixAt = 0;
	m_dwTextAlign = 12;
	m_bWaitingForKeyboard = FALSE;
	m_bMessageShown = FALSE;
	m_pMessageBox = NULL;
	m_pSoundSelect = NULL;
	m_pSoundBack = NULL;
	m_pSoundUp = NULL;
	m_pSoundDown = NULL;
	m_nScrollDirection = 0;
	m_fScrollSpeed = 0.0f;
	m_fScrollRemaining = 0.0f;
	m_dwScrollStart = 0;
	m_bSmoothScroll = TRUE;
	m_bBatchRunning = FALSE;
	m_pBatchList = NULL;
	m_pBatchSubList = NULL;
	m_bBatchStarted = FALSE;
	m_nItemHeight = 0;
	m_pCurrentList = NULL;
	m_bHorizontal = FALSE;
	m_nSkinVisibleItems = 0;
	m_pItemPositions = NULL;
	m_unknown164 = 0;
	m_pContextMenu = NULL;
	m_bContextMenuShown = FALSE;
	m_fContextMenuX = 0.0f;
	m_fContextMenuY = 0.0f;
	m_bShowPreviews = FALSE;
	m_unknownF10 = 1;
	m_bNoIcons = FALSE;
	m_pPassCode = NULL;
	m_bWaitingForPassCode = FALSE;
	m_dwListOpenTime = 0;
	m_bDeletePending = FALSE;
	m_bDeleting = FALSE;
	m_pRecentMenu = NULL;
	m_bRecentMenuShown = FALSE;
	memset(m_recent, 0, sizeof(m_recent));
	m_bScrollText = FALSE;
	m_dwIconLoadTime = GetTickCount() + 2000;
	m_fUnknownF40 = 0.0f;
	m_pItemEditor = NULL;
	m_bItemEditorShown = FALSE;
	m_unknownDFC = 0;
	m_unknownE00 = 0;

	m_pCommand = new CXmlElement;
	if (m_pCommand != NULL)
		m_pCommand->m_strName = "Command";
	m_previewTimer.Stop();
	m_bDirty = TRUE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x0010CF0B
CMenu::~CMenu()
{
	m_pFont = NULL;
	m_pSelectedFont = NULL;
	m_pMenuDoc = NULL;
	delete m_pCommand;
	m_pCommand = NULL;
	for (size_t i = 0; i < m_items.size(); i++)
		delete m_items[i];
	FreeIconCache();
	if (m_pTexture70 != NULL)
	{
		m_pTexture70->Release();
		m_pTexture70 = NULL;
	}
	if (m_pTexture74 != NULL)
	{
		m_pTexture74->Release();
		m_pTexture74 = NULL;
	}
	m_pSoundSelect = NULL;
	m_pSoundBack = NULL;
	m_pSoundUp = NULL;
	m_pSoundDown = NULL;
	delete m_pContextMenu;
	m_pContextMenu = NULL;
	delete m_pItemEditor;
	m_pItemEditor = NULL;
	delete m_pPassCode;
	m_pPassCode = NULL;
	delete g_pItemCache;
	g_pItemCache = NULL;
	delete m_pRecentMenu;
	m_pRecentMenu = NULL;
}

// 0x0010CEB2
void CMenu::PlaySound(CSound *sound)
{
	if (sound != NULL && sound->m_pBuffer != NULL)
	{
		sound->m_pBuffer->SetCurrentPosition(0);
		sound->Play(0);
	}
}

// Sounds are played only when the menu has them (m_bSounds).
#define MENU_SOUND(sound) \
	do \
	{ \
		if (m_bSounds) \
			PlaySound(sound); \
	} while (0)

// 0x00022647
// The volume setting is 0..100; DirectSound wants hundredths of dB.
void CMenu::UpdateSoundVolume()
{
	if (!m_bSounds)
		return;
	LONG volume = (m_pSettings->m_audio.m_nSoundVolume - 100) * 100;
	if (volume > 0)
		volume = 0;
	else if (volume < -6400)
		volume = -6400;
	CSound *sounds[4] = {m_pSoundSelect, m_pSoundBack, m_pSoundUp, m_pSoundDown};
	for (int i = 0; i < 4; i++)
	{
		if (sounds[i]->m_pBuffer != NULL)
			sounds[i]->m_pBuffer->SetVolume(volume);
	}
}

// 0x00111F27
HRESULT CMenu::Create(CXmlDocument *doc, CXBFont *font, CXBFont *selectedFont, CMenuEventSink *sink,
                      BOOL noIcons, int borderWidth, IDirect3DTexture8 *texture70,
                      IDirect3DTexture8 *texture74)
{
	if (doc == NULL || font == NULL)
		return E_FAIL;

	m_pSink = sink;
	m_nBorderWidth = borderWidth;
	m_pMenuDoc = doc;
	m_pFont = font;
	m_pSelectedFont = selectedFont != NULL ? selectedFont : font;
	m_fUnknownF20 = 1.0f;
	m_fUnknownF24 = 1.0f;
	m_bNoIcons = noIcons;
	if (!noIcons)
		m_nItemHeight = kDefaultItemHeight;
	if (texture70 != NULL)
	{
		m_pTexture70 = texture70;
		texture70->AddRef();
	}
	if (texture74 != NULL)
	{
		texture74->AddRef();
		m_pTexture74 = texture74;
	}

	// Lines are at least as tall as either font.
	if ((float)m_nItemHeight < GetFontHeight(m_pFont))
		m_nItemHeight = (int)GetFontHeight(m_pFont);
	if ((float)m_nItemHeight < GetFontHeight(m_pSelectedFont))
		m_nItemHeight = (int)GetFontHeight(m_pSelectedFont);

	m_pSoundSelect = g_soundEffects.Find("Select");
	if (m_pSoundSelect != NULL)
	{
		m_pSoundBack = g_soundEffects.Find("Back");
		m_pSoundUp = g_soundEffects.Find("Menu Up");
		m_pSoundDown = g_soundEffects.Find("Menu Down");
		m_bSounds = m_pSoundSelect != NULL && m_pSoundSelect->m_pBuffer != NULL;
		UpdateSoundVolume();
	}
	LoadList(NULL);
	return S_OK;
}

// "off", "no" or "false" (any case) turns sorting off.
static BOOL IsSortOff(const std::string &value)
{
	return StrCmpNoCase(value, "off") == 0 || StrCmpNoCase(value, "no") == 0 || StrCmpNoCase(value, "false") == 0;
}

// Resolves a relative path attribute in place.
static void ResolvePathAttribute(CXmlElement *item, const char *name, const std::string &value)
{
	if (value.size() != 0 && StrFind(value, ":") == -1)
	{
		std::string path;
		ResolvePath(value.c_str(), path);
		item->SetAttribute(name, path.c_str());
	}
}

// 0x00110F47
// Items are sorted by text unless the list has Sort="off" (or a <Sort>off
// </Sort> child, which affects the items after it). DelayLoad="true" lists
// skip the XBE scan of their items the first time (the attribute is then
// set to "False").
void CMenu::LoadList(CXmlElement *list)
{
	m_bReady = FALSE;
	if (m_items.size() != 0)
	{
		// Quirk: the items aren't deleted here (OpenList deletes them).
		m_items.clear();
	}

	CXmlElement *root = &m_pMenuDoc->m_root;
	m_pMenuDoc->m_pCurrent = root;
	BOOL noSort;
	if (list == NULL || list == root)
	{
		list = root;
		m_nDepth = 0;
		noSort = TRUE;
	}
	else
	{
		noSort = IsSortOff(list->GetAttribute("Sort"));
	}
	m_pCurrentList = list;

	if (list != NULL)
	{
		BOOL delayLoad = FALSE;
		std::string value = list->GetAttribute("DelayLoad");
		if (value.size() != 0)
			delayLoad = StrCmpNoCase(value, "True") == 0 || StrCmpNoCase(value, "On") == 0 ||
			            StrCmpNoCase(value, "1") == 0;

		int count = 0;
		CXmlElement *child = list->GetFirstChild();
		while (child != NULL)
		{
			if (StrCmpNoCase(child->m_strName, "sort") == 0)
			{
				noSort = IsSortOff(child->m_strValue);
				child = list->GetNextChild();
				continue;
			}

			if (StrCmpNoCase(child->m_strName, "Item") == 0)
			{
				// An item with an Icon/AltIcon attribute keeps it (made
				// absolute); one without gets the XBE scan.
				BOOL hasIcon = FALSE;
				if (!delayLoad)
				{
					for (size_t i = 0; i < child->m_attributes.size(); i++)
					{
						CXmlAttribute *attr = child->m_attributes[i];
						if (attr == NULL)
							continue;
						if (StrCmpNoCase(attr->m_strName, "Icon") == 0)
						{
							ResolvePathAttribute(child, "Icon", attr->m_strValue);
							hasIcon = TRUE;
						}
						else if (StrCmpNoCase(attr->m_strName, "AltIcon") == 0)
						{
							ResolvePathAttribute(child, "AltIcon", attr->m_strValue);
							hasIcon = TRUE;
						}
						else if (StrCmpNoCase(attr->m_strName, "Preview") == 0)
						{
							ResolvePathAttribute(child, "Preview", attr->m_strValue);
						}
					}
				}
				if (!hasIcon)
					FindItemIcon(child);
			}
			AddItem(child, noSort);
			child = list->GetNextChild();
			// Give the renderer a chance every 10 items.
			if (++count == 10)
			{
				count = 0;
				Sleep(1);
			}
		}
		if (delayLoad)
			list->SetAttribute("DelayLoad", "False");
	}

	// Line height and text scale from the font.
	if (!m_bNoIcons)
	{
		m_nItemHeight = kDefaultItemHeight;
		if (GetFontHeight(m_pFont) > (float)m_nItemHeight)
			m_nItemHeight = (int)GetFontHeight(m_pFont);
	}
	else
	{
		m_nItemHeight = (int)GetFontHeight(m_pFont);
	}
	unsigned fontHeight = (unsigned)GetFontHeight(m_pFont);
	if (fontHeight < (unsigned)m_nItemHeight / 2)
		m_fTextScale = ((float)m_nItemHeight * 0.5f - (float)fontHeight) / (float)fontHeight + 1.0f;
	else
		m_fTextScale = 1.0f;

	// The visible window.
	unsigned count = (unsigned)m_items.size();
	m_nItemCount = count;
	m_nVisibleItems = (int)(m_fAreaHeight / (float)(m_nItemHeight + m_nItemSpacing * 2));
	if (m_nSkinVisibleItems != 0)
		m_nVisibleItems = m_nSkinVisibleItems;
	if (count < (unsigned)m_nVisibleItems)
		m_nVisibleItems = count;
	if (m_nMaxVisibleItems != 0 && (unsigned)m_nMaxVisibleItems < (unsigned)m_nVisibleItems)
		m_nVisibleItems = m_nMaxVisibleItems;
	if ((unsigned)m_nVisibleItems < (unsigned)m_nFirstVisible)
		m_nFirstVisible = 0;
	m_nLastVisible = m_nVisibleItems - 1 + m_nFirstVisible;
	if (count < (unsigned)m_nSelected)
	{
		m_nSelected = 0;
	}
	else if ((unsigned)m_nLastVisible < (unsigned)m_nSelected)
	{
		m_nFirstVisible = m_nSelected - m_nLastVisible - 1 + m_nVisibleItems;
		m_nLastVisible = m_nVisibleItems - 1 + m_nFirstVisible;
	}

	// Load the icons of the first screenful now, the rest later.
	Sleep(100);
	for (unsigned i = 0; i < m_items.size() && (int)i <= m_nVisibleItems; i++)
	{
		CMenuItem *item = m_items[i];
		if (item != NULL && item->m_strIcon.size() != 0)
			item->LoadIcon();
	}
	m_unknown164 = 0;
	Sleep(1);
	m_bReady = TRUE;
	m_bDirty = TRUE;
	m_dwIconLoadTime = GetTickCount() + 2000;
	m_fUnknownF40 = 0.0f;
}

// 0x00110772
// Adds an <Item> or <List>. In sorted lists items are inserted by text
// (case-insensitive); quirk: an item whose text is already there is dropped.
void CMenu::AddItem(CXmlElement *element, BOOL noSort)
{
	// AltIcon if the file exists, else Icon.
	std::string icon = element->GetAttribute("AltIcon");
	if (icon.size() != 0 && StrFind(icon, ":") == -1)
		ResolvePath(icon.c_str(), icon);
	if ((int)icon.size() < 5 || !FileExists(icon.c_str()))
		icon = element->GetAttribute("Icon");

	CMenuItem *item;
	BOOL isList = FALSE;
	if (StrCmpNoCase(element->m_strName, "Item") == 0)
	{
		std::string text = element->m_strValue;
		if (text.size() == 0)
			text = element->GetAttribute("Text");
		item = new CMenuItem(text.c_str(), icon.c_str(), FALSE);
	}
	else if (StrCmpNoCase(element->m_strName, "List") == 0)
	{
		item = new CMenuItem(element->GetAttribute("Text").c_str(), icon.c_str(), TRUE);
		isList = TRUE;
	}
	else
	{
		return;
	}
	if (item == NULL)
		return;

	// Items without an Action are labels (no element).
	CXmlElement *target = NULL;
	if (isList || element->GetAttribute("Action").size() != 0)
		target = element;

	if (noSort)
	{
		if (target != NULL)
			item->m_pElement = target;
		m_items.push_back(item);
		return;
	}

	if (target != NULL)
		item->m_pElement = target;
	std::vector<CMenuItem *>::iterator it = m_items.begin();
	for (; it != m_items.end(); ++it)
	{
		if (*it == NULL)
			continue;
		int cmp = _wcsicmp((*it)->m_pszText, item->m_pszText);
		if (cmp > 0)
			break;
		if (cmp == 0)
		{
			delete item;
			return;
		}
	}
	item->m_pElement = target;
	m_items.insert(it, item);
}

// Tries base\name1, then each replacement in turn; returns the first file
// that exists, or "".
static std::string FindFirstExisting(std::string path, const char *const *steps, int count)
{
	if (FileExists(path.c_str()))
		return path;
	for (int i = 0; i < count; i += 2)
	{
		StrReplaceAll(path, steps[i], steps[i + 1]);
		if (FileExists(path.c_str()))
			return path;
	}
	return "";
}

// 0x001100F6
// Fills in an XBE item's Region, Icon, AltIcon and Preview, once (it then
// gets Scanned="1"):
// * Region from the XBE certificate.
// * Icon: E:\UDATA\<title id>\TitleImage.xbx, extracted from the XBE
//   (with a TitleMeta.xbx naming the game) if missing. When running from
//   DVD it's Z:\Media\<title id>.xbx instead.
// * AltIcon: icon.png, icon.jpg, avalaunch_icon.png, mxm_thumb.jpg,
//   default.png or default.jpg next to the XBE.
// * Preview (if the skin shows previews): preview.wmv, preview.xmv,
//   mxm_ss.wmv or default.wmv next to the XBE.
void CMenu::FindItemIcon(CXmlElement *item)
{
	if (item == NULL)
		return;
	std::string action = item->GetAttribute("Action");
	if ((int)action.size() < 5 || StrCmpNoCase(StrRight(action, 4), ".xbe") != 0 ||
	    item->GetAttribute("Scanned").size() != 0)
		return;

	std::string icon = item->GetAttribute("Icon");
	BOOL needIcon = (int)icon.size() < 5 ||
	                (StrCmpNoCase(StrRight(icon, 4), ".xbx") == 0 && !FileExists(icon.c_str()));
	if (needIcon)
	{
		EnterFileLock();
		SXbeInfo info;
		if (ReadXbeInfo(action.c_str(), &info))
		{
			StrFormat(icon, "%d", (int)info.gameRegion);
			item->SetAttribute("Region", icon.c_str());
			StrFormat(icon, "E:\\UDATA\\%08x\\TitleImage.xbx", info.titleId);
			if (!FileExists(icon.c_str()))
			{
				if (m_pSettings->m_bRunFromDVD)
					StrFormat(icon, "Z:\\Media\\%08x.xbx", info.titleId);
				item->SetAttribute("Icon", icon.c_str());
				if (!ExtractXbeTitleImage(action.c_str(), icon.c_str()))
				{
					// Remove the folder again if it was created for nothing.
					StrFormat(icon, "E:\\UDATA\\%08x", info.titleId);
					RemoveDirectoryA(icon.c_str());
				}
				else if (!m_pSettings->m_bRunFromDVD)
				{
					std::string meta;
					StrFormat(meta, "E:\\UDATA\\%08x\\TitleMeta.xbx", info.titleId);
					StrFormat(icon, "TitleName=%s", info.titleName.c_str());
					WriteTextFile(meta.c_str(), icon.c_str(), FALSE);
				}
			}
			else
			{
				item->SetAttribute("Icon", icon.c_str());
			}
		}
		LeaveFileLock();
	}
	else if (icon.size() != 0 && StrFind(icon, ":") != 0)
	{
		ResolvePath(icon.c_str(), icon);
		item->SetAttribute("Icon", icon.c_str());
	}

	std::string altIcon = item->GetAttribute("AltIcon");
	if (altIcon.size() == 0)
	{
		static const char *const kSteps[] = {
			".png", ".jpg",                            // icon.jpg
			"icon.jpg", "avalaunch_icon.png",
			"avalaunch_icon.png", "mxm_thumb.jpg",
			"mxm_thumb.jpg", "default.png",
			".png", ".jpg",                            // default.jpg
		};
		std::string path = GetPathDirectory(action.c_str()) + "\\icon.png";
		path = FindFirstExisting(path, kSteps, 10);
		if (path.size() != 0)
			item->SetAttribute("AltIcon", path.c_str());
	}
	else if (StrFind(altIcon, ":") != 0)
	{
		ResolvePath(altIcon.c_str(), altIcon);
		item->SetAttribute("AltIcon", altIcon.c_str());
	}

	if (m_bShowPreviews)
	{
		std::string preview = item->GetAttribute("Preview");
		if (preview.size() == 0)
		{
			static const char *const kSteps[] = {
				".wmv", ".xmv",
				"preview.xmv", "mxm_ss.wmv",
				"mxm_ss.wmv", "default.wmv",
			};
			std::string path = GetPathDirectory(action.c_str()) + "\\preview.wmv";
			path = FindFirstExisting(path, kSteps, 6);
			if (path.size() != 0)
				item->SetAttribute("Preview", path.c_str());
		}
		else if (StrFind(preview, ":") != 0)
		{
			ResolvePath(preview.c_str(), preview);
			item->SetAttribute("Preview", preview.c_str());
		}
	}
	item->SetAttribute("Scanned", "1");
}

// 0x0011159C
static unsigned __stdcall LoadListThread(void *param)
{
	CMenu *menu = (CMenu *)param;
	menu->LoadList(menu->m_pCurrentList);
	return 0;
}

// 0x001115B2
void CMenu::OpenList(CXmlElement *list)
{
	m_pCurrentList = list;
	// Wait for a load in progress.
	while (!m_bReady)
		Sleep(5);
	m_bReady = FALSE;
	if (m_pIconTexture != NULL)
	{
		m_pIconTexture->Release();
		m_pIconTexture = NULL;
	}
	for (size_t i = 0; i < m_items.size(); i++)
	{
		CMenuItem *item = m_items[i];
		if (item == NULL)
			continue;
		if (item->m_pTexture != NULL)
		{
			item->m_pTexture->Release();
			item->m_pTexture = NULL;
		}
		item->m_pTexture = NULL;
		delete item;
	}
	m_items.clear();

	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, LoadListThread, this, CREATE_SUSPENDED, NULL);
	if (thread == NULL)
	{
		LoadList(list);
		m_bLoadedInline = TRUE;
	}
	else
	{
		ResumeThread(thread);
		CloseHandle(thread);
		m_unknown064 = FALSE;
	}
	m_dwListOpenTime = GetTickCount();
}

// 0x00022635
void CMenu::ReloadList()
{
	if (m_pMenuDoc != NULL)
		OpenList(m_pCurrentList);
}

// 0x0010CE86
CMenuItem *CMenu::GetSelectedItem()
{
	if ((size_t)m_nSelected >= m_items.size())
		return NULL;
	return m_items[m_nSelected];
}

// 0x0010FDB8
void CMenu::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pSelectedFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
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
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// 0x0010FF6A
BOOL CMenu::CheckPassword(CXmlElement *item, std::string &password)
{
	if (item == NULL)
		return FALSE;

	BOOL isPasscode = FALSE;
	password = item->GetAttribute("Password");
	if (password.size() == 0)
	{
		password = item->GetAttribute("Passcode");
		isPasscode = TRUE;
		if (password.size() == 0)
			return TRUE;   // not locked
	}

	// Already unlocked this session (with the current password)?
	for (size_t i = 0; i < g_unlockedItems.size(); i++)
	{
		if (g_unlockedItems[i].item == item)
		{
			if (g_unlockedItems[i].password.size() != 0)
			{
				if (password.compare(g_unlockedItems[i].password) == 0)
					return TRUE;
				break;
			}
			break;
		}
	}

	if (!isPasscode)
	{
		// Masked keyboard input, up to 50 characters.
		g_virtualKeyboard.SetTitle(L"Password");
		g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bPassword = TRUE;
		g_virtualKeyboard.m_nMaxLength = 50;
		m_bWaitingForKeyboard = TRUE;
	}
	else
	{
		if (m_pPassCode == NULL)
		{
			m_pPassCode = new CPassCode;
			if (m_pPassCode != NULL)
			{
				m_pPassCode->SetTitle(L"Passcode");
				m_pPassCode->Init(m_pSelectedFont, NULL);
			}
		}
		if (m_pPassCode != NULL)
		{
			m_pPassCode->SetCode(password.c_str());
			m_pPassCode->Show();
			m_bWaitingForPassCode = TRUE;
		}
	}
	return FALSE;
}

// AskUser/MessageBox/InputBox take the caption from Arg1 or Caption, and
// the text from the element's value or Text (AskUser and InputBox also Arg2).
static std::string GetDialogCaption(CXmlElement *item)
{
	std::string caption = item->GetAttribute("Arg1");
	if (caption.size() == 0)
		caption = item->GetAttribute("Caption");
	return caption;
}

static std::string GetDialogText(CXmlElement *item, BOOL useArg2)
{
	std::string text = item->m_strValue;
	if (text.size() == 0)
	{
		text = item->GetAttribute("Text");
		if (text.size() == 0 && useArg2)
			text = item->GetAttribute("Arg2");
	}
	return text;
}

// 0x0011166B
BOOL CMenu::RunItem(CXmlElement *item, BOOL isList)
{
	if (isList)
	{
		if (m_bBatchRunning)
			return TRUE;

		// <List Batch="true">: run all its items one after the other
		// (FrameMove steps through them).
		std::string batch = item->GetAttribute("Batch");
		if (batch.size() != 0 && IsTrueString(batch.c_str()))
		{
			m_pBatchList = item;
			m_pBatchSubList = NULL;
			m_bBatchRunning = TRUE;
			m_bBatchStarted = FALSE;
			g_bCancelOperation = FALSE;
			g_virtualKeyboard.Open();
			return TRUE;
		}

		// Enter the sub-menu, remembering where we were.
		m_firstVisibleStack[m_nDepth] = m_nFirstVisible;
		m_selectedStack[m_nDepth] = m_nSelected;
		m_nDepth++;
		m_nSelected = 0;
		m_nFirstVisible = 0;
		MENU_SOUND(m_pSoundSelect);
		if (m_pSink->OnItemSelected(item))
			OpenList(item);
		return TRUE;
	}

	wchar_t caption[101];
	memset(caption, 0, sizeof(caption));
	std::string action = item->GetAttribute("Action");
	std::string text;

	if (StrCmpNoCase(action, "AskUser") == 0 || StrCmpNoCase(action, "MessageBox") == 0)
	{
		BOOL askUser = StrCmpNoCase(action, "AskUser") == 0;
		text = GetDialogCaption(item);
		ApplyKeyboardResult(text);
		MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), caption, 100);
		text = GetDialogText(item, askUser);
		ApplyKeyboardResult(text);
		StrReplaceAll(text, "\\n", "\n");
		// AskUser has Yes/No buttons.
		ShowMessage(text.c_str(), caption, askUser ? 4 : 0);
		return TRUE;
	}

	if (StrCmpNoCase(action, "InputBox") == 0)
	{
		// Opens the keyboard; the next item of a batch (or the caller) uses
		// the text as $KBResult$.
		wchar_t title[50];
		text = GetDialogCaption(item);
		MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), title, 50);
		text = GetDialogText(item, TRUE);
		g_virtualKeyboard.SetTitle(title);
		g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bPassword = FALSE;
		g_virtualKeyboard.m_nMaxLength = 64;
		if (text.size() != 0)
		{
			MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), title, 50);
			g_virtualKeyboard.SetText(title);
		}
		m_bWaitingForKeyboard = TRUE;
		return TRUE;
	}

	if (m_pSink == NULL)
		return TRUE;
	BOOL ok = m_pSink->OnItemSelected(item);
	if (ok)
		MENU_SOUND(m_pSoundSelect);

	// Results of Format and Rename are reported here (not inside a batch).
	if (!m_bBatchRunning)
	{
		std::string message;
		if (StrCmpNoCase(action, "Format") == 0)
		{
			StrFormat(message, ok ? "Finished formatting Drive %s" : "Failed to format Drive %s",
			          item->GetAttribute("Arg1").c_str());
			ShowMessage(message.c_str(), ok ? L"Success" : L"ERROR");
		}
		else if (StrCmpNoCase(action, "rename") == 0)
		{
			StrFormat(message, ok ? "%s successfully renamed to %s" : "Failed to rename %s to %s",
			          item->GetAttribute("Arg1").c_str(), item->GetAttribute("Arg2").c_str());
			ShowMessage(message.c_str(), ok ? L"Success" : L"ERROR");
		}
	}
	Sleep(5);
	return ok;
}

// 0x00110DCB
void CMenu::OpenRecentList()
{
	if (m_pRecentMenu == NULL)
	{
		m_pRecentMenu = new COptionMenu;
		if (m_pRecentMenu != NULL)
		{
			m_pRecentMenu->Create(m_pFont, L"Recent List", 0.0f, 0.0f, 0.0f, 0.0f, 10);
			std::string path = m_pSettings->m_strDataPath;
			path += "recent.dat";
			FILE *fp = fopen(path.c_str(), "rb");
			if (fp != NULL)
			{
				fread(m_recent, sizeof(m_recent), 1, fp);
				fclose(fp);
			}

			int count = 0;
			for (int i = 0; i < 10; i++)
			{
				SRecentItem *recent = &m_recent[i];
				if (recent->path[0] == 0)
					continue;
				wchar_t name[0x2A];
				MultiByteToWideChar(CP_ACP, 0, recent->name, (int)strlen(recent->name), name, 0x2A);
				m_pRecentMenu->AddOption(name, 0);
				count++;
			}
			if (count == 0)
			{
				delete m_pRecentMenu;
				m_pRecentMenu = NULL;
				ShowMessage("Recent List is empty.", L"Recent List");
			}
		}
	}
	if (m_pRecentMenu != NULL)
	{
		m_pRecentMenu->Show(0);
		m_bRecentMenuShown = TRUE;
	}
}

// 0x0010FEFC
void CMenu::OpenItemEditor()
{
	if (m_pItemEditor == NULL)
	{
		m_pItemEditor = new CMenuItemEditor;
		if (m_pItemEditor != NULL)
			m_pItemEditor->Init(m_pSelectedFont, 0);
	}
	if (m_pItemEditor != NULL)
	{
		m_pItemEditor->m_pSink = m_pSink;
		m_pItemEditor->Edit(GetSelectedItem());
		m_bItemEditorShown = TRUE;
	}
}

// 0x0010CE1D
BOOL CMenu::CaptureBootImage()
{
	if (m_pBootTexture != NULL)
	{
		m_pBootTexture->Release();
		m_pBootTexture = NULL;
	}
	m_pBootSurface = NULL;
	IDirect3DSurface8 *saved = AvGetSavedDataAddress();
	if (saved != NULL)
	{
		D3DSURFACE_DESC desc;
		saved->GetDesc(&desc);
		g_pd3dDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, 0, &m_pBootTexture);
		m_pBootTexture->GetSurfaceLevel(0, &m_pBootSurface);
		g_pd3dDevice->CopyRects(saved, NULL, 0, m_pBootSurface, NULL);
	}
	return saved != NULL;
}

// 0x00112356 (thread entry) / 0x001120A7
static unsigned __stdcall ScanFoldersThread(void *param)
{
	((CMenu *)param)->ScanFoldersNow();
	return 0;
}

// 0x00112364
void CMenu::ScanFolders()
{
	m_bReady = FALSE;
	ShowMessage("Searching for Xbox Titles.\nPlease Wait...", L"Menu");
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ScanFoldersThread, this, CREATE_SUSPENDED, NULL);
	if (thread == NULL)
	{
		ScanFoldersNow();
		return;
	}
	SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
	ResumeThread(thread);
	CloseHandle(thread);
}

// 0x001120A7
// Rebuilds the game lists: <List Path="..."> folders of the menu are
// scanned for XBEs (cached in d:\items.xml, resolved) and merged into the
// menu; items whose XBE is gone are removed.
void CMenu::ScanFoldersNow()
{
	m_pMenuDoc->m_bModified = false;
	std::string cacheFile;
	ResolvePath("d:\\items.xml", cacheFile);
	if (g_pItemCache == NULL)
	{
		g_pItemCache = new CXmlDocument;
		if (g_pItemCache == NULL)
			return;
	}
	if (!g_pItemCache->Load(cacheFile.c_str()))
	{
		g_pItemCache->m_pCurrent = &g_pItemCache->m_root;
		g_pItemCache->m_root.m_strName = "Cache";
	}
	CXmlElement *cacheRoot = &g_pItemCache->m_root;
	g_pItemCache->m_pCurrent = cacheRoot;
	CXmlElement *menuRoot = &m_pMenuDoc->m_root;
	m_pMenuDoc->m_pCurrent = menuRoot;
	BOOL changed = FALSE;
	CollectMenuFolders(menuRoot, cacheRoot);

	g_pItemCache->m_pCurrent = cacheRoot;
	CXmlElement *folder = g_pItemCache->FindElement(cacheRoot, "Folder");
	while (folder != NULL)
	{
		// Drop cached items whose XBE is gone.
		CXmlElement *item = folder->FindFirstChild("Item");
		while (item != NULL)
		{
			if (FileExists(item->GetAttribute("Action").c_str()))
			{
				item = folder->FindNextChild();
			}
			else
			{
				folder->RemoveChild(item);
				item = folder->FindFirstChild("Item");
				changed = TRUE;
			}
		}

		BOOL delayLoad = StrCmpNoCase(folder->GetAttribute("DelayLoad"), "False") != 0;
		ScanFolder(folder, folder->GetAttribute("Path").c_str(), "default.xbe", 1, delayLoad);
		g_pItemCache->m_pCurrent = cacheRoot;
		folder = g_pItemCache->FindNextElement(cacheRoot);
	}

	m_pMenuDoc->m_pCurrent = menuRoot;
	RemoveMissingXbes(menuRoot);
	m_pMenuDoc->m_pCurrent = menuRoot;
	g_pItemCache->m_pCurrent = cacheRoot;
	MergeFolderItems(cacheRoot, menuRoot);
	if (!m_pSettings->m_bRunFromDVD && (changed || g_pItemCache->m_bModified))
		g_pItemCache->SaveAs(cacheFile.c_str());

	LoadList(m_pCurrentList);
	m_bReady = TRUE;
	m_bMessageShown = FALSE;
}

// 0x00110A15
// After a removal the scan restarts from the top (the first child itself
// isn't looked at again).
void RemoveMissingXbes(CXmlElement *list)
{
	if (list == NULL)
		return;
	std::string action;
	CXmlElement *child = list->GetFirstChild();
	while (child != NULL)
	{
		if (StrCmpNoCase(child->m_strName, "List") == 0)
		{
			RemoveMissingXbes(child);
		}
		else if (StrCmpNoCase(child->m_strName, "Item") == 0)
		{
			action = child->GetAttribute("Action");
			if (StrCmpNoCase(StrRight(action, 4), ".xbe") == 0 && !FileExists(action.c_str()))
			{
				list->RemoveChild(child);
				list->GetFirstChild();
			}
		}
		child = list->GetNextChild();
	}
}

// --- Input --------------------------------------------------------------------

// The password being asked for (CheckPassword fills it).
static std::string s_strPassword;            // 0x0027A5B8
CMenuItem *g_pPendingItem;                   // 0x00278D00 item waiting for its password
CXmlElement *g_pPendingBatchElement;         // 0x00278CFC batch step waiting for its password
static int s_nPreviewIndex = -1;             // 0x0026C700 item whose preview is (being) shown
static BOOL s_bLastRunOk = TRUE;             // 0x0026C704 result of the last RunItem
int g_nMessageResult;                        // 0x00278CF8

// After a batch step was refused (No, Cancel, wrong password): leave the
// current sub-list, or stop the batch at the top.
void CMenu::LeaveBatchSubList()
{
	if (m_pBatchSubList == NULL)
	{
		m_bBatchRunning = FALSE;
		return;
	}
	CXmlElement *parent = m_pBatchSubList->m_pParent;
	if (parent == NULL || parent == m_pBatchList)
		m_pBatchSubList = NULL;
	else
		m_pBatchSubList = parent;
}

// Remembers that the pending item was unlocked with s_strPassword.
static void UnlockPendingItem(CXmlElement *element)
{
	SUnlockedItem unlocked;
	unlocked.item = element;
	unlocked.password = s_strPassword.c_str();
	g_unlockedItems.push_back(unlocked);
	s_strPassword.clear();
}

// Starts the smooth scroll animation (direction -1 up, 1 down). The speed
// follows the frame rate.
void CMenu::StartScroll(int direction)
{
	if (!m_bSmoothScroll)
		return;
	m_nScrollDirection = direction;
	m_dwScrollStart = GetTickCount();
	float fps = g_fFrameRate < 50.0f ? 50.0f : g_fFrameRate;
	if (m_nSkinVisibleItems == 0 || m_pItemPositions == NULL)
		m_fScrollSpeed = 1200.0f / fps;
	else
		m_fScrollSpeed = fps * 1.2f;
	m_fScrollRemaining = m_fScrollSpeed;
}

// Up: the nearest selectable item above (wrapping to the last item).
HRESULT CMenu::SelectPrevious()
{
	if (m_nItemCount == 0)
		return E_FAIL;
	if (m_nSelected == 0)
	{
		if (!m_bWrap)
			return E_FAIL;
		// The search here never matches, so this always lands on the last
		// item, selectable or not.
		m_nSelected = m_nItemCount - 1;
		m_nLastVisible = m_nSelected;
		m_nFirstVisible = m_nSelected - m_nVisibleItems + 1;
	}
	else
	{
		int i = (int)m_items.size() - 1;
		for (; i >= 0; i--)
		{
			if (i < m_nSelected && m_items[i] != NULL && !m_items[i]->IsLabel())
				break;
		}
		if (i < 0)
			return S_OK;
		m_nSelected = i;
		m_bDirty = TRUE;
		if ((unsigned)m_nSelected < (unsigned)m_nFirstVisible)
		{
			do
				m_nFirstVisible--;
			while ((unsigned)m_nSelected < (unsigned)m_nFirstVisible);
			m_nLastVisible = m_nVisibleItems - 1 + m_nFirstVisible;
			StartScroll(-1);
		}
		else if (m_bFixed)
		{
			StartScroll(-1);
		}
	}
	FinishMove(m_pSoundUp);
	return S_OK;
}

// Down: the nearest selectable item below (wrapping to the first one).
HRESULT CMenu::SelectNext()
{
	if (m_nItemCount == 0)
		return E_FAIL;
	if ((unsigned)m_nSelected < (unsigned)m_nItemCount - 1)
	{
		int i = 0;
		for (; i < (int)m_items.size(); i++)
		{
			if (m_nSelected < i && m_items[i] != NULL && !m_items[i]->IsLabel())
				break;
		}
		if (i == (int)m_items.size())
		{
			if ((int)m_items.size() < i)
				return S_OK;
		}
		else
		{
			m_nSelected = i;
			m_bDirty = TRUE;
		}
		if ((unsigned)m_nLastVisible < (unsigned)m_nSelected)
		{
			do
			{
				m_nFirstVisible++;
				m_nLastVisible++;
			} while ((unsigned)m_nLastVisible < (unsigned)m_nSelected);
			StartScroll(1);
		}
		else if (m_bFixed)
		{
			StartScroll(1);
		}
	}
	else
	{
		if (!m_bWrap)
			return E_FAIL;
		m_nSelected = 0;
		for (int i = 0; i < (int)m_items.size(); i++)
		{
			if (m_items[i] != NULL && !m_items[i]->IsLabel())
			{
				m_nSelected = i;
				m_bDirty = TRUE;
				break;
			}
		}
		m_nFirstVisible = m_nSelected;
		m_nLastVisible = m_nVisibleItems - 1 + m_nSelected;
	}
	FinishMove(m_pSoundDown);
	return S_OK;
}

// After moving the selection: sound, and the old icon goes away.
void CMenu::FinishMove(CSound *sound)
{
	MENU_SOUND(sound);
	m_fUnknownF40 = 0.0f;
	if (m_pIconTexture != NULL)
	{
		m_pIconTexture->Release();
		m_pIconTexture = NULL;
	}
}

// The visible item under the pointer, or -1.
int CMenu::ItemUnderPointer()
{
	unsigned i = (unsigned)m_nFirstVisible;
	for (; i < m_items.size(); i++)
	{
		if (!m_items[i]->IsLabel() && PointerInItem(i))
			return (int)i;
		if ((unsigned)m_nLastVisible < i)
			return -1;
	}
	return -1;
}

// IR remote digits run <Settings><Shortcut><IR_n> from Config.xml: an XBE
// path, or an action (without spaces).
static void RunShortcut(CMenu *menu, int action)
{
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
		return;
	CXmlElement *shortcuts = settings->FindFirstChild("Shortcut");
	if (shortcuts == NULL)
		return;
	std::string key;
	StrFormat(key, "IR_%u", (unsigned)(action - INPUT_IR_BACK) % 10);
	CXmlElement *shortcut = shortcuts->FindFirstChild(key.c_str());
	if (shortcut == NULL)
		return;
	std::string target = shortcut->m_strValue;
	if (target.size() == 0)
		return;
	if (StrCmpNoCase(StrRight(target, 4), ".xbe") == 0)
	{
		LaunchXbe(target.c_str(), NULL, -1, NULL);
	}
	else if (StrFind(target, " ") == -1)
	{
		CXmlElement *item = new CXmlElement;
		if (item != NULL)
		{
			item->SetAttribute("Action", target.c_str());
			menu->RunItem(item, FALSE);
			delete item;
		}
	}
}

// START on an item: the context menu.
HRESULT CMenu::OpenContextMenu()
{
	if ((unsigned)m_nSelected >= (unsigned)m_nItemCount)
		return E_FAIL;
	if (m_pContextMenu == NULL)
	{
		m_pContextMenu = new CDropDown;
		if (m_pContextMenu != NULL)
		{
			m_pContextMenu->AddItem(L"Select/Play");
			m_pContextMenu->AddItem(L"Launch in NTSC mode");
			m_pContextMenu->AddItem(L"Launch in PAL mode");
			m_pContextMenu->AddItem(L"Information...");
			m_pContextMenu->AddItem(L"Backup to Harddisk...");
			m_pContextMenu->AddItem(L"Delete Game...");
			m_pContextMenu->AddItem(L"Refresh List...");
			m_pContextMenu->AddItem(L"Recent Titles...");
			m_pContextMenu->AddItem(L"Edit Item...");
			m_pContextMenu->AddItem(L"Open Containing Folder");
			m_pContextMenu->AddItem(L"Open Saves Folder");
			m_pContextMenu->Create(m_pSelectedFont, m_fAreaX + 5.0f, m_fAreaY + 5.0f);
		}
		if (m_pContextMenu == NULL)
			return E_FAIL;
	}
	if (m_items.size() == 0)
		return E_FAIL;
	CMenuItem *item = GetSelectedItem();
	if (item == NULL)
		return E_FAIL;

	BOOL isXbe = StrCmpNoCase(StrRight(item->m_pElement->GetAttribute("Action"), 4), ".xbe") == 0;
	int region = 0;
	if (isXbe)
		region = atol(item->m_pElement->GetAttribute("Region").c_str());
	SPreferences &prefs = m_pSettings->m_prefs;
	BOOL fromDvd = m_pSettings->m_bRunFromDVD;
	BOOL isGame = !item->m_bIsList && !item->IsLabel() && isXbe;

	// NTSC for PAL (or any-region) games and vice versa.
	m_pContextMenu->SetEnabled(1, isXbe && (region == 0 || region == 4 || region == 7));
	m_pContextMenu->SetEnabled(2, isXbe && ((region > 0 && region < 4) || region == 7 || region == 0));
	m_pContextMenu->SetEnabled(4, fromDvd && isGame);
	m_pContextMenu->SetEnabled(5, prefs.m_bAllowUninstall && !fromDvd && isGame);
	m_pContextMenu->SetEnabled(8, prefs.m_bAllowItemEdit && !fromDvd && !item->IsLabel());
	m_pContextMenu->SetEnabled(7, !fromDvd && prefs.m_bAllowRecentList);
	m_pContextMenu->SetEnabled(9, prefs.m_bLinkToGame && isXbe && m_pCommand != NULL);
	m_pContextMenu->SetEnabled(10, prefs.m_bLinkToSave && isXbe && m_pCommand != NULL);
	m_pContextMenu->Show(m_fContextMenuX + 10.0f, m_fContextMenuY + 5.0f);
	m_bContextMenuShown = TRUE;

	// Locked items ask for the password first; the choice runs afterwards.
	if (CheckPassword(item->m_pElement, s_strPassword))
	{
		g_pPendingBatchElement = NULL;
	}
	else
	{
		g_pPendingBatchElement = item->m_pElement;
		g_pPendingItem = GetSelectedItem();
	}
	return E_FAIL;
}

// "Information...": what the item is, and for games the XBE's details and
// the folder size.
void CMenu::ShowItemInfo(CMenuItem *item, CXmlElement *element, const std::string &action, BOOL isXbe)
{
	std::string info;
	std::string line;
	if (!isXbe)
	{
		if (!item->m_bIsList)
			StrFormat(info, "Title: %s\nType: Action Command\nCommand: %s\n", element->m_strValue.c_str(),
			          action.c_str());
		else if (element->GetAttribute("Batch").size() == 0)
			StrFormat(info, "Title: %s\nType: Submenu\n", element->GetAttribute("Text").c_str());
		else
			StrFormat(info, "Title: %s\nType: Batch Process\n", element->GetAttribute("Text").c_str());
	}
	else
	{
		StrFormat(info, "Location: %s\n", action.c_str());
		EnterFileLock();
		SXbeInfo xbe;
		if (!ReadXbeInfo(action.c_str(), &xbe))
		{
			StrFormat(line, "Title: %s\n", element->m_strValue.c_str());
			info += line;
		}
		else
		{
			StrFormat(line, "Title: %s\n", xbe.titleName.c_str());
			info += line;
			StrFormat(line, "ID: %08x (%u)\n", xbe.titleId, xbe.titleId);
			info += line;
			if (xbe.gameRegion == 0)
				line = "Region: 0 (Homebrew)\n";
			else if (xbe.gameRegion == 1)
				line = "Region: 1 (North America -NTSC M)\n";
			else if (xbe.gameRegion == 2)
				line = "Region: 2 (Japan -NTSC J)\n";
			else if (xbe.gameRegion == 4)
				line = "Region: 4 (Europe -PAL)\n";
			else if (xbe.gameRegion < 8)
				line = "Region: Multi\n";
			else
				line = "Region: Multi (Debug)\n";
			info += line;
		}
		if (!m_pSettings->m_bRunFromDVD)
		{
			// Size of the game's folder (16 KB blocks).
			line = GetPathDirectory(action.c_str());
			ResetFileOperationStats();
			CountFolder(line.c_str());
			StrFormat(line, "Total Size: %0.02f MB\t%u Blocks\n", (double)g_ullOperationBytes * (1.0 / 1048576),
			          (unsigned)((double)g_ullOperationBytes * (1.0 / 16384)));
			info += line;
			StrFormat(line, "Contains: %u Files, %u Folders\n", g_nOperationFiles, g_nOperationFolders);
			info += line;
		}
		LeaveFileLock();
	}
	StrFormat(line, "Icon: %s\n", element->GetAttribute("Icon").c_str());
	info += line;
	StrFormat(line, "Alt. Icon: %s\n", element->GetAttribute("AltIcon").c_str());
	info += line;
	StrFormat(line, "Preview: %s\n", element->GetAttribute("Preview").c_str());
	info += line;
	BOOL locked = element->GetAttribute("Password").size() != 0 || element->GetAttribute("Passcode").size() != 0;
	info += locked ? "Protected: Yes\n" : "Protected: No\n";
	ShowMessage(info.c_str(), L"Information");
}

// Runs the context menu choice.
HRESULT CMenu::RunContextMenuChoice(int *action)
{
	CMenuItem *item;
	CXmlElement *element;
	std::string text;
	switch (m_pContextMenu->m_nSelection)
	{
	case 0:   // Select/Play
		*action = INPUT_A;
		return ProcessAction(action);

	case 4:   // Backup to Harddisk (from DVD only)
		if (!m_pSettings->m_bRunFromDVD)
			return S_OK;
		// fall through
	case 1:   // Launch in NTSC mode
	case 2:   // Launch in PAL mode
	case 3:   // Information
	{
		item = GetSelectedItem();
		if (item == NULL || (element = item->m_pElement) == NULL)
			return S_OK;
		std::string actionName = element->GetAttribute("Action");
		BOOL isXbe = StrCmpNoCase(StrRight(actionName, 4), ".xbe") == 0;
		int choice = m_pContextMenu->m_nSelection;
		if (m_pContextMenu->m_bCancelled)
			break;
		if (choice == 1 || choice == 2)
		{
			// Run with the region forced (and AutoVideoMode on for this
			// launch). Quirk: the Region attribute stays changed.
			BOOL autoVideo = m_pSettings->m_prefs.m_bAutoVideoMode;
			m_pSettings->m_prefs.m_bAutoVideoMode = TRUE;
			element->SetAttribute("Region", choice == 1 ? "1" : "4");
			RunItem(element, item->m_bIsList);
			m_pSettings->m_prefs.m_bAutoVideoMode = autoVideo;
		}
		else if (choice == 3)
		{
			ShowItemInfo(item, element, actionName, isXbe);
		}
		else if (isXbe && choice == 4 && !item->m_bIsList)
		{
			CopyDVD(actionName.c_str());
		}
		break;
	}

	case 5:   // Delete Game
	{
		if (!m_pSettings->m_prefs.m_bAllowUninstall)
			return S_OK;
		std::string title;
		item = GetSelectedItem();
		if (item != NULL)
		{
			element = item->m_pElement;
			if (element == NULL)
				return S_OK;
			title = element->m_strValue;
			text = element->GetAttribute("Action");
			int slash;
			if (StrCmpNoCase(StrRight(text, 4), ".xbe") == 0 && (slash = StrReverseFind(text, "\\")) > 3)
			{
				text = StrLeft(text, slash);
				if (DirectoryExists(text.c_str()))
				{
					m_strDeletePath = text;
					m_bDeletePending = TRUE;
				}
			}
		}
		if (m_bDeletePending)
		{
			StrFormat(text,
			          "The following title will be deleted from your hard disk?\nXbox Title:\t%s\nAre you sure you want to continue?",
			          title.c_str());
			ShowMessage(text.c_str(), L"Confirm", MSGBOX_BUTTONS_YESNO);
			if (m_pMessageBox != NULL)
				m_pMessageBox->SetSelection(1);   // TODO: verify (No as the default)
		}
		return S_OK;
	}

	case 6:   // Refresh List
		ScanFolders();
		return S_OK;

	case 7:   // Recent Titles
		if (!m_pSettings->m_prefs.m_bAllowRecentList)
			return S_OK;
		OpenRecentList();
		return S_OK;

	case 8:   // Edit Item
		if (m_pSink != NULL)
			m_pSink->OnPreview(NULL);
		OpenItemEditor();
		return S_OK;

	case 9:   // Open Containing Folder
	case 10:  // Open Saves Folder (E:\UDATA\<title id>)
	{
		item = GetSelectedItem();
		BOOL allowed = m_pContextMenu->m_nSelection == 9 ? m_pSettings->m_prefs.m_bLinkToGame
		                                                  : m_pSettings->m_prefs.m_bLinkToSave;
		if (!allowed || m_pCommand == NULL || item == NULL || item->m_pElement == NULL)
			return S_OK;
		text = item->m_pElement->GetAttribute("Action");
		if (StrCmpNoCase(StrRight(text, 4), ".xbe") != 0)
			break;
		if (m_pContextMenu->m_nSelection == 9)
		{
			text = GetPathDirectory(text.c_str());
			if (DirectoryExists(text.c_str()))
			{
				m_pCommand->SetAttribute("Action", "filemanager");
				m_pCommand->SetAttribute("Arg1", text.c_str());
				RunItem(m_pCommand, FALSE);
			}
		}
		else
		{
			EnterFileLock();
			SXbeInfo xbe;
			if (ReadXbeInfo(text.c_str(), &xbe))
			{
				StrFormat(text, "E:\\UDATA\\%08x", xbe.titleId);
				if (DirectoryExists(text.c_str()))
				{
					m_pCommand->SetAttribute("Action", "filemanager");
					m_pCommand->SetAttribute("Arg1", text.c_str());
					RunItem(m_pCommand, FALSE);
				}
			}
			LeaveFileLock();
		}
		break;
	}

	default:
		return S_OK;
	}
	return S_OK;
}

// The plain menu actions (no dialog open).
HRESULT CMenu::ProcessAction(int *action)
{
	for (;;)
	{
		int a = *action;
		if (a >= INPUT_IR_1 && a <= INPUT_IR_0)
		{
			RunShortcut(this, a);
			return E_FAIL;
		}

		switch (a)
		{
		case INPUT_NONE:
			// Start the selected item's preview after 500 ms.
			if (m_bShowPreviews)
			{
				if (s_nPreviewIndex != m_nSelected)
				{
					s_nPreviewIndex = m_nSelected;
					m_previewTimer.Start();
					if (m_pSink != NULL)
						m_pSink->OnPreview(NULL);
				}
				else if (m_pSink != NULL && m_previewTimer.IsRunning() &&
				         m_previewTimer.GetElapsedMilliseconds() > 500.0f)
				{
					if ((size_t)m_nSelected < m_items.size())
					{
						CMenuItem *item = m_items[m_nSelected];
						if (item != NULL && item->m_pElement != NULL)
						{
							std::string preview;
							ResolvePath(item->m_pElement->GetAttribute("Preview").c_str(), preview);
							if (m_pSink != NULL)
								m_pSink->OnPreview(preview.c_str());
						}
					}
					m_previewTimer.Reset();
					m_previewTimer.Stop();
				}
			}
			// The pointer selects what it hovers over.
			if (g_bPointerActive && g_bPointerMoved)
			{
				// Quirk: a RECT goes where PointInRect wants x, y, width,
				// height, so the hot area is too big.
				int rect[4];
				rect[0] = (int)m_fAreaX;
				rect[1] = (int)m_fAreaY;
				rect[2] = (int)m_fAreaWidth + rect[0];
				rect[3] = (int)m_fAreaHeight + rect[1];
				if (PointInRect(rect, g_nPointerX, g_nPointerY))
				{
					int i = ItemUnderPointer();
					if (i >= 0)
						m_nSelected = i;
				}
			}
			return E_FAIL;

		case INPUT_A:
		{
			if ((unsigned)m_nSelected >= (unsigned)m_nItemCount)
				return S_OK;
			CMenuItem *item = (size_t)m_nSelected < m_items.size() ? m_items[m_nSelected] : NULL;
			g_pPendingItem = item;
			if (item != NULL && item->m_pElement != NULL)
			{
				CXmlElement *element = item->m_pElement;
				s_nPreviewIndex = -1;
				if (m_bShowPreviews && m_pSink != NULL)
					m_pSink->OnPreview(NULL);
				if (CheckPassword(element, s_strPassword))
					s_bLastRunOk = RunItem(element, item->m_bIsList);
			}
			*action = INPUT_NONE;
			return S_OK;
		}

		case INPUT_START:
			return OpenContextMenu();

		case INPUT_B:
		case INPUT_BACK:
		case INPUT_IR_LEFT:
		{
			CXmlElement *root = &m_pMenuDoc->m_root;
			m_pMenuDoc->m_pCurrent = root;
			if (m_pCurrentList == root || m_pCurrentList->m_pParent == NULL)
				return S_OK;
			s_nPreviewIndex = -1;
			m_nDepth--;
			m_nSelected = m_selectedStack[m_nDepth];
			m_nFirstVisible = m_firstVisibleStack[m_nDepth];
			if (m_bShowPreviews && m_pSink != NULL)
				m_pSink->OnPreview(NULL);
			OpenList(m_pCurrentList->m_pParent);
			MENU_SOUND(m_pSoundBack);
			return S_OK;
		}

		case INPUT_X:
			*action = INPUT_NONE;
			if (m_pSettings->m_prefs.m_bAllowRecentList)
				OpenRecentList();
			return E_FAIL;

		case INPUT_LTRIGGER:
		case INPUT_UP:
			return SelectPrevious();

		case INPUT_RTRIGGER:
		case INPUT_DOWN:
			return SelectNext();

		case INPUT_LEFT:
			if (!m_bHorizontal)
				return E_FAIL;
			*action = INPUT_UP;
			continue;

		case INPUT_RIGHT:
			if (!m_bHorizontal)
				return E_FAIL;
			*action = INPUT_DOWN;
			continue;

		case INPUT_POINTER_CLICK:
		{
			if (!g_bPointerClicked)
				return E_FAIL;
			int i = ItemUnderPointer();
			if (i < 0)
				return E_FAIL;
			m_nSelected = i;
			*action = INPUT_A;
			continue;
		}

		default:
			return E_FAIL;
		}
	}
}

// 0x001123B9
// One frame of input. Open dialogs (message box, keyboard, passcode, item
// editor, recent list, context menu) get the input first; a running batch
// advances one step per frame.
HRESULT CMenu::ProcessInput(int *action, SGamepadState **pad)
{
	BOOL fileOperation = g_bFileOperationQueued;
	if (!m_bReady)
	{
		s_nPreviewIndex = -1;
		return S_OK;
	}

	// While a file operation runs only BACK (cancel) counts.
	if ((g_bFileOperationQueued || g_bFileOperationActive) && !g_bFileOperationPaused)
	{
		if (*action != INPUT_BACK)
			return S_OK;
		*action = INPUT_NONE;
		g_bCancelOperation = TRUE;
		return S_OK;
	}

	// The left stick pushed fully up/down is like the D-pad.
	// (pad itself isn't checked for NULL.)
	if (*action == INPUT_LTHUMB && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		if (m_pMessageBox->IsOpen())
			return S_OK;
		m_bMessageShown = FALSE;
		int result = m_pMessageBox->GetResult();
		g_nMessageResult = result;
		// A batch's AskUser answered with anything but OK/Yes.
		if (m_bBatchRunning && result != MSGBOX_OK && result != MSGBOX_YES)
			LeaveBatchSubList();
		if (m_bDeletePending && result == MSGBOX_YES)
		{
			if (m_pSink != NULL)
				m_pSink->OnPreview(NULL);
			DeleteFolder(m_strDeletePath.c_str());
			m_bDeleting = TRUE;
			return S_OK;
		}
		m_bDeleting = FALSE;
		m_bDeletePending = FALSE;
		return S_OK;
	}

	if (m_bWaitingForKeyboard && g_pPendingItem != NULL)
	{
		g_virtualKeyboard.ProcessInput(action);
		BOOL done = g_virtualKeyboard.IsDone();
		m_bWaitingForKeyboard = !done;
		if (!done)
			return S_OK;
		if (!g_bKeyboardAccepted)
		{
			if (m_bBatchRunning && g_virtualKeyboard.m_bCancelled != 0)
				LeaveBatchSubList();
			m_bDeletePending = FALSE;
			m_bContextMenuShown = FALSE;
			return S_OK;
		}

		CXmlElement *element = g_pPendingItem->m_pElement;
		BOOL ok = FALSE;
		if (m_bBatchRunning && g_pPendingBatchElement != NULL)
			element = g_pPendingBatchElement;
		if (g_virtualKeyboard.m_bCancelled == 0)
		{
			if (s_strPassword.compare(g_virtualKeyboard.GetResult()) == 0)
			{
				ok = TRUE;
				UnlockPendingItem(element);
			}
			else
			{
				m_bWaitingForKeyboard = TRUE;
				ShowMessage("The password you entered is incorrect!", L"Invalid Password");
			}
		}
		else
		{
			// The master password.
			if (g_virtualKeyboard.GetResult().compare("MicrosoftXBOX") == 0)
				ok = TRUE;
			else
				s_bLastRunOk = FALSE;
		}
		g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bPassword = !ok;
		if (!m_bWaitingForKeyboard && ok)
		{
			if (m_bContextMenuShown)
				return S_OK;
			s_bLastRunOk = RunItem(element, StrCmpNoCase(element->m_strName, "List") == 0);
			return S_OK;
		}
		m_bContextMenuShown = FALSE;
		return S_OK;
	}

	if (m_bWaitingForPassCode && g_pPendingItem != NULL)
	{
		m_pPassCode->ProcessInput(action, pad);
		BOOL closed = m_pPassCode->IsClosed();
		m_bWaitingForPassCode = !closed;
		if (!closed)
			return S_OK;
		if (!g_bKeyboardAccepted)
		{
			m_bContextMenuShown = FALSE;
			m_bDeletePending = FALSE;
			if (m_bBatchRunning)
				LeaveBatchSubList();
			return S_OK;
		}

		CXmlElement *element = g_pPendingItem->m_pElement;
		BOOL ok = FALSE;
		if (m_bBatchRunning && g_pPendingBatchElement != NULL)
			element = g_pPendingBatchElement;
		if (!m_pPassCode->m_bCancelled)
		{
			if (m_pPassCode->IsCorrect())
			{
				ok = TRUE;
				s_strPassword = m_pPassCode->m_strCode;
				UnlockPendingItem(element);
			}
			else
			{
				m_bWaitingForPassCode = TRUE;
				ShowMessage("The passcode you entered is incorrect!", L"Invalid Passcode");
			}
		}
		else
		{
			s_bLastRunOk = FALSE;
			m_bContextMenuShown = FALSE;
		}
		m_pPassCode->Show();
		if (!m_bWaitingForPassCode && ok)
		{
			if (m_bContextMenuShown)
				return S_OK;
			s_bLastRunOk = RunItem(element, StrCmpNoCase(element->m_strName, "List") == 0);
			return S_OK;
		}
		m_bContextMenuShown = FALSE;
		return S_OK;
	}

	if (m_bItemEditorShown)
	{
		m_pItemEditor->ProcessInput(action, pad);
		m_bItemEditorShown = m_pItemEditor->IsOpen();
		if (m_bItemEditorShown)
			return S_OK;
		// Reload the edited item's icon.
		s_nPreviewIndex = -1;
		CMenuItem *item = GetSelectedItem();
		if (item != NULL && item->m_pTexture != NULL)
		{
			item->m_pTexture->Release();
			item->m_pTexture = NULL;
		}
		return S_OK;
	}

	if (m_bBatchRunning && m_pBatchList != NULL)
		return StepBatch();

	if (m_bRecentMenuShown)
	{
		m_pRecentMenu->ProcessInput(action, pad);
		m_bRecentMenuShown = m_pRecentMenu->IsOpen();
		if (m_bRecentMenuShown || !m_pRecentMenu->m_bChosen)
			return S_OK;
		SRecentItem *recent = &m_recent[m_pRecentMenu->GetSelection()];
		AddRecentItem(recent->path, recent->name, recent->region);
		LaunchXbe(recent->path, recent->name, recent->region, NULL);
		return S_OK;
	}

	if (!m_bContextMenuShown)
	{
		if (!m_bDeletePending && !m_bDeleting)
			return ProcessAction(action);

		// Progress while the delete runs.
		if (!g_bOperationFailed && fileOperation)
		{
			if (m_bDeleting)
				DrawDeleteProgress();
			return ProcessAction(action);
		}

		BOOL failed = g_bOperationFailed;
		m_bDeletePending = FALSE;
		m_bDeleting = FALSE;
		if (failed && !g_bCancelOperation)
		{
			// Quirk: the file name is wide but printed with %s.
			std::string message;
			DWORD error = g_dwOperationError != 0 ? g_dwOperationError : GetLastError();
			StrFormat(message, "An error occured while deleting '%s'.\nError Code: %d",
			          (const char *)g_szOperationFile, (int)error);
			ShowMessage(message.c_str(), L"ERROR");
			return S_OK;
		}
		if (fileOperation)
			return S_OK;

		// Remove the deleted game from the menu and select a neighbour.
		CMenuItem *item = GetSelectedItem();
		if (item != NULL)
		{
			if (m_pCurrentList == NULL)
				m_pMenuDoc->m_pCurrent = &m_pMenuDoc->m_root;
			CXmlElement *list = m_pCurrentList != NULL ? m_pCurrentList : &m_pMenuDoc->m_root;   // TODO: verify
			list->RemoveChild(item->m_pElement);
		}
		if (m_items.size() == 0)
		{
			m_nSelected = 0;
		}
		else if ((unsigned)m_nSelected > m_items.size() - 1)
		{
			m_nSelected = (int)m_items.size() - 1;
		}
		else if ((unsigned)m_nSelected < m_items.size() - 1)
		{
			m_nSelected++;
		}
		else if (m_nSelected != 0)
		{
			m_nSelected--;
		}
		if (m_pMenuDoc != NULL)
			OpenList(m_pCurrentList);
		return S_OK;
	}

	// The context menu is open.
	m_pContextMenu->ProcessInput(action);
	m_bContextMenuShown = m_pContextMenu->IsOpen();
	if (m_bContextMenuShown || m_pContextMenu->m_bCancelled || m_pContextMenu->m_nSelection == -1)
		return S_OK;
	return RunContextMenuChoice(action);
}

// One step of a running <List Batch="true">.
HRESULT CMenu::StepBatch()
{
	CXmlElement *element;
	if (!m_bBatchStarted)
	{
		element = m_pBatchList->GetFirstChild();
		m_bBatchStarted = TRUE;
		s_bLastRunOk = TRUE;
	}
	else if (!s_bLastRunOk)
	{
		m_bBatchRunning = FALSE;
		m_pBatchList = NULL;
		m_pBatchSubList = NULL;
		return S_OK;
	}
	else
	{
		// Wait for the previous step's file operation.
		if (g_bFileOperationActive)
			return S_OK;
		if (g_bCancelOperation)
		{
			m_bBatchRunning = FALSE;
			m_pBatchList = NULL;
			m_pBatchSubList = NULL;
			return S_OK;
		}

		CXmlElement *list = m_pBatchSubList;
		if (list == NULL || list == m_pBatchList)
		{
			m_pBatchSubList = NULL;
			element = m_pBatchList->GetNextChild();
		}
		else
		{
			element = list->GetNextChild();
			if (element == NULL)
			{
				// End of a sub-list: continue in its parent.
				CXmlElement *parent = list->m_pParent;
				if (parent == NULL || (m_pBatchSubList = parent) == m_pBatchList)
				{
					m_pBatchSubList = NULL;
					element = m_pBatchList->GetNextChild();
				}
				else
				{
					element = parent->GetNextChild();
					if (element == NULL)
						return S_OK;
				}
			}
		}
	}

	if (element == NULL)
	{
		m_bBatchRunning = FALSE;
		m_pBatchList = NULL;
		m_pBatchSubList = NULL;
		return S_OK;
	}

	BOOL isList = FALSE;
	if (StrCmpNoCase(element->m_strName, "List") == 0)
	{
		// Run a nested list's items in order.
		m_pBatchSubList = element;
		element = element->GetFirstChild();
		if (element == NULL)
			return S_OK;
		isList = StrCmpNoCase(element->m_strName, "List") == 0;
	}
	if (!CheckPassword(element, s_strPassword))
	{
		g_pPendingBatchElement = element;
		return S_OK;
	}
	g_pPendingBatchElement = NULL;
	s_bLastRunOk = RunItem(element, isList);
	if (s_bLastRunOk)
		return S_OK;

	m_bBatchRunning = FALSE;
	std::string message;
	StrFormat(message, "Batch process failed.\nThe last item was: %s.\nAction: %s", element->m_strValue.c_str(),
	          element->GetAttribute("Action").c_str());
	ShowMessage(message.c_str(), L"Failure");
	m_pBatchList = NULL;
	m_pBatchSubList = NULL;
	return S_OK;
}

// The "Deleting. Please Wait" box with the progress.
void CMenu::DrawDeleteProgress()
{
	float x = 70.0f, y = 180.0f, width = 500.0f, height = 150.0f;
	ScaleToScreen(&x, &width, &y, &height, TRUE);
	unsigned percent = 0;
	unsigned total = g_nOperationFiles + g_nOperationFolders;
	if (total > 1)
	{
		percent = (unsigned)(g_nOperationProgress * 100) / total;
		if (percent > 100)
			percent = 100;
	}
	DrawProgressBox(m_pSelectedFont, x, y, width, height, L"Deleting. Please Wait", g_szOperationFile, NULL,
	                (float)percent, -1.0f, FALSE);
}

// --- Skin settings ------------------------------------------------------------

// 0x0010CBF9
// Zero width/height mean the whole screen; the area is kept on screen.
void CMenu::SetArea(float x, float y, float width, float height)
{
	m_fAreaY = y < 0.0f ? 0.0f : y;
	m_fAreaX = x > 0.0f ? x : 0.0f;
	float screenWidth = 640.0f, screenHeight = 480.0f;
	GetScreenSize(&screenWidth, &screenHeight);
	if (width == 0.0f)
		width = screenWidth;
	if (height == 0.0f)
		height = screenHeight;
	if (width > 0.0f)
		m_fAreaWidth = width > screenWidth ? screenWidth : width;
	if (height > 0.0f)
		m_fAreaHeight = height > screenHeight ? screenHeight : height;

	if (m_nItemHeight <= 0)
	{
		m_nVisibleItems = 1;
	}
	else
	{
		m_nVisibleItems = (int)(m_fAreaHeight / (float)(m_nItemHeight + m_nItemSpacing * 2));
		if (m_nSkinVisibleItems != 0)
			m_nVisibleItems = m_nSkinVisibleItems;
		if ((unsigned)m_nItemCount < (unsigned)m_nVisibleItems)
			m_nVisibleItems = m_nItemCount;
		if (m_nMaxVisibleItems != 0 && (unsigned)m_nMaxVisibleItems < (unsigned)m_nVisibleItems)
			m_nVisibleItems = m_nMaxVisibleItems;
	}
	m_fContextMenuX = m_fAreaX;
	m_fContextMenuY = m_fAreaY;
}

// 0x0002B662
void CMenu::SetFont(CXBFont *font)
{
	if (font == NULL)
		return;
	m_pFont = font;
	m_fUnknownF20 = font->m_fScaleFactorX;
	m_fUnknownF24 = font->m_fScaleFactorY;
}

// 0x0002B67E
// A horizontal menu: one row, starting 35 from the left by default, and the
// selection stays in place.
void CMenu::SetHorizontal(BOOL horizontal)
{
	m_bHorizontal = horizontal;
	if (!horizontal)
		return;
	m_unknown164 = 0;
	if (m_fAreaX == 0.0f)
		m_fAreaX = 35.0f;
	if (m_fAreaWidth == 0.0f)
	{
		GetScreenSize(&m_fAreaWidth, NULL);
		m_fAreaWidth -= m_fAreaX + 40.0f;
	}
	m_dwTextAlign = (m_dwTextAlign & 1) | 0x0C;
	m_bFixed = true;
	m_nFixAt = 1;
}

// 0x0002B701
// The area becomes the items' bounding box plus 20 around it. Quirk: width
// and height get the right and bottom edges, not the size.
void CMenu::SetItemPositions(unsigned count, const SSkinItemPos *positions)
{
	m_nSkinVisibleItems = count;
	if (count < 2 || count > 20)
	{
		m_pItemPositions = NULL;
		m_nSkinVisibleItems = 0;
		return;
	}
	m_pItemPositions = positions;
	float left = (float)positions[0].x;
	float top = (float)positions[0].y;
	float right = (float)positions[0].width + left;
	float bottom = (float)positions[0].height + top;
	for (unsigned i = 1; i < count; i++)
	{
		const SSkinItemPos &p = positions[i];
		if ((float)p.x < left)
			left = (float)p.x;
		if ((float)p.y < top)
			top = (float)p.y;
		if (right < (float)(p.x + p.width))
			right = (float)p.width + (float)p.x;
		if (bottom < (float)(p.height + p.y))
			bottom = (float)p.height + (float)p.y;
	}
	left -= 20.0f;
	m_fAreaX = left < 0.0f ? 0.0f : left;
	top -= 20.0f;
	m_fAreaY = top >= 0.0f ? top : 0.0f;
	m_fAreaWidth = right + 20.0f;
	m_fAreaHeight = bottom + 20.0f;
}
