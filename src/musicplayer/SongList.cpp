// CSongListControl, the song list box (0x000F0B92-0x000F42FE). See
// SongList.h.
#include "SongList.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

#include "../audio/AudioPlayer.h"
#include "../audio/MusicManager.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/SkinColors.h"

// A textured screen vertex (D3DFVF_XYZRHW | D3DFVF_TEX1).
struct SPanelVertex
{
	float x, y, z, rhw;
	float u, v;
};

static void SetPanelVertex(SPanelVertex *v, float x, float y, float u, float w)
{
	v->x = x;
	v->y = y;
	v->z = 1.0f;
	v->rhw = 1.0f;
	v->u = u;
	v->v = w;
}

// "mm:ss" of a length in seconds.
static std::wstring FormatMinutes(DWORD minutes, DWORD seconds)
{
	wchar_t text[100];
	swprintf(text, 100, L"%02d:%02d", minutes, seconds);
	return text;
}

// 0x000F0CAE
void DrawPanelTexture(IDirect3DTexture8 *texture, float x, float y, float width, float height, DWORD factor)
{
	SPanelVertex v[4];
	SetPanelVertex(&v[0], x - 0.5f, y - 0.5f, 0.1f, 0.1f);
	SetPanelVertex(&v[1], (x + width) - 0.5f, y - 0.5f, 0.9f, 0.1f);
	SetPanelVertex(&v[2], (x + width) - 0.5f, (y + height) - 0.5f, 0.9f, 0.9f);
	SetPanelVertex(&v[3], x - 0.5f, (y + height) - 0.5f, 0.1f, 0.9f);

	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_pd3dDevice->SetTexture(0, texture);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, factor);
	g_pd3dDevice->DrawVerticesUP(D3DPT_POLYGON, 4, v, sizeof(SPanelVertex));
}

// 0x000F12A3
CSongListControl::CSongListControl()
{
	m_nSoundtrack = -1;
	m_nFilled = (UINT)-1;
	m_fHeight = 0.0f;
	m_fWidth = 0.0f;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_pFont = NULL;
	m_bInitialized = FALSE;
	m_pTexture = NULL;
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	m_fFontScale = 1.0f;
	m_nSelected = 0;
	m_bMusicManager = FALSE;
	m_nTop = 0;
	m_items.clear();
	m_nCurrentSong = (UINT)-1;
	m_bSkinTexture = FALSE;
	// (Not set by the original: SetLayout sets the margins before the
	// first Render, and m_nLastVisible is left to the first Render.)
	m_fMarginLeft = m_fMarginTop = m_fMarginRight = m_fMarginBottom = 0.0f;
	m_nLastVisible = 0;
}

// 0x000F1057
CSongListControl::~CSongListControl()
{
}

// 0x000F1D2F
HRESULT CSongListControl::Init(CXBFont *font, float x, float y, float width, float height)
{
	m_pFont = font;
	if (!m_bInitialized && font != NULL)
	{
		g_textures.Find("Keyboard", &m_pTexture);
		m_bInitialized = TRUE;
	}
	m_fHeight = height;
	m_fWidth = width;
	m_fY = y;
	m_fX = x;
	return m_pFont != NULL ? S_OK : E_FAIL;
}

// 0x000F1BCE
void CSongListControl::SetLayout(IDirect3DTexture8 *texture, float height, float x, float y, float width,
                                 float marginLeft, float marginTop, float marginRight, float marginBottom)
{
	if (!m_bInitialized)
		return;
	if (height != 0.0f)
		m_fHeight = height;
	if (width != 0.0f)
		m_fWidth = width;
	if (y != 0.0f)
		m_fY = y;
	if (x != 0.0f)
		m_fX = x;
	if (marginLeft != 0.0f)
		m_fMarginLeft = marginLeft;
	if (marginTop != 0.0f)
		m_fMarginTop = marginTop;
	m_fMarginRight = marginRight > 0.0f ? marginRight : m_fMarginLeft;
	m_fMarginBottom = marginBottom > 0.0f ? marginBottom : m_fMarginTop;

	if (texture == NULL)
	{
		// The built-in panel; the "Keyboard" texture is looked up again
		// only after a skin texture (or if it was missing).
		if (m_bSkinTexture || m_pTexture == NULL)
			g_textures.Find("Keyboard", &m_pTexture);
		m_bSkinTexture = FALSE;
		m_fMarginTop = 5.0f;
		m_fMarginLeft = 5.0f;
		m_fMarginBottom = 5.0f;
		m_fMarginRight = 5.0f;
	}
	else
	{
		m_bSkinTexture = TRUE;
		m_pTexture = texture;
	}

	m_fScaleY = 1.0f;
	m_fScaleX = 1.0f;
	ScaleToScreen(NULL, &m_fScaleX, NULL, &m_fScaleY, FALSE);
	m_fMarginLeft *= m_fScaleX;
	m_fMarginTop = m_fScaleY * m_fMarginTop;
	m_fMarginRight *= m_fScaleX;
	m_fMarginBottom *= m_fScaleY;
	m_fFontScale = m_pFont->m_fScaleFactorX;
}

