// Screen scaling and 2D drawing helpers (0x0009E1D4-0x000A2D02).
// TODO: decompile
#pragma once

#include "../common/Platform.h"

struct IDirect3DTexture8;
class CXBFont;
typedef DWORD D3DCOLOR;

// The screen area (CAppSettings::m_viewport).
void GetScreenOrigin(float *x, float *y);                    // 0x0009E1D4
void GetScreenSize(float *width, float *height);             // 0x0009E216
void GetScreenEnd(float *right, float *bottom);              // 0x0009E258

// Skin coordinates (laid out for m_nSkinWidth x m_nSkinHeight) to screen
// coordinates. Any pointer can be NULL. With base640x480 the sizes are
// relative to 640x480 instead. Quirk: nothing changes (not even the
// offset) when the screen is the skin's size; zero sizes stay zero.
void ScaleToScreen(float *x, float *width, float *y, float *height, BOOL base640x480);   // 0x0009E34E

// Flat-colored primitives (Draw.cpp). Colors are ARGB; a color of 0 skips
// that part. alpha (0..1) replaces or scales the fill's alpha.
HRESULT DrawRect(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                 float alpha, DWORD borderWidth);                            // 0x000A087B
HRESULT DrawFrame(float x, float y, float width, float height, D3DCOLOR color, DWORD lineWidth);   // 0x000A1D37
void DimScreen(float alpha);                                                 // 0x000A1A39
// Dialog frame: a box with cut corners.
void DrawBevelBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                  float alpha, DWORD lineWidth);                             // 0x000A1BBF
// A dialog window: the outer box, and an inner panel between the caption
// (captionHeight) and the button row (footerHeight). TODO: decompile; what
// inset does exactly. The original takes width in xmm7.
void DrawWindowFrame(float x, float y, float width, float height, float inset, float captionHeight,
                     float footerHeight, D3DCOLOR borderColor, D3DCOLOR fillColor, D3DCOLOR innerBorderColor,
                     D3DCOLOR innerFillColor, DWORD borderWidth, DWORD innerBorderWidth);   // 0x000A10D2
// A message box button's box. TODO: decompile. The original takes x in
// xmm0.
void DrawButtonBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                   DWORD lineWidth);                                         // 0x000A1518
// direction: 1 up, 2 down, 3 left, 4 right
HRESULT DrawTriangle(float x, float y, float width, float height, int direction, D3DCOLOR borderColor,
                     D3DCOLOR fillColor, float alpha);                       // 0x000A0D78
HRESULT DrawGradientRect(float x, float y, float width, float height, D3DCOLOR color1, D3DCOLOR color2,
                         BOOL vertical, D3DCOLOR frameColor);                // 0x000A1F08
// A texture stretched over the rectangle (a zero size uses the texture's),
// with alpha 0..1. The last three arguments rotate/scale it. TODO: decompile
HRESULT DrawTexture(IDirect3DTexture8 *texture, float x, float y, float width, float height, float alpha,
                    const float *origin, float unknown, const float *transform);   // 0x000A246F
// A bar filled to percent (0-100), fill1 to fill2. TODO: decompile
HRESULT DrawProgressBar(float x, float y, float width, float height, float percent, D3DCOLOR borderColor,
                        D3DCOLOR backColor, D3DCOLOR fill1, D3DCOLOR fill2, BYTE gradient);   // 0x000A2897
// "text..." in a centered box, with animated dots.
HRESULT DrawBusyBox(CXBFont *font, const wchar_t *text);                     // 0x000A1D70

// The "please wait" box of file operations: title, current file, a second
// line and a progress bar (percent).
void DrawProgressBox(float x, float y, float width, float height, const wchar_t *title, const wchar_t *file,
                     const wchar_t *line2, float percent, float alpha, int unknown);    // 0x000A2D02
