// UI text. Every string has an ID (StringIds.h) with a built-in English text;
// a language file (<LanguageFile> in Config.xml, e.g. French.xml) can replace
// any of them by key:  <Resource><OK>D'accord</OK>...</Resource>
// A literal "\n" in a language file means a line break.
#pragma once

#include "../common/Platform.h"
#include "StringIds.h"

class CXmlDocument;

extern CXmlDocument *g_pLanguage;                    // 0x002761FC

// Replaces the current language document with fileName (if it exists) and
// clears the string caches.
void LoadLanguageFile(const char *fileName);         // 0x0013D304

// The text for id: the language file's value if non-empty, else English.
const char *GetDefaultString(int id);                // 0x0013A679

// Cached accessors used by the UI.
const char *GetStringA(int id);                      // 0x0013D242
const wchar_t *GetString(int id);                    // 0x0013D299
