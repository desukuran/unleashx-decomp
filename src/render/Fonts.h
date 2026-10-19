// Fonts. CXBFont is the XDK sample framework's bitmap font (.xpr); CXBFontEx
// adds TrueType support. Only the members UnleashX uses are declared.
#pragma once

#include "../common/Platform.h"

class CXBFont
{
public:
	CXBFont();                                                         // 0x000999xx (XDK sample)
	virtual ~CXBFont();
	HRESULT Create(const char *fileName, DWORD resourceOffset);       // 0x00099EC9 (XDK sample)
	void SetScaleFactors(float scaleX, float scaleY);                  // 0x00098FEB (XDK sample)

	BYTE m_reserved[0xA0];                                             // size 0xA4
};

class CXBFontEx : public CXBFont
{
public:
	CXBFontEx();                                                       // 0x00137887
	HRESULT CreateFromTTF(const wchar_t *fileName, int unknown, int size, int flags);  // 0x00137841 TODO: decompile

	BYTE m_reservedEx[0x40];                                           // size 0xE4
};

// Name -> font table (<Fonts> in a skin, and the global g_fonts).
class CFontTable
{
public:
	CFontTable();                                                      // 0x00137DF6 TODO: decompile
	~CFontTable();
	HRESULT Find(const char *name, CXBFont **font);                    // 0x00138089 TODO: decompile
	HRESULT Add(const char *name, CXBFont *font, BOOL owned);          // 0x00137FF6 TODO: decompile

	BYTE m_reserved[0x10];
};
