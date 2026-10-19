// CUpDown: the spinner control of the IP address and clock editors.
// Xbox only (Direct3D).
#include "UpDown.h"

#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"

// Shared by every spinner: the focused one's arrows fade in and out.
static float s_fArrowAlpha;              // 0x00278D5C
static float s_fArrowStep = 1.0f;        // 0x0026CC94
static DWORD s_dwLastMoveTimeout;        // 0x00278D60

// A textured screen vertex (D3DFVF_XYZRHW | D3DFVF_TEX1).
struct STexturedVertex
{
	float x, y, z, rhw;
	float u, v;
};

static void SetVertex(STexturedVertex *v, float x, float y, float u, float w)
{
	v->x = x;
	v->y = y;
	v->z = 1.0f;
	v->rhw = 1.0f;
	v->u = u;
	v->v = w;
}

// 0x000D84A1
CUpDown::CUpDown()
{
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_fPadX = 8.0f;
	m_pTexture = NULL;
	m_pSound = NULL;
	m_pFont = NULL;
	m_bCreated = FALSE;
	m_bFocused = FALSE;
	m_nSelected = 0;
	m_nLastMove = 0;
	m_fPadY = 19.0f;
	m_pSettings = CAppSettings::GetInstance();
	// (m_fArrowWidth/m_fArrowHeight are left to Create.)
}

// 0x000D80B4
CUpDown::~CUpDown()
{
	m_pSound = NULL;
	m_pFont = NULL;
}

float CUpDown::GetBoxWidth() const
{
	return m_pSettings->m_fFontScale * m_fWidth + m_fPadX * 2.0f;
}

float CUpDown::GetBoxHeight() const
{
	return m_pSettings->m_fFontScale * m_fHeight + m_fPadY * 2.0f;
}

// 0x000D81C2
HRESULT CUpDown::Create(CXBFont *font, float x, float y, float width, float height)
{
	if (m_bCreated)
		return S_OK;

	m_pFont = font;
	if (font == NULL)
		g_fonts.Find("MainFont", &m_pFont);
	if (m_pFont == NULL)
		return E_FAIL;

	g_textures.Find("Keyboard", &m_pTexture);
	m_fX = x;
	m_fY = y;
	m_fHeight = height;
	m_fWidth = width;
	float fontHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	if (height == 0.0f || fontHeight > height)
		m_fHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;

	if (width == 0.0f)
	{
		BOOL fitW;
		if (m_items.size() != 0)
		{
			for (int i = 0; i < (int)m_items.size(); i++)
			{
				float itemWidth = m_pFont->GetTextWidth(m_items[i].c_str());
				if (itemWidth > m_fWidth)
					m_fWidth = itemWidth;
			}
			fitW = m_fWidth == 0.0f;
		}
		else
		{
			// The original measures this one with &m_pTexture as the font (a
			// bug); only whether it exceeds the width (0) matters.
			fitW = m_pFont->GetTextWidth(L"W") + 4.0f > m_fWidth;
		}
		if (fitW)
			m_fWidth = m_pFont->GetTextWidth(L"W");
	}

	m_fArrowWidth = (m_pSettings->m_fFontScale * m_fWidth + m_fPadX * 2.0f) * 0.2f;
	if (m_fArrowWidth < 10.0f)
		m_fArrowWidth = 10.0f;
	m_fArrowHeight = (m_pSettings->m_fFontScale * m_fHeight + m_fPadY * 2.0f) * (1.0f / 12.0f);
	if (m_fArrowHeight < 10.0f)
		m_fArrowHeight = 10.0f;
	m_pSound = g_soundEffects.Find("Scroll");
	return S_OK;
}

// 0x000D8398
HRESULT CUpDown::AddItem(const wchar_t *text)
{
	m_items.push_back(std::wstring(text));
	return S_OK;
}

// 0x000D8469
HRESULT CUpDown::AddItems(const wchar_t *text)
{
	for (int i = 0; i < (int)wcslen(text); i++)
		AddItem(text + i);
	return S_OK;
}

// 0x000D8083
HRESULT CUpDown::SetItemText(int index, const wchar_t *text)
{
	if (index <= -1 || index >= (int)m_items.size())
		return E_FAIL;
	m_items[index] = text;
	return S_OK;
}

// 0x000D7837
BOOL CUpDown::IsSelectedEmpty()
{
	// (<= size: reads one past the end when the selection is size.)
	if (m_items.size() == 0 || (unsigned)m_nSelected > m_items.size())
		return FALSE;
	return wcslen(m_items[m_nSelected].c_str()) == 0;
}

// 0x000D787C
const wchar_t *CUpDown::GetText()
{
	// (<= size, as in IsSelectedEmpty.)
	if (m_items.size() == 0 || (unsigned)m_nSelected > m_items.size())
		return L"";
	return m_items[m_nSelected].c_str();
}

