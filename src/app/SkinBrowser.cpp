// CSkinBrowser: the skin chooser and the skin download list. See Screens.h.
//
// The local list: "Default", the built-in skins, then every folder under
// <Skin Path> that has a skin.xml (or a zip with one). A applies the
// highlighted skin (again: like START), START closes and saves the choice
// to Config.xml, B/BACK closes, Y opens the "Select Server" menu, the right
// stick click turns previews on and off.
//
// The online list (an RSS feed from AllXboxSkins.com or Xbox-Skins.net):
// A or X tick skins, BLACK fetches a preview, START downloads the ticked
// ones (each into its own folder, unzipped or unrarred), B/BACK goes back.
#include <algorithm>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/Http.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "../thirdparty/XUnzip.h"
#include "../ui/Language.h"
#include "../ui/MenuDialogs.h"
#include "../ui/StringIds.h"
#include "../xml/XmlDocument.h"
#include "../xml/XmlUtil.h"
#include "Screens.h"

// The skin applied in this visit (-1: none yet). 0x0026C9B0
static int s_nAppliedSkin = -1;
// The entry whose preview is loaded. 0x0026C9AC
static int s_nPreviewIndex = -1;
// The skin site last chosen. 0x0026C9B4
static int s_nServer = -1;
// Previews on (the right stick's click). 0x00278D1C
static BOOL s_bShowPreviews = FALSE;
// The preview of a skin is downloaded after its zip. 0x00278D18
static BOOL s_bPreviewNext = FALSE;
// What was downloaded this time. 0x00278D14
static DWORD s_dwDownloadedBytes = 0;
// The install thread. 0x0026C9A8
static HANDLE s_hInstallThread = INVALID_HANDLE_VALUE;

#define SKIN_SITE_ALLXBOXSKINS "http://www.allxboxskins.com/rssunleashxdash.xml"
#define SKIN_SITE_XBOXSKINS "http://www.xbox-skins.net/rss/uxdash.php"

// 0x00013215 (inlined): appends the title narrowed with the C++ locale.
// TODO: verify (the original goes through ctype<wchar_t>::narrow)
static void AppendNarrow(std::string &out, const wchar_t *text)
{
	if (text != NULL && wcslen(text) > 0)
		out.append(StrFromWide(text));
}

// 0x0002825D (this in esi)
SOnlineSkin::SOnlineSkin(const char *titleText, const char *authorText, const char *linkUrl, const char *thumbUrl)
{
	memset(title, 0, sizeof(title));
	memset(author, 0, sizeof(author));
	selected = FALSE;
	localThumb.erase();
	if (titleText == NULL || strlen(titleText) == 0)
		return;
	StrUtf8ToWide(titleText, title, 0x104);
	if (authorText != NULL && strlen(authorText) != 0)
		StrUtf8ToWide(authorText, author, 0x32);
	link = linkUrl;
	thumb = thumbUrl;
}

// The online list's order. 0x000F5D18
static bool OnlineSkinLess(const SOnlineSkin &a, const SOnlineSkin &b)
{
	return _wcsicmp(a.title, b.title) < 0;
}

// 0x000F6597 (this in esi)
CSkinBrowser::CSkinBrowser()
{
	m_pSettings = CAppSettings::GetInstance();
	ResolvePath(m_pSettings->m_prefs.m_strSkinPath.c_str(), m_strSkinPath);
	if (m_strSkinPath.size() != 0 && StrRight(m_strSkinPath, 1).compare("\\") != 0)
		m_strSkinPath.append("\\");
	m_nDownloadIndex = -1;
	m_bOpen = TRUE;
	m_bListed = TRUE;
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_pSymbolFont = NULL;
	m_pSoundMove = NULL;
	m_bClosing = FALSE;
	m_bKeyboardOpen = FALSE;
	m_nVisible = 0;
	m_nTop = 0;
	m_nSelected = 0;
	m_bSkinChanged = FALSE;
	m_pPreview = NULL;
	m_pSession = NULL;
	m_pFeed = NULL;
	m_bOnline = FALSE;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_dwListTick = 0;
	m_pUnchecked = NULL;
	m_pChecked = NULL;
	m_bDownloading = FALSE;
	m_bCancel = FALSE;
	m_nDownloaded = 0;
	m_bPreviewOnly = FALSE;
	m_pServerLogo = NULL;
	m_bInstalling = FALSE;
	m_pServerMenu = NULL;
	m_bServerMenuOpen = FALSE;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_fCheckSize = 0.0f;
}

// 0x000F5D35 (this in edi)
CSkinBrowser::~CSkinBrowser()
{
	m_pFont = NULL;
	m_pCaptionFont = NULL;
	m_pSoundMove = NULL;
	m_pSymbolFont = NULL;
	m_skins.clear();
	if (m_pFeed != NULL)
		m_pFeed->Clear();
	if (m_pSession != NULL)
	{
		delete m_pSession;
		m_pSession = NULL;
	}
	if (m_pFeed != NULL)
	{
		delete m_pFeed;
		m_pFeed = NULL;
	}
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	ReleaseTexture(m_pServerLogo);
	if (m_pServerMenu != NULL)
	{
		delete m_pServerMenu;
		m_pServerMenu = NULL;
	}
	ReleaseTexture(m_pPreview);
}

