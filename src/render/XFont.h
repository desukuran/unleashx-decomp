// XFONT, the XDK's TrueType renderer (XGraphics). On the Xbox this comes
// from <xfont.h>; on other hosts only what the recovered code uses is
// declared, so it can be syntax-checked.
#pragma once

#include "D3D.h"

#ifdef _XBOX
#define XFONT_TRUETYPE
#include <xfont.h>
#else
struct XFONT;

#define XFONT_LEFT 0
#define XFONT_RIGHT 2
#define XFONT_CENTER 6

HRESULT __stdcall XFONT_OpenTrueTypeFont(const wchar_t *fileName, unsigned cacheSize, XFONT **font);
void __stdcall XFONT_Release(XFONT *font);
unsigned __stdcall XFONT_GetTextHeight(XFONT *font);
HRESULT __stdcall XFONT_SetTextHeight(XFONT *font, unsigned height);
HRESULT __stdcall XFONT_SetTextAntialiasLevel(XFONT *font, unsigned level);
void __stdcall XFONT_SetTextAlignment(XFONT *font, unsigned mode);
void __stdcall XFONT_SetTextColor(XFONT *font, D3DCOLOR color);
unsigned __stdcall XFONT_GetIntercharacterSpacing(XFONT *font);
void __stdcall XFONT_SetClippingRectangle(XFONT *font, D3DRECT *rectangle);
void __stdcall XFONT_GetFontMetrics(XFONT *font, unsigned *cellHeight, unsigned *descent);
HRESULT __stdcall XFONT_GetTextExtent(XFONT *font, const wchar_t *text, unsigned length, unsigned *width);
HRESULT __stdcall XFONT_TextOut(XFONT *font, IDirect3DSurface8 *surface, const wchar_t *text, unsigned length, long x,
                                long y);
#endif
