// CGameManager, the game saves manager: the game and save lists, the scan,
// and the input (0x00119C3F-0x0011F187). See GameManager.h; the drawing
// is in GameManagerRender.cpp.
#include "GameManager.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../net/Http.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/ImageManager.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../system/Drives.h"
#include "../system/FileOps.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"

// The rows of a game's save grid: four saves a row, one to three rows.
static int SaveRows(const SGameEntry *game)
{
	int rows = (int)ceil((float)(UINT)game->saves.size() * 0.25f);
	if (rows > 3)
		return 3;
	return rows < 1 ? 1 : rows;
}

// The text after the '=' of a "Key=Value" line, up to the line's end.
static std::string MetaValue(std::string text)
{
	int equals = StrFind(text, "=", 0);
	if (equals != -1)
		text = StrMid(text, equals + 1, (int)text.size() - (equals + 1));
	int end = StrFind(text, "\r", 0);
	if (end != -1)
		text = StrLeft(text, end);
	return text;
}

// ---- CGameSaveItem ---------------------------------------------------------

// 0x0011D4B0
CGameSaveItem::CGameSaveItem(const XGAME_FIND_DATA &data, const char *gamePath)
{
	m_pszPath = NULL;
	m_pszTitle = NULL;
	m_pImage = NULL;
	m_pszPath = new char[0x104];
	memset(m_pszPath, 0, 0x104);
	if (gamePath != NULL)
	{
		strcpy(m_pszPath, gamePath);
		strcat(m_pszPath, data.wfd.cFileName);
	}
	else
	{
		strcpy(m_pszPath, data.szSaveGameDirectory);
	}
	StrUtf8ToWide(data.wfd.cFileName, m_szFolder, 20);
	m_pszTitle = new wchar_t[0x80];
	memset(m_pszTitle, 0, 0x80 * sizeof(wchar_t));
	swprintf(m_pszTitle, 0x80, L"\"%s\"", data.szSaveGameName);
	wchar_t date[12];
	wchar_t time[12];
	memset(date, 0, sizeof(date));
	memset(time, 0, sizeof(time));
	FormatFileTime(&data.wfd.ftLastWriteTime, date, time, FALSE, CAppSettings::GetInstance()->m_prefs.m_bUse24HFormat);
	swprintf(m_szDate, 20, L"%s %s", date, time);
	m_unknownC0 = 0;
	m_pImage = NULL;
	m_bImageTried = FALSE;
	m_fSizeKB = 0.0f;
	m_dwBlocks = 0;
	m_bContent = FALSE;
	m_unknownD8 = 0;
	ComputeSize();
}

// 0x0011D5DF
CGameSaveItem::CGameSaveItem(const XCONTENT_FIND_DATA &data, const char *gamePath)
{
	m_pszPath = NULL;
	m_pszTitle = NULL;
	m_pImage = NULL;
	// The content as a save: its folder data, and (quirk) only the first
	// character of its directory and of its name.
	XGAME_FIND_DATA save;
	memset(&save, 0, sizeof(save));   // (the original leaves the rest unset)
	save.wfd = data.wfd;
	save.szSaveGameDirectory[0] = data.szContentDirectory[0];
	save.szSaveGameName[0] = data.szDisplayName[0];
	// Quirk: this constructs a temporary, not this item.
	CGameSaveItem temporary(save, gamePath);
	// (The original leaves the rest of this item unset.)
	memset(m_szDate, 0, sizeof(m_szDate));
	memset(m_szFolder, 0, sizeof(m_szFolder));
	memset(m_szSize, 0, sizeof(m_szSize));
	memset(m_szBlocks, 0, sizeof(m_szBlocks));
	m_unknownC0 = 0;
	m_bImageTried = FALSE;
	m_fSizeKB = 0.0f;
	m_dwBlocks = 0;
	m_bContent = FALSE;
	m_unknownD8 = 0;
}

// 0x0011BF60
CGameSaveItem::~CGameSaveItem()
{
	std::string image = m_pszPath != NULL ? m_pszPath : "";
	StrAddBackslash(image);
	image.append("saveimage.xbx");
	g_pImageManager->Remove(image.c_str());
	if (m_pImage != NULL)
	{
		m_pImage->Release();
		m_pImage = NULL;
	}
	delete[] m_pszPath;
	m_pszPath = NULL;
	delete[] m_pszTitle;
	m_pszTitle = NULL;
}

