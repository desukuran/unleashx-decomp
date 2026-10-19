// Texture loading (jpg/png/bmp/dds through the virtual file layer, so files
// inside zipped skins work too).
#pragma once

#include "../common/Platform.h"

struct IDirect3DDevice8;
struct IDirect3DTexture8;
struct IDirect3DSurface8;

struct SImageInfo
{
	int width;
	int height;
	DWORD unknown[3];
};

extern IDirect3DDevice8 *g_pd3dDevice;   // 0x00277664

// width/height: requested size, or 1 to keep the file's size (TODO: verify).
// Returns FALSE on failure (and *texture = NULL).
BOOL LoadTextureFromFile(const char *path, IDirect3DDevice8 *device, IDirect3DTexture8 **texture,
                         int width, int height, SImageInfo *info);   // 0x0009ECB3 TODO: decompile

// D3DResource_Release (XDK) and clear the pointer.
void ReleaseTexture(IDirect3DTexture8 *&texture);   // inlined everywhere in the original

// The display mode's size (cached: read again when refresh is set or no
// height was cached yet). Either pointer may be NULL. The original takes
// width in esi and height in edi.
void GetDisplaySize(float *width, float *height, BOOL refresh);    // 0x0009F14B TODO: decompile

// The title image ($$XTIMAGE section) of an XBE: an XPR texture or a DDS.
BOOL LoadXbeIcon(const char *xbePath, IDirect3DTexture8 **texture);  // 0x0009FAAF TODO: decompile
// Writes a surface as a .bmp file. TODO: decompile (library code?)
HRESULT SaveSurfaceToBmp(IDirect3DSurface8 *surface, const char *path);   // 0x0016FD40
// The first texture of an .xpr file, copied with black as the color key.
BOOL LoadXprFileTexture(const char *path, IDirect3DTexture8 **texture);   // 0x000A2371 TODO: decompile

// Name -> texture table: the textures the dialogs share ("Keyboard", set
// from the skin's resource at startup).
class CTextureTable
{
public:
	// Not in the table: loads the file name and adds it. (texture in edi)
	HRESULT Find(const char *name, IDirect3DTexture8 **texture);           // 0x00137FA0 TODO: decompile
	HRESULT Add(const char *name, IDirect3DTexture8 *texture, BOOL owned);  // 0x00137F1F TODO: decompile

	BYTE m_reserved[0x10];
};

extern CTextureTable g_textures;   // 0x002784E8
