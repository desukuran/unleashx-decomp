// The file and directory object factories. See FileIO.h.
#include <cctype>
#include <cstring>

#include "Drives.h"
#include "FileIO.h"

// Which kind of object a path needs: 'H'ard disk, 'I'SO 9660, 'F'AT32, or
// 0 when the drive isn't there. (inlined in both factories)
static char GetPathFileSystem(const char *path, int *usbDrive)
{
	if (path == NULL || strlen(path) == 0)
		return 0;
	int drive = toupper(path[0]);
	if (path[1] != ':')
		return 0;
	// U: and T: (the title's data) are always the hard disk.
	if (drive == 'U' || drive == 'T')
		return 'H';
	SDriveInfo *info = GetDriveInfo((char)drive);
	if (info == NULL || !info->mounted || !info->hasMedia)
		return 0;
	if (path[0] == 'D' || path[0] == 'd')
	{
		// A data disc; game and video discs read through the kernel.
		if (GetDiscType() == DISC_DATA)
			return 'I';
	}
	else if (drive >= 'P' && drive <= 'S')
	{
		*usbDrive = drive - 'P';
		return 'F';
	}
	return 'H';
}

// 0x001376AF (path in edi)
IFile *CreateFileObject(const char *path)
{
	int usbDrive = 0;
	switch (GetPathFileSystem(path, &usbDrive))
	{
	case 'I':
		return new CFileISO9660;
	case 'F':
		return new CFileFAT32(usbDrive);
	case 'H':
		return new CFileHDD;
	}
	return NULL;
}

// 0x0013D5CF (path in eax)
CDirectory *CreateDirObject(const char *path)
{
	int usbDrive = 0;
	switch (GetPathFileSystem(path, &usbDrive))
	{
	case 'I':
		return new CDirISO9660;
	case 'F':
		return new CDirFAT32(usbDrive);
	case 'H':
		return new CDirHDD;
	}
	return NULL;
}
