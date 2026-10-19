// The game folder scan. A menu list like
//
//   <List Text="Games" DelayLoad="false"><Path>E:\Games</Path></List>
//
// is filled with the default.xbe of each sub-folder of E:\Games. The scan
// results are kept in the item cache (items.xml):
//
//   <Cache><Folder Path="E:\Games\" DelayLoad="False">
//     <Item Action="E:\Games\Halo\default.xbe" TStamp="..." Region="..." ID="..."
//           Icon="..." AltIcon="..." Preview="...">Halo</Item>
//   </Folder></Cache>
//
// and merged into the menu each time (CMenu::ScanFoldersNow).
#include "MenuScan.h"

#include <string>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../system/Storage.h"
#include "../xml/XmlDocument.h"
#include "../xml/XmlUtil.h"

BOOL g_bItemCacheChanged;   // 0x00276240
BOOL g_bMenuPending;        // 0x0027623C

// 0x000E1F23
// Each <Path> (with a drive letter) gets a cache <Folder>; DelayLoad="true"
// on the list makes it a quick scan.
void CollectMenuFolders(CXmlElement *menu, CXmlElement *cache)
{
	if (menu == NULL || cache == NULL)
		return;

	for (CXmlElement *child = menu->GetFirstChild(); child != NULL; child = menu->GetNextChild())
	{
		if (StrCmpNoCase(child->m_strName, "List") == 0)
		{
			CollectMenuFolders(child, cache);
			continue;
		}
		if (StrCmpNoCase(child->m_strName, "Path") != 0 || StrFind(child->m_strValue, ":") == -1)
			continue;

		std::string path = child->m_strValue;
		StrAddBackslash(path);
		BOOL delayLoad = FALSE;
		if (child->m_pParent != NULL)
		{
			std::string value = child->m_pParent->GetAttribute("DelayLoad");
			if (value.size() != 0)
				delayLoad = IsTrueString(value.c_str());
		}

		CXmlElement *folder;
		for (folder = cache->GetFirstChild(); folder != NULL; folder = cache->GetNextChild())
		{
			if (StrCmpNoCase(path, folder->GetAttribute("Path").c_str()) == 0)
				break;
		}

		if (folder == NULL)
		{
			if (!DirectoryExists(path.c_str()))
				continue;
			folder = AddChildElement(cache, "Folder", NULL, NULL);
			folder->SetAttribute("Path", path.c_str());
			g_bItemCacheChanged = TRUE;
			if (!delayLoad)
				folder->SetAttribute("DelayLoad", "False");
		}
		else if (!delayLoad)
		{
			folder->SetAttribute("DelayLoad", "False");
		}
		else
		{
			folder->RemoveAttribute("DelayLoad");
		}
	}
}

// Steps for the icon and preview searches: each pair replaces the first
// string with the second in the path and tries again.
static const char *const kAltIconSteps[] = {
	".png", ".jpg",                            // icon.jpg
	"icon.jpg", "avalaunch_icon.png",
	"avalaunch_icon.png", "mxm_thumb.jpg",
	"mxm_thumb.jpg", "default.png",
	".png", ".jpg",                            // default.jpg
};
static const char *const kPreviewSteps[] = {
	".wmv", ".xmv",                            // preview.xmv
	"preview.xmv", "mxm_ss.wmv",
	"mxm_ss", "default",                       // default.wmv
};

// Tries path, then the replacement steps (only the first `quickSteps` pairs
// when quick); returns the first file found or "".
static std::string FindFile(std::string path, const char *const *steps, int stepCount, BOOL quick,
                            int quickSteps)
{
	if (FileExists(path.c_str()))
		return path;
	for (int i = 0; i < stepCount; i++)
	{
		if (quick && i >= quickSteps)
			return "";
		StrReplaceAll(path, steps[i * 2], steps[i * 2 + 1]);
		if (FileExists(path.c_str()))
			return path;
	}
	return "";
}

