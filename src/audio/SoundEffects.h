// The menu sound effects (0x001380EF-0x0013860D): named sounds loaded on
// first use, the built-in names from the <Audio> sound settings. See
// SoundEffects.cpp.
#pragma once

#include <map>
#include <string>

#include "../common/Platform.h"

class CAppSettings;
class CSound;

// A loaded sound and its file. 0x20 bytes.
struct SSoundEffect
{
	// Loads file (the name stays empty when it fails). (this in esi)
	void Init(const char *file);                          // 0x0002E11F
	// (Re)loads the sound from file. (Quirk: the name is a _strdup copy
	// that's never freed.) (this on the stack)
	void Load(const char *file);                          // 0x0002E155

	std::string file;                      // +0x00
	CSound *sound;                         // +0x1C
};

class CSoundEffects
{
public:
	CSoundEffects();                                      // 0x0013860D (this in edi)
	// Stops and frees the sounds. (this in eax)
	~CSoundEffects();                                     // 0x00138492
	// Below a sound volume of 5 the sounds are silenced. (quiet in eax,
	// this in esi)
	void SetQuiet(BOOL quiet);                            // 0x001380EF
	// The named sound ("Scroll", "Select", "Action Complete", "Ambient",
	// "Keyboard Stroke", "Back", "Menu Up", "Menu Down", "Game Save
	// Select", "Memory Slot" come from the settings; others must have
	// been loaded), at the current volume; NULL when disabled or unknown.
	CSound *Find(const char *name);                       // 0x001381E0
	// (Re)loads the named sound from file. (this in eax)
	HRESULT Load(const char *name, const char *file, CSound **sound);   // 0x00138511

	CAppSettings *m_pSettings;             // +0x00
	BOOL m_bDisabled;                      // +0x04 !<EnableSoundEffect>
	BOOL m_bQuiet;                         // +0x08 sound volume below 5
	std::map<std::string, SSoundEffect> m_sounds;   // +0x0C

private:
	// The sound volume, or silence when quiet. (this in ecx, sound in eax)
	void ApplyVolume(CSound *sound);                      // 0x00137D64
};

extern CSoundEffects g_soundEffects;   // 0x002784D0
