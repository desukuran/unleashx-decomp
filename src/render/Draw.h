// Screen scaling (Screen.cpp) and 2D drawing helpers (Draw.cpp),
// 0x0009E1D4-0x000A2D02.
#pragma once

#include "../common/Platform.h"

struct IDirect3DTexture8;
class CXBFont;
typedef DWORD D3DCOLOR;

// The screen area (CAppSettings::m_viewport).
void GetScreenOrigin(float *x, float *y);                    // 0x0009E1D4
void GetScreenSize(float *width, float *height);             // 0x0009E216
void GetScreenEnd(float *right, float *bottom);              // 0x0009E258
struct SScreenRect
{
	DWORD x, y, width, height;
};
SScreenRect GetScreenRect();                                 // 0x0009E2A8
// A texture over the whole screen area.
void DrawTextureFullScreen(IDirect3DTexture8 *texture, float alpha);   // 0x000A282E

// Skin coordinates (laid out for m_nSkinWidth x m_nSkinHeight) to screen
// coordinates. Any pointer can be NULL. With base640x480 the sizes are
// relative to 640x480 instead. Quirk: nothing changes (not even the
// offset) when the screen is the skin's size; zero sizes stay zero.
void ScaleToScreen(float *x, float *width, float *y, float *height, BOOL base640x480);   // 0x0009E34E
// The same for whole numbers (zeros stay zero).
void ScaleToScreenInt(DWORD *x, DWORD *y, DWORD *width, DWORD *height);  // 0x0009E4D4
// x, y, width, height; the position rounds down, the size up.
void ScaleRectToScreen(int *rect);                                       // 0x0009FDCC

// A D3DMATRIX that scales, rotates (radians, about X, Y and Z) and
// translates. The original takes matrix in esi.
void BuildWorldMatrix(float *matrix, float x, float y, float z, float scaleX, float scaleY, float scaleZ,
                      float rotationX, float rotationY, float rotationZ);   // 0x0009F342

// Flat-colored primitives (Draw.cpp). Colors are ARGB; a color of 0 skips
// that part. alpha (0..1) replaces or scales the fill's alpha.
HRESULT DrawRect(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                 float alpha, DWORD borderWidth);                            // 0x000A087B
HRESULT DrawFrame(float x, float y, float width, float height, D3DCOLOR color, DWORD lineWidth);   // 0x000A1D37
void DimScreen(float alpha);                                                 // 0x000A1A39
// A rectangle from (left, top) to (right, bottom) shading from top to
// bottom. TODO: decompile (left in xmm2, top in xmm3, right in xmm4, bottom
// and topColor on the stack, bottomColor in ebx)
void DrawGradientBox(float left, float top, float right, float bottom, D3DCOLOR topColor,
                     D3DCOLOR bottomColor);                                      // 0x0009AD67
// Clips drawing to the rectangle (cut at the display's edges); disabled, or
// an empty rectangle, clears the clipping.
void SetScissorRect(BOOL enable, float x, float y, float width, float height);   // 0x0009F2B2
// Dialog frame: a box with cut corners.
void DrawBevelBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                  float alpha, DWORD lineWidth);                             // 0x000A1BBF
// A dialog window: a tab-shaped outer box and an inner panel between the
// caption (captionHeight) and the button row (footerHeight), margin in
// from the sides. The original takes width in xmm7.
void DrawWindowFrame(float x, float y, float width, float height, float margin, float captionHeight,
                     float footerHeight, D3DCOLOR borderColor, D3DCOLOR fillColor, D3DCOLOR innerBorderColor,
                     D3DCOLOR innerFillColor, DWORD borderWidth, DWORD innerBorderWidth);   // 0x000A10D2
// A message box button: an outlined octagon with a filled one inside. The
// original takes x in xmm0.
void DrawButtonBox(float x, float y, float width, float height, D3DCOLOR borderColor, D3DCOLOR fillColor,
                   DWORD lineWidth);                                         // 0x000A1518
// direction: 1 up, 2 down, 3 left, 4 right
HRESULT DrawTriangle(float x, float y, float width, float height, int direction, D3DCOLOR borderColor,
                     D3DCOLOR fillColor, float alpha);                       // 0x000A0D78
HRESULT DrawGradientRect(float x, float y, float width, float height, D3DCOLOR color1, D3DCOLOR color2,
                         BOOL vertical, D3DCOLOR frameColor);                // 0x000A1F08
// A texture stretched over the rectangle (a zero size uses the texture's),
// with alpha 0..1, rotated by angle degrees and scaled by scale[2] (NULL =
// 1, 1) around center (relative to x, y and scaled to the screen in place;
// NULL = the middle).
HRESULT DrawTexture(IDirect3DTexture8 *texture, float x, float y, float width, float height, float alpha,
                    float *center, float angle, const float *scale);   // 0x000A246F
// A progress bar: an octagon (3-pixel cuts) in backColor/borderColor, and
// inside it, 3 pixels in, the filled part (percent 0-100): fill1, or a
// gradient from fill2 to fill1 when gradient is set and they differ.
HRESULT DrawProgressBar(float x, float y, float width, float height, float percent, D3DCOLOR borderColor,
                        D3DCOLOR backColor, D3DCOLOR fill1, D3DCOLOR fill2, BYTE gradient);   // 0x000A2897
// "text..." in a centered box, with animated dots.
HRESULT DrawBusyBox(CXBFont *font, const wchar_t *text);                     // 0x000A1D70

// The FTP manager's progress box: title, up to two lines, the bytes done
// and a bar (percent; below 0 for none). height 0: fit the lines; width
// 0: the screen's. It stays above the screen's bottom. TODO: decompile.
// The original takes font in ebx.
void DrawTransferBox(CXBFont *font, float x, float y, float width, float height, const wchar_t *title,
                     const wchar_t *line1, const wchar_t *line2, float percent, BOOL showRate);   // 0x000A3267

// The "please wait" box of file operations: title, current file, a second
// line and a progress bar (percent); copies and moves also show the
// overall progress (totalPercent, -1 = none). TODO: decompile. The
// original takes font in ebx.
void DrawProgressBox(CXBFont *font, float x, float y, float width, float height, const wchar_t *title,
                     const wchar_t *file, const wchar_t *line2, float percent, float totalPercent,
                     BOOL showTotal);                                          // 0x000A2D02