// 0x000F0BB6
void CSongListControl::SetSelection(UINT index)
{
	if (index >= m_items.size())
		return;
	m_nSelected = index;
	if (index < m_nTop)
		m_nTop = index;
	// (Before the first Render the box is taken to hold 10 lines.)
	if (m_nLastVisible == 0)
		m_nLastVisible = 10;
	if (m_nLastVisible < index)
	{
		do
		{
			m_nLastVisible++;
			m_nTop++;
		} while (m_nLastVisible < m_nSelected);
	}
}

// (inlined)
void CSongListControl::MoveUp()
{
	if (m_nSelected == 0)
		return;
	m_nSelected--;
	if (m_nSelected < m_nTop)
		m_nTop = m_nSelected;
}

// (inlined)
void CSongListControl::MoveDown()
{
	if (m_nSelected >= m_items.size() - 1)
		return;
	m_nSelected++;
	if (m_nLastVisible < m_nSelected && m_nLastVisible != 0)
	{
		do
		{
			m_nLastVisible++;
			m_nTop++;
		} while (m_nLastVisible < m_nSelected);
	}
}

// 0x000F0BEC
HRESULT CSongListControl::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_NONE)
		return S_OK;
	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}
	// The triggers move a single line too.
	switch (*action)
	{
	case INPUT_LTRIGGER:
	case INPUT_UP:
		MoveUp();
		break;
	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		MoveDown();
		break;
	default:
		return S_OK;
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x000F0EB3
void CSongListControl::SetSoundtrack(int soundtrack)
{
	if (m_nSoundtrack == soundtrack)
		return;
	m_nFilled = (UINT)-1;
	m_nCurrentSong = (UINT)-1;
	m_nSoundtrack = soundtrack;
	m_items.clear();
	m_nTop = 0;
	m_nSelected = 0;
}

// 0x000F0FBD
void CSongListControl::Reload()
{
	m_nFilled = (UINT)-1;
	m_items.clear();
	Sleep(100);
	XSOUNDTRACK_DATA data;
	HANDLE find = XFindFirstSoundtrack(&data);
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (data.uSongCount != 0 && (int)data.uSoundtrackId != m_nSoundtrack)
			{
				DWORD songId = 0;
				DWORD length;
				wchar_t name[100];
				XGetSoundtrackSongInfo(data.uSoundtrackId, 0, &songId, &length, name, 100);
				break;
			}
		} while (XFindNextSoundtrack(find, &data));
		XFindClose(find);
	}
}

// 0x000F0ED6
void CSongListControl::UpdateCurrentSongTime()
{
	if (!m_bMusicManager || m_nCurrentSong == g_pMusicManager->GetCurrentSong())
		return;
	m_nCurrentSong = g_pMusicManager->GetCurrentSong();
	SSong *song = g_pMusicManager->GetSong(m_nCurrentSong);
	if (song == NULL)
		return;
	for (UINT i = 0; i < m_items.size(); i++)
	{
		SSongItem &item = m_items[i];
		if (item.index != song->index)
			continue;
		if (song->length == SONG_LENGTH_UNKNOWN || !item.time.empty())
			return;
		DWORD seconds = song->length / 1000;
		item.length = song->length;
		item.time = FormatMinutes(seconds / 60, seconds % 60);
		return;
	}
}

// 0x000F124B
void CSongListControl::SetMarked(UINT index, BOOL marked)
{
	if (index >= m_items.size())
		return;
	m_items[index].marked = marked;
	if (marked)
		UpdateCurrentSongTime();
}