// 0x000F5DEC (checked in eax, unchecked in ecx, this in ebx)
HRESULT CSkinBrowser::Init(IDirect3DTexture8 *checked, IDirect3DTexture8 *unchecked, CXBFontEx *font)
{
	if (m_pFont != NULL)
		return S_OK;
	if (font == NULL)
		return E_FAIL;
	m_pChecked = checked;
	m_pUnchecked = unchecked;
	if (checked != NULL)
	{
		D3DSURFACE_DESC desc;
		checked->GetLevelDesc(0, &desc);
		m_fCheckSize = (float)desc.Height;
	}
	m_pFont = font;
	m_pCaptionFont = m_pSettings->m_pDialogFont;
	if (m_pCaptionFont == NULL)
		m_pCaptionFont = font;
	m_fY = 10.0f;
	m_fX = 30.0f;
	m_fWidth = 580.0f;
	m_fHeight = 460.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
	m_pSoundMove = g_soundEffects.Find("Scroll");
	g_fonts.Find("XboxDings", &m_pSymbolFont);
	m_nSelected = -1;
	return S_OK;
}

// 0x000F6791 (this in ebx)
void CSkinBrowser::Show()
{
	m_fY = 10.0f;
	m_fX = 30.0f;
	m_fWidth = 580.0f;
	m_fHeight = 460.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
	StartList();
	m_bClosing = FALSE;
	m_bOnline = FALSE;
	m_bOpen = TRUE;
}

// 0x000F673A (this in esi)
void CSkinBrowser::StartList()
{
	if (!m_bListed)
		return;
	m_bListed = FALSE;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ListThread, this, 0, NULL);
	if (thread == NULL || thread == INVALID_HANDLE_VALUE)
	{
		ListSkins();
	}
	else
	{
		ResumeThread(thread);
		Sleep(5);
		CloseHandle(thread);
	}
	m_dwListTick = GetTickCount();
}

// 0x000F66EC
unsigned __stdcall CSkinBrowser::ListThread(void *param)
{
	if (param != NULL)
		((CSkinBrowser *)param)->ListSkins();
	return 0;
}

// 0x000F6098 (this on the stack)
HRESULT CSkinBrowser::ListSkins()
{
	m_skins.clear();
	m_skins.push_back(SFileItem("Default", TRUE, TRUE, 0));
	for (int i = 0; i < GetInternalSkinCount(); i++)
		m_skins.push_back(SFileItem(GetInternalSkinName(i), FALSE, FALSE, 0));

	if (m_strSkinPath.size() > 1)
	{
		if (StrFind(m_strSkinPath, ":\\", 0) == -1)
			ResolvePath(m_strSkinPath.c_str(), m_strSkinPath);
		if (StrRight(m_strSkinPath, 1).compare("\\") != 0)
			m_strSkinPath.append("\\");
		CFileFinder folders, files;
		folders.m_nFilter = 2;   // folders only
		files.m_nFilter = 1;     // files only
		if (folders.FindFirst(m_strSkinPath.c_str(), "*") != NULL)
		{
			while (strlen(folders.GetName()) != 0)
			{
				std::string folder = m_strSkinPath;
				folder.append(folders.GetName());
				BOOL isFolder = (folders.GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) != 0;
				if (files.FindFirst(folder.c_str(), "skin.xml") != NULL)
				{
					m_skins.push_back(SFileItem(folders.GetName(), FALSE, isFolder, 0));
				}
				else if (files.FindFirst(folder.c_str(), "*.zip") != NULL)
				{
					// A zipped skin; its preview is unzipped next to it.
					if (StrRight(folder, 1).compare("\\") != 0)
						folder.append("\\");
					folder.append(files.GetName());
					HZIP zip = OpenZip((void *)folder.c_str(), 0, ZIP_FILENAME);
					if (zip != NULL)
					{
						int index = -1;
						ZIPENTRY entry;
						memset(&entry, 0, sizeof(entry));
						if (FindZipItem(zip, "skin.xml", true, &index, &entry) == ZR_OK && index != -1)
						{
							folder = m_strSkinPath;
							folder.append(folders.GetName());
							m_skins.push_back(SFileItem(folders.GetName(), FALSE, isFolder, 0));
							if (StrRight(folder, 1).compare("\\") != 0)
								folder.append("\\");
							folder.append("preview.jpg");
							if (!FileExists(folder.c_str()))
							{
								memset(&entry, 0, sizeof(entry));
								if (FindZipItem(zip, "preview.jpg", true, &index, &entry) == ZR_OK && index != -1)
									UnzipItem(zip, index, (void *)folder.c_str(), 0, ZIP_FILENAME);
							}
						}
						CloseZip(zip);
					}
				}
				files.Close();
				folders.FindNext();
			}
		}
		folders.Close();
		if (m_skins.size() != 0)
			std::sort(m_skins.begin(), m_skins.end());
	}

	// First time: the current skin.
	if (m_nSelected == -1)
	{
		for (int i = 0; i < (int)m_skins.size(); i++)
		{
			if (_stricmp(m_skins[i].name, g_skin.m_strSkinName.c_str()) != 0)
				continue;
			m_nSelected = i;
			if (m_nVisible == 0)
				break;
			int top = i;
			if (i < m_nTop)
			{
				m_nBottom = m_nVisible - 1 + i;
			}
			else
			{
				if (i <= m_nBottom)
					break;
				m_nBottom = i;
				top = i - m_nVisible + 1;
				if (top < 0)
					top = 0;
			}
			m_nTop = top;
			break;
		}
	}
	if (m_nSelected == -1 || (int)m_skins.size() - 1 < m_nSelected)
	{
		m_nSelected = 0;
		m_nTop = 0;
	}
	m_bListed = TRUE;
	return S_OK;
}

