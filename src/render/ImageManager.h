// CImageManager: textures shared by file name (the game manager's title
// and save images). A texture is loaded once; later loads of the same
// file add a reference. See ImageManager.cpp.
#pragma once

#include <map>
#include <string>

#include "../common/Platform.h"

struct IDirect3DTexture8;

// 0x10 bytes (vtable and a map of file name to texture).
class CImageManager
{
public:
	CImageManager() {}                                    // 0x0013A57B (this in edi)
	virtual ~CImageManager();                             // 0x0013A58E

	// The texture of path, loaded at width x height (0 = the file's size)
	// on first use. Quirk: the first caller gets the cache's own reference;
	// later ones get one added. (this in ecx)
	IDirect3DTexture8 *Load(const char *path, UINT width, UINT height);   // 0x0013A38E
	// The cached texture of path, or NULL; no reference added. (this in
	// eax) Unused.
	IDirect3DTexture8 *Find(const char *path);            // 0x0013A320
	// Releases path's texture and forgets it; FALSE if it isn't cached.
	// (this in ebx)
	BOOL Remove(const char *path);                        // 0x0013A2C5

	std::map<std::string, IDirect3DTexture8 *> m_textures;   // +0x04
};

extern CImageManager *g_pImageManager;   // 0x002761E0