// 0x0011D34F
void CGameSaveItem::ComputeSize()
{
	memset(m_szSize, 0, sizeof(m_szSize));
	memset(m_szBlocks, 0, sizeof(m_szBlocks));
	SDriveInfo info;
	memset(&info, 0, sizeof(info));
	MapFolderToDrive(&info, 'V', m_pszPath);
	if (!info.mounted)
	{
		// Count the bytes: a block is 16 KB.
		g_nOperationFolders = 0;
		g_nOperationFiles = 0;
		g_ullOperationTotal = 0;
		g_ullOperationBytes = 0;
		g_ullOperationDone = 0;
		g_bFileOperationPaused = FALSE;
		g_bCancelOperation = FALSE;
		CountFolder(m_pszPath);
		double bytes = (double)(LONGLONG)g_ullOperationBytes;
		m_fSizeKB = (float)(bytes * (1.0 / 1024.0));
		m_dwBlocks = (DWORD)ceil(bytes * (1.0 / 16384.0));
	}
	else
	{
		m_dwBlocks = XGetDisplayBlocks("V:\\");
		m_fSizeKB = (float)(UINT)(m_dwBlocks << 4);
	}
	FormatDoubleW(m_szSize, 2, TRUE, m_fSizeKB);
	FormatIntW(m_szBlocks, m_dwBlocks);
	wcscat(m_szSize, L" KB");
	wcscat(m_szBlocks, L" Blocks");
}

// 0x0011C3B0
BOOL CGameSaveItem::LoadImage()
{
	if (m_pImage == NULL && !m_bImageTried)
	{
		std::string path = m_pszPath;
		StrAddBackslash(path);
		path.append("saveimage.xbx");
		if (!FileExists(path.c_str()))
		{
			// The game's (the folder above).
			path = m_pszPath;
			int slash = StrReverseFind(path, "\\", -1);
			if (slash != -1)
			{
				path = StrLeft(path, slash);
				path.append("\\");
				path.append("saveimage.xbx");
			}
		}
		StrReplaceAll(path, "\\\\", "\\");
		if (FileExists(path.c_str()))
			m_pImage = g_pImageManager->Load(path.c_str(), 48, 48);
		m_bImageTried = TRUE;
	}
	return m_pImage != NULL;
}

// ---- SGameEntry ------------------------------------------------------------

// 0x0011BA03
SGameEntry::SGameEntry(const char *gamePath, const wchar_t *gameName, BOOL memoryUnit, const char *id)
{
	image = NULL;
	strcpy(titleId, id);
	strcpy(path, gamePath);
	wcscpy(name, gameName);
	StrUtf8ToWide(id, folderName, 20);
	titleNumber = 0;
	saveCount = 0;
	contentCount = 0;
	image = NULL;
	imagesLoaded = FALSE;
	onMemoryUnit = memoryUnit;
	// (Not set by the original.)
	sizeKB = 0.0f;
	blocks = 0;
	unknown1C = 0;
	memset(sizeText, 0, sizeof(sizeText));
	memset(blocksText, 0, sizeof(blocksText));
	memset(countText, 0, sizeof(countText));
}

// 0x00119C82
SGameEntry::~SGameEntry()
{
	FreeSaves();
}

// 0x00119C3F
void SGameEntry::FreeSaves()
{
	if (saves.size() != 0)
	{
		for (std::vector<CGameSaveItem *>::iterator it = saves.begin(); it != saves.end(); ++it)
			delete *it;
		saves.clear();
	}
	if (image != NULL)
	{
		image->Release();
		image = NULL;
	}
}

// 0x0011D64D
void SGameEntry::LoadSaves()
{
	FreeSaves();
	UINT savesFound = 0;
	UINT contentFound = 0;
	blocks = 0;
	sizeKB = 0.0f;
	if (!onMemoryUnit)
	{
		// The saves (XFindFirstSaveGame on the game's folder as M:).
		SDriveInfo info;
		memset(&info, 0, sizeof(info));
		if (DirectoryExists(path) && (MapFolderToDrive(&info, 'M', path), info.mounted))
		{
			XGAME_FIND_DATA data;
			HANDLE find = XFindFirstSaveGame("M:\\", &data);
			if (find != INVALID_HANDLE_VALUE)
			{
				do
				{
					CGameSaveItem *save = new CGameSaveItem(data, path);
					if (save != NULL)
					{
						saves.push_back(save);
						savesFound++;
						sizeKB = save->m_fSizeKB + sizeKB;
					}
				} while (XFindNextSaveGame(find, &data));
				XFindClose(find);
			}
			blocks = XGetDisplayBlocks("M:\\");
			if (blocks != 0)
				sizeKB = (float)(UINT)(blocks << 4);
		}
		// The downloaded content (the same folder under TDATA, as T:).
		std::string content = path;
		StrReplaceAll(content, "UDATA", "TDATA");
		if (DirectoryExists(content.c_str()))
		{
			MapFolderToDrive(&info, 'T', content.c_str());
			if (info.mounted)
			{
				XCONTENT_FIND_DATA data;
				HANDLE find = XFindFirstContent("T:\\", 0, &data);
				if (find != INVALID_HANDLE_VALUE)
				{
					do
					{
						CGameSaveItem *item = new CGameSaveItem(data, path);
						if (item != NULL)
						{
							item->m_bContent = TRUE;
							saves.push_back(item);
							contentFound++;
							sizeKB = item->m_fSizeKB + sizeKB;
						}
					} while (XFindNextContent(find, &data));
					XFindClose(find);
				}
				blocks += XGetDisplayBlocks("T:\\");
				if (blocks != 0)
					sizeKB = (float)(UINT)(blocks << 4);
			}
		}
		FormatDoubleW(sizeText, 2, TRUE, sizeKB);
		FormatIntW(blocksText, blocks);
		wcscat(sizeText, L" KB");
		wcscat(blocksText, L" Blocks");
		swprintf(countText, 44, L"%u Saves, %u Contents", savesFound, contentFound);
	}
	else
	{
		// A memory unit: each save folder's SaveMeta.xbx name is read, but
		// (quirk) no save is added, so the game shows none.
		std::string folder = path;
		if (StrRight(folder, 1).compare("\\") != 0)
			folder.append("\\");
		std::string pattern = folder;
		pattern.append("*");
		WIN32_FIND_DATAA found;
		HANDLE find = FindFirstFileA(pattern.c_str(), &found);
		if (find != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					std::string meta = folder;
					meta.append(found.cFileName);
					meta.append("\\SaveMeta.xbx");
					if (FileExists(meta.c_str()))
					{
						std::string saveName = ReadMetaFile(meta.c_str());
						if (saveName.size() != 0)
						{
							saveName = MetaValue(saveName);
							wchar_t title[0x80];
							StrUtf8ToWide(saveName.c_str(), title, 0x80);
							(void)title;
						}
					}
				}
			} while (FindNextFileA(find, &found));
			CloseHandle(find);
		}
		FormatDoubleW(sizeText, 2, TRUE, sizeKB);
		FormatIntW(blocksText, blocks);
		FormatIntW(countText, (int)saves.size());
		wcscat(sizeText, L" KB");
		wcscat(blocksText, L" Blocks");
		wcscat(countText, L" Saves");
	}
}

