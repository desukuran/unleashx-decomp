// Screen scaling and 2D drawing helpers (0x0009E1D4-0x000A2D02).
// TODO: decompile
#pragma once

#include "../common/Platform.h"

struct IDirect3DTexture8;

// The screen area (CAppSettings::m_viewport).
void GetScreenOrigin(float *x, float *y);                    // 0x0009E1D4
void GetScreenSize(float *width, float *height);             // 0x0009E216
void GetScreenEnd(float *right, float *bottom);              // 0x0009E258

// Skin coordinates (laid out for m_nSkinWidth x m_nSkinHeight) to screen
// coordinates. Any pointer can be NULL. With base640x480 the sizes are
// relative to 640x480 instead. Quirk: nothing changes (not even the
// offset) when the screen is the skin's size; zero sizes stay zero.
void ScaleToScreen(float *x, float *width, float *y, float *height, BOOL base640x480);   // 0x0009E34E

// The "please wait" box of file operations: title, current file, a second
// line and a progress bar (percent).
void DrawProgressBox(float x, float y, float width, float height, const wchar_t *title, const wchar_t *file,
                     const wchar_t *line2, float percent, float alpha, int unknown);    // 0x000A2D02