// 0x000F5EE9 (this in ebx)
void CSkinBrowser::ShowServerMenu()
{
	if (m_pServerMenu == NULL)
	{
		m_pServerMenu = new COptionMenu(FALSE);
		if (m_pServerMenu != NULL)
		{
			m_pServerMenu->Create(m_pFont, L"Select Server", 0.0f, 0.0f, 0.0f, 0.0f, 10);
			m_pServerMenu->AddOption(L"AllXboxSkins.com", 0);
			m_pServerMenu->AddOption(L"Xbox-Skins.net", 0);
		}
	}
	if (m_pServerMenu != NULL)
	{
		m_pServerMenu->Show(0);
		m_bServerMenuOpen = TRUE;
	}
}

// 0x000F5F78 (this in edi)
void CSkinBrowser::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
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

// Makes sure parent has a child element called name; *element is it.
// (0x000F6701, element in esi)
static BOOL EnsureChild(CXmlElement **element, CXmlElement *parent, const char *name)
{
	if (parent == NULL)
		return FALSE;
	if (*element == NULL)
	{
		AddChildElement(parent, name, NULL, NULL);
		*element = parent->FindFirstChild(name);
	}
	return *element != NULL;
}

// (inlined in ProcessInput)
void CSkinBrowser::UpdatePreview()
{
	if (s_nPreviewIndex == m_nSelected)
		return;
	s_nPreviewIndex = m_nSelected;
	ReleaseTexture(m_pPreview);
	if (!s_bShowPreviews)
		return;
	std::string path;
	if (!m_bOnline)
	{
		path = m_strSkinPath;
		if (path.size() >= 2 && StrRight(path, 1).compare("\\") != 0)
			path.append("\\");
		path.append(m_skins[m_nSelected].name);
		path.append("\\");
		path.append("preview.jpg");
	}
	else if (m_online.size() != 0)
	{
		path = m_online[m_nSelected].localThumb;
	}
	if (FileExists(path.c_str()) &&
	    !LoadTextureFromFile(path.c_str(), g_pd3dDevice, &m_pPreview, 320, 240, NULL))
		m_pPreview = NULL;
}

// (inlined in ProcessInput)
void CSkinBrowser::ScrollToSelection()
{
	int selected = m_nSelected;
	if (selected < m_nTop)
	{
		int bottom = m_nVisible - 1 + selected;
		m_nTop = selected;
		m_nBottom = bottom;
		int last = (m_bOnline ? (int)m_online.size() : (int)m_skins.size()) - 1;
		if (last < bottom)
		{
			m_nBottom = last;
			m_nTop = last - m_nVisible + 1;
		}
	}
	if (m_nBottom < selected)
	{
		m_nBottom = selected;
		m_nTop = selected - m_nVisible + 1;
		if (m_nTop < 0)
		{
			m_nTop = 0;
			m_nBottom = m_nVisible - 1;
		}
	}
}

