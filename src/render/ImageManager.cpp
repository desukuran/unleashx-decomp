// CImageManager (0x0013A2C5-0x0013A5F2). See ImageManager.h.
#include "ImageManager.h"

#include "../common/StrUtil.h"
#include "D3D.h"
#include "Texture.h"

CImageManager *g_pImageManager;   // 0x002761E0

// 0x0013A58E
CImageManager::~CImageManager()
{
	for (std::map<std::string, IDirect3DTexture8 *>::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
	{
		if (it->second != NULL)
			it->second->Release();
	}
	m_textures.clear();
}

// 0x0013A38E
IDirect3DTexture8 *CImageManager::Load(const char *path, UINT width, UINT height)
{
	std::map<std::string, IDirect3DTexture8 *>::iterator it = m_textures.find(path);
	if (it != m_textures.end())
	{
		if (it->second != NULL)
			it->second->AddRef();
		return it->second;
	}
	if (g_pd3dDevice->IsBusy())
		Sleep(1);
	IDirect3DTexture8 *texture = NULL;
	if (StrCmpNoCase(StrRight(std::string(path), 4), ".xbx") == 0)
	{
		// An .xbx: an .xpr image, else anything D3DX reads (at the asked
		// size, then at the file's own).
		if (!LoadXprTexture(path, &texture) && !LoadXprFileTexture(path, &texture))
		{
			if (FAILED(D3DXCreateTextureFromFileExA(g_pd3dDevice, path, width != 0 ? width : D3DX_DEFAULT,
			                                        height != 0 ? height : D3DX_DEFAULT, 1, 0, (D3DFORMAT)-1, 1,
			                                        D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &texture)) &&
			    FAILED(D3DXCreateTextureFromFileExA(g_pd3dDevice, path, D3DX_DEFAULT, D3DX_DEFAULT, 1, 0,
			                                        (D3DFORMAT)-1, 1, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL,
			                                        NULL, &texture)))
				texture = NULL;
		}
	}
	else if (!LoadTextureFromFile(path, g_pd3dDevice, &texture, width, height, NULL))
	{
		texture = NULL;
	}
	if (texture != NULL)
		m_textures[path] = texture;
	return texture;
}

// 0x0013A320
IDirect3DTexture8 *CImageManager::Find(const char *path)
{
	std::map<std::string, IDirect3DTexture8 *>::iterator it = m_textures.find(path);
	return it != m_textures.end() ? it->second : NULL;
}

// 0x0013A2C5
BOOL CImageManager::Remove(const char *path)
{
	std::map<std::string, IDirect3DTexture8 *>::iterator it = m_textures.find(path);
	if (it == m_textures.end())
		return FALSE;
	if (it->second != NULL)
		it->second->Release();
	m_textures.erase(it);
	return TRUE;
}