// 0x000F1273
void CSongListControl::ClearMarks()
{
	for (UINT i = 0; i < m_items.size(); i++)
		SetMarked(i, FALSE);
}

// 0x000F0B92
void CSongListControl::ToggleMark(UINT index)
{
	if (index < m_items.size())
		m_items[index].marked = !m_items[index].marked;
}

// 0x000F0C97
const wchar_t *CSongListControl::GetSelectedName()
{
	return m_items[m_nSelected].name.c_str();
}

// The sort of Fill: by the song's index. (0x000F0E2B)
static bool SongItemLess(const SSongItem &a, const SSongItem &b)
{
	return a.index < b.index;
}

// A line for the list.
static SSongItem MakeSongItem(const std::wstring &name, const std::wstring &time, UINT index, DWORD length)
{
	SSongItem item;   // (0x00029932)
	item.name = name;
	item.time = time;
	item.index = index;
	item.marked = FALSE;
	item.length = length;
	return item;
}

// 0x000F2241
void CSongListControl::Fill(UINT next)
{
	wchar_t text[100];
	std::wstring name;
	std::wstring time;

	if (m_bMusicManager)
	{
		// The music manager's current soundtrack (copied by 0x0002998B).
		SSoundtrack soundtrack = g_pMusicManager->m_soundtracks[g_pMusicManager->m_nSoundtrack];
		if (soundtrack.isFolder)
		{
			for (UINT i = 0; i < g_pMusicManager->GetSongCount(); i++)
			{
				SSong *song = g_pMusicManager->GetSong(i);
				if (song == NULL)
					continue;
				memset(text, 0, sizeof(text));
				swprintf(text, 100, L"%u. %s", song->index + 1, song->name);
				name = text;
				if (song->length != SONG_LENGTH_UNKNOWN)
					time = FormatMinutes(song->length / 1000 / 60, song->length / 1000 % 60);
				else
					time.clear();
				m_items.push_back(MakeSongItem(name, time, song->index, song->length));
			}
		}
		else
		{
			// An Xbox soundtrack, by its own song count. Quirk: an unknown
			// length keeps the previous line's time.
			for (UINT i = 0; i < soundtrack.songCount; i++)
			{
				SSong *song = g_pMusicManager->GetSong(i);
				if (song == NULL)
					continue;
				swprintf(text, 100, L"%u. %s", song->index + 1, song->name);
				name = text;
				if (song->length != SONG_LENGTH_UNKNOWN)
					time = FormatMinutes(song->length / 1000 / 60, song->length / 1000 % 60);
				m_items.push_back(MakeSongItem(name, time, song->index, song->length));
			}
		}
		std::sort(m_items.begin(), m_items.end(), SongItemLess);
		m_nFilled = m_items.size();
	}
	else if (m_nSoundtrack == 0)
	{
		// The audio CD: "<n>. Track <n>" with the TOC's lengths.
		if (g_cdToc.hDevice == INVALID_HANDLE_VALUE)
			return;
		m_nFilled = g_cdToc.nTracks;
		for (UINT i = 0; i < m_nFilled; i++)
		{
			swprintf(text, 100, L"%u. Track %u", i + 1, i + 1);
			name = text;
			DWORD minutes, seconds;
			CdTocGetTrackLength(&g_cdToc, i, &minutes, &seconds, NULL);
			time = FormatMinutes(minutes, seconds);
			m_items.push_back(MakeSongItem(name, time, i, (minutes * 60 + seconds) * 1000));
		}
	}
	else
	{
		// An Xbox soundtrack: one song per call; the end is when the next
		// one can't be read.
		DWORD songId, length;
		wchar_t title[100];
		if (XGetSoundtrackSongInfo((DWORD)m_nSoundtrack, next, &songId, &length, title, 100) && length != 0)
		{
			name = title;
			length /= 1000;
			time = FormatMinutes(length / 60, length % 60);
			m_items.push_back(MakeSongItem(name, time, next, length * 1000));
		}
		else
		{
			m_nFilled = m_items.size();
		}
	}
}

