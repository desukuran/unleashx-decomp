// Starting another XBE (0x000A648C-0x000A6F7A): D: is mapped to the XBE's
// folder and the kernel is asked to run D:\<name>. See XbeLaunch.cpp.
#pragma once

#include "../common/Platform.h"

// UnleashX's own use of the 0xC00 bytes of launch data, read by the started
// program (UXUpdate.xbe, or anything that knows the format).
struct SXbeLaunchData
{
	DWORD magic;               // +0x000 XBE_LAUNCH_ARGS_MAGIC, or the caller's title ID (Live Update)
	char path[0x12C];          // +0x004 the command line, or a path
	char name[0x1C4];          // +0x130 a file name (Live Update)
	BYTE type;                 // +0x2F4 XBE_LAUNCH_ARGS, XBE_LAUNCH_FROM_UPDATE
	BYTE reserved[0x90B];
};

#define XBE_LAUNCH_ARGS_MAGIC 0xEE456777
#define XBE_LAUNCH_ARGS 1
#define XBE_LAUNCH_FROM_UPDATE 4   // launched as LDT_FROM_UPDATE

// Starts path, with args as its command line (SXbeLaunchData) when there
// are any. Returns only on failure: 0 when the path has no folder, else the
// error (a symbolic link failure has 0x4000000 set).
DWORD LaunchXbeImage(const char *path, const char *args);                 // 0x000A67D5
// Writes the title info so path starts with launchData after a quick
// reboot (type LDT_FROM_UPDATE when launchData's type byte says so). Also
// leaves Z:\LaunchXbe.txt (and Z:\NotExist.txt) behind for debugging.
DWORD LaunchXbeWithData(const char *path, LAUNCH_DATA *launchData);       // 0x000A648C
// XLaunchNewImage of path with launchData; an empty path starts the
// dashboard.
DWORD LaunchXbeWithLaunchData(const char *path, LAUNCH_DATA *launchData); // 0x000A6AFF
// Starts the dashboard at path with LD_LAUNCH_DASHBOARD reasons: the music
// screen (an audio CD), the settings, and the memory screen for U:. Only
// the first one is used.
DWORD LaunchDashboardMusic(const char *path);                             // 0x000A6EB0
DWORD LaunchDashboardSettings(const char *path);                          // 0x000A6EE7
DWORD LaunchDashboardMemory(const char *path);                            // 0x000A6F24
