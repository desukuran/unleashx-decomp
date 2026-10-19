// The game folder scan: <List Path="..."> lists are filled with the XBEs
// found under the path, cached in items.xml. TODO: decompile
#pragma once

#include "../common/Platform.h"

class CXmlElement;

// Adds a <Folder Path=...> to the cache for each <List Path=...> of the menu.
void CollectMenuFolders(CXmlElement *menu, CXmlElement *cache);          // 0x000E1F23
// Scans path (depth levels deep) for files named like pattern
// ("default.xbe") and adds them to the folder as <Item>s.
void ScanFolder(CXmlElement *folder, const char *path, const char *pattern, int depth,
                BOOL delayLoad);                                         // 0x000E4459
// Copies the cached folder items into the matching menu lists.
void MergeFolderItems(CXmlElement *cache, CXmlElement *menu);            // 0x000E28D5
// Removes (recursively) items whose XBE doesn't exist.
void RemoveMissingXbes(CXmlElement *list);                               // 0x00110A15