// 0x0011C792
BOOL SGameEntry::LoadImages()
{
	if (!imagesLoaded)
	{
		if (image == NULL)
		{
			std::string file = path;
			file.append("\\TitleImage.xbx");
			StrReplaceAll(file, "\\\\", "\\");
			if (FileExists(file.c_str()))
				image = g_pImageManager->Load(file.c_str(), 96, 96);
		}
		// A save without an image of its own shows the game's.
		for (std::vector<CGameSaveItem *>::iterator it = saves.begin(); it != saves.end(); ++it)
		{
			CGameSaveItem *save = *it;
			if (save == NULL)
				continue;
			save->LoadImage();
			if (save->m_pImage == NULL && image != NULL)
			{
				image->AddRef();
				save->m_pImage = image;
			}
		}
		imagesLoaded = TRUE;
	}
	return image != NULL;
}

// ---- CGameManager ----------------------------------------------------------

// 0x0011C649
CGameManager::CGameManager()
{
	m_fX = m_fY = m_fWidth = m_fHeight = 0.0f;
	m_pTitleFont = NULL;
	m_pFont = NULL;
	m_pSymbolFont = NULL;
	m_pSound48 = NULL;
	m_pSoundSelect = NULL;
	m_pSoundBack = NULL;
	m_pSoundGame = NULL;
	m_pSoundSlot = NULL;
	m_pDefaultIcon = NULL;
	m_pEnvMap = NULL;
	m_pMessageBox = NULL;
	m_bMessageShown = FALSE;
	m_bLoaded = FALSE;
	m_bOpen = FALSE;
	m_bInSaves = FALSE;
	m_nGame = 0;
	m_nSave = 0;
	m_nSaveColumn = 0;
	m_nSaveRow = 0;
	m_nSaveRows = 0;
	m_pCopyMenu = NULL;
	m_pDownloadMenu = NULL;
	m_pSession = NULL;
	m_bDownloadMode = FALSE;
	m_unknown178 = 0;
	m_bDownloading = FALSE;
	m_unknown174 = 0;
	m_bMenuShown = FALSE;
	m_bDownloadMenuShown = FALSE;
	m_bCopyMenuShown = FALSE;
	m_bDeletePending = FALSE;
	m_bWaitForCopy = FALSE;
	m_pTexture68 = NULL;
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	if (g_pImageManager == NULL)
		g_pImageManager = new CImageManager;
	m_pSettings = CAppSettings::GetInstance();
	// (Not set by the original.)
	m_pSoundDone = NULL;
	m_unknown1BC = 0;
	memset(m_world, 0, sizeof(m_world));
	memset(m_unknown204, 0, sizeof(m_unknown204));
}

