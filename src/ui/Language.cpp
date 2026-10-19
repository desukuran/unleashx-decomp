#include "Language.h"

#include <cstring>
#include <map>
#include <string>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../xml/XmlDocument.h"

CXmlDocument *g_pLanguage;   // 0x002761FC

// Caches so each string is looked up and converted only once.
static std::map<int, std::string> g_stringCacheA;    // 0x00278024
static std::map<int, std::wstring> g_stringCacheW;   // 0x00278030

struct SLanguageString
{
	int id;
	const char *key;
	const char *text;
};

// The original is one big switch statement; this table holds the same data.
static const SLanguageString kStrings[] = {
#include "StringTable.inc"
};

// 0x0013A631
// The language file's text for key, or NULL. Converts "\n" escapes in place.
static const char *LookupLanguageKey(const char *key)
{
	if (g_pLanguage == NULL)
		return NULL;
	g_pLanguage->m_pCurrent = &g_pLanguage->m_root;
	CXmlElement *element = g_pLanguage->FindElement(&g_pLanguage->m_root, key);
	if (element == NULL)
		return NULL;
	StrReplaceAll(element->m_strValue, "\\n", "\n");
	return element->m_strValue.c_str();
}

// 0x0013A679
const char *GetDefaultString(int id)
{
	for (size_t i = 0; i < sizeof(kStrings) / sizeof(kStrings[0]); i++)
	{
		if (kStrings[i].id != id)
			continue;
		const char *text = LookupLanguageKey(kStrings[i].key);
		if (text != NULL && strlen(text) != 0)
			return text;
		return kStrings[i].text;
	}
	return " ";
}

// 0x0013D242
const char *GetStringA(int id)
{
	std::map<int, std::string>::iterator it = g_stringCacheA.find(id);
	if (it == g_stringCacheA.end())
		g_stringCacheA[id] = GetDefaultString(id);
	return g_stringCacheA[id].c_str();
}

// 0x0013D299
const wchar_t *GetString(int id)
{
	std::wstring &cached = g_stringCacheW[id];
	if (cached.size() == 0)
	{
		wchar_t wide[0x402];
		StrUtf8ToWide(GetStringA(id), wide, 0x400);
		cached = wide;
	}
	return cached.c_str();
}

// 0x0013D304
void LoadLanguageFile(const char *fileName)
{
	if (g_pLanguage != NULL)
	{
		g_pLanguage->Clear();
		delete g_pLanguage;
		g_pLanguage = NULL;
	}

	if (fileName != NULL && strlen(fileName) != 0)
	{
		if (!FileExists(fileName))
			return;   // note: the caches aren't cleared in this case
		g_pLanguage = new CXmlDocument();
		if (g_pLanguage != NULL && !g_pLanguage->Load(fileName))
		{
			delete g_pLanguage;
			g_pLanguage = NULL;
			return;   // nor here
		}
	}

	g_stringCacheA.clear();
	g_stringCacheW.clear();
}
