// Starting another XBE. Every launcher maps D: to the XBE's folder first
// (\??\D: -> \Device\Harddisk0\PartitionN\<folder>), so the program finds
// its files on D: as if it had been started from a disc. Then either
// XLaunchNewImage("D:\<name>") or, to pass launch data the program reads at
// boot, the title info is written and the box quick-reboots.
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "Storage.h"
#include "XbeLaunch.h"

// A kernel ANSI_STRING with its own buffer, and a std::string to fill it
// from. 0x24 bytes. (Also used by MapDriveLink.)
class CKernelString
{
public:
	CKernelString()                                       // 0x000A2357 (this in eax)
	{
		m_string.Buffer = NULL;
		Alloc(0, TRUE);
	}
	~CKernelString()                                      // 0x000A1A11 (this in esi)
	{
		Alloc(0, FALSE);
	}
	// A zeroed buffer of size bytes (none for 0); keep: the old buffer is
	// dropped without being freed. FALSE when the allocation fails.
	// (this in esi)
	BOOL Alloc(USHORT size, BOOL keep);                   // 0x0009D427
	std::string &Text() { return m_text; }                // 0x0009D489 (this in eax)
	// Copies s into the buffer, growing it when needed. (this in eax)
	BOOL Set(const char *s);                              // 0x0009EBBF
	// The string, filled from Text(); NULL when that failed. (this in esi)
	PSTRING Get() { return Set(m_text.c_str()) ? &m_string : NULL; }   // 0x0009EC7A

	ANSI_STRING m_string;                  // +0x00
	std::string m_text;                    // +0x08
};

BOOL CKernelString::Alloc(USHORT size, BOOL keep)
{
	if (!keep && m_string.Buffer != NULL)
		delete[] m_string.Buffer;
	m_string.Length = 0;
	m_string.Buffer = NULL;
	m_string.MaximumLength = 0;
	if (size != 0)
	{
		m_string.Buffer = new char[size];
		if (m_string.Buffer == NULL)
			return FALSE;
		memset(m_string.Buffer, 0, size);
		m_string.MaximumLength = size;
	}
	return TRUE;
}

BOOL CKernelString::Set(const char *s)
{
	if (s == NULL)
	{
		Alloc(0, FALSE);
		return FALSE;
	}
	size_t length = strlen(s);
	if (m_string.Buffer == NULL || length + 1 > m_string.MaximumLength)
		Alloc((USHORT)(length + 1), FALSE);
	if (m_string.Buffer == NULL || length >= m_string.MaximumLength)
		return FALSE;
	memcpy(m_string.Buffer, s, length);
	m_string.Length = (USHORT)length;
	m_string.Buffer[length] = '\0';
	return TRUE;
}

// 0x000A39D9
LONG MapDriveLink(const char *link, const char *device, BOOL replace)
{
	CKernelString linkName;
	CKernelString deviceName;
	deviceName.Text() = device;
	linkName.Text() = link;
	if (replace)
		IoDeleteSymbolicLink(linkName.Get());
	return IoCreateSymbolicLink(linkName.Get(), deviceName.Get());
}

// What the launchers return when D: couldn't be mapped (with the status).
#define LAUNCH_LINK_FAILED 0x4000000

// The start the launchers share (inlined into each). Maps D: to path's
// folder; launchPath becomes "D:\<name>" (lowercase) and devicePath the
// folder's kernel path. FALSE, with nothing done, when path has no folder;
// otherwise *status is IoCreateSymbolicLink's result.
static BOOL MapXbeFolder(const char *path, std::string &launchPath, std::string &devicePath, NTSTATUS *status)
{
	if (path == NULL)
		return FALSE;
	std::string lower = StrToLower(path);
	int slash;
	if (lower.size() == 0 || (slash = StrReverseFind(lower, "\\", -1)) == -1)
		return FALSE;
	launchPath = "D:\\";
	launchPath.append(StrMid(lower, slash + 1, (int)lower.size() - (slash + 1)));
	// A root folder's path ends in a backslash that the link mustn't have.
	devicePath = GetXbeDevicePath(path);
	if (StrRight(GetPathDirectory(path), 2) == ":\\" && StrRight(devicePath, 1) == "\\")
		devicePath = StrLeft(devicePath, (int)devicePath.size() - 1);

	CKernelString device;
	device.Text() = devicePath;
	ANSI_STRING link;
	link.Length = 6;
	link.MaximumLength = 7;
	link.Buffer = (PCHAR) "\\??\\D:";   // 0x0025A858
	IoDeleteSymbolicLink(&link);
	*status = IoCreateSymbolicLink(&link, device.Get());
	return TRUE;
}

