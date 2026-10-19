#include "Drives.h"

BOOL g_bDriveFMapped;  // 0x0026BBF4
BOOL g_bDriveGMapped;  // 0x0027637C

// 0x000AB8F3
void SetDriveFMapped(BOOL mapped)
{
	if (g_bDriveFMapped == mapped)
		return;
	if (!mapped)
		UnmountDrive('F');
	else
		MountDrive('F');
	g_bDriveFMapped = mapped;
}

// 0x000AB8D1
void SetDriveGMapped(BOOL mapped)
{
	if (g_bDriveGMapped == mapped)
		return;
	if (!mapped)
		UnmountDrive('G');
	else
		MountDrive('G');
	g_bDriveGMapped = mapped;
}
