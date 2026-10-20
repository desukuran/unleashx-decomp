// Loading skins: built-in (INT_SKINS), zipped and plain folders.
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../settings/AppSettings.h"
#include "../thirdparty/XUnzip.h"
#include "../xml/XmlDocument.h"
#include "Skin.h"

// Built-in skins, stored as a zip in the XBE's INT_SKINS section. 0x0026A470
static const struct
{
	const char *name;
	const char *file;
} g_internalSkins[] = {
	{"NoImages_Blue -By Jezz_X", "Jezz_NoImages_Blue.xml"},
	{"NoImages_Green -By Jezz_X", "Jezz_NoImages_Green.xml"},
	{"NoImages - By Jezz_X", "Jezz_NoImages.xml"},
	{"BlueShapes - By Kthulu", "BlueShapes.xml"},
	{"GreenShapes - By Kthulu", "GreenShapes.xml"},
	{"OrangeShapes - By Kthulu", "OrangeShapes.xml"},
	{"PurpleShapes - By Kthulu", "PurpleShapes.xml"},
	{"RedShapes - By Kthulu", "RedShapes.xml"},
};

// The size of the INT_SKINS zip (the release's, unless the build passes the
// size of its own assets/internal_skins.zip).
#ifndef INT_SKINS_SIZE
#define INT_SKINS_SIZE 0x2CC5
#endif

// 0x000F8E27
int GetInternalSkinCount()
{
	return sizeof(g_internalSkins) / sizeof(g_internalSkins[0]);
}

// (inlined in the skin browser)
const char *GetInternalSkinName(int index)
{
	return g_internalSkins[index].name;
}

// 0x000F90B0
int FindInternalSkin(const char *name)
{
	for (int i = 0; i < GetInternalSkinCount(); i++)
		if (stricmp(name, g_internalSkins[i].name) == 0)
			return i;
	return -1;
}

// Unzips one entry of the INT_SKINS zip into a string document.
static BOOL LoadInternalSkin(int index, CXmlDocument *doc)
{
	BOOL loaded = FALSE;
	std::string entry(g_internalSkins[index].file);
	void *section = XLoadSection("INT_SKINS");
	if (section != NULL)
	{
		HZIP zip = OpenZip(section, INT_SKINS_SIZE, ZIP_MEMORY);
		if (zip != NULL)
		{
			ZIPENTRY ze;
			memset(&ze, 0, sizeof(ze));
			GetZipItem(zip, -1, &ze);
			int count = ze.index;
			char *buf = new char[0x4000];
			if (buf != NULL)
			{
				*(DWORD *)buf = 0;
				for (int i = 0; i < count; i++)
				{
					memset(&ze, 0, sizeof(ze));
					if (GetZipItem(zip, i, &ze) == ZR_OK && StrCmpNoCase(entry.c_str(), ze.name) == 0 &&
					    UnzipItem(zip, i, buf, ze.unc_size, ZIP_MEMORY) == ZR_OK)
					{
						loaded = doc->LoadFromString(buf);
						break;
					}
				}
				delete[] buf;
			}
			CloseZip(zip);
		}
	}
	XFreeSection("INT_SKINS");
	return loaded;
}

