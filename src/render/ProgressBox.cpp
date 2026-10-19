// The progress boxes of the long operations (copies, deletes, extracting,
// FTP transfers) and the unused caption bar. Xbox only (Direct3D).
#include <cwchar>

#include "../skin/SkinColors.h"
#include "../system/FileOps.h"
#include "Draw.h"
#include "Fonts.h"

// Between the lines, and the box's padding.
#define BOX_SPACING 5.0f
// The bar's colors: from dark to full blue.
#define PROGRESS_FILL 0xFF0000FF
#define PROGRESS_FILL_DARK 0xFF000040

// The title with dots that count up every half second: "Copying...",
// "Copying.", "Copying..".
static void FormatBusyTitle(wchar_t *out, const wchar_t *title)
{
	DWORD phase = (GetTickCount() / 500) % 3;
	const wchar_t *dots = phase < 1 ? L"..." : phase == 2 ? L".." : L".";
	swprintf(out, 260, L"%ls%ls", title, dots);
}

// The sizes and position both boxes share. A zero height fits the lines
// (rows), a zero width is the screen's less 70. Quirk: a box that would
// reach past the screen's bottom is moved up only until it ends 5 pixels
// below it.
static void FitBox(float *y, float *width, float *height, float rows, float lineHeight)
{
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, NULL, &scaleY, TRUE);
	if (*height == 0.0f)
		*height = (lineHeight + rows + BOX_SPACING) * scaleY;
	if (*width == 0.0f)
	{
		GetScreenSize(width, NULL);
		*width = (*width - 70.0f) * scaleX;
	}
	float bottom;
	GetScreenEnd(NULL, &bottom);
	if (*y + *height > bottom)
		*y -= *y + *height - bottom - BOX_SPACING;
}

// A bar at x, y over width less the "100%" column, its percentage right
// aligned after it.
static void DrawPercentBar(CXBFont *font, float x, float y, float width, float lineHeight, float percentWidth,
                           float percent)
{
	wchar_t text[260];
	swprintf(text, 260, L"%d%ls", (int)percent, L"%");
	float maxWidth = width - 10.0f;
	DrawProgressBar(x, y, maxWidth - percentWidth, lineHeight, percent, g_colorScheme.DIALOG_BORDERCOLOR,
	                g_colorScheme.DIALOG_BACKCOLOR, PROGRESS_FILL, PROGRESS_FILL_DARK, TRUE);
	font->DrawText(x + width - 10.0f, lineHeight * 0.5f + y, g_colorScheme.OPTION_ACTIVETEXTCOLOR, text,
	               XBFONT_RIGHT | XBFONT_CENTER_Y, maxWidth);
}

// "Elapse: mm:ss  Remaining: mm:ss Rate: n KB/s" of the running operation.
static void DrawTimes(CXBFont *font, float x, float y, float width)
{
	wchar_t text[260];
	swprintf(text, 260, L"Elapse: %02d:%02d  Remaining: %02d:%02d Rate: %0.02f KB/s", g_dwOperationElapsed / 60,
	         g_dwOperationElapsed % 60, g_dwOperationRemaining / 60, g_dwOperationRemaining % 60,
	         g_dOperationRate);
	font->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_LEFT, width - 10.0f);
}

// Draws the title (with dots) and the two lines; returns the y after them.
static float DrawBoxLines(CXBFont *font, float x, float y, float width, float lineHeight, const wchar_t *title,
                          const wchar_t *line1, const wchar_t *line2)
{
	if (title != NULL)
	{
		wchar_t text[260];
		FormatBusyTitle(text, title);
		font->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_LEFT, width - 10.0f);
		y = lineHeight + y + BOX_SPACING;
	}
	if (line1 != NULL)
	{
		font->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, line1, XBFONT_LEFT, width - 10.0f);
		y = lineHeight + y + BOX_SPACING;
	}
	if (line2 != NULL)
	{
		font->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, line2, XBFONT_LEFT, width - 10.0f);
		y = lineHeight + y + BOX_SPACING;
	}
	return y;
}

