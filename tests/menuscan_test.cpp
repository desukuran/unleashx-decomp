// The game folder scan on a fake E: drive: two games under E:\Games are
// cached and merged into the menu list.
#include <cstdio>

#include "../src/settings/AppSettings.h"
#include "../src/ui/MenuScan.h"
#include "../src/xml/XmlDocument.h"

int main()
{
	g_pSettings = CAppSettings::GetInstance();
	g_pSettings->m_strHomePath = "e:\\dash\\";
	CXmlDocument menu;
	if (!menu.LoadFromString("<Menu><List Text=\"Games\" DelayLoad=\"false\"><Path>E:\\Games</Path></List></Menu>"))
	{
		printf("FAIL: parse\n");
		return 1;
	}
	CXmlDocument cache;
	cache.m_root.m_strName = "Cache";

	CollectMenuFolders(&menu.m_root, &cache.m_root);
	CXmlElement *folder = cache.FindElement(&cache.m_root, "Folder");
	if (folder == NULL)
	{
		printf("FAIL: no folder\n%s\n", cache.ToString().c_str());
		return 1;
	}
	ScanFolder(folder, folder->GetAttribute("Path").c_str(), "default.xbe", 1, FALSE);
	MergeFolderItems(&cache.m_root, &menu.m_root);

	printf("cache:\n%s\nmenu:\n%s\n", cache.ToString().c_str(), menu.ToString().c_str());

	// Both games, with the title from the XBE (one has an icon.png).
	CXmlElement *list = menu.FindElement(&menu.m_root, "List");
	int items = 0, withIcon = 0;
	for (CXmlElement *item = list->FindFirstChild("Item"); item != NULL; item = list->FindNextChild())
	{
		items++;
		if (item->GetValue() != "UnleashX Xbox Launcher")
			printf("FAIL: title '%s'\n", item->GetValue().c_str());
		if (item->GetAttribute("AltIcon").size() != 0)
			withIcon++;
	}
	if (items != 2 || withIcon != 1)
	{
		printf("FAIL: %d items, %d with AltIcon\n", items, withIcon);
		return 1;
	}
	return 0;
}