// 0x0011BC38
CGameManager::~CGameManager()
{
	m_pFont = NULL;
	m_pSymbolFont = NULL;
	m_pTitleFont = NULL;
	if (m_pSound48 != NULL)
	{
		delete m_pSound48;
		m_pSound48 = NULL;
	}
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (m_pDefaultIcon != NULL)
	{
		m_pDefaultIcon->Release();
		m_pDefaultIcon = NULL;
	}
	if (m_pEnvMap != NULL)
	{
		m_pEnvMap->Release();
		m_pEnvMap = NULL;
	}
	if (m_pTexture68 != NULL)
	{
		m_pTexture68->Release();
		m_pTexture68 = NULL;
	}
	// (The shared sounds are only forgotten.)
	m_pSoundSelect = NULL;
	m_pSoundBack = NULL;
	m_pSoundGame = NULL;
	m_pSoundSlot = NULL;
	m_pSoundDone = NULL;
	if (m_pCopyMenu != NULL)
	{
		delete m_pCopyMenu;
		m_pCopyMenu = NULL;
	}
	if (m_pDownloadMenu != NULL)
	{
		delete m_pDownloadMenu;
		m_pDownloadMenu = NULL;
	}
	if (m_pSession != NULL)
	{
		delete m_pSession;
		m_pSession = NULL;
	}
	if (m_games.size() != 0)
	{
		for (std::vector<SGameEntry *>::iterator it = m_games.begin(); it != m_games.end(); ++it)
			delete *it;
		m_games.clear();
	}
	if (g_pImageManager != NULL)
	{
		delete g_pImageManager;
		g_pImageManager = NULL;
	}
}

// 0x0011E048
HRESULT CGameManager::Init(CXBFont *font, IDirect3DTexture8 *defaultIcon, IDirect3DTexture8 *envMap)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont == NULL)
	{
		m_pSettings = CAppSettings::GetInstance();
		m_pFont = font;
		if (defaultIcon != NULL)
		{
			defaultIcon->AddRef();
			m_pDefaultIcon = defaultIcon;
		}
		if (envMap != NULL)
		{
			envMap->AddRef();
			m_pEnvMap = envMap;
		}
		m_pSoundSelect = g_soundEffects.Find("Select");
		m_pSoundBack = g_soundEffects.Find("Back");
		m_pSoundDone = g_soundEffects.Find("Action Complete");
		g_soundEffects.Load("Game Save Select", m_pSettings->m_audio.m_strSound[SOUND_GAME_SAVE_SELECT].c_str(),
		                    &m_pSoundGame);
		g_soundEffects.Load("Memory Slot", m_pSettings->m_audio.m_strSound[SOUND_MEMORY_SLOT].c_str(), &m_pSoundSlot);
		Reload();
		m_menu.Create(font, L"Select", 0.0f, 0.0f, 0.0f, 0.0f, 10);
		m_menu.AddOption(L"Copy", 0);
		m_menu.AddOption(L"Delete", 0);
		g_fonts.Find("XboxDings", &m_pSymbolFont);
		m_pTitleFont = m_pSettings->m_pDialogFont;
		// A world transform: 45 back, tilted a quarter radian.
		memset(m_world, 0, sizeof(m_world));
		m_world[5] = m_world[10] = m_world[15] = 1.0f;
		BuildWorldMatrix(m_world, 0.0f, 0.0f, 45.0f, 1.0f, 1.0f, 1.0f, 0.25f, 0.0f, 0.0f);
	}
	Show();
	m_bOpen = TRUE;
	return S_OK;
}

// 0x0011BAFA
void CGameManager::Show()
{
	m_bOpen = TRUE;
	if (m_pDownloadMenu != NULL)
		m_pDownloadMenu->Show(0);
	if (m_pSession != NULL)
		m_pSession->Cancel();
	// 480x440 at (80, 20) on 640x480. Quirk: the height scales by the
	// raw factor, the width by the factor clamped to at least 1.
	m_fX = 80.0f;
	m_fWidth = 480.0f;
	m_fY = 20.0f;
	m_fHeight = 440.0f;
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	ScaleToScreen(&m_fX, &m_fScaleX, &m_fY, &m_fScaleY, TRUE);
	m_fHeight = m_fScaleY * m_fHeight;
	if (m_fScaleX < 1.0f)
		m_fScaleX = 1.0f;
	if (m_fScaleY < 1.0f)
		m_fScaleY = 1.0f;
	m_fWidth = m_fScaleX * m_fWidth;
	CSound *sounds[4] = {m_pSoundSelect, m_pSoundBack, m_pSoundGame, m_pSoundSlot};
	for (int i = 0; i < 4; i++)
	{
		if (sounds[i] != NULL && sounds[i]->m_pBuffer != NULL)
			sounds[i]->m_pBuffer->SetVolume(m_pSettings->GetSoundVolume());
	}
}

