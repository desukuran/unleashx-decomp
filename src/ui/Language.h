// Language string tables (English.xml etc.).
#pragma once

class CXmlDocument;

extern CXmlDocument *g_pLanguage;                    // 0x002761FC

// Replaces the current language document with fileName (if it exists).
void LoadLanguageFile(const char *fileName);         // 0x0013D304 TODO: decompile
