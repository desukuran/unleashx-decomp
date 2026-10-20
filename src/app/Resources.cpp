// LoadResources: the startup thread that loads the fonts, resource.xpr and
// the 3D meshes. Run from DVD they come from D:\Media; otherwise the media
// (built into the XBE as a zip) is unpacked into a "Media" save game folder
// on U: (or X:), along with textures borrowed from the MS dashboard.
#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../render/D3D.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "../system/XipArchive.h"
#include "../thirdparty/XUnzip.h"
#include "UnleashX.h"

// The MEDIA_FILE section: a zip of the media files. iMedia.dat in the
// media folder records the size of the one that was unpacked. The size is
// the release's, unless the build passes that of its own assets/media.zip.
#ifndef MEDIA_FILE_SIZE
#define MEDIA_FILE_SIZE 0x5A21F
#endif

// A bitmap font is taken as missing when its line is under 5 pixels.
static const float MIN_FONT_HEIGHT = 5.0f;

// Unpacks a file from a XIP archive into the media folder, unless a copy
// of the same size is there. path gets the copy's path.
static void ExtractXipFile(CXipArchive &xip, unsigned index, const SXipItem &item, const char *folder,
                           std::string &path)
{
	path = folder;
	path.append(item.name);
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data) || data.nFileSizeLow != item.entry.size)
		xip.Extract(index, path.c_str());
}

// The MS dashboard's wall, logo and orb textures, from the XIP archives in
// C:\xboxdashdata.* (or C:\).
static void LoadDashboardTextures(CUnleashX *app, const char *folder)
{
	CFileFinder finder;
	std::string dashDir;
	// Without a match FindFirst gives NULL; UnleashX's string assign
	// (0x000110E1) takes that as empty.
	const char *found = finder.FindFirst("C:\\", "xboxdashdata*");
	std::string path = found != NULL ? found : "";
	if (path.size() == 0)
	{
		if (finder.FindFirst("C:\\", "default.xip") != NULL)
			dashDir = "C:\\";
	}
	else
	{
		StrFormat(dashDir, "C:\\%s", path.c_str());
	}
	finder.Close();
	if (dashDir.size() == 0)
		return;
	if (StrRight(dashDir, 1).compare("\\") != 0)
		dashDir.append("\\");

	path = dashDir;
	path.append("default.xip");
	CXipArchive xip;
	memset(&xip, 0, sizeof(xip));
	if (xip.Open(path.c_str()) < 0)
		return;
	for (unsigned i = 0; i < xip.m_wFileCount; i++)
	{
		SXipItem item;
		memset(&item, 0, sizeof(item));
		if (xip.GetItem(i, &item) < 0)
			continue;
		if (stricmp(item.name, "cellwall.xbx") == 0)
		{
			ExtractXipFile(xip, i, item, folder, path);
			LoadXprTexture(path.c_str(), &app->m_pDashCellWall);
		}
		else if ((app->m_dwScreenWidth == 640 && stricmp(item.name, "xboxlogo.xbx") == 0) ||
		         (app->m_dwScreenWidth > 640 && stricmp(item.name, "xboxlogow.xbx") == 0))
		{
			// The widescreen logo for wider screens.
			ExtractXipFile(xip, i, item, folder, path);
			LoadXprFileTexture(path.c_str(), &app->m_pDashLogo);
		}
	}
	xip.Close();

	if (app->m_pDashCellWall == NULL || app->m_pDashLogo == NULL)
		return;
	path = dashDir;
	path.append("mainmenu5.xip");
	memset(&xip, 0, sizeof(xip));
	if (xip.Open(path.c_str()) < 0)
		return;
	for (unsigned i = 0; i < xip.m_wFileCount; i++)
	{
		SXipItem item;
		memset(&item, 0, sizeof(item));
		if (xip.GetItem(i, &item) >= 0 && stricmp(item.name, "xbox4.xbx") == 0)
		{
			ExtractXipFile(xip, i, item, folder, path);
			LoadXprFileTexture(path.c_str(), &app->m_pDashOrb);
			break;
		}
	}
	xip.Close();
}

