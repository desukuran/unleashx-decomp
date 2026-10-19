// Drive letter mapping (C:, E:, F:, G:, X:, Y:, Z: symbolic links).
#pragma once

#include "../common/Platform.h"

BOOL MountDrive(char drive);       // 0x000AAC8B TODO: decompile
void UnmountDrive(char drive);     // 0x000AA99A TODO: decompile

// F: and G: are optional (EnableDriveF/EnableDriveG); these track whether
// they're mapped and mount/unmount on change.
extern BOOL g_bDriveFMapped;       // 0x0026BBF4
extern BOOL g_bDriveGMapped;       // 0x0027637C

inline BOOL IsDriveFMapped()       // 0x000AA690
{
	return g_bDriveFMapped;
}

inline BOOL IsDriveGMapped()       // 0x000AA68A
{
	return g_bDriveGMapped;
}

void SetDriveFMapped(BOOL mapped); // 0x000AB8F3
void SetDriveGMapped(BOOL mapped); // 0x000AB8D1
