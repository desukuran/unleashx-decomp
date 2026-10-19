// Texture loading (jpg/png/bmp/dds through the virtual file layer, so files
// inside zipped skins work too).
#pragma once

#include "../common/Platform.h"
#include "D3D.h"

struct IDirect3DDevice8;
struct IDirect3DTexture8;
struct IDirect3DSurface8;

// D3DXIMAGE_INFO (Xbox): the file's own picture.
struct SImageInfo
{
	int width;
	int height;
	DWORD depth;
	DWORD mipLevels;
	D3DFORMAT format;
};

extern IDirect3DDevice8 *g_pd3dDevice;   // 0x00277664

// Reads the file (through the virtual file layer) and makes a texture of
// its own size; if that fails, one that fits the display mode (width and
// height below 2: the picture's size), and then a 16-bit one that fits
// the TV. Big pictures are DXT3 with <TextureCompression>. info gets the
// picture's own size. FALSE on failure (and *texture = NULL). (path in ecx)
BOOL LoadTextureFromFile(const char *path, IDirect3DDevice8 *device, IDirect3DTexture8 **texture,
                         int width, int height, SImageInfo *info);   // 0x0009ECB3

// D3DResource_Release (XDK) and clear the pointer.
void ReleaseTexture(IDirect3DTexture8 *&texture);   // inlined everywhere in the original

// The display mode's size (cached: read again when refresh is set or no
// height was cached yet). Either pointer may be NULL. The original takes
// width in esi and height in edi.
void GetDisplaySize(float *width, float *height, BOOL refresh);    // 0x0009F14B
// A 640x480 copy of the front or back buffer into *texture (released
// first). (texture in eax)
HRESULT CaptureBackBuffer(IDirect3DTexture8 **texture, BOOL backBuffer);   // 0x0009F19C

// The start of an .xbx (and of an XBE's XPR title image): the XPR header
// with one texture's resource header. 0x24 bytes.
struct SXbxHeader
{
	DWORD magic;          // "XPR0" or "XPR1"
	DWORD totalSize;
	DWORD headerSize;     // 0x800: the data starts there
	D3DResource texture;  // Common, Data, Lock
	DWORD format;
	DWORD size;
	DWORD endMarker;      // 0xFFFFFFFF
};

// The title image ($$XTIMAGE section) of an XBE: an XPR texture or a DDS.
// (XbeImage.cpp)
BOOL LoadXbeIcon(const char *xbePath, IDirect3DTexture8 **texture);  // 0x0009FAAF
// Writes a copy of texture's top level as an .xpr file (creating its
// folder). (texture in a register; XbeImage.cpp)
BOOL SaveTextureToXpr(const char *path, IDirect3DTexture8 *texture);   // 0x000A4225
// A texture from an .xpr file holding one texture (the MS dashboard's .xbx
// files), read straight into it.
bool LoadXprTexture(const char *path, IDirect3DTexture8 **texture);   // 0x0009F909
// The first texture of an .xpr file, copied with black as the color key.
// (path in eax)
BOOL LoadXprFileTexture(const char *path, IDirect3DTexture8 **texture);   // 0x000A2371

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
