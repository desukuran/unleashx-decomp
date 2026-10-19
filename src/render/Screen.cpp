// The screen area (CAppSettings::m_viewport) and scaling from skin
// coordinates to it. The original reads the settings through its own copy
// of the settings pointer (0x002788E4).
#include <cmath>

#include "../settings/AppSettings.h"
#include "Draw.h"

// 0x0009E1D4 (x in ecx, y in edx)
void GetScreenOrigin(float *x, float *y)
{
	if (x != NULL)
		*x = (float)g_pSettings->m_viewport.X;
	if (y != NULL)
		*y = (float)g_pSettings->m_viewport.Y;
}

// 0x0009E216 (width in ecx, height in edx)
void GetScreenSize(float *width, float *height)
{
	if (width != NULL)
		*width = (float)g_pSettings->m_viewport.Width;
	if (height != NULL)
		*height = (float)g_pSettings->m_viewport.Height;
}

// 0x0009E258 (right in ecx, bottom in edx)
void GetScreenEnd(float *right, float *bottom)
{
	if (right != NULL)
		*right = (float)(g_pSettings->m_viewport.Width + g_pSettings->m_viewport.X);
	if (bottom != NULL)
		*bottom = (float)(g_pSettings->m_viewport.Height + g_pSettings->m_viewport.Y);
}

// 0x0009E2A8
SScreenRect GetScreenRect()
{
	SScreenRect rect;
	rect.x = g_pSettings->m_viewport.X;
	rect.y = g_pSettings->m_viewport.Y;
	rect.width = g_pSettings->m_viewport.Width;
	rect.height = g_pSettings->m_viewport.Height;
	return rect;
}

// 0x0009E34E (x in esi, width in edi)
void ScaleToScreen(float *x, float *width, float *y, float *height, BOOL base640x480)
{
	const SViewport &viewport = g_pSettings->m_viewport;
	DWORD skinWidth = (DWORD)g_pSettings->m_nSkinWidth;
	DWORD skinHeight = (DWORD)g_pSettings->m_nSkinHeight;
	if (viewport.Width == skinWidth && viewport.Height == skinHeight)
		return;

	if (x != NULL)
		*x = (float)viewport.Width / (float)skinWidth * *x + (float)viewport.X;
	if (y != NULL)
		*y = (float)viewport.Height / (float)skinHeight * *y + (float)viewport.Y;
	if (width != NULL && *width != 0.0f)
	{
		if (base640x480)
			*width = (float)viewport.Width * *width * (1.0f / 640.0f);
		else
			*width = (float)viewport.Width / (float)skinWidth * *width;
	}
	if (height != NULL && *height != 0.0f)
	{
		if (base640x480)
			*height = (float)viewport.Height * *height * (1.0f / 480.0f);
		else
			*height = (float)viewport.Height / (float)skinHeight * *height;
	}
}

// 0x0009E4D4 (x in ebx)
// Zero values (and offsets) stay zero.
void ScaleToScreenInt(DWORD *x, DWORD *y, DWORD *width, DWORD *height)
{
	float fx = 0.0f;
	float fy = 0.0f;
	float fwidth = 0.0f;
	float fheight = 0.0f;
	if (x != NULL && *x != 0)
		fx = (float)*x;
	if (y != NULL && *y != 0)
		fy = (float)*y;
	if (width != NULL && *width != 0)
		fwidth = (float)*width;
	if (height != NULL && *height != 0)
		fheight = (float)*height;
	ScaleToScreen(&fx, &fwidth, &fy, &fheight, FALSE);
	if (x != NULL && *x != 0)
		*x = (DWORD)(LONGLONG)fx;
	if (y != NULL && *y != 0)
		*y = (DWORD)(LONGLONG)fy;
	if (width != NULL && *width != 0)
		*width = (DWORD)(LONGLONG)fwidth;
	if (height != NULL && *height != 0)
		*height = (DWORD)(LONGLONG)fheight;
}

// 0x0009FDCC (rect in ebx)
// x, y, width, height; the position rounds down, the size up.
void ScaleRectToScreen(int *rect)
{
	if (rect == NULL)
		return;
	float x = (float)rect[0];
	float y = (float)rect[1];
	float width = (float)rect[2];
	float height = (float)rect[3];
	ScaleToScreen(&x, &width, &y, &height, FALSE);
	rect[0] = (int)floor(x);
	rect[1] = (int)floor(y);
	rect[2] = (int)ceil(width);
	rect[3] = (int)ceil(height);
}