// 0x000F3DA5
void CSongListControl::UpdatePlayingTime(UINT index)
{
	static DWORD s_dwNextUpdate = GetTickCount() + 2000;   // 0x0027A414
	if (GetTickCount() < s_dwNextUpdate)
		return;
	s_dwNextUpdate = GetTickCount() + 2000;
	DWORD length;
	g_pMusicManager->GetCurrentInfo(NULL, NULL, &length);
	if (length == SONG_LENGTH_UNKNOWN)
		return;
	// Quirk: index is the song's index, used as the line number.
	SSongItem &item = m_items[index];
	if (!item.time.empty())
		return;
	DWORD seconds = length / 1000;
	item.length = seconds * 1000;
	item.time = FormatMinutes(seconds / 60, seconds % 60);
}

// 0x000F3E6A
HRESULT CSongListControl::Render()
{
	m_pFont->SetScaleFactors(m_fFontScale, m_fFontScale);
	float x = m_fX;
	float y = m_fY;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(&x, &scaleX, &y, &scaleY, FALSE);
	float width = m_fWidth * scaleX;
	float height = m_fHeight * scaleY;

	if (!m_bSkinTexture)
	{
		DrawPanelTexture(m_pTexture, x, y, width, height);
		DrawBevelBox(m_fMarginLeft + x, m_fMarginTop + y, (width - m_fMarginLeft) - m_fMarginRight,
		             (height - m_fMarginTop) - m_fMarginBottom, g_colorScheme.MM_BORDERCOLOR,
		             g_colorScheme.MMLightBackColor, 0.95f, 2);
	}
	else
	{
		DrawTexture(m_pTexture, x, y, width, height, 1.0f, NULL, 0.0f, NULL);
	}
	SetScissorRect(TRUE, m_fMarginLeft + x, m_fMarginTop + y, width - (m_fMarginRight + m_fMarginLeft),
	               (height - m_fMarginTop) - m_fMarginBottom);

	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	float bottom = ((height + y) - lineHeight * 0.5f) - m_fMarginBottom;
	float lineX = m_fMarginLeft + x;
	float lineY;
	if (!m_bSkinTexture)
	{
		lineX += 5.0f;
		lineY = m_fMarginTop + y + 5.0f;
	}
	else
	{
		lineY = m_fMarginTop + y;
	}

	if (m_nFilled != m_items.size())
		Fill(m_items.size());

	const wchar_t marker[2] = {0x00BB, 0};   // "»"
	for (UINT i = m_nTop; i < m_items.size(); i++)
	{
		SSongItem &item = m_items[i];
		if (m_nSelected == i)
			DrawBevelBox(lineX - 1.0f, lineY - 1.0f, ((width - m_fMarginLeft) - m_fMarginRight) - 6.0f,
			             lineHeight + 1.0f, 0, g_colorScheme.MMDarkBackColor, 0.8f, 1);
		wchar_t text[100];
		swprintf(text, 100, L"%s%s", item.marked ? marker : L"  ", item.name.c_str());
		float timeX = (((width + lineX) - m_fMarginLeft) - m_fMarginRight) - 10.0f;
		if (!item.marked && m_nSelected != i)
		{
			m_pFont->DrawText(lineX + 1.0f, lineY, g_colorScheme.MMForeColor, text, XBFONT_LEFT, 0.0f);
			m_pFont->DrawText(timeX, lineY, g_colorScheme.MMForeColor, item.time.c_str(), XBFONT_RIGHT, 0.0f);
		}
		else
		{
			m_pFont->DrawText(lineX + 1.0f, lineY, g_colorScheme.MMActiveForeColor, text, XBFONT_LEFT, 0.0f);
			m_pFont->DrawText(timeX, lineY, g_colorScheme.MMActiveForeColor, item.time.c_str(), XBFONT_RIGHT,
			                  0.0f);
			if (item.marked)
				UpdatePlayingTime(item.index);
		}
		lineY = lineHeight + lineY;
		if (lineY > bottom)
		{
			// The box is full: remember the last line and scroll the
			// selection into view.
			m_nLastVisible = i;
			while (m_nLastVisible < m_nSelected)
			{
				m_nTop++;
				m_nLastVisible++;
			}
			if (m_nTop > m_nSelected)
				m_nTop = m_nSelected;
			break;
		}
	}
	SetScissorRect(FALSE, 0.0f, 0.0f, 0.0f, 0.0f);
	return S_OK;
}
