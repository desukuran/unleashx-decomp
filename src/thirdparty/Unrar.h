// unrarlib as wrapped for the Xbox (urarlib_*). Library code: only the
// interface UnleashX uses is reconstructed. TODO: verify the signatures.
#pragma once

#include "../common/Platform.h"

struct ArchiveList_struct
{
	char *name;                 // +0x00
	BYTE m_unknown04[0x1C];
	DWORD attributes;           // +0x20 FILE_ATTRIBUTE_DIRECTORY for folders
	ArchiveList_struct *next;   // +0x24
};

// Lists the archive; returns the number of entries.
int urarlib_list(const char *rarFile, ArchiveList_struct **list, const char *password);   // 0x00078582
// Extracts fileToExtract (NULL: everything) into targetPath; nonzero on success.
int urarlib_get(const char *rarFile, const char *targetPath, const char *fileToExtract, const char *password,
                void *offset);                                                           // 0x00078811
void urarlib_freelist(ArchiveList_struct *list);                                           // 0x0007850E (list in eax)
