// Host stand-ins for rendering/media pieces the skin code calls.
#include <cstdio>
#include <cstring>

#include "../src/media/WMVideoPlayer.h"
#include "../src/render/Fonts.h"
#include "../src/system/Storage.h"
#include "../src/render/D3D.h"
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
void GetDisplaySize(float *width, float *height, BOOL)
{
	if (width != NULL)
		*width = 640.0f;
	if (height != NULL)
		*height = 480.0f;
}

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
BOOL CWMVideoPlayer::Init(CXBFontEx *) { return S_OK; }
BOOL CWMVideoPlayer::IsOpen() { return FALSE; }
BOOL CWMVideoPlayer::Render() { return S_OK; }
HRESULT CWMVideoPlayer::ProcessInput(int *, SGamepadState **) { return S_OK; }
void CWMVideoPlayer::SetFullScreen(BOOL) {}
void CWMVideoPlayer::SetAudio(BOOL) {}
void CWMVideoPlayer::SetRect(int, int, int, int) {}
BOOL CWMVideoPlayer::Open(const char *) { return TRUE; }


// XBE title images (Xbox-only XbeImage.cpp): FTP SITE TITLES, the game scan
BOOL LoadXbeIcon(const char *, IDirect3DTexture8 **texture) { *texture = NULL; return FALSE; }
BOOL ExtractXbeTitleImage(const char *, const char *) { return FALSE; }
HRESULT XGWriteSurfaceToFile(IDirect3DSurface8 *, const char *) { return E_FAIL; }
void Get2DSurfaceDesc(D3DResource *, DWORD, D3DSURFACE_DESC *desc) { memset(desc, 0, sizeof(*desc)); }
HRESULT IDirect3DDevice8::CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, DWORD, IDirect3DTexture8 **texture)
{
	*texture = NULL;
	return E_FAIL;
}
HRESULT IDirect3DTexture8::GetSurfaceLevel(UINT, IDirect3DSurface8 **surface) { *surface = NULL; return E_FAIL; }
ULONG IDirect3DTexture8::Release() { return 0; }
ULONG IDirect3DSurface8::Release() { return 0; }
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface8 *, const void *, const void *, IDirect3DSurface8 *, const void *,
                                   const void *, DWORD, D3DCOLOR)
{
	return E_FAIL;
}
