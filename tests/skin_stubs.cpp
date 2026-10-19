// Host stand-ins for rendering/media pieces the skin code calls.
#include <cstdio>

#include "../src/media/WMVideoPlayer.h"
#include "../src/render/Fonts.h"
#include "../src/render/Texture.h"
#include "../src/skin/Skin.h"
#include "../src/skin/SkinColors.h"

IDirect3DDevice8 *g_pd3dDevice;
CFontTable g_fonts;

BOOL LoadTextureFromFile(const char *path, IDirect3DDevice8 *, IDirect3DTexture8 **texture, int, int, SImageInfo *)
{
	printf("[stub] LoadTexture(%s)\n", path);
	*texture = NULL;
	return FALSE;
}
void ReleaseTexture(IDirect3DTexture8 *&texture) { texture = NULL; }
float GetScreenWidth(int) { return 640.0f; }

CXBFont::CXBFont() {}
CXBFont::~CXBFont() {}
HRESULT CXBFont::Create(const char *fileName, DWORD) { printf("[stub] CXBFont::Create(%s)\n", fileName); return S_OK; }
void CXBFont::SetScaleFactors(float, float) {}
CXBFontEx::CXBFontEx() {}
HRESULT CXBFontEx::CreateFromTTF(const wchar_t *, int, int, int) { return E_FAIL; }
CFontTable::CFontTable() {}
CFontTable::~CFontTable() {}
HRESULT CFontTable::Find(const char *, CXBFont **font) { *font = NULL; return E_FAIL; }
HRESULT CFontTable::Add(const char *name, CXBFont *, BOOL) { printf("[stub] font %s\n", name); return S_OK; }

CWMVideoPlayer::CWMVideoPlayer() {}
CWMVideoPlayer::~CWMVideoPlayer() {}
void CWMVideoPlayer::SetFont(CXBFont *) {}
void CWMVideoPlayer::Unknown2() {}
void CWMVideoPlayer::Unknown3() {}
HRESULT CWMVideoPlayer::Update(BOOL *, int) { return S_OK; }
void CWMVideoPlayer::SetFullScreen(BOOL) {}
void CWMVideoPlayer::SetAudio(BOOL) {}
void CWMVideoPlayer::SetRect(int, int, int, int) {}
BOOL CWMVideoPlayer::Open(const char *) { return TRUE; }

