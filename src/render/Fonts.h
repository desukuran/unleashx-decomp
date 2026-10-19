// Fonts. CXBFont is the XDK sample framework's bitmap font (.xpr), in the
// XBMC-era version with a small vtable; it's library code, so only what
// UnleashX uses is declared. CXBFontEx is UnleashX's own: the same font,
// or a TrueType one drawn with XFONT (see FontEx.cpp). CFontTable maps
// names to fonts (FontTable.cpp).
#pragma once

#include <vector>

#include "../app/XBApp.h"
#include "../common/Platform.h"
#include "D3D.h"

struct XFONT;

// CXBFont::DrawText flags (XDK sample framework)
#define XBFONT_LEFT 0x00000000
#define XBFONT_RIGHT 0x00000001
#define XBFONT_CENTER_X 0x00000002
#define XBFONT_CENTER_Y 0x00000004
#define XBFONT_TRUNCATED 0x00000008

// The bitmap font. vtable 0x0025B224 (slots take this on the stack). The
// destructor isn't virtual: deleting a CXBFontEx through a CXBFont pointer
// runs only this one.
class CXBFont
{
public:
	CXBFont();                                                         // 0x0009906E (XDK sample; this in edx)
	~CXBFont();                                                        // 0x00099051 (this in eax)
	HRESULT Create(const char *fileName, DWORD resourceOffset);       // 0x00099EC9 (XDK sample)
	// From a font texture and the glyph data that follows it in an .xpr.
	// (this in esi, texture in eax)
	HRESULT Create(IDirect3DTexture8 *texture, const void *fontData);  // 0x000998F0 (XDK sample)
	HRESULT Destroy();                                                 // 0x00098FFE (XDK sample; this in edi)
	void SetScaleFactors(float scaleX, float scaleY);                  // 0x00098FEB (XDK sample)
	// Floors the pen position. (this in esi)
	void SetCursorPosition(float x, float y);                          // 0x000995A2 (XDK sample)
	// Brackets a group of DrawText calls (render states are set once).
	HRESULT Begin();                                                   // 0x0009978B (XDK sample)
	HRESULT End();                                                     // 0x0009968D (XDK sample)
	HRESULT DrawText(float x, float y, DWORD color, const wchar_t *text, DWORD flags,
	                 float maxWidth);                                  // 0x00098E3A (XDK sample)
	// The first length characters of text.
	HRESULT DrawTextN(float x, float y, DWORD color, const wchar_t *text, int length, DWORD flags,
	                  float maxWidth);                                 // 0x00099AAD (XDK sample)
	// Width of the text in pixels (returned in xmm0 in the original).
	float GetTextWidth(const wchar_t *text);                           // 0x00099571
	// The whole text's size; either output can be NULL. (height in esi)
	HRESULT GetTextExtent(const wchar_t *text, float *width, float *height, BOOL firstLineOnly);   // 0x00098FC3 (XDK sample)
	// The size of the first length characters (scaled); firstLineOnly
	// stops at a newline. The original takes this in eax, width in edx
	// and height in esi.
	HRESULT GetTextExtentN(const wchar_t *text, int length, BOOL firstLineOnly, float *width,
	                       float *height);                             // 0x00098E9E (XDK sample)

	// Draws text into a surface (it becomes the render target).
	virtual HRESULT DrawToSurface(IDirect3DSurface8 *surface, const wchar_t *text, int length, DWORD width,
	                              DWORD height);                       // 0, 0x00099E24
	virtual HRESULT Unused1();                                         // 1, 0x000326FA returns 0
	// Nothing for a bitmap font.
	virtual HRESULT SetTextHeight(DWORD height);                       // 2, 0x00098E35
	virtual HRESULT SetTextColor(D3DCOLOR color);                      // 3, 0x00098E22
	// The size in whole pixels; length -1 for all of it.
	virtual HRESULT GetTextExtentInt(const wchar_t *text, int length, int *width, int *height);   // 4, 0x00099345

	float m_fCellHeight;                                               // +0x04 TODO: verify (the file manager's line spacing)
	BYTE m_reserved08[0x08];
	float m_fFontHeight;                                               // +0x10
	float m_fScaleFactorX;                                             // +0x14
	float m_fScaleFactorY;                                             // +0x18
	BYTE m_reserved1C[0x04];
	float m_fCursorX;                                                  // +0x20
	float m_fCursorY;                                                  // +0x24
	BYTE m_reserved28[0x74];
	DWORD m_dwNestedBeginCount;                                        // +0x9C
	D3DCOLOR m_dwTextColor;                                            // +0xA0 (bitmap fonts' SetTextColor)
};

