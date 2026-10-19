// Fonts. CXBFont is the XDK sample framework's bitmap font (.xpr); CXBFontEx
// adds TrueType support. Only the members UnleashX uses are declared.
#pragma once

#include "../common/Platform.h"

// CXBFont::DrawText flags (XDK sample framework)
#define XBFONT_LEFT 0x00000000
#define XBFONT_RIGHT 0x00000001
#define XBFONT_CENTER_X 0x00000002
#define XBFONT_CENTER_Y 0x00000004
#define XBFONT_TRUNCATED 0x00000008

class CXBFont
{
public:
	CXBFont();                                                         // 0x0009906E (XDK sample; this in edx)
	virtual ~CXBFont();                                                // 0x00099051 (this in eax)
	HRESULT Create(const char *fileName, DWORD resourceOffset);       // 0x00099EC9 (XDK sample)
	HRESULT Destroy();                                                 // 0x00098FFE (XDK sample; this in edi)
	void SetScaleFactors(float scaleX, float scaleY);                  // 0x00098FEB (XDK sample)
	// Brackets a group of DrawText calls (render states are set once).
	HRESULT Begin();                                                   // 0x0009978B (XDK sample)
	HRESULT End();                                                     // 0x0009968D (XDK sample)
	HRESULT DrawText(float x, float y, DWORD color, const wchar_t *text, DWORD flags,
	                 float maxWidth);                                  // 0x00098E3A (XDK sample)
	// Width of the text in pixels (returned in xmm0 in the original).
	float GetTextWidth(const wchar_t *text);                           // 0x00099571
	// The size of the first length characters (scaled); firstLineOnly
	// stops at a newline. The original takes this in eax, width in edx
	// and height in esi.
	// The whole text's size; either output can be NULL. (height in esi)
	HRESULT GetTextExtent(const wchar_t *text, float *width, float *height, BOOL firstLineOnly);   // 0x00098FC3 (XDK sample)
	HRESULT GetTextExtentN(const wchar_t *text, int length, BOOL firstLineOnly, float *width,
	                       float *height);                             // 0x00098E9E (XDK sample)

	BYTE m_reserved04[0x0C];
	float m_fFontHeight;                                               // +0x10
	float m_fScaleFactorX;                                             // +0x14
	float m_fScaleFactorY;                                             // +0x18
	BYTE m_reserved1C[0x88];                                           // size 0xA4
};

class CXBFontEx : public CXBFont
{
public:
	CXBFontEx();                                                       // 0x00137887
	// A bitmap font from an .xpr image in memory (an XBE section).
	// (this on the stack, data in ecx) TODO: decompile
	HRESULT CreateFromMemory(const void *data);                        // 0x00137A8F
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

// Line height of a font: the TrueType face's height, or the bitmap font's
// height times its vertical scale.
float GetFontHeight(CXBFont *font);                                    // 0x00021EA0 TODO: decompile
