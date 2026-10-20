// The folder walk that fills a file operation's list (see FileOps.h).
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "FileIO.h"
#include "FileOps.h"

// 0x000132B3 (this in esi)
SFileListItem::SFileListItem(const std::string &itemPath, BOOL folder, DWORD itemSize)
	: path(itemPath), isFolder(folder), size(itemSize)
{
}

// 0x000F57E4 (path, list, dir on the stack; LTCG copies at 0x00082AB0,
// 0x000A5705, 0x00109980, 0x0012889A)
static void CollectFolder(const char *path, std::vector<SFileListItem> *list, CDirectory *dir)
{
	if (g_bCancelOperation)
		return;
	if (list == NULL)
		list = &g_fileList;
	std::string folder = path;
	if (path == NULL || folder.size() == 0)
		return;
	StrReplaceAll(folder, "\\\\?\\", "");
	if (StrRight(folder, 1).compare("\\") != 0)
		folder.append("\\");
	char pattern[0x10C];
	strcpy(pattern, folder.c_str());
	strcat(pattern, "*");

	// The files first...
	WIN32_FIND_DATAA data;
	HANDLE find = dir->FindFirst(pattern, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (g_bCancelOperation)
				break;
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && strlen(data.cFileName) != 0)
			{
				std::string file = folder;
				file.append(data.cFileName);
				list->push_back(SFileListItem(std::string(file.c_str()), FALSE, data.nFileSizeLow));
				g_ullOperationTotal += data.nFileSizeLow;
				g_ullOperationBytes += data.nFileSizeLow;
				g_nOperationFiles++;
			}
		} while (dir->FindNext(find, &data) == TRUE);
		dir->FindClose(find);
	}

	// ...then the folders, each followed by its contents.
	find = dir->FindFirst(pattern, &data);
	if (find != INVALID_HANDLE_VALUE)
	{
		// A CDirectory whose search handle is always 1 keeps one search at
		// a time, so after each subfolder the search is started again and
		// walked back to where it was.
		BOOL sharedSearch = (find == (HANDLE)1);
		do
		{
			if (g_bCancelOperation)
				break;
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				std::string sub = folder;
				sub.append(data.cFileName);
				g_nOperationFolders++;
				memset(g_szOperationFile, 0, sizeof(g_szOperationFile));
				swprintf(g_szOperationFile, 0x12F, L"%hs", sub.c_str());
				list->push_back(SFileListItem(std::string(sub.c_str()), TRUE, 0));
				CollectFolder(sub.c_str(), list, dir);
				if (sharedSearch)
				{
					sub = data.cFileName;
					dir->FindFirst(pattern, &data);
					do
					{
						if (sub.compare(data.cFileName) == 0)
							break;
					} while (dir->FindNext(find, &data));
				}
			}
		} while (dir->FindNext(find, &data) == TRUE);
		dir->FindClose(find);
	}
	if (g_bCancelOperation)
		list->clear();
}

// 0x000F5B4B (LTCG copies: 0x00084186, 0x000A6436, 0x0010A5C6, 0x001292EF)
void CollectFiles(const char *path, std::vector<SFileListItem> *list)
{
	if (g_bCancelOperation)
		return;
	if (list == NULL)
		list = &g_fileList;
	CDirectory *dir = CreateDirObject(path);
	if (dir == NULL)
		return;
	CollectFolder(path, list, dir);
	delete dir;
	g_dwOperationRemaining = 0;
	g_dwOperationElapsed = 0;
	g_dwOperationUnknown640 = 0;
}
