// CInternetSession::DrawProgress, the download box. See Http.h.
#include <cstring>
#include <cwchar>

#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/SkinColors.h"
#include "Http.h"

// A size as "1.50 MB", "12.00 KB" or "512 B". (inlined in DrawProgress)
static void FormatSize(wchar_t *out, float bytes)
{
	if (bytes * (1.0f / 1048576.0f) >= 1.0f)
		swprintf(out, 20, L"%.02f MB", (double)(bytes * (1.0f / 1048576.0f)));
	else if (bytes * (1.0f / 1024.0f) >= 1.0f)
		swprintf(out, 20, L"%.02f KB", (double)(bytes * (1.0f / 1024.0f)));
	else
		swprintf(out, 20, L"%.0f B", (double)bytes);
}

// 0x000D99F7 (font in ebx)
void CInternetSession::DrawProgress(CInternetSession *session, IDirect3DTexture8 *texture, CXBFont *font)
{
	if (font == NULL)
		return;
	wchar_t text[0x104];
	memset(text, 0, sizeof(text));
	std::string line;
	float lineHeight = font->m_fFontHeight * font->m_fScaleFactorY;

	// A 500x160 box (64 taller with a texture), centered.
	float x = 0.0f;
	float y = 0.0f;
	float width = 500.0f;
	float height = 160.0f;
	ScaleToScreen(NULL, &width, NULL, &height, FALSE);
	GetDisplaySize(&x, &y, FALSE);
	if (texture != NULL)
		height += 64.0f;
	y = (y - height) * 0.5f;
	x = (x - width) * 0.5f;
	DrawRect(x, y, width, height, g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	y += 2.0f;
	DrawRect(x + 2.0f, y + 1.0f, width - 4.0f, lineHeight + 5.0f, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
	         g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	x += 4.0f;
	y = lineHeight * 0.5f + 2.0f + y;

	// "name 42% Completed..."
	if (!session->m_bHashCacheName && strchr(session->m_szFileName, '?') == NULL)
		MultiByteToWideChar(CP_ACP, 0, session->m_szFileName, (int)strlen(session->m_szFileName), text, 0x104);
	else
		wcscpy(text, L"");
	const wchar_t *dots;
	switch (GetTickCount() / 500 % 3)
	{
	case 0: dots = L"..."; break;
	case 2: dots = L".."; break;
	default: dots = L"."; break;
	}
	wchar_t title[0x104];
	swprintf(title, 0x104, L"%ls %d%ls Completed%ls", text, session->GetPercent(), L"%", dots);
	font->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, title, XBFONT_CENTER_Y, 0.0f);

	if (texture != NULL)
	{
		float imageWidth = 468.0f;
		float imageHeight = 60.0f;
		ScaleToScreen(NULL, &imageWidth, NULL, &imageHeight, FALSE);
		y = lineHeight * 0.5f + 10.0f + y;
		DrawTexture(texture, x + 12.0f, y, imageWidth, imageHeight, 1.0f, NULL, 0.0f, NULL);
		y += lineHeight * 0.5f + imageHeight + 4.0f;
	}
	else
	{
		y += lineHeight + 10.0f;
	}

	font->Begin();
	StrFormat(line, "From:\t%s", session->m_bHashCacheName ? "..." : session->m_url.host);
	memset(text, 0, sizeof(text));
	MultiByteToWideChar(CP_ACP, 0, line.c_str(), (int)line.size(), text, 0x104);
	font->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_CENTER_Y, 0.0f);
	y += lineHeight;

	// "Est. Time:\t1:02 Sec (1.00 MB of 2.00 MB)"
	wchar_t total[20];
	wchar_t received[20];
	wchar_t timeLeft[20];
	if (session->m_dwDataSize == 0)
		wcscpy(total, L"unknown");
	else
		FormatSize(total, (float)session->m_dwDataSize);
	FormatSize(received, (float)session->m_dwReceived);
	DWORD seconds = session->m_dwSecondsLeft;
	if (seconds == 0 && session->m_dwDataSize == 0)
		wcscpy(timeLeft, L"(Unknown)");
	else if ((float)seconds * (1.0f / 3600.0f) >= 1.0f)
		// Quirk: the minutes are worked out wrong (nearly always 0).
		swprintf(timeLeft, 20, L"%d:%02d:%02d Sec", seconds / 3600, (seconds / 60 % seconds) / 3600, seconds % 60);
	else if ((float)seconds * (1.0f / 60.0f) >= 1.0f)
		swprintf(timeLeft, 20, L"%02d:%02d Sec", seconds / 60, seconds % 60);
	else
		swprintf(timeLeft, 20, L"%d Sec", seconds);
	swprintf(text, 0x104, L"Est. Time:\t%ls (%ls of %ls)", timeLeft, received, total);
	font->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_CENTER_Y, 0.0f);
	y += lineHeight;

	wchar_t rate[20];
	if (session->m_fRate * (1.0f / 1048576.0f) >= 1.0f)
		swprintf(rate, 20, L"%.01f MB/Sec", (double)(session->m_fRate * (1.0f / 1048576.0f)));
	else if (session->m_fRate * (1.0f / 1024.0f) >= 1.0f)
		swprintf(rate, 20, L"%.01f KB/Sec", (double)(session->m_fRate * (1.0f / 1024.0f)));
	else
		swprintf(rate, 20, L"%.0f B/Sec", (double)session->m_fRate);
	swprintf(text, 0x104, L"Transfer Rate:\t%ls", rate);
	font->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_CENTER_Y, 0.0f);
	y += lineHeight;

	memset(text, 0, sizeof(text));
	switch (session->m_nState)
	{
	case HTTP_OPENING: wcscpy(text, L"Status:\tOpening..."); break;
	case HTTP_CONNECTING: wcscpy(text, L"Status:\tConnecting..."); break;
	case HTTP_CONNECTED: wcscpy(text, L"Status:\tConnected."); break;
	case HTTP_DOWNLOADING: wcscpy(text, L"Status:\tDownloading..."); break;
	case HTTP_FINISHED: wcscpy(text, L"Status:\tFinished."); break;
	}
	font->DrawText(x, y, g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_CENTER_Y, 0.0f);
	font->End();

	// The bar, when the size is known.
	if (session->m_dwDataSize != 0)
	{
		y = lineHeight + y + 2.0f;
		float percentWidth = font->GetTextWidth(L" 100%");
		DrawProgressBar(x, y - (lineHeight - 10.0f) * 0.5f, (width - 8.0f) - percentWidth, 12.0f,
		                (float)session->GetPercent(), g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
		                0xFF0000FF, 0xFF000040, 1);
		swprintf(text, 0x104, L" %d%ls", session->GetPercent(), L"%");
		font->DrawText(x + width - 10.0f, y, g_colorScheme.CAPTION_TEXTCOLOR, text, XBFONT_RIGHT | XBFONT_CENTER_Y,
		               0.0f);
	}
}
