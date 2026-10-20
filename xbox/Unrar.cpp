// UnleashX's unrar interface (src/thirdparty/Unrar.h: urarlib_* with an
// UnrarSink told about each file) on XBMC's UnrarXLib (thirdparty/unrar,
// unrar 3.51 like the one UnleashX linked). XBMC's version reports files
// through a progress callback instead: the sink hears of each file as it
// starts, and of the last one again if the extraction fails.
#include "../src/thirdparty/Unrar.h"

#include <stdlib.h>
#include <string.h>

// UnrarXLib's interface (UnrarX.hpp), declared here because its
// ArchiveList_struct is a different type from UnleashX's.
struct RAR20_archive_entry
{
	char *Name;
	wchar_t *NameW;
	unsigned short NameSize;
	unsigned long PackSize;
	__int64 UnpSize;
	unsigned char HostOS;
	unsigned long FileCRC;
	unsigned long FileTime;
	unsigned char UnpVer;
	unsigned char Method;
	unsigned long FileAttr;
	__int64 iOffset;
};
struct archivelist
{
	RAR20_archive_entry item;
	archivelist *next;
};
typedef bool (*progress_callback)(void *, int, const char *);
int urarlib_get(char *rarfile, char *targetPath, char *fileToExtract, char *libpassword, __int64 *iOffset,
                progress_callback progress, void *context);
int urarlib_list(char *rarfile, archivelist **ppList, char *libpassword, bool stopattwo);
void urarlib_freelist(archivelist *list);

int urarlib_list(const char *rarFile, ArchiveList_struct **list, const char *password)
{
	*list = NULL;
	archivelist *items = NULL;
	if (!urarlib_list((char *)rarFile, &items, (char *)password, false))
	{
		urarlib_freelist(items);
		return 0;
	}
	int count = 0;
	ArchiveList_struct **tail = list;
	for (archivelist *p = items; p != NULL; p = p->next)
	{
		ArchiveList_struct *entry = (ArchiveList_struct *)calloc(1, sizeof(ArchiveList_struct));
		const RAR20_archive_entry &item = p->item;
		entry->name = _strdup(item.Name != NULL ? item.Name : "");
		entry->nameSize = (WORD)strlen(entry->name);
		entry->packedSize = item.PackSize;
		entry->unpackedSize = (DWORD)item.UnpSize;
		entry->hostOS = item.HostOS;
		entry->crc = item.FileCRC;
		entry->fileTime = item.FileTime;
		entry->unpackVersion = item.UnpVer;
		entry->method = item.Method;
		entry->attributes = item.FileAttr;
		*tail = entry;
		tail = &entry->next;
		count++;
	}
	urarlib_freelist(items);
	return count;
}

void urarlib_freelist(ArchiveList_struct *list)
{
	while (list != NULL)
	{
		ArchiveList_struct *next = list->next;
		free(list->name);
		free(list);
		list = next;
	}
}

struct SSinkContext
{
	UnrarSink *sink;
	char lastFile[MAX_PATH];
};

static bool OnProgress(void *context, int percent, const char *file)
{
	SSinkContext *c = (SSinkContext *)context;
	if (percent != -1 || file == NULL)
		return true;
	strncpy(c->lastFile, file, sizeof(c->lastFile) - 1);
	bool proceed = true;
	if (c->sink != NULL)
		c->sink->OnFile(file, true, &proceed);
	return proceed;
}

int urarlib_get(const char *rarFile, const char *targetPath, const char *fileToExtract, const char *password,
                UnrarSink *sink)
{
	SSinkContext context;
	context.sink = sink;
	memset(context.lastFile, 0, sizeof(context.lastFile));
	int ok = urarlib_get((char *)rarFile, (char *)targetPath, (char *)fileToExtract, (char *)password, NULL,
	                     OnProgress, &context);
	if (!ok && sink != NULL)
	{
		bool proceed = false;
		sink->OnFile(context.lastFile[0] != 0 ? context.lastFile : rarFile, false, &proceed);
	}
	return ok;
}