// Extracts a zipped skin into tempDir (once per zip: skinsrc.dat remembers
// which zip is there). Returns FALSE if the zip couldn't be opened.
static BOOL ExtractZippedSkin(CSkin *skin, const char *zipPath, const std::string &tempDir)
{
	skin->m_strTempDir = tempDir;
	skin->m_strTempDir += "skinsrc.dat";
	std::string source = ReadTextFile(skin->m_strTempDir.c_str());
	if (source.size() != 0 && StrCmpNoCase(source, zipPath) == 0)
		return TRUE;   // already extracted

	// Remove the previous skin's files (saved .xbx files stay).
	std::string pattern(tempDir);
	pattern += "*";
	WIN32_FIND_DATAA fd;
	HANDLE find = FindFirstFileA(pattern.c_str(), &fd);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				std::string file(tempDir);
				file += fd.cFileName;
				if (StrCmpNoCase(StrRight(file, 4), ".xbx") != 0)
					DeleteFileA(file.c_str());
			}
		} while (FindNextFileA(find, &fd));
	}
	if (find != INVALID_HANDLE_VALUE)
		CloseHandle(find);

	HZIP zip = OpenZip((void *)zipPath, 0, ZIP_FILENAME);
	if (zip == NULL)
		return FALSE;

	std::string dest(tempDir);
	ZIPENTRY ze;
	memset(&ze, 0, sizeof(ze));
	GetZipItem(zip, -1, &ze);
	BOOL allOk = TRUE;
	int count = ze.index;
	for (int i = 0; i < count; i++)
	{
		memset(&ze, 0, sizeof(ze));
		if (GetZipItem(zip, i, &ze) != ZR_OK)
			continue;

		// Skip the zip's top folder.
		dest = tempDir;
		std::string name;
		name = ze.name;
		int slash = StrFind(name, "/");
		if (slash != -1)
			name = StrMid(name, slash + 1, (int)name.size() - (slash + 1));
		dest += name;
		StrReplaceAll(dest, "/", "\\");

		CreateDirectoryTree(GetPathDirectory(dest.c_str()).c_str());
		SetFileAttributesA(dest.c_str(), FILE_ATTRIBUTE_NORMAL);
		DeleteFileA(dest.c_str());
		MakeFatxPath(dest.c_str(), dest);

		if ((int)dest.size() >= 15 && StrCmpNoCase(StrLeft(StrRight(dest, 12), 9), "UXSplash.") == 0)
			skin->m_strSplashFile = dest;

		if (UnzipItem(zip, i, (void *)dest.c_str(), 0, ZIP_FILENAME) == ZR_OK)
			SetFileAttributesA(dest.c_str(), FILE_ATTRIBUTE_NORMAL);
		else
			allOk = FALSE;
	}
	if (count < 1 || allOk)
	{
		std::string text;
		StrFormat(text, "%s", zipPath);
		WriteTextFile(skin->m_strTempDir.c_str(), text.c_str(), FALSE);
	}
	CloseZip(zip);
	return TRUE;
}