// Unpacks the built-in media zip into folder, unless iMedia.dat says this
// version is there already. FALSE if the zip couldn't be opened.
static BOOL UnpackMedia(const char *folder)
{
	std::string path = folder;
	path.append("iMedia.dat");
	std::string stamp = ReadTextFile(path.c_str());
	if (stamp.size() != 0 && (int)stamp.size() < 12 && atol(stamp.c_str()) == MEDIA_FILE_SIZE)
		return TRUE;

	BOOL ok = TRUE;
	void *media = XLoadSection("MEDIA_FILE");
	HZIP zip = OpenZip(media, MEDIA_FILE_SIZE, ZIP_MEMORY);
	if (zip == NULL && media != NULL)
	{
		// Unzipping from memory failed: unzip a copy on disk (Media.dat).
		// Quirk: the copy is created with CREATE_NEW, so a stale Media.dat
		// makes this fail.
		path = folder;
		path.append("Media.dat");
		HANDLE file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 1 /* CREATE_NEW */,
		                          FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			WriteFile(file, media, MEDIA_FILE_SIZE, NULL, NULL);
			CloseHandle(file);
			zip = OpenZip((void *)path.c_str(), 0, ZIP_FILENAME);
		}
	}
	if (zip == NULL)
	{
		ok = FALSE;
	}
	else
	{
		ZIPENTRY entry;
		memset(&entry, 0, sizeof(entry));
		GetZipItem(zip, -1, &entry);
		int count = entry.index;
		for (int i = 0; i < count; i++)
		{
			memset(&entry, 0, sizeof(entry));
			if (GetZipItem(zip, i, &entry) != ZR_OK)
				continue;
			std::string file = folder;
			file.append(entry.name);
			// An existing UXSplash.jpg is kept: it may be the user's own.
			if (stricmp(entry.name, "UXSplash.jpg") == 0 && FileExists(file.c_str()))
				continue;
			DeleteFileA(file.c_str());
			// Quirk: the retry unzips "to memory" at the path string, with
			// no room (length 0).
			if (UnzipItem(zip, i, (void *)file.c_str(), 0, ZIP_FILENAME) != ZR_OK)
				UnzipItem(zip, i, (void *)file.c_str(), 0, ZIP_MEMORY);
		}
		CloseZip(zip);
		path = folder;
		path.append("iMedia.dat");
		StrFormat(stamp, "%d", MEDIA_FILE_SIZE);
		WriteTextFile(path.c_str(), stamp.c_str(), FALSE);
	}
	XFreeSection("MEDIA_FILE");
	return ok;
}

// The menu's font: fontFile if it loads (tried on a scratch font first).
static void LoadMenuFont(CXBFont &menuFont, const char *fontFile, BOOL *ok)
{
	CXBFont probe;
	if (probe.Create(fontFile, 0) >= 0)
	{
		menuFont.Destroy();
		menuFont.Create(fontFile, 0);
		probe.Destroy();
	}
	else if (ok != NULL)
	{
		*ok = FALSE;
	}
}

// The globe's sky sphere wears CellWall: the first frame's first subset.
static void SetSphereTexture(CXBMesh &mesh, IDirect3DTexture8 *texture)
{
	mesh.m_pMeshFrames->pSubsets->pTexture = texture;
}