// 0x000E2D80
// Adds (or refreshes) the cache item for one XBE. A quick scan doesn't
// re-read XBEs that are already cached, and looks for fewer icon/preview
// names.
static void AddXbeToFolder(CXmlElement *folder, const char *xbePath, BOOL quick)
{
	WIN32_FILE_ATTRIBUTE_DATA attributes;
	if (!GetFileAttributesExA(xbePath, GetFileExInfoStandard, &attributes))
	{
		return;
	}

	std::string path = xbePath;
	std::string stamp;
	BOOL cached = FALSE;
	BOOL rescan = TRUE;
	CXmlElement *item = folder->FindFirstChild("Item");
	while (item != NULL)
	{
		if (StrCmpNoCase(path, item->GetAttribute("Action").c_str()) == 0)
		{
			cached = TRUE;
			// Rescan only if the icon went missing (and not in a quick scan).
			if (quick || FileExists(item->GetAttribute("Icon").c_str()))
				rescan = FALSE;
			break;
		}
		item = folder->FindNextChild();
	}

	SXbeInfo info;
	if (rescan && ReadXbeInfo(path.c_str(), &info))
	{
		StrFormat(stamp, "%0x", attributes.ftLastWriteTime.dwLowDateTime);
		if (!cached)
		{
			item = AddChildElement(folder, "Item", info.titleName.c_str(), NULL);
			item->SetAttribute("TStamp", stamp.c_str());
		}
		else if (item->GetValue().compare(info.titleName) != 0)
		{
			item->SetValue(info.titleName.c_str());
			item->SetAttribute("TStamp", stamp.c_str());
		}
		if (item == NULL)
		{
			return;
		}

		g_bItemCacheChanged = TRUE;
		item->SetAttribute("Action", path.c_str());
		std::string text;
		StrFormat(text, "%d", (int)info.gameRegion);
		item->SetAttribute("Region", text.c_str());

		// Same title image logic as CMenu::FindItemIcon.
		StrFormat(text, "E:\\UDATA\\%08x\\TitleImage.xbx", info.titleId);
		if (!FileExists(text.c_str()))
		{
			if (g_pSettings->m_bRunFromDVD)
				StrFormat(text, "Z:\\Media\\%08x.xbx", info.titleId);
			if (!ExtractXbeTitleImage(path.c_str(), text.c_str()))
			{
				StrFormat(text, "E:\\UDATA\\%08x", info.titleId);
				RemoveDirectoryA(text.c_str());
			}
			else
			{
				item->SetAttribute("Icon", text.c_str());
				if (!g_pSettings->m_bRunFromDVD)
				{
					std::string meta;
					StrFormat(meta, "E:\\UDATA\\%08x\\TitleMeta.xbx", info.titleId);
					StrFormat(text, "TitleName=%s", info.titleName.c_str());
					WriteTextFile(meta.c_str(), text.c_str(), TRUE);
				}
			}
		}
		else
		{
			item->SetAttribute("Icon", text.c_str());
		}
		StrFormat(text, "%08x", info.titleId);
		item->SetAttribute("ID", text.c_str());

		std::string dir = xbePath;
		int slash = StrReverseFind(dir, "\\");
		if (slash != -1)
			dir = StrLeft(dir, slash + 1);
		std::string file = FindFile(dir + "icon.png", kAltIconSteps, 5, FALSE, 0);
		if (file.size() != 0)
			item->SetAttribute("AltIcon", file.c_str());

		// Also attractmode.xmv first here.
		file = dir + "attractmode.xmv";
		if (!FileExists(file.c_str()))
			file = FindFile(dir + "preview.wmv", kPreviewSteps, 3, quick, 1);
		if (file.size() != 0)
			item->SetAttribute("Preview", file.c_str());
	}
	else if (cached && item != NULL)
	{
		// Cached: refresh name and region if the XBE changed.
		StrFormat(stamp, "%0x", attributes.ftLastWriteTime.dwLowDateTime);
		if (stamp.compare(item->GetAttribute("TStamp")) != 0 && ReadXbeInfo(path.c_str(), &info))
		{
			g_bItemCacheChanged = TRUE;
			item->SetValue(info.titleName.c_str());
			item->SetAttribute("TStamp", stamp.c_str());
			StrFormat(stamp, "%d", (int)info.gameRegion);
			item->SetAttribute("Region", stamp.c_str());
		}

		// Fill in a missing AltIcon or Preview.
		std::string dir = xbePath;
		int slash = StrReverseFind(dir, "\\");
		if (slash != -1)
			dir = StrLeft(dir, slash + 1);
		if (item->GetAttribute("AltIcon").size() == 0)
		{
			std::string file = FindFile(dir + "icon.png", kAltIconSteps, 5, quick, 1);
			if (file.size() != 0)
			{
				item->SetAttribute("AltIcon", file.c_str());
				g_bItemCacheChanged = TRUE;
			}
		}
		if (item->GetAttribute("Preview").size() == 0)
		{
			std::string file = FindFile(dir + "preview.wmv", kPreviewSteps, 3, quick, 1);
			if (file.size() != 0)
			{
				item->SetAttribute("Preview", file.c_str());
				g_bItemCacheChanged = TRUE;
			}
		}
	}
}

