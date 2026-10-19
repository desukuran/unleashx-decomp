// CFontTable: fonts by name. The global g_fonts holds the built-in fonts
// (and the skin's, see Resources.cpp); a skin's <Fonts> get a table of
// their own. A name that isn't in the table but names an existing file is
// loaded as a bitmap font (.xpr).
#include "Fonts.h"

#include <cstring>

#include "../common/FileUtil.h"
#include "../common/XboxKernel.h"

CFontTable g_fonts;   // 0x002784C0

// 0x0002E245
SFontEntry::SFontEntry(const char *name, CXBFont *font, BOOL owned)
{
	this->owned = owned;
	this->font = font;
	this->name[0] = '\0';
	// (Not terminated when the name is 255 characters or more.)
	if (name != NULL && strlen(name) != 0)
		strncpy(this->name, name, 0xFF);
	if (this->font == NULL && strlen(this->name) != 0)
	{
		this->font = new CXBFont;
		if (this->font != NULL && FAILED(this->font->Create(this->name, 0)))
		{
			delete this->font;
			this->font = NULL;
			return;
		}
		this->owned = TRUE;
	}
}

// 0x00137DF6
CFontTable::CFontTable()
{
	m_entries.reserve(3);
}

// 0x00137D05
// Quirk: a CXBFontEx is deleted as a CXBFont (the destructor isn't
// virtual), so its resource and TrueType font stay allocated.
CFontTable::~CFontTable()
{
	for (std::vector<SFontEntry>::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
	{
		SFontEntry entry = *it;
		if (entry.owned && entry.font != NULL)
		{
			entry.font->Destroy();
			delete entry.font;
		}
	}
	m_entries.clear();
}

// 0x00137C19
// (Each entry is copied to be compared.)
CXBFont *CFontTable::FindFont(const char *name)
{
	if (name == NULL || strlen(name) == 0)
		return NULL;
	for (size_t i = 0; i < m_entries.size(); i++)
	{
		if (_mbsicmp(name, m_entries[i].name) == 0)
			return m_entries[i].font;
	}
	return NULL;
}

// 0x00137C83
SFontEntry *CFontTable::FindEntry(const char *name)
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

// 0x00138089
HRESULT CFontTable::Find(const char *name, CXBFont **font)
{
	if (font == NULL)
		return E_FAIL;
	*font = FindFont(name);
	if (*font == NULL)
	{
		if (!FileExists(name))
			return E_FAIL;
		SFontEntry entry(name, NULL, FALSE);
		if (entry.font == NULL)
			return E_FAIL;
		m_entries.push_back(entry);
		*font = entry.font;
	}
	return S_OK;
}

// 0x00137FF6
HRESULT CFontTable::Add(const char *name, CXBFont *font, BOOL owned)
{
	if (font == NULL)
		return E_FAIL;
	SFontEntry *entry = FindEntry(name);
	if (entry == NULL)
	{
		SFontEntry added(name, font, owned);
		if (added.font == NULL)
			return E_FAIL;
		m_entries.push_back(added);
	}
	else
	{
		if (entry->owned)
		{
			// (Destroy before the NULL check.)
			entry->font->Destroy();
			if (entry->font != NULL)
				delete entry->font;
		}
		entry->font = font;
		entry->owned = owned;
	}
	return S_OK;
}
