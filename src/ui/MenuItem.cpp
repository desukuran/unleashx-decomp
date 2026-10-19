#include "MenuItem.h"

#include <cstring>
#include <map>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../render/D3D.h"
#include "../render/Texture.h"

// Icons by lowercased path, shared by all menu items. Each entry holds one
// reference.
static std::map<std::string, IDirect3DTexture8 *> s_iconCache;   // 0x0027794C

// 0x000220C2
// Quirk: the icon is only kept when the item has text, and m_pszText isn't
// set at all without text (it was left uninitialized).
CMenuItem::CMenuItem(const char *text, const char *iconPath, BOOL isList)
{
	m_pElement = NULL;
	m_pTexture = NULL;
	m_pszText = NULL;
	m_strIcon.clear();
	m_unknown24 = TRUE;
	m_bIsList = FALSE;
	if (text != NULL && strlen(text) != 0)
	{
		size_t len = strlen(text);
		m_pszText = new wchar_t[len + 1];
		memset(m_pszText, 0, len * sizeof(wchar_t));
		StrUtf8ToWide(text, m_pszText, (unsigned)len);
		// The original strdup()s the path to lowercase it and never frees it.
		m_strIcon = iconPath == NULL ? std::string("") : StrToLower(iconPath);
	}
	m_bIsList = isList;
}

// 0x00022183
CMenuItem::~CMenuItem()
{
	delete[] m_pszText;
	m_pszText = NULL;
	if (m_pTexture != NULL)
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}
	m_pElement = NULL;
	m_strIcon.clear();
}

// Reads an .xbx (an XPR with a single texture) straight into a new texture.
// Returns FALSE if the file isn't one; *texture may still have been set.
static BOOL LoadXbxIcon(HANDLE file, IDirect3DTexture8 **texture)
{
	// XPR header followed by the texture's resource header.
	struct
	{
		XPR_HEADER xpr;
		D3DResource texture;   // D3DTexture: Common, Data, Lock, Format, Size
		DWORD format;
		DWORD size;
		DWORD endMarker;       // 0xFFFFFFFF
	} header;
	memset(&header, 0, sizeof(header));
	DWORD read;
	BOOL ok = ReadFile(file, &header, 0x24, &read, NULL);
	ok &= read == 0x24 && (header.xpr.dwMagic == XPR0_MAGIC || header.xpr.dwMagic == XPR1_MAGIC) &&
	      header.xpr.dwHeaderSize == 0x800 && header.endMarker == 0xFFFFFFFF &&
	      header.texture.Common < 0x828480 && D3DResource_GetType(&header.texture) == D3DRTYPE_TEXTURE;
	if (!ok)
		return FALSE;

	D3DSURFACE_DESC desc;
	Get2DSurfaceDesc(&header.texture, 0, &desc);
	if (desc.Height == 0)
	{
		desc.Height = 128;
		desc.Width = 128;
		desc.Format = D3DFMT_DXT1;
	}
	*texture = NULL;
	g_pd3dDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, 0, texture);
	if (*texture == NULL)
		return TRUE;   // handled, but no texture

	D3DLOCKED_RECT locked;
	(*texture)->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
	SetFilePointer(file, 0x800, NULL, FILE_BEGIN);
	DWORD dataSize = header.xpr.dwTotalSize - header.xpr.dwHeaderSize;
	if (!ReadFile(file, locked.pBits, dataSize, &read, NULL) || read != dataSize)
		*texture = NULL;   // sic: the texture is leaked
	return TRUE;
}

// 0x0002220D
void CMenuItem::LoadIcon()
{
	if (m_pTexture != NULL || m_strIcon.size() == 0)
		return;

	if (StrFind(m_strIcon, ":") == -1)
		ResolvePath(m_strIcon.c_str(), m_strIcon);
	if (!FileExists(m_strIcon.c_str()))
	{
		m_strIcon.clear();
		return;
	}

	std::string key = StrToLower(m_strIcon);
	std::map<std::string, IDirect3DTexture8 *>::iterator it = s_iconCache.find(key);
	if (it != s_iconCache.end())
	{
		m_pTexture = s_iconCache[key];
		if (m_pTexture != NULL)
		{
			m_pTexture->AddRef();
			return;
		}
	}

	if (StrCmpNoCase(StrRight(key, 4), ".xbx") == 0)
	{
		HANDLE file = CreateFileA(m_strIcon.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			BOOL handled = LoadXbxIcon(file, &m_pTexture);
			if (handled)
			{
				if (m_pTexture == NULL)
					m_strIcon.clear();
				CloseHandle(file);
			}
			else
			{
				// Not a single-texture XPR: let D3DX try, then the packed
				// resource loader.
				CloseHandle(file);
				if (g_pd3dDevice != NULL &&
				    D3DXCreateTextureFromFileExA(g_pd3dDevice, key.c_str(), (UINT)-1, (UINT)-1, 1, 0,
				                                 (D3DFORMAT)-1, 1, 1, 1, 0, NULL, NULL, &m_pTexture) == S_OK)
				{
					Sleep(1);
				}
				else if (!LoadXprFileTexture(m_strIcon.c_str(), &m_pTexture))
				{
					m_pTexture = NULL;
					m_strIcon.clear();
				}
			}
		}
	}
	else
	{
		BOOL loaded;
		if (StrCmpNoCase(StrRight(key, 4), ".xbe") == 0)
			loaded = LoadXbeIcon(key.c_str(), &m_pTexture);
		else
			loaded = LoadTextureFromFile(m_strIcon.c_str(), g_pd3dDevice, &m_pTexture, 128, 128, NULL);
		if (!loaded)
		{
			m_pTexture = NULL;
			m_strIcon.clear();
		}
	}

	if (m_pTexture != NULL)
	{
		s_iconCache[key] = m_pTexture;
		m_pTexture->AddRef();
	}
	else
	{
		m_strIcon.clear();
	}
}

void FreeIconCache()
{
	for (std::map<std::string, IDirect3DTexture8 *>::iterator it = s_iconCache.begin(); it != s_iconCache.end(); ++it)
	{
		if (it->second != NULL)
			it->second->Release();
	}
	s_iconCache.clear();
}
