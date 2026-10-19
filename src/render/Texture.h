// Texture loading (jpg/png/bmp/dds through the virtual file layer, so files
// inside zipped skins work too).
#pragma once

#include "../common/Platform.h"

struct IDirect3DDevice8;
struct IDirect3DTexture8;

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

// Width of the current render target in pixels.
float GetScreenWidth(int unused);                                   // 0x0009F14B TODO: decompile