// 0x000D78B8
HRESULT CUpDown::Render()
{
	float boxWidth = GetBoxWidth();
	float boxHeight = GetBoxHeight();
	float left = m_fX;
	float top = m_fY;
	float right = boxWidth + left;
	float bottom = boxHeight + top;

	// The key texture in three columns: the left half, the middle column
	// stretched, the right half. The end columns have no width (they're
	// drawn as lines of zero area), so only the middle shows.
	STexturedVertex v[16];
	SetVertex(&v[0], left - 0.5f, top - 0.5f, 0.0f, 0.0f);
	SetVertex(&v[1], left - 0.5f, top - 0.5f, 0.5f, 0.0f);
	SetVertex(&v[2], left - 0.5f, bottom - 0.5f, 0.5f, 1.0f);
	SetVertex(&v[3], left - 0.5f, bottom - 0.5f, 0.0f, 1.0f);
	SetVertex(&v[4], left - 0.5f, top - 0.5f, 0.5f, 0.0f);
	SetVertex(&v[5], right - 0.5f, top - 0.5f, 0.5f, 0.0f);
	SetVertex(&v[6], right - 0.5f, bottom - 0.5f, 0.5f, 1.0f);
	SetVertex(&v[7], left - 0.5f, bottom - 0.5f, 0.5f, 1.0f);
	SetVertex(&v[8], right - 0.5f, top - 0.5f, 0.5f, 0.0f);
	SetVertex(&v[9], right - 0.5f, top - 0.5f, 1.0f, 0.0f);
	SetVertex(&v[10], right - 0.5f, bottom - 0.5f, 1.0f, 1.0f);
	SetVertex(&v[11], right - 0.5f, bottom - 0.5f, 0.5f, 1.0f);
	// Built (without texture coordinates) but not drawn.
	SetVertex(&v[12], left - 0.5f, top - 0.5f, 0.0f, 0.0f);
	SetVertex(&v[13], right - 0.5f, top - 0.5f, 0.0f, 0.0f);
	SetVertex(&v[14], right - 0.5f, bottom - 0.5f, 0.0f, 0.0f);
	SetVertex(&v[15], left - 0.5f, bottom - 0.5f, 0.0f, 0.0f);

	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_pd3dDevice->SetTexture(0, m_pTexture);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	DWORD textColor = g_colorScheme.KB_KEY_NORMAL;
	if (m_bFocused)
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.KB_KEYBG_HIGHLIGHT);
		textColor = 0xFFFFFFFF;
	}
	else
	{
		g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
	}
	g_pd3dDevice->DrawVerticesUP(D3DPT_QUADLIST, 12, v, sizeof(STexturedVertex));

	if (m_bFocused)
	{
		// The arrows: the one last used fully lit, the other pulsing.
		s_fArrowAlpha = s_fArrowStep * 0.025f + s_fArrowAlpha;
		float arrowX = (GetBoxWidth() - m_fArrowWidth) * 0.5f + m_fX;
		float alpha = m_nLastMove == 1 ? 1.0f : s_fArrowAlpha;
		DrawTriangle(arrowX, m_fY + 4.0f, m_fArrowWidth, m_fArrowHeight, 1, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
		             g_colorScheme.DIALOG_INNER_BACKCOLOR, alpha);
		float arrowY = GetBoxHeight() + m_fY - 4.0f - m_fArrowHeight;
		alpha = m_nLastMove == -1 ? 1.0f : s_fArrowAlpha;
		DrawTriangle(arrowX, arrowY, m_fArrowWidth, m_fArrowHeight, 2, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
		             g_colorScheme.DIALOG_INNER_BACKCOLOR, alpha);
		if (0.0f >= s_fArrowAlpha)
			s_fArrowStep = 1.0f;
		else if (s_fArrowAlpha >= 1.0f)
			s_fArrowStep = -1.0f;
	}

	m_pFont->DrawText((right + left) * 0.5f, (bottom + top) * 0.5f, textColor, GetText(),
	                  XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
	return S_OK;
}

// 0x000D7EF8
HRESULT CUpDown::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action == INPUT_LTHUMB && pad != NULL && *pad != NULL)
	{
		if ((*pad)->fY1 == 1.0f)
			*action = INPUT_UP;
		else if ((*pad)->fY1 == -1.0f)
			*action = INPUT_DOWN;
	}

	switch (*action)
	{
	case INPUT_LTRIGGER:
	case INPUT_UP:
		if (m_items.size() != 0)
		{
			// The next non-empty item (giving up after a full turn).
			int tries = 0;
			for (;;)
			{
				m_nSelected = m_nSelected == (int)m_items.size() - 1 ? 0 : m_nSelected + 1;
				if (tries > (int)m_items.size())
					break;
				tries++;
				if (!IsSelectedEmpty())
					break;
			}
			PlaySound(m_pSound);
			m_nLastMove = 1;
		}
		break;

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		if (m_items.size() != 0)
		{
			int tries = 0;
			for (;;)
			{
				m_nSelected = (m_nSelected == 0 ? (int)m_items.size() : m_nSelected) - 1;
				if (tries > (int)m_items.size())
					break;
				tries++;
				if (!IsSelectedEmpty())
					break;
			}
			PlaySound(m_pSound);
			m_nLastMove = -1;
		}
		break;

	default:
		// Any other input unlights the arrow, but at most once every 2s
		// (the timeout is shared by all spinners).
		if (GetTickCount() > s_dwLastMoveTimeout)
		{
			m_nLastMove = 0;
			s_dwLastMoveTimeout = GetTickCount() + 2000;
		}
		break;
	}
	return S_OK;
}
