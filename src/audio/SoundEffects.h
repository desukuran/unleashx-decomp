// Menu sound effects.
#pragma once

#include "../common/Platform.h"

class CSoundEffects
{
public:
	void SetQuiet(BOOL quiet);   // 0x001380EF TODO: decompile

	int m_unknown00;             // +0x00
	BOOL m_bDisabled;            // +0x04 !<EnableSoundEffect>
	BOOL m_bQuiet;               // +0x08 sound volume below 5
};

extern CSoundEffects g_soundEffects;   // 0x002784D0
