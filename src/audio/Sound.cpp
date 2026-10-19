// Sound effects. Only PlaySound is decompiled; CSound is declared in
// Sound.h.
#include "Sound.h"

// 0x0009F12E (sound in esi)
void PlaySound(CSound *sound)
{
	if (sound == NULL || sound->m_pBuffer == NULL)
		return;
	sound->m_pBuffer->SetCurrentPosition(0);
	sound->Play(0);
}
