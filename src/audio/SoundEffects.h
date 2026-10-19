// Menu sound effects.
#pragma once

#include "../common/Platform.h"

class CSound;

class CSoundEffects
{
public:
	void SetQuiet(BOOL quiet);   // 0x001380EF TODO: decompile
	// The named sound of the skin/sound set ("Select", "Back", "Menu Up",
	// "Menu Down", ...), or NULL.
	CSound *Find(const char *name);   // 0x001381E0 TODO: decompile
	// (Re)loads the named sound from file. (this in eax) TODO: decompile
	HRESULT Load(const char *name, const char *file, CSound **sound);   // 0x00138511

	int m_unknown00;             // +0x00
	BOOL m_bDisabled;            // +0x04 !<EnableSoundEffect>
	BOOL m_bQuiet;               // +0x08 sound volume below 5
};

extern CSoundEffects g_soundEffects;   // 0x002784D0