// 0x0011C52C
void CGameManager::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL)
		{
			if (FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
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
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// Sorts the hard disk's games by name (case-insensitive). (Quirk: a game
// on a memory unit is "less" than anything; there are none yet when it's
// used.) 0x00020C2D
static bool GameLess(const SGameEntry *a, const SGameEntry *b)
{
	if (a->onMemoryUnit)
		return true;
	return _wcsicmp(a->name, b->name) < 0;
}

// 0x0011DB91
void CGameManager::ScanGames()
{
	if (m_bLoaded)
		return;
	m_games.clear();
	// First E:\UDATA, then the memory units H: to O: (slots 1-8).
	int first = 0;
	int last = 1;
	for (;;)
	{
		for (int slot = first; slot < last; slot++)
		{
			BOOL memoryUnit = FALSE;
			WIN32_FIND_DATAA found;
			HANDLE find;
			if (slot == 0)
			{
				find = FindFirstFileA("E:\\UDATA\\*", &found);
			}
			else
			{
				char letter = (char)(slot + 'G');
				SDriveInfo *drive = GetDriveInfo(letter);
				if (drive == NULL || !drive->mounted)
					continue;
				std::string pattern;
				StrFormat(pattern, "%c:\\*", letter);
				find = FindFirstFileA(pattern.c_str(), &found);
				memoryUnit = TRUE;
			}
			UnmountDrive('M');
			if (find != INVALID_HANDLE_VALUE)
			{
				do
				{
					if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						continue;
					// A game folder: TitleMeta.xbx has "TitleName=...".
					std::string folder = "E:\\UDATA\\";
					if (memoryUnit)
						StrFormat(folder, "%c:\\", slot + 'G');
					folder.append(found.cFileName);
					folder.append("\\TitleMeta.xbx");
					std::string meta = ReadMetaFile(folder.c_str());
					if (meta.size() != 0)
					{
						if (!memoryUnit)
							StrFormat(folder, "E:\\UDATA\\%s\\", found.cFileName);
						else
							StrFormat(folder, "%c:\\%s\\", slot + 'G', found.cFileName);
						meta = MetaValue(meta);
						wchar_t title[0x80];
						StrUtf8ToWide(meta.c_str(), title, 0x80);
						SGameEntry *game = new SGameEntry(folder.c_str(), title, slot != 0, found.cFileName);
						if (game != NULL)
						{
							game->titleNumber = atol(found.cFileName);
							game->LoadSaves();
							m_games.push_back(game);
							Sleep(5);
						}
					}
				} while (FindNextFileA(find, &found));
				CloseHandle(find);
				MountDrive('M');
			}
			Sleep(5);
			MountDrive('M');
		}
		if (first != 0)
			break;
		// The hard disk's games in order of name, then the memory units.
		first = 1;
		last = 9;
		if (m_games.size() != 0)
			std::sort(m_games.begin(), m_games.end(), GameLess);
	}

	// The first six games' images now (the rest when drawn).
	m_nSaveRows = 0;
	if (m_games.size() != 0)
	{
		Sleep(100);
		int index = 0;
		for (std::vector<SGameEntry *>::iterator it = m_games.begin(); it != m_games.end(); ++it, ++index)
		{
			if (*it == NULL)
				continue;
			(*it)->LoadImages();
			if (index >= 5)
				break;
		}
		if (m_games[0] != NULL)
			m_nSaveRows = (int)ceil((float)(UINT)m_games[0]->saves.size() * 0.25f);
	}
	if (m_nGame > 0 && m_nGame > (int)m_games.size() - 1)
		m_nGame = (int)m_games.size() - 1 < 0 ? 0 : (int)m_games.size() - 1;
	m_bMessageShown = FALSE;
	m_bLoaded = TRUE;
}

// 0x0011DFC4
unsigned __stdcall CGameManager::ScanThread(void *param)
{
	if (param != NULL)
		((CGameManager *)param)->ScanGames();
	return param == NULL;
}

// 0x0011DFDD
void CGameManager::Reload()
{
	m_bLoaded = FALSE;
	m_games.clear();   // (the games aren't freed)
	ShowMessage(GetStringA(STR_MSG_GAMESAVES_SEARCH), GetString(STR_INFORMATION), MSGBOX_BUTTONS_OK);
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0xA0000, ScanThread, this, CREATE_SUSPENDED, NULL);
	if (thread == NULL || thread == INVALID_HANDLE_VALUE)
	{
		ScanGames();
	}
	else
	{
		ResumeThread(thread);
		Sleep(5);
		CloseHandle(thread);
	}
}

// (inlined)
void CGameManager::OpenMenu()
{
	// Copy only works on a save.
	m_menu.SetOptionEnabled(0, m_bInSaves);
	m_menu.Show(0);
	m_bMenuShown = TRUE;
}

// (inlined)
void CGameManager::OpenCopyMenu(SGameEntry *game)
{
	if (m_pCopyMenu == NULL)
	{
		m_pCopyMenu = new COptionMenu;
		if (m_pCopyMenu == NULL)
			return;
		m_pCopyMenu->Create(m_pFont, L"Copy To", 0.0f, 0.0f, 0.0f, 0.0f, 10);
		m_pCopyMenu->AddOption(L"Xbox Hard disk", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-1A", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-1B", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-2A", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-2B", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-3A", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-3B", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-4A", 0);
		m_pCopyMenu->AddOption(L"Memory Unit-4B", 0);
	}
	if (m_pCopyMenu == NULL)
		return;
	m_menu.m_bCancelled = FALSE;
	m_menu.m_bChosen = FALSE;
	m_bMenuShown = TRUE;
	// The hard disk only from a memory unit; the memory units present,
	// but not the one the game is on.
	int destinations = game->onMemoryUnit ? 1 : 0;
	m_pCopyMenu->SetOptionEnabled(0, game->onMemoryUnit);
	for (int slot = 1; slot < 9; slot++)
	{
		SDriveInfo *drive = GetDriveInfo((char)(slot + 'G'));
		BOOL usable = drive != NULL && drive->mounted && drive->letter != game->path[0];
		if (usable)
			destinations++;
		m_pCopyMenu->SetOptionEnabled(slot, usable);
	}
	if (destinations == 0)
	{
		ShowMessage(GetStringA(STR_MSG_GAMESAVES_INSERT_MU), GetString(STR_INFORMATION), MSGBOX_BUTTONS_OK);
		return;
	}
	m_pCopyMenu->Show(0);
	m_bCopyMenuShown = TRUE;
	PlaySound(m_pSoundSelect);
}

// (inlined) destination 0: the hard disk, 1-8: the memory units H: to O:.
void CGameManager::CopySave(SGameEntry *game, CGameSaveItem *save, int destination)
{
	std::string target;
	BOOL toMemoryUnit = TRUE;
	SDriveInfo *drive;
	if (destination == 0)
	{
		toMemoryUnit = FALSE;
		StrFormat(target, "E:\\UDATA\\%s\\", game->titleId);
		PlaySound(m_pSoundSelect);
		drive = GetDriveInfo('E');
	}
	else
	{
		// A memory unit with at least a megabyte free; the save keeps its
		// path without the UDATA/TDATA level.
		drive = GetDriveInfo((char)(destination + 'G'));
		if (drive == NULL || !drive->mounted || drive->freeBytes <= 0x100000)
		{
			ShowMessage(GetStringA(STR_MSG_GAMESAVES_NO_DEST), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
			return;
		}
		StrFormat(target, "%c:\\%s\\", drive->letter, save->m_pszPath + 3);
		StrReplaceAll(target, "UDATA\\", "");
		StrReplaceAll(target, "TDATA\\", "");
		PlaySound(m_pSoundSelect);
	}
	if (target.size() == 0 || drive == NULL)
		return;
	// Quirk: the save's size in KB is compared with the free bytes.
	if ((double)save->m_fSizeKB > (double)(LONGLONG)drive->freeBytes)
	{
		ShowMessage(GetStringA(STR_MSG_GAMESSAVES_BAD_DEST), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return;
	}
	CreateDirectoryTree(target.c_str());
	// The game folder's own files (TitleMeta.xbx, TitleImage.xbx...) first.
	std::string pattern;
	StrFormat(pattern, "%s*", game->path);
	WIN32_FIND_DATAA found;
	HANDLE find = FindFirstFileA(pattern.c_str(), &found);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			std::string destinationFile;
			StrFormat(pattern, "%s%s", game->path, found.cFileName);
			if (toMemoryUnit)
				StrFormat(destinationFile, "%s:\\%s\\%s", StrLeft(target, 1).c_str(), game->titleId, found.cFileName);
			else
				StrFormat(destinationFile, "%s%s", target.c_str(), found.cFileName);
			CopyFileA(pattern.c_str(), destinationFile.c_str(), TRUE);
		} while (FindNextFileA(find, &found));
		CloseHandle(find);
	}
	// Then the save's folder (on the hard disk, by its folder name).
	if (!toMemoryUnit && wcslen(save->m_szFolder) > 0)
		target.append(StrFromWide(save->m_szFolder));
	CopyFiles(save->m_pszPath, target.c_str());
}

// (inlined)
void CGameManager::AskDelete(SGameEntry *game)
{
	std::string text;
	if (!m_bInSaves)
	{
		// Quirk: the save and content counts are never filled in (0).
		std::string name = StrFromWide(game->name);
		StrFormat(text, GetStringA(STR_MSG_GAMESAVES_DELETE), name.c_str(), game->blocks, game->saveCount,
		          game->contentCount);
	}
	else
	{
		CGameSaveItem *save = game->saves[m_nSave];
		if (save != NULL)
		{
			std::string name = StrFromWide(save->m_pszTitle);
			StrFormat(text, GetStringA(STR_MSG_GAMESAVES_DELETE_ITEM), name.c_str(), save->m_dwBlocks);
		}
	}
	if (text.size() != 0)
	{
		ShowMessage(text.c_str(), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
		if (m_pMessageBox != NULL)
			m_pMessageBox->SetSelection(1);
		m_bDeletePending = TRUE;
	}
	PlaySound(m_pSoundSelect);
}

// (inlined) after a Yes to the delete question.
void CGameManager::DeleteSelected()
{
	SGameEntry *game = m_games[m_nGame];
	if (game == NULL)
	{
		PlaySound(m_pSoundSelect);
		return;
	}
	std::string text;
	std::string name;
	SDriveInfo info;
	memset(&info, 0, sizeof(info));
	MapFolderToDrive(&info, 'M', game->path);
	if (!m_bInSaves)
	{
		// The whole game: each save the XAPI way (quirk: by its quoted
		// title, so this fails), then the folder.
		name = StrFromWide(game->name);
		if (info.mounted)
		{
			for (std::vector<CGameSaveItem *>::iterator it = game->saves.begin(); it != game->saves.end(); ++it)
			{
				if (*it != NULL)
					XDeleteSaveGame("M:\\", (*it)->m_pszTitle);
			}
		}
		DeleteFolder(game->path);
	}
	else
	{
		CGameSaveItem *save = game->saves[m_nSave];
		if (save != NULL)
		{
			name = StrFromWide(save->m_pszTitle);
			if (info.mounted)
				XDeleteSaveGame("M:\\", save->m_pszTitle);
			DeleteFolder(save->m_pszPath);
		}
	}
	while (g_bFileOperationQueued)
		Sleep(10);
	MountDrive('M');
	if (g_bOperationFailed)
	{
		StrFormat(text, GetStringA(STR_MSG_GAMESAVES_DELETE_ERROR), name.c_str(), g_dwOperationError);
		ShowMessage(text.c_str(), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
	}
	else
	{
		StrFormat(text, GetStringA(STR_MSG_GAMESAVES_DELETE_SUCCESS), name.c_str());
		ShowMessage(text.c_str(), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
		if (m_bInSaves)
		{
			CGameSaveItem *&save = game->saves[m_nSave];
			if (save->m_pImage != NULL)
			{
				save->m_pImage->Release();
				save->m_pImage = NULL;
			}
			if (save != NULL)
			{
				delete save;
				save = NULL;
			}
			game->saves.erase(game->saves.begin() + m_nSave);
			if (game->saves.size() == 0)
			{
				m_nSave = 0;
				m_bInSaves = FALSE;
			}
			else
			{
				if (m_nSave >= (int)game->saves.size() - 1)
					m_nSave = (int)game->saves.size() - 1 < 0 ? 0 : (int)game->saves.size() - 1;
				game->LoadImages();
			}
		}
		else
		{
			// (The game isn't freed.)
			m_games.erase(m_games.begin() + m_nGame);
			if (m_games.size() == 0)
				m_nGame = 0;
			else if (m_nGame >= (int)m_games.size() - 1)
				m_nGame = (int)m_games.size() - 1;
		}
		m_nSaveRows = (int)ceil((float)(UINT)m_games[m_nGame]->saves.size() * 0.25f);
		if (m_nSaveRow > m_nSaveRows)
			m_nSaveRow = m_nSaveRows;
	}
	PlaySound(m_pSoundSelect);
}

// 0x0011E249
HRESULT CGameManager::ProcessInput(int *action, SGamepadState **pad)
{
	if (!m_bLoaded)
	{
		m_bWaitForCopy = TRUE;
		return S_OK;
	}
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		if (m_pMessageBox->IsOpen())
			return S_OK;
		m_bMessageShown = FALSE;
		if (m_pMessageBox->GetResult() != MSGBOX_YES)
		{
			m_bDeletePending = FALSE;
			return S_OK;
		}
		if (!m_bDeletePending)
			return S_OK;
		m_bDeletePending = FALSE;
		DeleteSelected();
		return S_OK;
	}

	// While a copy runs input is ignored; the first input after it plays
	// "Action Complete". (Quirk: so does the first after the scan.)
	if ((g_bFileOperationQueued || g_bFileOperationActive) && !g_bFileOperationPaused)
	{
		m_bWaitForCopy = TRUE;
	}
	else if (m_bWaitForCopy)
	{
		m_bWaitForCopy = FALSE;
		PlaySound(m_pSoundDone);
	}
	else if (*action == INPUT_LTHUMB)
	{
		// The left stick moves between the games.
		if (*pad != NULL)
		{
			if ((*pad)->fY1 == 1.0f)
				*action = INPUT_UP;
			else if ((*pad)->fY1 == -1.0f)
				*action = INPUT_DOWN;
		}
		if (*action != INPUT_LTHUMB)
			m_bInSaves = FALSE;
	}

	if (m_bCopyMenuShown)
	{
		m_pCopyMenu->ProcessInput(action, pad);
		if (m_pCopyMenu->IsOpen())
			return S_OK;
		m_bCopyMenuShown = FALSE;
		SGameEntry *game = m_games[m_nGame];
		CGameSaveItem *save = game->saves[m_nSave];
		if (save == NULL)
			return S_OK;
		if (save->m_bContent)
		{
			std::string text;
			StrFormat(text, GetStringA(STR_MSG_GAMESAVES_CONTENT), save->m_pszPath);
			ShowMessage(text.c_str(), GetString(STR_INFORMATION), MSGBOX_BUTTONS_OK);
			return S_OK;
		}
		int choice = m_pCopyMenu->GetSelection();
		if (choice == -1)
			PlaySound(m_pSoundBack);
		else
			CopySave(game, save, choice);
		return S_OK;
	}
	if (m_bMenuShown)
	{
		m_menu.ProcessInput(action, pad);
		if (m_menu.IsOpen())
			return S_OK;
		SGameEntry *game = m_games[m_nGame];
		m_bMenuShown = FALSE;
		int choice = m_menu.GetSelection();
		if (choice == -1)
			PlaySound(m_pSoundBack);
		else if (choice == 0)
			OpenCopyMenu(game);
		else if (choice == 1 && game != NULL)
			AskDelete(game);
		return S_OK;
	}
	if (m_bDownloadMode && m_bDownloadMenuShown)
	{
		// The removed save download (never reached: nothing sets it).
		if (m_pDownloadMenu != NULL && !m_bDownloading && !m_unknown174)
		{
			m_pDownloadMenu->ProcessInput(action, pad);
			if (!m_pDownloadMenu->IsOpen())
			{
				if (m_pDownloadMenu->GetSelection() == -1)
				{
					m_bDownloadMode = FALSE;
					m_bDownloadMenuShown = FALSE;
					PlaySound(m_pSoundBack);
				}
				else if (m_pDownloadMenu->GetSelection() > -1)
				{
					m_pDownloadMenu->Show(0);
				}
			}
		}
		if (*action != INPUT_BACK && *action != INPUT_B)
			return S_OK;
		*action = INPUT_NONE;
		if (m_pSession != NULL)
			m_pSession->Cancel();
		m_bDownloading = FALSE;
		return S_OK;
	}

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		OpenMenu();
		PlaySound(m_pSoundSelect);
		break;
	case INPUT_B:
	case INPUT_BACK:
		*action = INPUT_NONE;
		if (m_bDownloadMode)
		{
			if (m_pSession != NULL)
				m_pSession->Cancel();
			m_bDownloading = FALSE;
		}
		else
		{
			m_bOpen = FALSE;
		}
		PlaySound(m_pSoundBack);
		break;
	case INPUT_BLACK:
		Reload();
		PlaySound(m_pSoundSelect);
		break;
	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (*action == INPUT_LTRIGGER)
			m_bInSaves = FALSE;
		if (m_bInSaves && m_nSaveRow != 0)
		{
			// Up a row of saves.
			m_nSaveRow--;
			m_nSave = m_nSave - 4 < 0 ? 0 : m_nSave - 4;
			PlaySound(m_pSoundSlot);
			break;
		}
		if (m_nGame < 1)
			break;
		m_bInSaves = FALSE;
		m_nGame--;
		goto newGame;
	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (*action == INPUT_RTRIGGER)
			m_bInSaves = FALSE;
		if (m_bInSaves && m_nSaveRow < m_nSaveRows - 1)
		{
			// Down a row of saves.
			m_nSaveRow++;
			SGameEntry *game = m_games[m_nGame];
			int save = m_nSave + 4;
			if (save >= (int)game->saves.size() - 1)
				save = (int)game->saves.size() - 1;
			m_nSave = save;
			m_nSaveColumn = save % 4;
			PlaySound(m_pSoundSlot);
			break;
		}
		if (m_nGame >= (int)m_games.size() - 1)
			break;
		m_bInSaves = FALSE;
		m_nGame++;
	newGame:
		PlaySound(m_pSoundGame);
		m_nSaveColumn = 0;
		m_nSaveRow = 0;
		m_nSaveRows = (int)ceil((float)(UINT)m_games[m_nGame]->saves.size() * 0.25f);
		break;
	case INPUT_LEFT:
		if (m_nSave < 1)
		{
			// Back to the game.
			if (m_bInSaves)
				PlaySound(m_pSoundSlot);
			m_bInSaves = FALSE;
			break;
		}
		if (m_nSaveColumn != 0)
		{
			m_nSave--;
			m_nSaveColumn--;
		}
		else
		{
			m_bInSaves = FALSE;
		}
		PlaySound(m_pSoundSlot);
		break;
	case INPUT_RIGHT:
	{
		if (m_games.size() == 0 || m_games[m_nGame]->saves.size() == 0)
		{
			m_bInSaves = FALSE;
			break;
		}
		SGameEntry *game = m_games[m_nGame];
		if (!m_bInSaves)
		{
			// Into the saves, at the first.
			m_bInSaves = TRUE;
			m_nSave = 0;
			m_nSaveRow = 0;
			m_nSaveColumn = 0;
		}
		else
		{
			if (m_nSaveColumn > 2 || m_nSave >= (int)game->saves.size() - 1)
				break;
			m_nSave++;
			m_nSaveColumn++;
		}
		PlaySound(m_pSoundSlot);
		break;
	}
	}
	return S_OK;
}