// 0x000F67FD (this in ecx, pad in eax)
HRESULT CSkinBrowser::ProcessInput(int *action, SGamepadState **pad)
{
	// (Nothing opens this keyboard.)
	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen || g_virtualKeyboard.m_bCancelled || wcslen(g_virtualKeyboard.GetText()) == 0)
			return S_OK;
		std::string folder = m_strSkinPath;
		if (StrRight(folder, 1).compare("\\") != 0)
			folder.append("\\");
		folder.append(g_virtualKeyboard.GetResult());
		if (CreateDirectoryA(folder.c_str(), NULL))
			StartList();
		return S_OK;
	}
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		if (!m_pMessageBox->IsOpen())
			m_bMessageShown = FALSE;
		return S_OK;
	}

	if (m_bServerMenuOpen)
	{
		// "Select Server": fetch that site's feed.
		m_pServerMenu->ProcessInput(action, pad);
		m_bServerMenuOpen = m_pServerMenu->IsOpen();
		if (m_bServerMenuOpen || !m_pServerMenu->m_bChosen)
			return S_OK;
		std::string url;
		std::string logo = m_pSettings->m_strDataPath;
		int server = m_pServerMenu->GetSelection();
		if (server == 0)
		{
			url = SKIN_SITE_ALLXBOXSKINS;
			logo.append("\\allxboxskins.jpg");
		}
		else if (server == 1)
		{
			url = SKIN_SITE_XBOXSKINS;
			logo.append("\\xboxskins.jpg");
		}
		if (url.size() != 0)
		{
			m_bOnline = TRUE;
			if (s_nServer != server)
			{
				s_nServer = server;
				if (m_pSession != NULL)
				{
					delete m_pSession;
					m_pSession = NULL;
				}
				m_online.clear();
			}
			if (m_pSession == NULL)
			{
				if (m_pFeed != NULL)
				{
					m_pFeed->Clear();
					delete m_pFeed;
					m_pFeed = NULL;
				}
				m_nSelected = 0;
				m_pSession = new CInternetSession;
				if (m_pSession == NULL)
				{
					m_bOnline = FALSE;
				}
				else
				{
					m_pSession->Get(url.c_str(), TRUE);
					ReleaseTexture(m_pServerLogo);
					if (D3DXCreateTextureFromFileA(g_pd3dDevice, logo.c_str(), &m_pServerLogo) != S_OK)
						m_pServerLogo = NULL;
				}
			}
		}
		return S_OK;
	}

	if (m_bDownloading)
	{
		// Only B/BACK, to stop it.
		if (*action != INPUT_BACK && *action != INPUT_B)
			return S_OK;
		*action = INPUT_NONE;
		if (m_pSession != NULL)
			m_pSession->Cancel();
		m_bDownloading = FALSE;
		return S_OK;
	}
	if (m_bInstalling)
		return S_OK;

	// Quirk: the stick only counts when pushed all the way.
	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	int count;
	for (;;)
	{
		switch (*action)
		{
		case INPUT_A:
			if (m_bOnline)
			{
				if (m_online.size() != 0)
					m_online[m_nSelected].selected = !m_online[m_nSelected].selected;
				m_bSkinChanged = TRUE;
				return S_OK;
			}
			if (m_nSelected == 0 || m_skins[m_nSelected].isParent)
			{
				// "Default".
				if (s_nAppliedSkin != m_nSelected)
				{
					s_nAppliedSkin = m_nSelected;
					g_skin.ChangeSkin(NULL);
					m_bSkinChanged = TRUE;
					return S_OK;
				}
			}
			else if (s_nAppliedSkin != m_nSelected)
			{
				s_nAppliedSkin = m_nSelected;
				SFileItem &item = m_skins[m_nSelected];
				std::string path;
				if (!item.isFolder)
				{
					// A built-in skin, by its name.
					g_skin.ChangeSkin(item.name);
				}
				else
				{
					// The folder's skin.xml, else its zip.
					ResolvePath(m_strSkinPath.c_str(), path);
					if (path.size() >= 2 && StrRight(path, 1).compare("\\") != 0)
						path.append("\\");
					path.append(item.name);
					path.append("\\");
					path.append("skin.xml");
					if (!FileExists(path.c_str()))
					{
						CFileFinder finder;
						StrReplaceAll(path, "\\skin.xml", "");
						finder.m_nFilter = 1;
						if (finder.FindFirst(path.c_str(), "*.zip") != NULL)
						{
							path.append("\\");
							path.append(finder.GetName());
						}
						else
						{
							path.erase();
						}
					}
					g_skin.ChangeSkin(path.c_str());
				}
				m_bSkinChanged = TRUE;
				return S_OK;
			}
			// Already applied: A acts as START.
			*action = INPUT_START;
			continue;

		case INPUT_START:
			if (m_bOnline)
			{
				// Download the ticked skins.
				m_nDownloadIndex = -1;
				m_bDownloading = TRUE;
				m_bCancel = FALSE;
				m_nDownloaded = 0;
				m_bPreviewOnly = FALSE;
				return S_OK;
			}
			if (m_bSkinChanged && !m_pSettings->m_bRunFromDVD)
			{
				// Save the choice as <Settings><Preference><Skin>.
				m_bSkinChanged = FALSE;
				CXmlElement *settings = g_config.FindElement(NULL, "Settings");
				g_config.m_pCurrent = &g_config.m_root;
				if (EnsureChild(&settings, &g_config.m_root, "Settings"))
				{
					CXmlElement *preference = settings->FindFirstChild("Preference");
					EnsureChild(&preference, settings, "Preference");
					if (preference != NULL)
					{
						CXmlElement *skin = preference->FindFirstChild("Skin");
						EnsureChild(&skin, preference, "Skin");
						if (skin != NULL)
						{
							skin->SetValue(g_skin.m_strSkinName.c_str());
							std::string file;
							ResolvePath(g_config.m_strFileName.c_str(), file);
							if (!g_config.SaveAs(file.c_str()))
							{
								std::string message;
								StrFormat(message, "Unable to Save settings. Error Code: %d", (int)GetLastError());
								ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
								return E_FAIL;
							}
							g_skin.SaveSplashForBoot();
						}
					}
				}
			}
			m_bClosing = FALSE;
			m_bOpen = FALSE;
			return S_OK;

		case INPUT_B:
		case INPUT_BACK:
			if (m_bOnline)
			{
				// Back to the local list.
				if (m_pSession != NULL)
					m_pSession->Cancel();
				m_nSelected = -1;
				m_nVisible = 0;
				StartList();
				m_bOnline = FALSE;
				*action = INPUT_NONE;
				return S_OK;
			}
			*action = INPUT_NONE;
			m_bClosing = FALSE;
			m_bOpen = FALSE;
			return S_OK;

		case INPUT_X:
			// Tick and move on.
			if (!m_bOnline || m_nSelected >= (int)m_online.size())
				return S_OK;
			m_online[m_nSelected].selected = !m_online[m_nSelected].selected;
			if (m_online[m_nSelected].selected && m_nSelected < (int)m_online.size() - 1)
				m_nSelected++;
			return S_OK;

		case INPUT_Y:
		case INPUT_IR_INFO:
			if (!m_bOnline)
				ShowServerMenu();
			else
				StartList();
			m_bOnline = FALSE;
			ReleaseTexture(m_pPreview);
			return S_OK;

		case INPUT_BLACK:
			// Fetch the highlighted skin's preview. (Quirk: the index
			// check lets one past the end through.)
			if (m_bDownloading || !m_bOnline || (int)m_online.size() < m_nSelected)
				return S_OK;
			{
				SOnlineSkin &skin = m_online[m_nSelected];
				if (skin.localThumb.size() == 0 && skin.thumb.size() != 0 &&
				    StrCmpNoCase(StrRight(skin.thumb, 4).c_str(), ".jpg") == 0)
				{
					m_pSession->Get(skin.thumb.c_str(), TRUE);
					m_bPreviewOnly = TRUE;
					m_bDownloading = TRUE;
					s_bShowPreviews = TRUE;
				}
			}
			return S_OK;

		case INPUT_RTHUMB_CLICK:
			s_bShowPreviews = !s_bShowPreviews;
			if (s_bShowPreviews)
			{
				UpdatePreview();
				ScrollToSelection();
			}
			else
			{
				ReleaseTexture(m_pPreview);
			}
			return S_OK;

		case INPUT_LTRIGGER:
		case INPUT_UP:
			count = m_bOnline ? (int)m_online.size() : (int)m_skins.size();
			if (count == 0 || m_nSelected < 1)
				return S_OK;
			m_nSelected--;
			break;

		case INPUT_RTRIGGER:
		case INPUT_DOWN:
			count = m_bOnline ? (int)m_online.size() : (int)m_skins.size();
			if (count == 0 || count - 1 <= m_nSelected)
				return S_OK;
			m_nSelected++;
			break;

		default:
			return S_OK;
		}
		break;
	}
	PlaySound(m_pSoundMove);
	UpdatePreview();
	ScrollToSelection();
	return S_OK;
}

