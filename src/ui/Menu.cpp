#include "Menu.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../app/Screens.h"
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
	m_unknownF18 = 0;
	m_pFont = NULL;
	m_pSelectedFont = NULL;
	m_pMenuDoc = NULL;
	m_unknownF08 = 1;
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
	m_unknownF30 = 0;
	m_unknownF31 = 0;
	m_unknownF34 = 0;
	m_unknownF14 = 12;
	m_bWaitingForKeyboard = FALSE;
	m_bMessageShown = FALSE;
	m_pMessageBox = NULL;
	m_pSoundSelect = NULL;
	m_pSoundBack = NULL;
	m_pSoundUp = NULL;
	m_pSoundDown = NULL;
	m_unknown01C = 0;
	m_fUnknown20 = 0.0f;
	m_fUnknown24 = 0.0f;
	m_unknown028 = 0;
	m_unknown124 = 1;
	m_bBatchRunning = FALSE;
	m_pBatchList = NULL;
	m_nBatchIndex = 0;
	m_unknown138 = 0;
	m_nItemHeight = 0;
	m_pCurrentList = NULL;
	m_unknown160 = 0;
	m_nSkinVisibleItems = 0;
	m_unknown16C = 0;
	m_unknown164 = 0;
	m_pUnknown170 = NULL;
	m_unknown174 = 0;
	m_fUnknown178 = 0.0f;
	m_fUnknown17C = 0.0f;
	m_bShowPreviews = FALSE;
	m_unknownF10 = 1;
	m_bFontItemHeight = FALSE;
	m_pPassCode = NULL;
	m_bWaitingForPassCode = FALSE;
	m_dwListOpenTime = 0;
	m_unknown19C = 0;
	m_unknown1A0 = 0;
	m_pRecentMenu = NULL;
	m_bRecentMenuShown = FALSE;
	memset(m_recent, 0, sizeof(m_recent));
	m_unknownF38 = 0;
	m_dwIconLoadTime = GetTickCount() + 2000;
	m_fUnknownF40 = 0.0f;
	m_pItemEditor = NULL;
	m_bItemEditorShown = FALSE;
	m_unknownDFC = 0;
	m_unknownE00 = 0;

	m_pCommand = new CXmlElement;
	if (m_pCommand != NULL)
		m_pCommand->m_strName = "Command";
	m_timer.Stop();
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
	delete (CMessageBox *)m_pUnknown170;   // TODO: verify the type
	m_pUnknown170 = NULL;
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
                      BOOL fontItemHeight, int unknownF18, IDirect3DTexture8 *texture70,
                      IDirect3DTexture8 *texture74)
{
	if (doc == NULL || font == NULL)
		return E_FAIL;

	m_pSink = sink;
	m_unknownF18 = unknownF18;
	m_pMenuDoc = doc;
	m_pFont = font;
	m_pSelectedFont = selectedFont != NULL ? selectedFont : font;
	m_fUnknownF20 = 1.0f;
	m_fUnknownF24 = 1.0f;
	m_bFontItemHeight = fontItemHeight;
	if (!fontItemHeight)
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
	if (!m_bFontItemHeight)
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
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(0, 0, 0, 0, 0, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->AddButton(GetString(STR_OK));
			m_pMessageBox->AddButton(GetString(STR_CANCEL));
			m_pMessageBox->AddButton(GetString(STR_ABORT));
			m_pMessageBox->AddButton(GetString(STR_RETRY));
			m_pMessageBox->AddButton(GetString(STR_IGNORE));
			m_pMessageBox->AddButton(GetString(STR_YES));
			m_pMessageBox->AddButton(GetString(STR_NO));
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
		g_virtualKeyboard.Show();
		g_bKeyboardPassword = TRUE;
		g_nKeyboardMaxLength = 50;
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
				m_pPassCode->Init();
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
			m_nBatchIndex = 0;
			m_bBatchRunning = TRUE;
			m_unknown138 = 0;
			g_bCancelOperation = FALSE;
			g_virtualKeyboard.Show();   // TODO: verify (vtable slot 3)
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
		g_virtualKeyboard.Show();
		g_bKeyboardPassword = FALSE;
		g_nKeyboardMaxLength = 64;
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