// 0x000E8938
// Thread exit code 0 when everything loaded. Quirk: Initialize also calls
// it directly (after a failure, to retry), and the success path still ends
// with _endthreadex. The early failures return 1 or 100 without it.
unsigned __stdcall LoadResources(void *param)
{
	CUnleashX *app = (CUnleashX *)param;
	BOOL retried = FALSE;
	unsigned result = 0;
	if (app == NULL)
	{
		_endthreadex(result);
		return result;
	}
	CXBFontEx *fonts = app->m_fonts;
	fonts[0].Destroy();
	fonts[5].Destroy();
	fonts[4].Destroy();
	fonts[3].Destroy();
	fonts[2].Destroy();
	// The main font: the copy built into the XBE, else Font.xpr.
	void *section = XLoadSection("DEF_FONT");
	if ((section == NULL || fonts[1].CreateFromMemory(section) < 0) && fonts[1].Create("Font.xpr", 0) < 0)
		return 1;
	XFreeSection("DEF_FONT");

	std::string path;
	std::string mediaDir;
	char folder[MAX_PATH];
	folder[0] = '\0';
	BOOL loaded = FALSE;
	if (g_pSettings->m_bRunFromDVD)
	{
		// From DVD: everything is in D:\Media (Splash.jpg there is the
		// loading image).
		Sleep(50);
		if (app->m_bNoHardDisk)
			app->PrepareHardDisk();
		if (DirectoryExists("D:\\Media"))
		{
			loaded = TRUE;
			mediaDir = "D:\\Media\\";
			path = mediaDir;
			path.append("Splash.jpg");
			if (FileExists(path.c_str()) &&
			    D3DXCreateTextureFromFileA(app->m_pd3dDevice, path.c_str(), &app->m_pDefaultLoadingIcon) != S_OK)
				app->m_pDefaultLoadingIcon = NULL;
			LoadMenuFont(fonts[0], "MenuFont.xpr", &loaded);
			app->m_resource.Destroy();
			if (app->m_resource.Create("resource.xpr", 0, NULL) < 0)
			{
				loaded = FALSE;
			}
			else
			{
				app->m_pCellWall = app->m_resource.GetTexture("CellWall");
				if (app->m_pCellWall == NULL)
				{
					loaded = FALSE;
				}
				else
				{
					app->m_pEarthEnvMap = app->m_resource.GetTexture("EarthEnvMap");
					app->m_pOrb = app->m_resource.GetTexture("Orb");
				}
			}
			if (fonts[5].Create("XboxDings.xpr", 0) < 0)
				loaded = FALSE;
			else
				g_fonts.Add("XboxDings", &fonts[5], FALSE);
			if (fonts[4].Create("XBoxBook12.xpr", 0) < 0)
				loaded = FALSE;
			if (fonts[2].Create("Xbox14.xpr", 0) < 0)
				loaded = FALSE;
			if (app->m_outerSphereMesh.Create("OuterSphere.xbg", &app->m_resource) < 0)
				loaded = FALSE;
			else
				SetSphereTexture(app->m_outerSphereMesh, app->m_pCellWall);
			if (app->m_crossHairMesh.Create("CrossHair.xbg", NULL) < 0)
				loaded = FALSE;
		}
	}

	std::string xbePath;
	StrFormat(xbePath, "%s%s", g_pSettings->m_strHomePath.c_str(), g_pSettings->m_strXbeName.c_str());
	MountTitleDataDrive(xbePath.c_str());

	// The media folder: the "Media" save game on U:, else on the utility
	// drive X:, else a plain folder on U: or X:.
	if (!loaded && XCreateSaveGame("U:\\", L"Media", OPEN_ALWAYS, XCREATESAVEGAME_NOCOPY, folder, MAX_PATH) != 0)
	{
		if (!DirectoryExists("X:\\"))
		{
			MountDrive('X');
			XMountUtilityDrive(FALSE);
		}
		if (XCreateSaveGame("X:\\", L"Media", OPEN_ALWAYS, 0, folder, MAX_PATH) != 0)
		{
			strcpy(folder, "U:\\0064122817A8");
			if (!DirectoryExists(folder))
				CreateDirectoryTree(folder);
			if (!DirectoryExists(folder))
			{
				strcpy(folder, "X:\\9e115330\\0064122817A8");
				if (!DirectoryExists(folder))
					CreateDirectoryTree(folder);
			}
		}
	}
	// A small, nearly full utility drive is formatted. (Quirk: the media
	// folder may be on it.)
	if (folder[0] != '\0' && folder[0] == 'X')
	{
		SDriveInfo *x = GetDriveInfo('X');
		if (x != NULL && x->mounted && x->totalBytes < 0x38400000 && x->freeBytes < 0x6400000)
			FormatDrive('X');
	}

	while (!loaded)
	{
		if (DirectoryExists(folder))
		{
			path = folder;
			if (StrRight(path, 1).compare("\\") != 0)
			{
				strcat(folder, "\\");
				path = folder;
			}
			// The data path: the same folder as a full E: path (U: is
			// E:\UDATA\9e115330), e.g. E:\UDATA\9e115330\0064122817A8\.
			mediaDir = folder;
			std::string &dataPath = g_pSettings->m_strDataPath;
			dataPath = StrLeft(mediaDir, 3);
			dataPath.append("9e115330\\");
			dataPath.append(StrMid(mediaDir, 3, (int)mediaDir.size() - 3));
			StrReplaceAll(dataPath, "U:\\", "E:\\UDATA\\");

			// The save game's icon.
			path = "U:\\SaveImage.xbx";
			if (!FileExists(path.c_str()))
			{
				void *image = XLoadSection("$$XSIMAGE");
				if (image != NULL)
				{
					HANDLE handle = XGetSectionHandle("$$XSIMAGE");
					DWORD size = XGetSectionSize(handle) < 0x1000 ? 0x1000 : XGetSectionSize(handle);
					WriteBinaryFile(path.c_str(), image, size);
				}
				XFreeSection("$$XSIMAGE");
			}

			if (app->m_pDashCellWall == NULL)
				LoadDashboardTextures(app, folder);

			if (!UnpackMedia(folder))
				return 100;   // (no _endthreadex)
		}

		// Load from the media folder.
		path = folder;
		StrAddBackslash(path);
		XBUtil_SetMediaPath(path.c_str());
		std::string effects = path;
		if (app->m_pDashLogo != NULL)
			app->m_pDefaultLoadingIcon = app->m_pDashLogo;
		CXBFont probe;
		if (probe.Create("Arial.xpr", 0) >= 0)
		{
			fonts[0].Destroy();
			fonts[0].Create("Arial.xpr", 0);
			probe.Destroy();
		}
		fonts[3].Destroy();
		if (fonts[3].Create("XBoxBook14.xpr", 0) < 0)
			goto failed;
		fonts[1].Destroy();
		if (fonts[1].Create("XBoxBook14.xpr", 0) < 0)
			goto failed;
		app->m_resource.Destroy();
		if (app->m_resource.Create("resource.xpr", 0, NULL) < 0)
			goto failed;
		if (app->m_pCellWall == NULL)
			app->m_pCellWall = app->m_resource.GetTexture("CellWall");
		if (app->m_pEarthEnvMap == NULL)
			app->m_pEarthEnvMap = app->m_resource.GetTexture("EarthEnvMap");
		if (app->m_pOrb == NULL)
			app->m_pOrb = app->m_resource.GetTexture("Orb");
		// The fonts that didn't come with the media.
		if (GetFontHeight(&fonts[5]) < MIN_FONT_HEIGHT)
		{
			fonts[5].Destroy();
			if (fonts[5].Create("XboxDings.xpr", 0) < 0)
				goto failed;
			g_fonts.Add("XboxDings", &fonts[5], FALSE);
		}
		if (GetFontHeight(&fonts[4]) < MIN_FONT_HEIGHT)
		{
			fonts[4].Destroy();
			if (fonts[4].Create("XBoxBook12.xpr", 0) < 0)
				goto failed;
		}
		if (GetFontHeight(&fonts[2]) < MIN_FONT_HEIGHT)
		{
			fonts[2].Destroy();
			if (fonts[2].Create("Xbox14.xpr", 0) < 0)
				fonts[2].Create("Arial.xpr", 0);
		}
		if (app->m_outerSphereMesh.Create("OuterSphere.xbg", &app->m_resource) < 0)
			goto failed;
		SetSphereTexture(app->m_outerSphereMesh, app->m_pCellWall);
		if (app->m_crossHairMesh.Create("CrossHair.xbg", NULL) < 0)
			goto failed;
		// Done once the media has dsstdfx.bin too.
		effects = path;
		effects.append("dsstdfx.bin");
		if (FileExists(effects.c_str()))
			break;

	failed:
		// Something's missing: unpack and load once more.
		result = 1;
		if (retried)
			goto done;
		retried = TRUE;
		result = 0;
	}

	g_fonts.Add("MenuFont", &fonts[0], FALSE);
	g_fonts.Add("MainFont", &fonts[1], FALSE);
	g_fonts.Add("SmallFont", &fonts[4], FALSE);
	g_fonts.Add("InfoFont", &fonts[3], FALSE);
	g_fonts.Add("CaptionFont", &fonts[2], FALSE);
	g_pSettings->m_pDialogFont = &fonts[2];
	if (app->m_pDefaultLoadingIcon != NULL)
		g_pSettings->m_pLoadingIcon = app->m_pDefaultLoadingIcon;

done:
	_endthreadex(result);
	return result;
}
