// The menu sound effects. See SoundEffects.h.
#include "SoundEffects.h"

#include <cstdlib>
#include <cstring>

#include "../settings/AppSettings.h"
#include "Sound.h"

CSoundEffects g_soundEffects;   // 0x002784D0

// The sounds that come from <Audio>, by name.
static const char *const s_builtInSounds[SOUND_EFFECT_COUNT] = {
	"Scroll", "Select", "Action Complete", "Ambient", "Keyboard Stroke",
	"Back", "Menu Up", "Menu Down", "Game Save Select", "Memory Slot",
};

// 0x0002E155
void SSoundEffect::Load(const char *path)
{
	if (path == NULL || strlen(path) == 0)
		return;
	if (sound == NULL)
	{
		sound = new CSound;
	}
	else
	{
		if (sound->m_pBuffer != NULL)
		{
			IDirectSoundBuffer_SetCurrentPosition(sound->m_pBuffer, 0);
			sound->Stop();
		}
		sound->Destroy();
	}
	if (sound != NULL && FAILED(sound->Create(path, 0)))
	{
		delete sound;
		sound = NULL;
		return;
	}
	file = strdup(path);
}

// 0x0002E11F
void SSoundEffect::Init(const char *path)
{
	file = path;
	sound = NULL;
	Load(file.c_str());
	if (sound == NULL)
		file.clear();
}

// 0x0013860D
CSoundEffects::CSoundEffects()
{
	m_bQuiet = FALSE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00138492
CSoundEffects::~CSoundEffects()
{
	for (std::map<std::string, SSoundEffect>::iterator it = m_sounds.begin(); it != m_sounds.end(); ++it)
	{
		CSound *sound = it->second.sound;
		if (sound != NULL)
		{
			sound->Stop();
			sound->Destroy();
			delete sound;
		}
	}
	m_sounds.clear();
}

// 0x00137D64
void CSoundEffects::ApplyVolume(CSound *sound)
{
	if (sound == NULL || sound->m_pBuffer == NULL)
		return;
	LONG volume = m_bQuiet ? DSBVOLUME_MIN : m_pSettings->GetSoundVolume();
	IDirectSoundBuffer_SetVolume(sound->m_pBuffer, volume);
}

// 0x001380EF
void CSoundEffects::SetQuiet(BOOL quiet)
{
	if (quiet == m_bQuiet)
		return;
	m_bQuiet = quiet;
	for (std::map<std::string, SSoundEffect>::iterator it = m_sounds.begin(); it != m_sounds.end(); ++it)
	{
		if (it->second.sound != NULL)
			ApplyVolume(it->second.sound);
	}
}

// 0x001381E0
CSound *CSoundEffects::Find(const char *name)
{
	if (m_bDisabled || name == NULL || strlen(name) == 0)
		return NULL;
	if (m_sounds.find(name) == m_sounds.end())
	{
		// A built-in one: loaded from its <Audio> setting now.
		std::string key = name;
		SSoundEffect effect;
		effect.sound = NULL;
		for (int i = 0; i < SOUND_EFFECT_COUNT; i++)
		{
			if (key.compare(s_builtInSounds[i]) == 0)
			{
				effect.Load(m_pSettings->m_audio.m_strSound[i].c_str());
				break;
			}
		}
		if (effect.sound != NULL)
			m_sounds[name] = effect;
	}
	std::map<std::string, SSoundEffect>::iterator it = m_sounds.find(name);
	if (it == m_sounds.end())
		return NULL;
	CSound *sound = it->second.sound;
	if (sound == NULL)
		return NULL;
	ApplyVolume(sound);
	return sound;
}

// 0x00138511
HRESULT CSoundEffects::Load(const char *name, const char *file, CSound **sound)
{
	if (sound == NULL || name == NULL || strlen(name) == 0)
		return E_FAIL;
	*sound = NULL;
	if (m_bDisabled)
		return E_FAIL;
	std::string key = name;
	*sound = Find(name);
	if (*sound == NULL)
	{
		SSoundEffect effect;
		effect.Init(file);
		if (effect.sound == NULL)
			return E_FAIL;
		ApplyVolume(effect.sound);
		m_sounds[key] = effect;
		*sound = effect.sound;
		return S_OK;
	}
	// Known: loaded again from the new file.
	m_sounds[key].Load(file);
	if (m_sounds[key].sound == NULL)
		*sound = NULL;
	return S_OK;
}