// 0x000E4459
// Quirk: depth only grows (it's incremented after each sub-folder, never
// passed down smaller), so anything above 1 recurses without bound.
void ScanFolder(CXmlElement *folder, const char *path, const char *pattern, int depth, BOOL delayLoad)
{
	if (path == NULL)
		return;
	std::string dir = path;
	if (dir.size() == 0)
		return;
	if (StrFind(dir, ":") == -1)
		ResolvePath(dir.c_str(), dir);
	StrAddBackslash(dir);

	std::string name = pattern;
	StrReplaceAll(name, "*.", "default.");
	StrRemoveChar(name, '*');

	BOOL quick = delayLoad || g_pSettings->m_bRunFromDVD;
	AddXbeToFolder(folder, (dir + name).c_str(), quick);

	WIN32_FIND_DATAA data;
	HANDLE find = FindFirstFileA((dir + "*").c_str(), &data);
	if (find == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			std::string file;
			StrFormat(file, "%s%s\\%s", dir.c_str(), data.cFileName, name.c_str());
			AddXbeToFolder(folder, file.c_str(), quick);
			if (depth > 1)
			{
				ScanFolder(folder, (dir + data.cFileName).c_str(), pattern, depth, delayLoad);
				depth++;
			}
		}
	} while (FindNextFileA(find, &data) == TRUE);
	CloseHandle(find);
}

// 0x000E28D5
// Puts the cached items of each <Path>'s folder into the list. Items already
// in the list (same Action) only get a missing name, Icon, AltIcon or
// Preview, and the ID. New items go at the end, or right after the <Path>
// in the root <Menu>.
void MergeFolderItems(CXmlElement *cache, CXmlElement *list)
{
	if (cache == NULL || list == NULL)
		return;

	CXmlElement *child = list->GetFirstChild();
	while (child != NULL)
	{
		if (StrCmpNoCase(child->m_strName, "List") == 0)
		{
			MergeFolderItems(cache, child);
		}
		else if (StrCmpNoCase(child->m_strName, "Path") == 0 && StrFind(child->m_strValue, ":") != -1)
		{
			std::string path = child->m_strValue;
			StrReplaceAll(path, "/", "\\");
			StrAddBackslash(path);

			CXmlElement *folder = cache->FindFirstChild("Folder");
			while (folder != NULL && StrCmpNoCase(path, folder->GetAttribute("Path").c_str()) != 0)
				folder = cache->FindNextChild();

			if (folder != NULL)
			{
				for (CXmlElement *cached = folder->GetFirstChild(); cached != NULL; cached = folder->GetNextChild())
				{
					CXmlElement *item = list->FindFirstChild("Item");
					while (item != NULL &&
					       stricmp(cached->GetAttribute("Action").c_str(), item->GetAttribute("Action").c_str()) != 0)
						item = list->FindNextChild();

					if (item != NULL)
					{
						if (item->m_strValue.size() == 0)
							item->SetValue(cached->m_strValue.c_str());
						for (size_t i = 0; i < cached->m_attributes.size(); i++)
						{
							CXmlAttribute *attr = cached->m_attributes[i];
							if (attr == NULL)
								continue;
							const std::string &attrName = attr->m_strName;
							BOOL copy;
							if (StrCmpNoCase(attrName, "Icon") == 0 && item->GetAttribute("Icon").size() == 0)
								copy = TRUE;
							else if (StrCmpNoCase(attrName, "ID") == 0)
								copy = TRUE;
							else if (StrCmpNoCase(attrName, "AltIcon") == 0)
								copy = item->GetAttribute("AltIcon").size() == 0;
							else if (StrCmpNoCase(attrName, "Preview") == 0)
								copy = item->GetAttribute("Preview").size() == 0;
							else
								copy = FALSE;
							if (copy)
								item->SetAttribute(attrName.c_str(), attr->m_strValue.c_str());
						}
					}
					else if (StrCmpNoCase(list->m_strName, "Menu") == 0)
					{
						InsertChildElement(list, child, "Item", cached->m_strValue.c_str(),
						                   cached->m_strAttributes.c_str());
					}
					else
					{
						AddChildElement(list, "Item", cached->m_strValue.c_str(), cached->m_strAttributes.c_str());
					}
				}

				// FindFirstChild moved the list's iteration: find our place again.
				CXmlElement *c = list->GetFirstChild();
				while (c != child)
					c = list->GetNextChild();
			}
		}
		child = list->GetNextChild();
	}
}