// A font that is either the bitmap CXBFont (from an .xpr file, or from
// memory) or a TrueType font drawn with XFONT. The methods below shadow
// CXBFont's: each one checks for the TrueType font first. vtable
// 0x0025B23C, 0xE4 bytes. (No destructor of its own: the TrueType font is
// never released.)
class CXBFontEx : public CXBFont
{
public:
	CXBFontEx();                                                       // 0x00137887 (this in edx)
	// A bitmap font from an .xpr image in memory (an XBE section). (this
	// on the stack, data in ecx)
	HRESULT CreateFromMemory(const void *data);                        // 0x00137A8F
	// A TrueType font (with a 16 KB glyph cache). (this in esi)
	HRESULT CreateFromTTF(const wchar_t *fileName, D3DCOLOR color, DWORD height, DWORD antialiasLevel);   // 0x00137841

	// Gets the back buffer to draw into on the first of nested calls.
	// (this in eax)
	HRESULT Begin();                                                   // 0x00031CCB
	// Releases it on the last one: S_FALSE while still nested, E_FAIL
	// without a Begin. (this in eax)
	HRESULT End();                                                     // 0x00031CFD
	// TrueType: the width is estimated from the average character width
	// (see FontEx.cpp). (this in edi, width in ebx, height in eax)
	HRESULT GetTextExtent(const wchar_t *text, float *width, float *height, BOOL firstLineOnly);   // 0x00031D3C
	// (this in eax)
	HRESULT DrawTextN(float x, float y, DWORD color, const wchar_t *text, int length, DWORD flags,
	                  float maxWidth);                                 // 0x001378CC

	// Re-measures the average character width. (this on the stack)
	virtual HRESULT SetTextHeight(DWORD height);                       // 2, 0x001377E1
	virtual HRESULT SetTextColor(D3DCOLOR color);                      // 3, 0x001377AF

	CXBPackedResource m_xpr;               // +0xA4 for CreateFromMemory
	XFONT *m_pTrueTypeFont;                // +0xBC NULL: a bitmap font
	DWORD m_unknownC0;                     // +0xC0 0, never read
	IDirect3DSurface8 *m_pBackBuffer;      // +0xC4 between Begin and End
	DWORD m_dwTextHeight;                  // +0xC8
	D3DCOLOR m_dwTrueTypeColor;            // +0xCC
	D3DRECT m_clipRect;                    // +0xD0
	DWORD m_dwAverageCharWidth;            // +0xE0 pixels
};

// A named font, owned (destroyed with the table) or not. 0x10C bytes.
struct SFontEntry
{
	// font NULL: loads the bitmap font file name (owned when it worked).
	// (font in eax, this in esi)
	SFontEntry(const char *name, CXBFont *font, BOOL owned);           // 0x0002E245

	char name[0x104];                      // +0x000
	CXBFont *font;                         // +0x104
	BOOL owned;                            // +0x108
};

// Name -> font table (<Fonts> in a skin, and the global g_fonts). Names
// match in any case.
class CFontTable
{
public:
	// (this in esi)
	CFontTable();                                                      // 0x00137DF6
	// Destroys and deletes the owned fonts.
	~CFontTable();                                                     // 0x00137D05 (deleting 0x000269FD)
	// Not in the table: an existing file of that name is loaded as a
	// bitmap font and added. (font in edi)
	HRESULT Find(const char *name, CXBFont **font);                    // 0x00138089
	// Replaces the font of that name (destroying the old one if owned),
	// or adds it. E_FAIL for no font.
	HRESULT Add(const char *name, CXBFont *font, BOOL owned);          // 0x00137FF6

private:
	CXBFont *FindFont(const char *name);                               // 0x00137C19
	SFontEntry *FindEntry(const char *name);                           // 0x00137C83

	std::vector<SFontEntry> m_entries;     // +0x00
};

// Fonts shared by every skin (MainFont, MenuFont, InfoFont, SmallFont,
// CaptionFont, XboxDings).
extern CFontTable g_fonts;   // 0x002784C0

// Line height of a font: the TrueType face's height, or the bitmap font's
// height times its vertical scale. (font in eax)
float GetFontHeight(CXBFont *font);                                    // 0x00021EA0
// The pen position for DrawFontText (rounded down). (font in eax)
void SetFontCursor(CXBFont *font, float x, float y);                   // 0x00021E58
// Draws text at the pen position. (font in esi)
void DrawFontText(CXBFont *font, DWORD color, const wchar_t *text, DWORD flags, float maxWidth);   // 0x00021ECB