// 0x000A67D5
DWORD LaunchXbeImage(const char *path, const char *args)
{
	std::string launchPath, devicePath;
	NTSTATUS status;
	if (!MapXbeFolder(path, launchPath, devicePath, &status))
		return 0;
	if (status != 0)
		return status | LAUNCH_LINK_FAILED;

	if (args != NULL && strlen(args) != 0)
	{
		// A command line goes in the launch data, which only survives a
		// reboot into the title.
		std::string fileName = GetPathFileName(path, TRUE);
		SXbeLaunchData data;
		memset(&data, 0, sizeof(data));
		data.magic = XBE_LAUNCH_ARGS_MAGIC;
		strcpy(data.path, args);
		data.type = XBE_LAUNCH_ARGS;
		SXbeInfo info;
		info.titleId = 0;
		info.gameRegion = 0;
		ReadXbeInfo(path, &info);
		XWriteTitleInfoAndRebootA(fileName.c_str(), devicePath.c_str(), LDT_TITLE, info.titleId,
		                          (PLAUNCH_DATA)&data);
	}
	return XLaunchNewImageA(launchPath.c_str(), NULL);
}

// 0x000A648C
DWORD LaunchXbeWithData(const char *path, LAUNCH_DATA *launchData)
{
	if (launchData == NULL)
		return 0;
	std::string launchPath, devicePath;
	NTSTATUS status;
	if (!MapXbeFolder(path, launchPath, devicePath, &status))
		return 0;
	if (status != 0)
		return status | LAUNCH_LINK_FAILED;

	std::string fileName = GetPathFileName(path, TRUE);
	SXbeInfo info;
	info.titleId = 0;
	info.gameRegion = 0;
	ReadXbeInfo(path, &info);
	DWORD type = LDT_TITLE;
	if (((SXbeLaunchData *)launchData)->type == XBE_LAUNCH_FROM_UPDATE)
		type = LDT_FROM_UPDATE;
	// Debugging leftovers on the cache drive.
	std::string text;
	StrFormat(text, "%s %u", path, type);
	WriteTextFile("Z:\\LaunchXbe.txt", text.c_str(), FALSE);
	if (!FileExists(path))
		WriteTextFile("Z:\\NotExist.txt", text.c_str(), FALSE);
	XWriteTitleInfoAndRebootA(fileName.c_str(), devicePath.c_str(), type, info.titleId, launchData);
	return 0;
}

// 0x000A6AFF
DWORD LaunchXbeWithLaunchData(const char *path, LAUNCH_DATA *launchData)
{
	if (path == NULL || strlen(path) == 0)
		return XLaunchNewImageA(NULL, launchData);
	std::string launchPath, devicePath;
	NTSTATUS status;
	if (!MapXbeFolder(path, launchPath, devicePath, &status))
		return 0;
	if (status != 0)
		return status | LAUNCH_LINK_FAILED;
	return XLaunchNewImageA(launchPath.c_str(), launchData);
}

// 0x000A6EB0
DWORD LaunchDashboardMusic(const char *path)
{
	LD_LAUNCH_DASHBOARD data;
	memset(&data, 0, sizeof(data));
	data.dwReason = XLD_LAUNCH_DASHBOARD_MUSIC;
	return LaunchXbeWithLaunchData(path, (PLAUNCH_DATA)&data);
}

// 0x000A6EE7 (no callers)
DWORD LaunchDashboardSettings(const char *path)
{
	LD_LAUNCH_DASHBOARD data;
	memset(&data, 0, sizeof(data));
	data.dwReason = XLD_LAUNCH_DASHBOARD_SETTINGS;
	return LaunchXbeWithLaunchData(path, (PLAUNCH_DATA)&data);
}

// 0x000A6F24 (no callers)
DWORD LaunchDashboardMemory(const char *path)
{
	LD_LAUNCH_DASHBOARD data;
	memset(&data, 0, sizeof(data));
	data.dwReason = XLD_LAUNCH_DASHBOARD_MEMORY;
	data.dwParameter1 = 'U';
	data.dwParameter2 = 0x10;   // TODO: verify (the unit's size or slot?)
	return LaunchXbeWithLaunchData(path, (PLAUNCH_DATA)&data);
}