// 0x000E9CCE
// Builds the menu at startup: the built-in menu from Config.xml's <Menu>
// (or the default one), then the game folders from the item cache, checked
// against the disk (gone XBEs are dropped, folders rescanned). The cache is
// saved when it changed (not when run from DVD).
unsigned __stdcall MenuLoaderThread(void *)
{
	std::string folderPath;
	std::string cachePath;
	ResolvePath("d:\\items.xml", cachePath);
	CXmlDocument cache;
	if (!cache.Load(cachePath.c_str()))
		cache.m_root.m_strName = "Cache";

	// The menu: the lists, items, paths and sorts of Config.xml's <Menu>.
	CXmlElement *root = &g_defaultMenu.m_root;
	if (root->m_strName.size() == 0 || root->GetFirstChild() == NULL)
	{
		root->m_strName = "Menu";
		CXmlElement *menu = g_config.FindElement(&g_config.m_root, "Menu");
		if (menu != NULL)
		{
			for (CXmlElement *child = menu->GetFirstChild(); child != NULL; child = menu->GetNextChild())
			{
				const std::string &name = child->m_strName;
				if (StrCmpNoCase(name, "List") == 0 || StrCmpNoCase(name, "Item") == 0 ||
				    StrCmpNoCase(name, "Path") == 0 || StrCmpNoCase(name, "Sort") == 0)
					AppendElement(root, CloneElement(child), &g_defaultMenu);
			}
		}
	}
	if (root->GetFirstChild() == NULL)
		BuildDefaultMenu();

	CollectMenuFolders(root, &cache.m_root);
	bool removed = false;
	for (CXmlElement *folder = cache.FindElement(&cache.m_root, "Folder"); folder != NULL;
	     folder = cache.FindNextElement(&cache.m_root))
	{
		CXmlElement *item = folder->FindFirstChild("Item");
		while (item != NULL)
		{
			if (!FileExists(item->GetAttribute("Action").c_str()))
			{
				folder->RemoveChild(item);
				item = folder->FindFirstChild("Item");
				removed = true;
			}
			else
			{
				item = folder->FindNextChild();
			}
		}
		BOOL delayLoad = StrCmpNoCase(folder->GetAttribute("DelayLoad"), "False") != 0;
		folderPath = folder->GetAttribute("Path");
		if (DirectoryExists(folderPath.c_str()))
			ScanFolder(folder, folderPath.c_str(), "default.xbe", 1, delayLoad);
	}
	MergeFolderItems(&cache.m_root, root);

	if (!g_pSettings->m_bRunFromDVD && (g_bItemCacheChanged || removed || cache.m_bModified))
	{
		cache.SaveAs(cachePath.c_str());
		g_bItemCacheChanged = FALSE;
	}
	cache.Clear();
	g_bMenuPending = TRUE;
	_endthreadex(0);
	return 0;
}