// 0x000A2D02 (font in ebx)
void DrawProgressBox(CXBFont *font, float x, float y, float width, float height, const wchar_t *title,
                     const wchar_t *file, const wchar_t *line2, float percent, float totalPercent, BOOL showTimes)
{
	float lineHeight = font->m_fScaleFactorY * font->m_fFontHeight;
	// The rows' height: 5, or the title's line plus 10, then a line plus 5
	// for each other part.
	float rows = BOX_SPACING;
	if (title != NULL)
		rows = lineHeight + 10.0f;
	if (file != NULL)
		rows = lineHeight + rows + BOX_SPACING;
	if (line2 != NULL)
		rows = lineHeight + rows + BOX_SPACING;
	if (percent >= 0.0f)
		rows = lineHeight + rows + BOX_SPACING;
	if (totalPercent >= 0.0f)
		rows = lineHeight + rows + BOX_SPACING;
	if (showTimes)
		rows = lineHeight + rows + BOX_SPACING;
	FitBox(&y, &width, &height, rows, lineHeight);

	DrawBevelBox(x, y, width, height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f,
	             2);
	x += BOX_SPACING;
	y += BOX_SPACING;
	y = DrawBoxLines(font, x, y, width, lineHeight, title, file, line2);
	y += 3.0f;
	float percentWidth = font->GetTextWidth(L"100%");
	if (percent >= 0.0f)
	{
		DrawPercentBar(font, x, y, width, lineHeight, percentWidth, percent);
		y = lineHeight + y + BOX_SPACING;
	}
	if (totalPercent >= 0.0f)
	{
		DrawPercentBar(font, x, y, width, lineHeight, percentWidth, totalPercent);
		y = lineHeight + y + BOX_SPACING;
	}
	if (showTimes)
		DrawTimes(font, x, y, width);
}

// 0x000A3267 (font in ebx)
void DrawTransferBox(CXBFont *font, float x, float y, float width, float height, const wchar_t *title,
                     const wchar_t *line1, const wchar_t *line2, float percent, BOOL showTimes)
{
	float lineHeight = font->m_fScaleFactorY * font->m_fFontHeight;
	float rows = BOX_SPACING;
	if (title != NULL)
		rows = lineHeight + 10.0f;
	if (line1 != NULL)
		rows = lineHeight + rows + BOX_SPACING;
	if (line2 != NULL)
		rows = lineHeight + rows + BOX_SPACING;
	if (percent >= 0.0f)
		rows = (lineHeight + BOX_SPACING) * 2.0f + rows;   // the byte counts and the bar
	if (showTimes)
		rows = lineHeight + rows + BOX_SPACING;
	FitBox(&y, &width, &height, rows, lineHeight);

	DrawBevelBox(x, y, width, height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f,
	             2);
	x += BOX_SPACING;
	y += BOX_SPACING;
	y = DrawBoxLines(font, x, y, width, lineHeight, title, line1, line2);
	y += 3.0f;
	float percentWidth = font->GetTextWidth(L"100%");
	if (percent >= 0.0f)
	{
		// The byte counts (only their low 32 bits) once the size is known;
		// their line is kept either way.
		if (g_ullOperationBytes != 0)
		{
			wchar_t text[260];
			swprintf(text, 260, L"Current Transfer %12lu Bytes  %12lu Bytes", (unsigned long)(DWORD)g_ullOperationBytes,
			         (unsigned long)(DWORD)g_ullOperationDone);
			font->DrawText(x, y, g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_LEFT, width - 10.0f);
		}
		float barY = lineHeight + y + BOX_SPACING;
		DrawPercentBar(font, x, barY, width, lineHeight, percentWidth, percent);
		y = lineHeight + barY + BOX_SPACING;
	}
	if (showTimes)
		DrawTimes(font, x, y, width);
}

// 0x000A2163 (width in xmm0, font in esi). Nothing calls it.
// A caption: a bar over 80% of the width with the text in it.
HRESULT DrawCaptionBar(CXBFont *font, const wchar_t *text, float x, float y, float width, float height)
{
	float barWidth = width * 0.8f;
	DrawGradientRect(x, y, barWidth, height, g_colorScheme.CAPTION_BACKCOLOR, g_colorScheme.DIALOG_BACKCOLOR, FALSE,
	                 0);
	font->DrawText(x + 3.0f, font->m_fScaleFactorY * font->m_fFontHeight * 0.5f + y,
	               g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_CENTER_Y | XBFONT_TRUNCATED, barWidth - 3.0f);
	return S_OK;
}