// Unpacks a downloaded skin into its folder: a zip with skin.xml at the
// top is used as it is; other zips (and RARs) are unpacked, and a folder
// inside with the skin.xml is moved up. 0 when done. 0x000F73C6
static unsigned __stdcall InstallSkinThread(void *param)
{
	const char *path = (const char *)param;
	if (path == NULL)
		return 1;
	std::string zipFile = _strdup(path);   // (never freed)
	std::string folder = GetPathDirectory(zipFile.c_str());
	Sleep(100);
	HZIP zip = OpenZip((void *)zipFile.c_str(), 0, ZIP_FILENAME);
	if (zip == NULL)
	{
		// Not a zip: try it as a RAR.
		std::string rarFile = StrLeft(zipFile, (int)zipFile.size() - 4);
		rarFile.append(".rar");
		MoveFileA(zipFile.c_str(), rarFile.c_str());
		UnRarFile(rarFile.c_str(), folder.c_str());
		while (g_bFileOperationQueued)
			Sleep(100);
		if (g_bOperationFailed)
			return 1;
	}
	else
	{
		int index = -1;
		ZIPENTRY entry;
		memset(&entry, 0, sizeof(entry));
		ZRESULT found = FindZipItem(zip, "skin.xml", true, &index, &entry);
		CloseZip(zip);
		if (found == ZR_OK && index != -1)
			return 0;
		UnzipFile(zipFile.c_str(), folder.c_str());
		while (g_bFileOperationQueued)
			Sleep(100);
	}
	if (g_bOperationFailed)
		return 1;

	CountFolder(folder.c_str());
	for (std::vector<SFileListItem>::iterator it = g_fileList.begin(); it != g_fileList.end(); ++it)
	{
		if (it->isFolder || StrCmpNoCase(StrRight(it->path, 8).c_str(), "skin.xml") != 0)
			continue;
		std::string skinFolder = GetPathDirectory(it->path.c_str());
		if (StrCmpNoCase(skinFolder, folder.c_str()) != 0)
		{
			MoveFiles(skinFolder.c_str(), folder.c_str());
			while (g_bFileOperationQueued)
				Sleep(100);
		}
		break;
	}
	return 0;
}