// 0x000FFE1A
// path is a built-in skin name, a skin.xml path or a .zip. Afterwards the
// skin is marked changed so PollLoad() fires OnSkinLoaded.
BOOL CSkin::Load(const char *path)
{
	if (path == NULL || strlen(path) == 0)
	{
		m_strSkinName = "[Default]";
		SetDefaultMainLayout();
		m_bChanged = TRUE;
		m_dwLoadingFadeEnd = GetTickCount() + 1000;
		return TRUE;
	}

	BOOL loaded = FALSE;
	CXmlDocument *doc = new CXmlDocument();
	std::string file(path);   // the original copies through a leaked _strdup

	int internal = FindInternalSkin(file.c_str());
	if (internal != -1)
		loaded = LoadInternalSkin(internal, doc);

	BOOL isZip = doc != NULL && file.size() >= 5 && StrCmpNoCase(StrRight(file, 4), ".zip") == 0;
	BOOL parse = TRUE;
	if (!isZip)
	{
		m_bZipped = FALSE;
		if (!loaded)
		{
			// UXSplash.jpg (or .png) next to the skin.
			std::string splash = GetPathDirectory(file.c_str());
			StrAddBackslash(splash);
			splash += "UXSplash.jpg";
			if (FileExists(splash.c_str()))
			{
				m_strSplashFile = splash;
			}
			else
			{
				splash = StrLeft(splash, (int)splash.size() - 3);
				splash += "png";
				if (FileExists(splash.c_str()))
					m_strSplashFile = splash;
			}
		}
	}
	else
	{
		// Zipped skins are extracted to the utility drive.
		SetDefaults();
		std::string tempDir;
		if (!m_pSettings->m_bRunFromDVD)
		{
			tempDir = GetPersistentDataPath(L"SkinFile", 4);
			if (tempDir.size() == 0)
				tempDir = GetPersistentDataPath(L"SkinFile", 1);
		}
		else
		{
			tempDir = "Z:\\Media\\Skin\\";
			CreateDirectoryTree(tempDir.c_str());
		}

		if (tempDir.size() == 0)
		{
			delete doc;
			doc = NULL;
			parse = FALSE;
		}
		else
		{
			StrAddBackslash(tempDir);
			if (!ExtractZippedSkin(this, path, tempDir))
			{
				delete doc;
				doc = NULL;
			}
			else
			{
				file = tempDir;
				file += "skin.xml";
				m_bZipped = TRUE;
			}
		}
	}

	if (parse && doc != NULL)
	{
		if (!loaded)
		{
			if (!doc->Load(file.c_str()))
			{
				doc = NULL;   // the original leaks the document here
				parse = FALSE;
			}
			else
			{
				m_strSkinFile = doc->m_strFileName;
				int slash = StrReverseFind(m_strSkinFile, "\\");
				if (slash != -1)
				{
					m_strSkinDir = StrLeft(m_strSkinFile, slash);
					// The skin's name is its folder name (or the zip's folder).
					if (!m_bZipped)
					{
						if (m_strSkinDir.size() != 0 && (slash = StrReverseFind(m_strSkinDir, "\\")) != -1)
							m_strSkinName = StrMid(m_strSkinDir, slash, (int)m_strSkinDir.size() - slash);
					}
					else
					{
						file = path;
						slash = StrReverseFind(file, "\\");
						if (slash != -1)
						{
							file = StrLeft(file, slash);
							slash = StrReverseFind(file, "\\");
							if (slash != -1)
								m_strSkinName = StrMid(file, slash, (int)file.size() - slash);
						}
					}
					StrRemoveChar(m_strSkinName, '\\');
				}
			}
		}
		else
		{
			m_strSkinFile = file;
			m_strSkinName = file;
		}

		if (parse)
		{
			Parse(doc);
			delete doc;
		}
	}

	if (!parse || doc == NULL)
		SetDefaultMainLayout();

	m_bChanged = TRUE;
	m_dwLoadingFadeEnd = GetTickCount() + 1000;
	return TRUE;
}

// 0x00101708
static unsigned int __stdcall SkinLoaderThread(void *param)
{
	CSkin *skin = (CSkin *)param;
	if (skin != NULL)
	{
		skin->Load(skin->m_strLoadFile.c_str());
		skin->m_bLoaded = TRUE;
		Sleep(100);
	}
	return 0;
}

// 0x00101743
// Loads a skin on a background thread; PollLoad() reports when it's done.
void CSkin::BeginLoad(const char *path)
{
	m_bLoading = TRUE;
	m_bLoaded = FALSE;
	m_strLoadFile = path;   // via a leaked _strdup in the original
	m_strSplashFile.clear();

	if (m_hLoadThread != NULL && m_hLoadThread != (HANDLE)-1)
	{
		WaitForSingleObject(m_hLoadThread, INFINITE);
		CloseHandle(m_hLoadThread);
		m_hLoadThread = NULL;
	}

	m_hLoadThread = (HANDLE)_beginthreadex(NULL, 0, SkinLoaderThread, this, 0, NULL);
	if (m_hLoadThread == NULL || m_hLoadThread == (HANDLE)-1)
	{
		SkinLoaderThread(this);
		m_bLoaded = TRUE;
	}
	else
	{
		SetThreadPriority(m_hLoadThread, THREAD_PRIORITY_ABOVE_NORMAL);
	}
}

// 0x000F9D63 (this in eax)
BOOL CSkin::SaveSplashForBoot()
{
	if (!m_bZipped && XDeleteSaveGame("U:\\", L"SkinFile"))
		XDeleteSaveGame("X:\\", L"SkinFile");
	std::string target = "E:\\UDATA\\9e115330\\0064122817A8\\Splash.jpg";
	DeleteFileA(target.c_str());
	if (m_strSplashFile.size() != 0)
		CopyFileA(m_strSplashFile.c_str(), target.c_str(), FALSE);
	return TRUE;
}
