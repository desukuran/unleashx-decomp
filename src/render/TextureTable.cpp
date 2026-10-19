// CTextureTable: textures by name (g_textures, the textures the dialogs
// share). A name that isn't in the table is loaded as a texture file.
#include "Texture.h"

#include <cstdlib>
#include <cstring>

#include "../common/XboxKernel.h"

CTextureTable g_textures;   // 0x002784E8

// 0x0002E307
STextureEntry::STextureEntry(const char *name, IDirect3DTexture8 *texture, BOOL owned)
{
	this->owned = owned;
	if (texture == NULL)
	{
		this->texture = NULL;
	}
	else
	{
		texture->AddRef();
		this->texture = texture;
	}
	this->name[0] = '\0';
	// (Not terminated when the name is 255 characters or more.)
	if (name != NULL && strlen(name) != 0)
		strncpy(this->name, name, 0xFF);
	if (this->texture == NULL && strlen(this->name) != 0)
	{
		if (D3DXCreateTextureFromFileA(g_pd3dDevice, name, &this->texture) == S_OK)
			this->owned = TRUE;
		else
			this->texture = NULL;
	}
}

// 0x00137BB9
CTextureTable::~CTextureTable()
{
	for (std::vector<STextureEntry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
	{
		STextureEntry entry = *it;
		if (entry.texture != NULL)
		{
			entry.texture->Release();
			// (An owned texture then gets operator delete(NULL): nothing.)
		}
	}
	m_entries.clear();
}

// 0x00137ACD
// (Each entry is copied to be compared.)
IDirect3DTexture8 *CTextureTable::FindTexture(const char *name)
{
	if (name == NULL || strlen(name) == 0)
		return NULL;
	for (size_t i = 0; i < m_entries.size(); i++)
	{
		if (_mbsicmp(name, m_entries[i].name) == 0)
			return m_entries[i].texture;
	}
	return NULL;
}

// 0x00137B37
STextureEntry *CTextureTable::FindEntry(const char *name)
{
	if (name == NULL || strlen(name) == 0)
		return NULL;
	for (size_t i = 0; i < m_entries.size(); i++)
	{
		if (_mbsicmp(name, m_entries[i].name) == 0)
			return &m_entries[i];
	}
	return NULL;
}

// 0x00137FA0
HRESULT CTextureTable::Find(const char *name, IDirect3DTexture8 **texture)
{
	if (texture == NULL)
		return E_FAIL;
	*texture = FindTexture(name);
	if (*texture == NULL)
	{
		STextureEntry entry(name, NULL, FALSE);
		if (entry.texture == NULL)
			return E_FAIL;
		m_entries.push_back(entry);
		*texture = entry.texture;
	}
	return S_OK;
}

// 0x00137F1F
HRESULT CTextureTable::Add(const char *name, IDirect3DTexture8 *texture, BOOL owned)
{
	if (texture == NULL)
		return E_FAIL;
	STextureEntry *entry = FindEntry(name);
	if (entry != NULL)
	{
		// Quirk: an owned texture is freed with operator delete instead of
		// being released, and the new one isn't AddRef'd (unlike when it's
		// added).
		if (entry->owned)
			operator delete(entry->texture);
		entry->owned = owned;
		entry->texture = texture;
		return S_OK;
	}
	STextureEntry added(name, texture, owned);
	if (added.texture == NULL)
		return E_FAIL;
	m_entries.push_back(added);
	return S_OK;
}