// 0x000F76A4 (this on the stack)
HRESULT CSkinBrowser::RenderOnline()
{
	std::string message;
	if (!m_bDownloading)
	{
		// The feed.
		if (!m_pSession->IsDone())
		{
			wchar_t text[100];
			memset(text, 0, sizeof(text));
			switch (m_pSession->m_nState)
			{
			case HTTP_OPENING: wcscpy(text, GetString(STR_HTTP_OPENING)); break;
			case HTTP_CONNECTING: wcscpy(text, GetString(STR_HTTP_CONNECTING)); break;
			case HTTP_CONNECTED: wcscpy(text, GetString(STR_HTTP_CONNECTED)); break;
			case HTTP_DOWNLOADING:
				swprintf(text, 100, L"Retrieving %d", (int)m_pSession->GetPercent());
				wcscat(text, L"%");
				break;
			case HTTP_FINISHED: wcscpy(text, GetString(STR_HTTP_FINISHED)); break;
			}
			DrawBusyBox(m_pFont, text);
			return S_OK;
		}
		if (m_pFeed == NULL)
		{
			if (m_pSession->m_nStatus != 200)
			{
				StrFormat(message, "Unable to retrieve file: HTTP STATUS %d", m_pSession->m_nStatus);
				ShowMessage(message.c_str(), GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
				if (m_pSession != NULL)
				{
					delete m_pSession;
					m_pSession = NULL;
				}
				m_bOnline = FALSE;
				return S_OK;
			}
			// Read the feed: <Channel><Item><Title/Author/Link/Thumb>.
			m_pFeed = new CXmlDocument;
			if (m_pFeed == NULL)
			{
				m_bOnline = FALSE;
				ShowMessage("Unexpected Error", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
				return E_FAIL;
			}
			BOOL loaded = !m_pSession->m_bSavedToFile ? m_pFeed->LoadFromString(m_pSession->GetData())
			                                          : m_pFeed->Load(m_pSession->m_szFile);
			if (!loaded)
			{
				m_pFeed->Clear();
				ShowMessage("Unable to load file. Invalid format", GetString(STR_HTTP_ERROR), MSGBOX_BUTTONS_OK);
				if (m_pSession != NULL)
				{
					delete m_pSession;
					m_pSession = NULL;
				}
				if (m_pFeed != NULL)
				{
					delete m_pFeed;
					m_pFeed = NULL;
				}
				m_bOnline = FALSE;
				return S_OK;
			}
			m_online.clear();
			m_pFeed->m_pCurrent = &m_pFeed->m_root;
			CXmlElement *channel = m_pFeed->FindElement(&m_pFeed->m_root, "Channel");
			if (channel != NULL)
			{
				for (CXmlElement *item = channel->FindFirstChild("Item"); item != NULL;
				     item = channel->FindNextChild())
				{
					std::string title, author, thumb, link;
					for (CXmlElement *field = item->GetFirstChild(); field != NULL; field = item->GetNextChild())
					{
						if (StrCmpNoCase(field->m_strName, "Title") == 0)
						{
							title = field->m_strValue;
						}
						else if (StrCmpNoCase(field->m_strName, "Author") == 0)
						{
							author = field->m_strValue;
						}
						else if (StrCmpNoCase(field->m_strName, "Link") == 0)
						{
							link = field->m_strValue;
						}
						else if (StrCmpNoCase(field->m_strName, "Thumb") == 0)
						{
							// Only .jpg previews.
							thumb = field->m_strValue;
							if (thumb.size() != 0 && StrCmpNoCase(StrRight(thumb, 4).c_str(), ".jpg") != 0)
								thumb.erase();
						}
					}
					if (link.size() == 0 || title.size() == 0 || StrCmpNoCase(StrLeft(link, 5).c_str(), "http:") != 0)
						continue;
					// Skins already installed (by folder name) aren't offered.
					BOOL installed = FALSE;
					for (std::vector<SFileItem>::iterator it = m_skins.begin(); it != m_skins.end(); ++it)
					{
						if (StrCmpNoCase(title, it->name) == 0)
						{
							installed = TRUE;
							break;
						}
					}
					if (!installed)
						m_online.push_back(SOnlineSkin(title.c_str(), author.c_str(), link.c_str(), thumb.c_str()));
				}
				std::sort(m_online.begin(), m_online.end(), OnlineSkinLess);
			}
			m_nTop = 0;
			m_nSelected = 0;
			return S_OK;
		}
	}

	// The list: preview, then the entries with their tick boxes.
	float x = m_fX + 3.0f;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float y = lineHeight * 0.5f + m_pCaptionFont->m_fFontHeight * m_pCaptionFont->m_fScaleFactorY + m_fY + 23.0f;
	float width = m_fWidth - 26.0f;
	float bottom = (m_fHeight + m_fY - m_pSymbolFont->m_fFontHeight * m_pSymbolFont->m_fScaleFactorY) - 20.0f;
	if (m_online.size() != 0)
	{
		if (m_pPreview != NULL)
			DrawTexture(m_pPreview, x, y - lineHeight * 0.5f, width, bottom - y, 1.0f, NULL, 0.0f, NULL);
		bottom -= lineHeight * 0.5f;
		wchar_t text[314];
		int i = m_nTop;
		for (std::vector<SOnlineSkin>::iterator it = m_online.begin() + i; it != m_online.end(); ++it, i++)
		{
			if (it->author[0] == 0)
				swprintf(text, 314, L"%ls", it->title);
			else
				swprintf(text, 314, L"%ls - %ls", it->title, it->author);
			DWORD flags = XBFONT_CENTER_Y | XBFONT_TRUNCATED;
			if (i == m_nSelected)
			{
				DrawGradientRect(x - 1.0f, (y - 1.0f) - lineHeight * 0.5f, width, lineHeight + 3.0f,
				                 g_colorScheme.OPTION_ACTIVEBACKCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, 0);
				flags = XBFONT_CENTER_Y;
			}
			m_pFont->DrawText(m_fCheckSize + x + 4.0f, y,
			                  it->selected ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.OPTION_TEXTCOLOR,
			                  text, flags, width);
			DrawTexture(it->selected ? m_pChecked : m_pUnchecked, x + 1.0f, y - m_fCheckSize * 0.5f, m_fCheckSize,
			            m_fCheckSize, 1.0f, NULL, 0.0f, NULL);
			y = lineHeight + y + 3.0f;
			if (bottom < y)
				break;
		}
		bottom += lineHeight * 0.5f;
	}

	// The legend.
	if (m_pSymbolFont != NULL)
	{
		float legendX = m_fX + 10.0f;
		float legendY = (m_pSymbolFont->m_fFontHeight * m_pSymbolFont->m_fScaleFactorY - 5.0f) + bottom;
		const wchar_t *download = m_bDownloading ? L"Cancel" : L"Download";
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"C", XBFONT_CENTER_Y, 0.0f);
		legendX += m_pSymbolFont->GetTextWidth(L"C");
		m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"Toggle", XBFONT_CENTER_Y, 0.0f);
		legendX += m_pFont->GetTextWidth(L"Toggle") + 25.0f;
		const wchar_t *button = m_bDownloading ? L"B" : L"G";
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, button, XBFONT_CENTER_Y, 0.0f);
		legendX += m_pSymbolFont->GetTextWidth(button);
		m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, download, XBFONT_CENTER_Y, 0.0f);
		legendX += m_pFont->GetTextWidth(download) + 25.0f;
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"D", XBFONT_CENTER_Y, 0.0f);
		legendX += m_pSymbolFont->GetTextWidth(L"D");
		m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"skin selector", XBFONT_CENTER_Y, 0.0f);
		legendX += m_pFont->GetTextWidth(L"skin selector") + 25.0f;
		m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"J", XBFONT_CENTER_Y, 0.0f);
		legendX += m_pSymbolFont->GetTextWidth(L"J");
		m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"Preview", XBFONT_CENTER_Y, 0.0f);
	}

	if (!m_bDownloading)
		return S_OK;
	if (m_bCancel)
	{
		m_bPreviewOnly = FALSE;
		m_bDownloading = FALSE;
		m_pSession->Cancel();
		s_bPreviewNext = FALSE;
		return S_OK;
	}

	// The first ticked skin.
	if (!m_bPreviewOnly && m_nDownloadIndex == -1)
	{
		m_nDownloaded = 0;
		s_dwDownloadedBytes = 0;
		int i = 0;
		for (std::vector<SOnlineSkin>::iterator it = m_online.begin(); it != m_online.end(); ++it, i++)
		{
			if (it->selected && it->link.size() != 0)
			{
				m_nDownloadIndex = i;
				m_bInstalling = FALSE;
				m_pSession->Get(it->link.c_str(), FALSE);
				s_bPreviewNext = FALSE;
				break;
			}
		}
		if (m_nDownloadIndex == -1)
		{
			m_bDownloading = FALSE;
			ShowMessage("Please select at least 1 item to download", GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			return S_OK;
		}
	}

	SOnlineSkin &skin = m_online[m_nDownloadIndex];
	if (!m_pSession->IsDone())
	{
		CInternetSession::DrawProgress(m_pSession, m_pServerLogo, m_pFont);
		return S_OK;
	}
	if (m_bInstalling)
	{
		// Waiting for the install thread.
		DrawBusyBox(m_pFont, L"Installing. Please Wait");
		if (WaitForSingleObject(s_hInstallThread, 5) != WAIT_OBJECT_0)
			return S_OK;
		DWORD code;
		GetExitCodeThread(s_hInstallThread, &code);
		if (code != 0)
		{
			ShowMessage("Unexpected error occured while installing Skin.\nFile may be corrupt or damaged.",
			            GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			s_bPreviewNext = FALSE;
			if (m_nDownloaded != 0)
				m_nDownloaded--;
		}
		CloseHandle(s_hInstallThread);
		s_hInstallThread = INVALID_HANDLE_VALUE;
		m_bInstalling = FALSE;
		return S_OK;
	}
	if (m_bMessageShown)
		return S_OK;

	if (m_pSession->m_nStatus != 200)
	{
		StrFormat(message, "An error occured while trying to download '%s'.\nError Code: HTTP STATUS %d",
		          m_pSession->m_szFileName, m_pSession->m_nStatus);
		ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		m_bDownloading = FALSE;
		return S_OK;
	}

	if (m_bPreviewOnly)
	{
		// BLACK's preview.
		if (m_pSession->m_bSavedToFile)
		{
			ReleaseTexture(m_pPreview);
			if (!LoadTextureFromFile(m_pSession->GetCachePath(), g_pd3dDevice, &m_pPreview, 320, 240, NULL))
				m_pPreview = NULL;
			else
				m_online[m_nSelected].localThumb = m_pSession->GetCachePath();
		}
		m_bPreviewOnly = FALSE;
		m_bDownloading = FALSE;
		return S_OK;
	}

	if (m_pSession->m_bSavedToFile)
	{
		// <skins>\<title>\<TitleWithoutSpaces>.zip, then (next time round)
		// its preview.jpg.
		std::string path = m_strSkinPath;
		AppendNarrow(path, skin.title);
		if (!s_bPreviewNext)
		{
			if (CreateDirectoryTree(path.c_str()))
			{
				m_nDownloaded++;
				s_dwDownloadedBytes += m_pSession->m_dwDataSize;
				path.append("\\");
				std::string name = StrFromWide(skin.title);
				StrRemoveChar(name, ' ');
				path.append(name);
				path.append(".zip");
				MoveFileExA(m_pSession->m_szFile, path.c_str(), 3);   // REPLACE_EXISTING | COPY_ALLOWED
				if (skin.thumb.c_str() != NULL && skin.thumb.size() != 0)
				{
					s_bPreviewNext = TRUE;
					m_pSession->Get(skin.thumb.c_str(), FALSE);
				}
				// (The path's buffer goes to the thread; it's copied there
				// after a moment.)
				s_hInstallThread = (HANDLE)_beginthreadex(NULL, 0, InstallSkinThread, (void *)path.c_str(), 0, NULL);
				if (s_hInstallThread != NULL && s_hInstallThread != INVALID_HANDLE_VALUE)
				{
					m_bInstalling = TRUE;
					Sleep(5);
				}
			}
		}
		else
		{
			s_bPreviewNext = FALSE;
			if (CreateDirectoryTree(path.c_str()))
			{
				path.append("\\");
				path.append("preview.jpg");
				MoveFileExA(m_pSession->m_szFile, path.c_str(), 3);
			}
		}
	}
	if (s_bPreviewNext)
		return S_OK;

	// The next ticked skin, or the summary.
	int previous = m_nDownloadIndex;
	int i = previous;
	for (std::vector<SOnlineSkin>::iterator it = m_online.begin() + previous; it != m_online.end(); ++it, i++)
	{
		if (previous < i && it->selected && it->link.size() != 0)
		{
			m_nDownloadIndex = i;
			m_pSession->Get(it->link.c_str(), FALSE);
			break;
		}
	}
	if (previous != m_nDownloadIndex && m_nDownloaded < 10)
		return S_OK;
	if (s_dwDownloadedBytes == 0)
		StrFormat(message, "Downloaded %d Skins", m_nDownloaded);
	else
		StrFormat(message, "Downloaded %d Skins totalling %.02fKB", m_nDownloaded,
		          (double)((float)s_dwDownloadedBytes * (1.0f / 1024.0f)));
	if (m_nDownloaded < 10)
	{
		ShowMessage(message.c_str(), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
	}
	else
	{
		message.append("\nPlease keep your download to a maximum of 10 skins");
		ShowMessage(message.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
	}
	m_bDownloading = FALSE;
	return S_OK;
}

// 0x000F8685 (this in esi)
HRESULT CSkinBrowser::Render()
{
	if (m_pFont == NULL)
		return S_OK;
	DimScreen(0.3f);
	m_pFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	m_pCaptionFont->SetScaleFactors(m_pSettings->m_fFontScale, m_pSettings->m_fFontScale);
	float symbolHeight = m_pSymbolFont->m_fFontHeight * m_pSymbolFont->m_fScaleFactorY;
	float captionHeight = m_pCaptionFont->m_fFontHeight * m_pCaptionFont->m_fScaleFactorY;
	float bottom = (m_fY + m_fHeight - symbolHeight) - 20.0f;
	DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, 10.0f, captionHeight, symbolHeight + 10.0f,
	                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 2);
	DrawGradientRect(m_fX + 10.0f, m_fY + 5.0f, m_fWidth * 0.6f - 20.0f, captionHeight + 10.0f,
	                 g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE, 0);
	m_pCaptionFont->DrawText(m_fX + 13.0f, captionHeight * 0.5f + m_fY + 10.0f, g_colorScheme.CAPTION_TEXTCOLOR,
	                         m_bOnline ? L"Download Skin" : L"Select Skin", XBFONT_CENTER_Y | XBFONT_TRUNCATED,
	                         m_fWidth * 0.75f - 20.0f - 3.0f);

	if (!m_bListed)
	{
		// Still listing: "Please Wait" after a quarter of a second.
		if (GetTickCount() - m_dwListTick < 251)
			return S_OK;
		DrawBusyBox(m_pFont, L"Please Wait");
		return S_OK;
	}

	if (!m_bOnline)
	{
		if (m_skins.size() == 0)
			return E_FAIL;
		float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
		if (m_nVisible == 0)
		{
			m_nVisible = (int)((m_fHeight - (lineHeight + captionHeight + 60.0f)) / (lineHeight + 3.0f));
			m_nTop = 0;
			m_nBottom = m_nVisible - 1;
		}
		float x = m_fX + 13.0f;
		float y = captionHeight + m_fY + 23.0f;
		float width = m_fWidth - 26.0f;
		if (m_pPreview != NULL)
			DrawTexture(m_pPreview, x, y, width, bottom - y, 1.0f, NULL, 0.0f, NULL);
		bottom -= lineHeight;
		int i = m_nTop;
		for (std::vector<SFileItem>::iterator it = m_skins.begin() + i; it != m_skins.end(); ++it, i++)
		{
			if (m_nBottom < i)
				break;
			DWORD color;
			if (i == m_nSelected)
			{
				DrawGradientRect(x - 1.0f, y - 1.0f, width, lineHeight + 3.0f, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
				                 g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
				color = g_colorScheme.OPTION_ACTIVETEXTCOLOR;
			}
			else
			{
				color = g_colorScheme.DIALOG_TEXTCOLOR;
			}
			m_pFont->DrawText(x, y, color, it->label.c_str(), XBFONT_TRUNCATED, width);
			y = lineHeight + y + 3.0f;
			if (bottom < y)
			{
				int shown = i - m_nTop;
				m_nBottom = i;
				m_nVisible = shown + 1;
				if (i < m_nSelected)
				{
					m_nBottom = m_nSelected;
					m_nTop = m_nSelected - m_nVisible + 1;
					if (m_nTop < 0)
					{
						m_nTop = 0;
						m_nBottom = shown;
					}
				}
				break;
			}
		}
		if (m_nSelected == -1)
			m_nSelected = 0;

		// The legend: A Select, START/AA Save, D Download skins.
		if (m_pSymbolFont != NULL)
		{
			float legendX = m_fX + 10.0f;
			float legendY = (symbolHeight - 5.0f) + lineHeight + bottom;
			m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"A", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pSymbolFont->GetTextWidth(L"A");
			m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"Select", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pFont->GetTextWidth(L"Select") + 30.0f;
			m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"G", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pSymbolFont->GetTextWidth(L"G");
			m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"/", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pFont->GetTextWidth(L"/");
			m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"AA", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pSymbolFont->GetTextWidth(L"AA");
			m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L" Save", XBFONT_CENTER_Y, 0.0f);
			// (measured with a different text)
			legendX += m_pFont->GetTextWidth(L"(2X) Save") + 30.0f;
			m_pSymbolFont->DrawText(legendX, legendY, 0xFFFFFFFF, L"D", XBFONT_CENTER_Y, 0.0f);
			legendX += m_pSymbolFont->GetTextWidth(L"D");
			m_pFont->DrawText(legendX, legendY, g_colorScheme.DIALOG_TEXTCOLOR, L"Download skins", XBFONT_CENTER_Y,
			                  0.0f);
		}
		if (g_skin.m_bLoading)
		{
			DrawBusyBox(m_pFont, L"Please Wait");
			return S_OK;
		}
	}
	else
	{
		RenderOnline();
	}

	// The pop-ups.
	if (m_bKeyboardOpen)
		g_virtualKeyboard.Render();
	else if (m_bMessageShown && m_pMessageBox != NULL)
		m_pMessageBox->Render();
	else if (m_bServerMenuOpen)
		m_pServerMenu->Render();
	return S_OK;
}
