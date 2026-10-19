// CVirtualKeyboard: the on-screen keyboard. See VirtualKeyboard.h.
// (Not decompiled, as nothing calls them: 0x000DBB67 draws text with a
// shadow, 0x000DBEDE draws language names, 0x000DD83A reads the gamepad
// with key repeat.)
#include "VirtualKeyboard.h"

#include <cmath>
#include <cstring>
#include <cwchar>
#include <cwctype>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../input/XBInput.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"

CVirtualKeyboard g_virtualKeyboard;   // 0x00278328

// The sample's English strings (index: see GetKeyboardString).
static const wchar_t *const s_englishStrings[21] = {
	L"English",                                   // 0 the language
	L"Choose Keyboard",
	L"Sample graphics. Don't use in your game",
	L"Select",
	L"Back",
	L"Help",
	L"Space",                                     // 6
	L"Backspace",                                 // 7
	L"Shift",                                     // 8
	L"Caps Lock",                                 // 9
	L"Alphabet",                                  // 10
	L"Symbols",                                   // 11
	L"Accents",                                   // 12
	L"Done",                                      // 13
	L"Select",
	L"Cancel",
	L"Toggle\nmode",
	L"Display help",
	L"Backspace",
	L"Space",
	L"Trigger buttons move cursor",
};

// 0x0026C23C: English, and an empty second entry.
static const SKeyboardLanguage s_languages[2] = {
	{1, 0, s_englishStrings},
	{0, 0, NULL},
};
static const wchar_t *const *s_pKeyboardStrings = s_englishStrings;   // 0x0026A69C

// 0x000DBAD8 (strings in ecx, index in eax)
static const wchar_t *GetKeyboardString(const wchar_t *const *strings, unsigned index)
{
	if (strings != NULL && index < 21)
		return strings[index];
	return L"";
}

// 0x000DBE81
static const wchar_t *GetKeyboardString(unsigned index)
{
	return GetKeyboardString(s_pKeyboardStrings, index);
}

// 0x000DBAEB (ch in ax). Latin-1 lower case (and Ÿ to ÿ).
static wchar_t ToLower(wchar_t ch)
{
	if ((ch >= 0x41 && ch <= 0x5A) || (ch >= 0xC0 && ch <= 0xDF))
		return (wchar_t)(ch + 0x20);
	if (ch == 0x178)
		return 0xFF;
	return ch;
}

// 0x000DBB12 (ch in ax). Quirk: also turns ÷ into × and ÿ into ß.
static wchar_t ToUpper(wchar_t ch)
{
	if ((ch >= 0x61 && ch <= 0x7A) || (ch >= 0xE0 && ch <= 0xFF))
		return (wchar_t)(ch - 0x20);
	return ch;
}

// 0x000DBB39: upper case with exactly one of shift and caps lock.
wchar_t CVirtualKeyboard::ApplyCase(int ch)
{
	if ((m_bCapsLock != 0) != (m_bShift != 0))
		return ToUpper((wchar_t)ch);
	return ToLower((wchar_t)ch);
}

// The gamepad port that last had input (remembered), else the first one
// with input, else the first connected.
static int s_nKeyboardPort = -1;   // 0x0026CC24

// 0x000DBBD5 (pad in eax): any button, trigger or stick off center.
static BOOL HasInput(const XBGAMEPAD *pad)
{
	if (pad->wButtons != 0)
		return TRUE;
	for (int i = 0; i < 8; i++)
	{
		if (pad->bAnalogButtons[i] > 30)
			return TRUE;
	}
	return pad->fX1 > 0.25f || pad->fX1 < -0.25f || pad->fY1 > 0.25f || pad->fY1 < -0.25f || pad->fX2 > 0.25f ||
	       pad->fX2 < -0.25f || pad->fY2 > 0.25f || pad->fY2 < -0.25f;
}

// 0x000DC05A
static XBGAMEPAD *GetKeyboardGamepad()
{
	if (s_nKeyboardPort != -1 && g_Gamepads[s_nKeyboardPort].hDevice != NULL)
		return &g_Gamepads[s_nKeyboardPort];
	int first = -1;
	for (int port = 0; port < 4; port++)
	{
		if (g_Gamepads[port].hDevice == NULL)
			continue;
		if (first == -1)
			first = port;
		if (HasInput(&g_Gamepads[port]))
		{
			s_nKeyboardPort = port;
			return &g_Gamepads[port];
		}
	}
	return first == -1 ? NULL : &g_Gamepads[first];
}

// 0x000DE5B4
CVirtualKeyboard::CVirtualKeyboard()
{
	m_fRepeatDelay = 0.333f;
	m_fKeyHeight = 42.0f;
	m_fKeyWidth = 30.0f;
	m_bCapsLock = FALSE;
	m_bShift = FALSE;
	m_bHidden = FALSE;
	m_nCursor = 0;
	m_nLayout = KEYBOARD_ALPHABET;
	m_nRow = 0;
	m_nCol = 0;
	m_nLastCol = 0;
	m_pKeyTexture = NULL;
	m_pLanguage = &s_languages[0];
	m_nRows = 4;
	for (int i = 0; i < 5; i++)
		m_unknownDC[i] = 0;
	m_fX = 0.0f;
	m_fY = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_fTextHeight = 0.0f;
	m_fTextWidth = 0.0f;
	m_fTitleHeight = 0.0f;
	m_bCancelled = FALSE;
	m_bDone = FALSE;
	m_bNumeric = FALSE;
	m_bPassword = FALSE;
	m_cursorTimer.Resume();
	m_bClickSound = FALSE;
	m_nMaxLength = 64;
	m_pKeystroke = GetKeystroke();
	m_pEvents = NULL;
	m_bEmbedded = FALSE;
	m_pIR = NULL;
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x000DBE8C
void CVirtualKeyboard::SetLanguage(int language)
{
	if ((unsigned)language > 1)
		return;
	m_nLanguage = language;
	m_pLanguage = &s_languages[language];
	m_dwLanguageId = m_pLanguage->languageId;
	s_pKeyboardStrings = m_pLanguage->strings;
}

// 0x000DBEB2
void CVirtualKeyboard::PlayClick()
{
	if (m_bClickSound && m_pClickSound->m_pBuffer != NULL)
	{
		m_pClickSound->m_pBuffer->SetCurrentPosition(0);
		m_pClickSound->Play(0);
	}
}

// 0x000DFD78
HRESULT CVirtualKeyboard::Create(CXBFont *font, CXBFont * /*font2*/, IDirect3DTexture8 *keys,
                                 CXBPackedResource *resource, float x, float y, CVirtualKeyboardEvent *events)
{
	m_fX = x;
	m_fY = y;
	if (m_fWidth == 0.0f)
		m_fWidth = 640.0f;
	if (m_fHeight == 0.0f)
		m_fHeight = 480.0f;
	if (font == NULL)
		return E_FAIL;
	m_pFont = font;
	m_pFont->SetScaleFactors(1.0f, 1.0f);
	m_fTextWidth = m_pFont->GetTextWidth(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890 W");
	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	m_fTextHeight = lineHeight + 10.0f;
	m_fKeyHeight = lineHeight + (float)(m_bEmbedded ? 6 : 12);
	m_fKeyWidth = m_pFont->GetTextWidth(L"W") + 6.0f;
	m_pClickSound = g_soundEffects.Find("Keyboard Stroke");
	m_bClickSound = m_pClickSound != NULL;
	if (keys == NULL && resource != NULL)
		keys = resource->GetTexture("Keyboard");
	if (keys != NULL)
		m_pKeyTexture = keys;
	m_pEvents = events;
	SetLanguage(0);
	BuildLayouts();
	Open();
	return S_OK;
}

// The layouts (0x000DE720 builds them key by key). Widths: 'K' the key
// width, 'C' the caps lock key's, else pixels.
struct SKeyDef
{
	int code;
	int width;   // -1 = key, -2 = caps
};
#define K -1
#define C -2
static const SKeyDef s_alphabet[4][14] = {
	{{KEY_SHIFT, C}, {'A', K}, {'B', K}, {'C', K}, {'D', K}, {'E', K}, {'F', K}, {'G', K}, {'H', K}, {'I', K},
	 {'J', K}, {'1', K}, {'2', K}, {'3', K}},
	{{KEY_CAPSLOCK, C}, {'K', K}, {'L', K}, {'M', K}, {'N', K}, {'O', K}, {'P', K}, {'Q', K}, {'R', K}, {'S', K},
	 {'T', K}, {'4', K}, {'5', K}, {'6', K}},
	{{KEY_SYMBOLS, C}, {'U', K}, {'V', K}, {'W', K}, {'X', K}, {'Y', K}, {'Z', K}, {KEY_LEFT, 61}, {KEY_RIGHT, 61},
	 {'7', K}, {'8', K}, {'9', K}, {0, 0}, {0, 0}},
	{{KEY_DONE, C}, {' ', 185}, {KEY_BACKSPACE, 123}, {'0', 61}, {'.', K}, {0, 0}},
};
static const SKeyDef s_symbols[4][14] = {
	{{KEY_SHIFT, C}, {'!', K}, {'@', K}, {'#', K}, {'$', K}, {'%', K}, {'^', K}, {'&', K}, {'(', K}, {')', K},
	 {'_', K}, {'*', K}, {'+', K}, {'-', K}},
	{{KEY_CAPSLOCK, 110}, {'/', K}, {'\\', K}, {'[', K}, {']', K}, {0xA3, K}, {0xA5, K}, {'|', K}, {';', K},
	 {':', K}, {'\'', K}, {'"', K}, {'<', K}, {'>', K}},
	{{KEY_ACCENTS, 110}, {'?', K}, {0xBF, K}, {0xA1, K}, {'=', K}, {'{', K}, {'}', K}, {KEY_LEFT, 61},
	 {KEY_RIGHT, 61}, {0xAB, K}, {0xBB, K}, {',', K}, {0, 0}, {0, 0}},
	{{KEY_DONE, C}, {' ', 185}, {KEY_BACKSPACE, 123}, {'0', 61}, {'.', K}, {0, 0}},
};
static const SKeyDef s_accents[4][14] = {
	{{KEY_SHIFT, C}, {0xE0, K}, {0xE1, K}, {0xE2, K}, {0xE3, K}, {0xE4, K}, {0xE5, K}, {0xE6, K}, {0xE7, K},
	 {0xE8, K}, {0xE9, K}, {0xA9, K}, {0xA2, K}, {0xAE, K}},
	{{KEY_CAPSLOCK, 110}, {0xEA, K}, {0xEB, K}, {0xEC, K}, {0xED, K}, {0xEE, K}, {0xEF, K}, {0xF1, K}, {0xF2, K},
	 {0xF3, K}, {0xF4, K}, {0xB9, K}, {0xB2, K}, {0xB3, K}},
	{{KEY_ALPHABET, 110}, {0xF5, K}, {0xF6, K}, {0xF8, K}, {0xF9, K}, {0xFA, K}, {0xFB, K}, {KEY_LEFT, 61},
	 {KEY_RIGHT, 61}, {0xFC, K}, {0xFD, K}, {0xFF, K}, {0, 0}, {0, 0}},
	{{KEY_DONE, C}, {' ', 185}, {KEY_BACKSPACE, 123}, {'0', 61}, {'.', K}, {0, 0}},
};
#undef K
#undef C

// 0x000DDA78 (key in esi, code in eax): the special keys get a label.
static void MakeKey(SKeyboardKey &key, int code, int width)
{
	key.code = code;
	key.width = width;
	key.label = L"";
	switch (code)
	{
	case ' ':
		key.label = GetKeyboardString(6);
		break;
	case KEY_LEFT:
		key.label.assign(1, L'<');
		break;
	case KEY_RIGHT:
		key.label.assign(1, L'>');
		break;
	case KEY_BACKSPACE:
		key.label = GetKeyboardString(7);
		break;
	case KEY_SHIFT:
		key.label = GetKeyboardString(8);
		break;
	case KEY_CAPSLOCK:
		key.label = GetKeyboardString(9);
		break;
	case KEY_ALPHABET:
		key.label = GetKeyboardString(10);
		break;
	case KEY_SYMBOLS:
		key.label = GetKeyboardString(11);
		break;
	case KEY_ACCENTS:
		key.label = GetKeyboardString(12);
		break;
	case KEY_DONE:
		key.label = GetKeyboardString(13);
		break;
	}
}

// 0x000DE720
// Resets the input, sizes the box from the key and font sizes, and builds
// the three layouts.
void CVirtualKeyboard::BuildLayouts()
{
	m_nRow = 0;
	m_nCol = 1;
	m_nLastCol = 1;
	m_nLayout = KEYBOARD_ALPHABET;
	m_bCapsLock = FALSE;
	m_bShift = FALSE;
	m_strText.erase(0, std::wstring::npos);
	m_nCursor = 0;
	m_nRows = 4;
	if (m_fKeyHeight < 10.0f)
		m_fKeyHeight = 42.0f;
	int keysHeight = (int)(m_fKeyHeight * 4.0f + 3.0f);
	unsigned keyWidth = (unsigned)(LONGLONG)m_fKeyWidth;
	if (keyWidth < 30)
		keyWidth = 30;
	m_layouts.clear();
	unsigned capsWidth = (unsigned)(LONGLONG)m_pFont->GetTextWidth(L"CAPS LOCK");
	if (capsWidth < 110)
		capsWidth = 110;
	int rowWidth = (int)(keyWidth * 13 + 19 + capsWidth);

	// Without a position: centered, above the bottom of the area.
	if (m_fY == 0.0f)
	{
		m_fY = m_fHeight - ((float)(keysHeight + 30) + m_fTextHeight + 30.0f);
		if (m_fY < 0.0f)
			m_fY = 0.0f;
	}
	m_fHeight = (float)(keysHeight + 30) + m_fTextHeight;
	if (m_fTextWidth > m_fWidth)
		m_fTextWidth = m_fWidth - 20.0f;
	if (m_fX == 0.0f)
	{
		m_fX = (m_fWidth - (float)(rowWidth + 20)) * 0.5f;
		if (m_fX < 0.0f)
			m_fX = 0.0f;
	}
	// Wider than the keys when the text field needs it (at most the area
	// less 80), the keys centered.
	float width = (float)(rowWidth + 20);
	m_fKeysX = 10.0f;
	if (m_fTextWidth + 20.0f > width)
	{
		width = m_fTextWidth + 20.0f;
		if (width > m_fWidth - 80.0f)
		{
			width = m_fWidth - 80.0f;
			m_fTextWidth = width - 20.0f;
		}
		m_fKeysX = (width - (float)rowWidth) * 0.5f;
		m_fX = (m_fWidth - (m_fKeysX * 2.0f + (float)rowWidth)) * 0.5f;
		if (m_fX < 0.0f)
			m_fX = 0.0f;
	}
	m_fWidth = width;

	const SKeyDef(*layouts[3])[14] = {s_alphabet, s_symbols, s_accents};
	for (int layout = 0; layout < 3; layout++)
	{
		KeyboardLayoutRows rows;
		for (int row = 0; row < 4; row++)
		{
			KeyboardRow keys;
			for (int col = 0; col < 14 && layouts[layout][row][col].width != 0; col++)
			{
				const SKeyDef &def = layouts[layout][row][col];
				int width2 = def.width == -1 ? (int)keyWidth : def.width == -2 ? (int)capsWidth : def.width;
				SKeyboardKey key;
				MakeKey(key, def.code, width2);
				keys.push_back(key);
			}
			rows.push_back(keys);
		}
		m_layouts.push_back(rows);
	}
}

// 0x000DC9D7
void CVirtualKeyboard::Open()
{
	// The screen's scale (at least 1:1) for the font and the keys.
	m_fScaleX = 1.0f;
	m_fScaleY = 1.0f;
	ScaleToScreen(NULL, &m_fScaleX, NULL, &m_fScaleY, TRUE);
	if (m_fScaleX < 1.0f)
		m_fScaleX = 1.0f;
	if (m_fScaleY < 1.0f)
		m_fScaleY = 1.0f;
	m_pFont->SetScaleFactors(m_fScaleX, m_fScaleY);
	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	m_fTextHeight = lineHeight + 10.0f;
	m_fKeyHeight = lineHeight + (float)(m_bEmbedded ? 6 : 12) * m_fScaleY;
	m_fKeyWidth = m_pFont->GetTextWidth(L"W") + m_fScaleX * 6.0f;
	m_bDone = FALSE;
	m_bCancelled = FALSE;
	m_nRow = 0;
	if (m_bNumeric)
	{
		m_nCol = 11;   // on the '1'
		m_nLayout = KEYBOARD_ALPHABET;
	}
	else
	{
		m_nCol = 1;
	}
	m_nLastCol = 1;
	m_bCapsLock = FALSE;
	m_bShift = FALSE;
	m_strText.erase(0, std::wstring::npos);
	m_nCursor = 0;
	if (m_strTitle.size() != 0)
		m_fTitleHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight * m_fScaleY * 1.1f + 10.0f;
	else
		m_fTitleHeight = 0.0f;

	if (m_fY == 0.0f)
		return;
	// Placed from the first-time layout: at the bottom (embedded: under the
	// text), centered.
	static float s_fFirstY;          // 0x0027A358
	static float s_fFirstX;          // 0x0027A354
	static float s_fFirstWidth;      // 0x0027A350
	static float s_fFirstHeight;     // 0x0027A34C
	static float s_fFirstKeysX;      // 0x0027A348
	static unsigned s_nSaved;        // 0x0027A35C (bits)
	if (!(s_nSaved & 1))
	{
		s_nSaved |= 1;
		s_fFirstY = m_fY;
	}
	if (!(s_nSaved & 2))
	{
		s_fFirstX = m_fX;
		s_nSaved |= 2;
	}
	if (!(s_nSaved & 4))
	{
		s_fFirstWidth = m_fWidth;
		s_nSaved |= 4;
	}
	if (!(s_nSaved & 8))
	{
		s_fFirstHeight = m_fHeight;
		s_nSaved |= 8;
	}
	if (!(s_nSaved & 0x10))
	{
		s_fFirstKeysX = m_fKeysX;
		s_nSaved |= 0x10;
	}
	// (Only the first x and width are used again.)
	(void)s_fFirstY;
	(void)s_fFirstHeight;
	(void)s_fFirstKeysX;
	m_fX = s_fFirstX;
	m_fWidth = s_fFirstWidth;
	float keysHeight = (float)(m_nRows - 1) + (float)m_nRows * m_fKeyHeight;
	m_fHeight = (float)((int)(LONGLONG)keysHeight + 30) + m_fTextHeight;
	if (!m_bEmbedded)
		m_fY = 480.0f;
	ScaleToScreen(&m_fX, NULL, &m_fY, NULL, FALSE);
	if (!m_bEmbedded)
	{
		m_fY -= m_fHeight + 30.0f;
	}
	else
	{
		float screenHeight = 480.0f;
		GetScreenSize(NULL, &screenHeight);
		m_fY = (screenHeight - 5.0f) - keysHeight - (m_fTextHeight + 20.0f);
		m_fY += (float)(int)m_pSettings->m_viewport.Y;
	}
	m_fWidth *= m_fScaleX;
	GetScreenSize(&m_fX, NULL);
	if (m_fWidth > m_fX)
		m_fX = m_fWidth;
	m_fX = (m_fX - m_fWidth) * 0.5f + (float)(int)m_pSettings->m_viewport.X;
}

// 0x000DC0DF: the box (with the title above it), in the dialog colors.
void CVirtualKeyboard::DrawFrame()
{
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetRenderState(D3DRS_ZENABLE, TRUE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetVertexShader(D3DFVF_XYZRHW);
	float top = m_fY - m_fTitleHeight;
	float left = m_fX - 0.5f;
	float right = m_fWidth + m_fX - 0.5f;
	float bottom = m_fHeight + top + m_fTitleHeight - 0.5f;
	top -= 0.5f;
	float vertices[4][4] = {
		{left, top, 0.0f, 1.0f}, {right, top, 0.0f, 1.0f}, {right, bottom, 0.0f, 1.0f}, {left, bottom, 0.0f, 1.0f}};
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.DIALOG_BACKCOLOR);
	device->DrawVerticesUP(D3DPT_QUADLIST, 4, vertices, 0x10);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.DIALOG_BORDERCOLOR);
	device->DrawVerticesUP(D3DPT_LINELOOP, 4, vertices, 0x10);
}

// 0x000DC27D: the text field.
void CVirtualKeyboard::DrawTextBox()
{
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetVertexShader(D3DFVF_XYZRHW);
	float x = m_fX + 10.0f;
	float y = m_fY + 10.0f;
	float left = x - 0.5f;
	float top = y - 0.5f;
	float right = m_fTextWidth * m_fScaleX + x - 0.5f;
	float bottom = m_fTextHeight + y - 0.5f;
	float vertices[4][4] = {
		{left, top, 0.0f, 1.0f}, {right, top, 0.0f, 1.0f}, {right, bottom, 0.0f, 1.0f}, {left, bottom, 0.0f, 1.0f}};
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.KB_KEY_TEXTBACKCOLOR);
	device->DrawVerticesUP(D3DPT_QUADLIST, 4, vertices, 0x10);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.KB_KEY_TEXTBORDERCOLOR);
	device->DrawVerticesUP(D3DPT_LINELOOP, 4, vertices, 0x10);
}

// 0x000DC3CB
// The key from the key texture tinted by color (0 = white; the disabled
// marker 0xFF0000FF skips it), a border when highlighted, and the label
// (words at 3/4 size). Quirk: the texture's end caps get no width, so its
// middle column is stretched over the key.
void CVirtualKeyboard::DrawKey(float x, float y, const SKeyboardKey &key, DWORD color, DWORD textColor)
{
	if (color == 0xFF0000FF || key.code == 0)
		return;
	wchar_t character[2];
	character[0] = ApplyCase(key.code);
	character[1] = L'\0';
	const wchar_t *text = key.label.size() == 0 ? character : key.label.c_str();
	float left = x + 1.0f;
	float top = y + 1.0f;
	float bottom = (m_fKeyHeight + y) - 1.0f + 2.0f;
	float right = ((float)key.width * m_fScaleX + x) - 1.0f + 2.0f;
	float l = left - 0.5f;
	float t = top - 0.5f;
	float r = right - 0.5f;
	float b = bottom - 0.5f;
	// x, y, z, rhw, u, v
	float vertices[12][6] = {
		{l, t, 1.0f, 1.0f, 0.0f, 0.0f}, {l, t, 1.0f, 1.0f, 0.5f, 0.0f},
		{l, b, 1.0f, 1.0f, 0.5f, 1.0f}, {l, b, 1.0f, 1.0f, 0.0f, 1.0f},
		{l, t, 1.0f, 1.0f, 0.5f, 0.0f}, {r, t, 1.0f, 1.0f, 0.5f, 0.0f},
		{r, b, 1.0f, 1.0f, 0.5f, 1.0f}, {l, b, 1.0f, 1.0f, 0.5f, 1.0f},
		{r, t, 1.0f, 1.0f, 0.5f, 0.0f}, {r, t, 1.0f, 1.0f, 1.0f, 0.0f},
		{r, b, 1.0f, 1.0f, 1.0f, 1.0f}, {r, b, 1.0f, 1.0f, 0.5f, 1.0f},
	};
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	device->SetTexture(0, m_pKeyTexture);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, color != 0 ? color : 0xFFFFFFFF);
	device->DrawVerticesUP(D3DPT_QUADLIST, 12, vertices, 0x18);
	if (g_colorScheme.KB_KEYBG_HIGHLIGHT & color)
	{
		textColor = 0xFFFFFFFF;
		DrawBevelBox(left - 1.0f, top - 1.0f, (right + 1.0f) - left, (bottom + 1.0f) - top,
		             g_colorScheme.KB_KEY_ACTIVEBORDER, 0, 1.0f, 1);
	}
	float centerX = (right + left) * 0.5f;
	float centerY = (bottom + top) * 0.5f;
	if (key.label.size() > 1 && iswlower(key.label[1]))
	{
		float scaleX = m_pFont->m_fScaleFactorX;
		float scaleY = m_pFont->m_fScaleFactorY;
		m_pFont->SetScaleFactors(0.75f, 0.75f);
		m_pFont->DrawText(centerX, centerY, textColor, text, XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
		m_pFont->SetScaleFactors(scaleX, scaleY);
		return;
	}
	m_pFont->DrawText(centerX, centerY, textColor, text, XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
}

// 0x000DCD8B
// The title bar, the text (scrolled so the cursor is in view; stars for a
// password) and the blinking I-beam cursor.
void CVirtualKeyboard::DrawText(float x, float y)
{
	if (m_strTitle.size() != 0)
	{
		DrawRect(m_fX + 3.0f, (m_fY - m_fTitleHeight) + 3.0f, m_fWidth - 6.0f, m_fTitleHeight,
		         g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.CAPTION_BACKCOLOR, 1.0f, 1);
		m_pFont->DrawText(m_fX + 5.0f, (m_fY + 3.0f) - m_fTitleHeight * 0.5f, g_colorScheme.CAPTION_TEXTCOLOR,
		                  m_strTitle.c_str(), XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 10.0f);
	}

	wchar_t stars[66];
	memset(stars, 0, sizeof(stars));
	UINT count = (UINT)m_strText.size();
	UINT start = 0;
	if (!m_bPassword || count == 0)
	{
		if (m_nCursor == 0)
		{
			while (m_pFont->GetTextWidth(m_strText.substr(0, count).c_str()) > m_fTextWidth)
				count--;
		}
		else
		{
			UINT length = m_nCursor;
			while (m_pFont->GetTextWidth(m_strText.substr(start, length).c_str()) > m_fTextWidth)
			{
				start++;
				length--;
			}
			length = count;
			while (m_pFont->GetTextWidth(m_strText.substr(start, length).c_str()) > m_fTextWidth - 2.0f)
			{
				count--;
				length--;
			}
		}
		m_pFont->DrawText(x, y, g_colorScheme.KB_KEY_TEXTCOLOR, m_strText.substr(start, count).c_str(),
		                  XBFONT_CENTER_Y, 0.0f);
	}
	else
	{
		count = count > 64 ? 64 : count;
		wcsncpy(stars, m_strText.substr(0, count).c_str(), count);
		for (UINT i = 0; i < count; i++)
			stars[i] = L'*';
		m_pFont->DrawText(x, y, g_colorScheme.KB_KEY_TEXTCOLOR, stars, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
		                  m_fTextWidth);
	}

	// The cursor shows for 3/4 of every second.
	if (fmodf(m_cursorTimer.GetElapsedSeconds(), 1.0f) >= 0.75f)
		return;
	float offset = 0.0f;
	if (m_nCursor != 0)
	{
		// Quirk: measured on the visible text, not from where it starts.
		std::wstring prefix;
		if (m_bPassword)
			prefix.assign(stars, m_nCursor < 64 ? m_nCursor : 64);
		else
			prefix = m_strText.substr(start, count).substr(0, m_nCursor);
		m_pFont->GetTextExtent(prefix.c_str(), &offset, NULL, FALSE);
		x += offset;
	}
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.KB_KEY_TEXTCOLOR);
	y -= m_fKeyHeight * 0.5f;
	float half = m_fScaleX * 2.0f;
	float top = y + 2.0f;
	float bottom = y + m_fKeyHeight;
	device->Begin(D3DPT_LINELIST);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - half, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x + half, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - 1.0f, top, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - 1.0f, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x - half, bottom, 1.0f, 1.0f);
	device->SetVertexData4f(D3DVSDE_VERTEX, x + half, bottom, 1.0f, 1.0f);
	device->End();
}

// Keys a numbers-only keyboard greys out: everything but the digits, '.',
// the cursor keys and the KEY_* codes.
static BOOL IsNonNumericKey(int code)
{
	if (code == KEY_LEFT || code == KEY_RIGHT)
		return FALSE;
	return (code >= 0x20 && code < 0x2E) || (code > 0x2E && code < 0x30) || (code > 0x39 && code < 0x10000);
}

// 0x000DD35E
void CVirtualKeyboard::RenderKeys()
{
	m_pFont->SetScaleFactors(m_fScaleX, m_fScaleY);
	if (!m_bEmbedded)
	{
		DimScreen(0.3f);
		DrawFrame();
		DrawTextBox();
		DrawText(m_fX + 12.0f, m_fTextHeight * 0.5f + m_fY + 10.0f);
	}
	KeyboardLayoutRows &rows = m_layouts[m_nLayout];
	float y = (float)((double)(m_fTextHeight + m_fY) + 15.0);
	for (UINT row = 0; row < m_nRows; row++)
	{
		float left = m_fKeysX + m_fX;
		float offset = 0.0f;
		KeyboardRow &keys = rows[row];
		for (UINT col = 0; col < keys.size(); col++)
		{
			const SKeyboardKey &key = keys[col];
			DWORD color = 0;
			DWORD textColor = g_colorScheme.KB_KEY_NORMAL;
			BOOL disabled = FALSE;
			switch (key.code)
			{
			case KEY_SHIFT:
			case KEY_CAPSLOCK:
				// Pressed when on; disabled on the symbols.
				if (m_nLayout == KEYBOARD_SYMBOLS)
					disabled = TRUE;
				else if ((m_nLayout == KEYBOARD_ALPHABET || m_nLayout == KEYBOARD_ACCENTS) &&
				         (key.code == KEY_SHIFT ? m_bShift : m_bCapsLock))
					color = g_colorScheme.KB_KEY_PRESSED;
				if (m_bNumeric)
					disabled = TRUE;
				break;
			case KEY_SYMBOLS:
			case KEY_ACCENTS:
				if (m_bNumeric)
					disabled = TRUE;
				break;
			case KEY_DONE:
				if (m_bEmbedded)
					disabled = TRUE;
				break;
			default:
				if (m_bNumeric && IsNonNumericKey(key.code))
					disabled = TRUE;
				break;
			}
			if (disabled)
			{
				color = g_colorScheme.KB_KEYBG_DISABLED;
				textColor = g_colorScheme.KB_KEY_DISABLED;
			}
			if (row == m_nRow && col == m_nCol)
				color |= g_colorScheme.KB_KEYBG_HIGHLIGHT;
			if (key.code != 0)
			{
				DrawKey(offset + left, y, key, color, textColor);
				// Wider gaps after the first key and before the digit block.
				float gap = (col == 0 || (row < 2 && col == 10) || (row == 2 && col == 8) || (row == 3 && col == 2))
				                ? m_fScaleX * 4.0f
				                : m_fScaleX;
				offset = gap + (float)key.width * m_fScaleX + offset;
			}
		}
		y += 1.0f + m_fKeyHeight;
	}
}

// 0x000DDA5A
HRESULT CVirtualKeyboard::Render()
{
	if (!IsDone() && !m_bHidden)
		RenderKeys();
	return S_OK;
}

// 0x000DD605
// A character goes into the text at the cursor (while it fits) or to the
// event sink; the rest edit, move the cursor or switch layouts. Most click.
void CVirtualKeyboard::PressKey(int code)
{
	if (code < 0x10000 && code != KEY_LEFT && code != KEY_RIGHT)
	{
		if (m_pEvents != NULL)
		{
			m_pEvents->OnChar(ApplyCase(code));
		}
		else if (m_strText.size() < m_nMaxLength)
		{
			// Only while the text (as shown) is narrower than the field.
			wchar_t stars[67];
			memset(stars, 0, sizeof(stars));
			const wchar_t *shown;
			if (!m_bPassword || m_strText.size() == 0)
			{
				shown = m_strText.c_str();
			}
			else
			{
				wcsncpy(stars, m_strText.c_str(), m_strText.size());
				for (UINT i = 0; i < m_strText.size(); i++)
					stars[i] = L'*';
				shown = stars;
			}
			float width = 0.0f;
			m_pFont->GetTextExtent(shown, &width, NULL, FALSE);
			if (width < m_fTextWidth)
			{
				m_strText.insert(m_nCursor, 1, ApplyCase(code));
				m_nCursor++;
				PlayClick();
			}
		}
		m_bShift = FALSE;
		return;
	}
	switch (code)
	{
	case KEY_CAPSLOCK:
		m_bCapsLock = !m_bCapsLock;
		break;
	case KEY_LEFT:
		if (m_pEvents != NULL)
		{
			m_pEvents->OnKey(0x25);
			return;
		}
		if (m_nCursor == 0)
			return;
		m_nCursor--;
		break;
	case KEY_RIGHT:
		if (m_pEvents != NULL)
		{
			m_pEvents->OnKey(0x27);
			return;
		}
		if (m_nCursor >= m_strText.size())
			return;
		m_nCursor++;
		break;
	case KEY_BACKSPACE:
		if (m_pEvents != NULL)
		{
			m_pEvents->OnBackspace();
			return;
		}
		if (m_nCursor == 0)
			return;
		m_nCursor--;
		m_strText.erase(m_nCursor, 1);
		break;
	case KEY_DELETE:
		if (m_pEvents != NULL)
		{
			m_pEvents->OnDelete();
			return;
		}
		if (m_strText.size() == 0 || m_nCursor >= m_strText.size())
			return;
		m_strText.erase(m_nCursor, 1);
		break;
	case KEY_SHIFT:
		m_bShift = !m_bShift;
		break;
	case KEY_ALPHABET:
		m_nLayout = KEYBOARD_ALPHABET;
		break;
	case KEY_SYMBOLS:
		m_nLayout = KEYBOARD_SYMBOLS;
		break;
	case KEY_ACCENTS:
		m_nLayout = KEYBOARD_ACCENTS;
		break;
	case KEY_DONE:
		m_bDone = TRUE;
		break;
	case KEY_LAYOUT3:
		m_nLayout = 3;
		return;
	default:
		return;
	}
	PlayClick();
}

// 0x000DDB0B
// The highlighted key can't be selected: no key there, shift/caps lock on
// the symbols, Done when embedded, or a non-digit when numbers-only.
BOOL CVirtualKeyboard::IsKeyDisabled()
{
	int code = GetSelectedKey().code;
	if (m_nLayout == KEYBOARD_SYMBOLS && (code == KEY_SHIFT || code == KEY_CAPSLOCK || code == 0))
		return TRUE;
	if (m_nLanguage != 0)
		return FALSE;
	if (m_bEmbedded && code == KEY_DONE)
		return TRUE;
	if (!m_bNumeric)
		return code == 0;
	if (code == KEY_LEFT || code == KEY_RIGHT)
		return FALSE;
	return code == KEY_SHIFT || code == KEY_CAPSLOCK || code == KEY_SYMBOLS || code == KEY_ACCENTS || code == 0 ||
	       IsNonNumericKey(code);
}

// 0x000DDBDD
// Remembers the column for moving between rows of different widths.
// (Quirk: the arrow keys have labels, so their cases never apply.)
void CVirtualKeyboard::RememberColumn()
{
	const SKeyboardKey &key = GetSelectedKey();
	if (key.label.size() != 0)
		return;
	if (key.code == KEY_LEFT)
		m_nLastCol = m_nLastCol > 7 ? 8 : 7;
	else if (key.code == KEY_RIGHT)
		m_nLastCol = m_nLastCol > 9 ? 10 : 9;
	else
		m_nLastCol = m_nCol;
}

// 0x000DDC52
void CVirtualKeyboard::MoveRight()
{
	do
	{
		if (m_nCol == m_layouts[m_nLayout][m_nRow].size() - 1)
			m_nCol = 0;
		else
			m_nCol++;
	} while (IsKeyDisabled());
	RememberColumn();
}

void CVirtualKeyboard::MoveLeft()
{
	do
	{
		UINT col = m_nCol;
		if (col == 0)
			col = (UINT)m_layouts[m_nLayout][m_nRow].size();
		m_nCol = col - 1;
	} while (IsKeyDisabled());
	RememberColumn();
}

// 0x000DDCC9
// Down a row (wrapping), keeping to the key under the last column where
// the rows' keys differ in width.
void CVirtualKeyboard::MoveDown()
{
	do
	{
		UINT row = m_nRow;
		UINT col = m_nCol;
		if (row == 1)
		{
			if (col == 7 || col == 8)
			{
				m_nLastCol = col;
				m_nCol = 7;
			}
			else if (col == 9 || col == 10)
			{
				m_nLastCol = col;
				m_nCol = 8;
			}
			else if (col > 10)
			{
				m_nLastCol = col;
				m_nCol = col - 2;
			}
		}
		else if (row == 2)
		{
			if (col == 0 || col > 6)
			{
				if (col == 7 || col == 8)
				{
					m_nCol = 2;
				}
				else if (col == 9 || col == 10)
				{
					m_nLastCol = col;
					m_nCol = 3;
				}
				else if (col == 11)
				{
					m_nCol = 4;
				}
			}
			else
			{
				m_nLastCol = col;
				m_nCol = 1;
			}
		}
		else if (row == 3)
		{
			if (col == 1)
			{
				m_nCol = m_nLastCol > 6 ? 6 : m_nLastCol;
			}
			else if (col == 2)
			{
				m_nCol = m_nLastCol > 10 ? 10 : m_nLastCol;
				if (m_nCol < 7)
					m_nCol = 7;
			}
			else if (col == 3)
			{
				UINT last = m_nLastCol > 12 ? 12 : m_nLastCol;
				m_nCol = last;
				if (last == 10)
					m_nCol = 12;
				else if (last < 11)
					m_nCol = 11;
			}
			else if (col == 4)
			{
				m_nCol = 13;
			}
		}
		m_nRow = row == m_nRows - 1 ? 0 : row + 1;
	} while (IsKeyDisabled());
}

// 0x000DDDF7
// Up a row (wrapping); see MoveDown.
void CVirtualKeyboard::MoveUp()
{
	do
	{
		UINT row = m_nRow;
		UINT col = m_nCol;
		if (row == 0)
		{
			if (col >= 2 && col <= 6)
			{
				m_nCol = 1;
				m_nLastCol = col;
			}
			else if (col >= 7 && col <= 10)
			{
				m_nCol = 2;
				m_nLastCol = col;
			}
			else if (col == 11 || col == 12)
			{
				m_nCol = 3;
				m_nLastCol = col;
			}
			else if (col == 13)
			{
				m_nLastCol = 13;
				m_nCol = 4;
			}
		}
		else if (row == 2)
		{
			if (col == 7)
			{
				if (!m_bNumeric)
				{
					m_nCol = m_nLastCol > 8 ? 8 : m_nLastCol;
					if (m_nCol < 7)
						m_nCol = 7;
				}
				else
				{
					// Numbers-only: wraps on to the bottom row.
					m_nLastCol = 7;
					m_nCol = 2;
					m_nRow = 0;
				}
			}
			else if (col == 8)
			{
				if (m_bNumeric)
				{
					m_nLastCol = 9;
					m_nCol = 2;
					m_nRow = 0;
				}
				else
				{
					m_nCol = m_nLastCol > 10 ? 10 : m_nLastCol;
					if (m_nCol < 9)
						m_nCol = 9;
				}
			}
			else if (col == 9)
			{
				m_nCol = 11;
			}
			else if (col == 10)
			{
				m_nCol = 12;
			}
			else if (col == 11)
			{
				m_nCol = 13;
			}
		}
		else if (row == 3)
		{
			if (col == 1 && !m_bNumeric)
			{
				m_nCol = m_nLastCol > 6 ? 6 : m_nLastCol;
			}
			else if (col == 2)
			{
				m_nCol = m_nLastCol < 9 ? (m_nLastCol > 7 ? 7 : m_nLastCol) : 8;
				if (m_nCol < 7)
					m_nCol = 7;
			}
			else if (col == 3)
			{
				m_nCol = m_nLastCol <= 11 ? (m_nLastCol > 9 ? 9 : m_nLastCol) : 10;
				if (m_nCol < 9)
					m_nCol = 9;
			}
			else if (col > 3)
			{
				m_nCol = 11;
			}
		}
		m_nRow = (m_nRow == 0 ? m_nRows : m_nRow) - 1;
	} while (IsKeyDisabled());
}

// 0x000DDF85
void CVirtualKeyboard::PressSelected()
{
	PressKey(GetSelectedKey().code);
}

// 0x000DDFCD
// The gamepad: A presses the highlighted key, START is Done, BACK cancels
// (both consume the action), B deletes, X is backspace, Y shift, BLACK a
// space, WHITE the next layout, the triggers (or the left stick sideways)
// move the cursor, a stick click caps lock, the d-pad moves the highlight,
// the remote's digits type digits. The debug keyboard's arrows move the
// cursor too.
void CVirtualKeyboard::HandleAction(int *action)
{
	if (m_bHidden)
		return;
	if (m_pKeystroke->virtualKey == 0x25)   // VK_LEFT
	{
		PressKey(KEY_LEFT);
		return;
	}
	if (m_pKeystroke->virtualKey == 0x27)   // VK_RIGHT
	{
		PressKey(KEY_RIGHT);
		return;
	}
	int code;
	switch (*action)
	{
	case INPUT_A:
		PressSelected();
		*action = 0x30;
		return;
	case INPUT_START:
		if (!m_bEmbedded)
			m_bDone = TRUE;
		*action = 0x30;
		return;
	case INPUT_B:
		code = KEY_DELETE;
		break;
	case INPUT_BACK:
		m_bCancelled = TRUE;
		*action = 0x30;
		return;
	case INPUT_X:
		code = KEY_BACKSPACE;
		break;
	case INPUT_Y:
		if (m_bNumeric)
			return;
		code = KEY_SHIFT;
		break;
	case INPUT_WHITE:
		if (m_nLanguage == 0 && !m_bNumeric)
		{
			code = m_nLayout == KEYBOARD_SYMBOLS ? KEY_ALPHABET : KEY_SYMBOLS;
		}
		else if (m_nLayout == KEYBOARD_ALPHABET)
		{
			if (m_bNumeric)
				return;
			code = KEY_SYMBOLS;
		}
		else if (m_nLayout == KEYBOARD_SYMBOLS)
		{
			code = KEY_ACCENTS;
		}
		else if (m_nLayout == KEYBOARD_ACCENTS)
		{
			code = KEY_ALPHABET;
		}
		else
		{
			return;
		}
		break;
	case INPUT_BLACK:
		if (m_bNumeric)
			return;
		code = ' ';
		break;
	case INPUT_LTRIGGER:
		code = KEY_LEFT;
		break;
	case INPUT_RTRIGGER:
		code = KEY_RIGHT;
		break;
	case INPUT_UP:
		MoveUp();
		return;
	case INPUT_DOWN:
		MoveDown();
		return;
	case INPUT_LEFT:
		MoveLeft();
		return;
	case INPUT_RIGHT:
		MoveRight();
		return;
	case INPUT_LTHUMB:
	{
		XBGAMEPAD *pad = GetKeyboardGamepad();
		if (pad == NULL)
			return;
		if (pad->fX1 < -0.35f)
			code = KEY_LEFT;
		else if (pad->fX1 > 0.35f)
			code = KEY_RIGHT;
		else
			return;
		break;
	}
	case INPUT_LTHUMB_CLICK:
	case INPUT_RTHUMB_CLICK:
		if (m_bNumeric)
			return;
		code = KEY_CAPSLOCK;
		break;
	case INPUT_IR_0:
		code = '0';
		break;
	default:
		if (*action >= INPUT_IR_1 && *action <= INPUT_IR_1 + 8)
		{
			code = '1' + (*action - INPUT_IR_1);
			break;
		}
		return;
	}
	PressKey(code);
}

// 0x000DE1EA
// The debug keyboard (and the remote's character codes): typing finds and
// highlights the key, then presses it. Backspace/Delete, Enter, the arrows
// and caps lock act on their keys. With an event sink, characters and those
// keys go to it.
int CVirtualKeyboard::HandleKeystroke()
{
	const char *keystroke = (const char *)m_pKeystroke;
	if (keystroke == NULL)
		return 0;
	char virtualKey = keystroke[0];
	char ch = keystroke[1];
	if (virtualKey != 0)
	{
		switch ((unsigned char)virtualKey)
		{
		case 0x60: ch = '0'; break;   // the number pad
		case 0x61: ch = '1'; break;
		case 0x62: ch = '2'; break;
		case 0x63: ch = '3'; break;
		case 0x64: ch = '4'; break;
		case 0x65: ch = '5'; break;
		case 0x66: ch = '6'; break;
		case 0x67: ch = '7'; break;
		case 0x68: ch = '8'; break;
		case 0x69: ch = '9'; break;
		case 0x6A: ch = '*'; break;
		case 0x6B: ch = '+'; break;
		case 0x6D: ch = '-'; break;
		case 0x6E: ch = '.'; break;
		case 0x6F: ch = '/'; break;
		case 0x08:   // VK_BACK
		case 0x2E:   // VK_DELETE
			if (m_pEvents != NULL)
			{
				m_pEvents->OnKey(virtualKey);
				break;
			}
			m_nCol = 2;
			m_nRow = 3;
			PressKey(KEY_BACKSPACE);
			break;
		case 0x0D:   // VK_RETURN
			if (m_pEvents != NULL)
			{
				m_pEvents->OnKey(virtualKey);
				break;
			}
			m_nCol = 0;
			m_nRow = 3;
			PressKey(KEY_DONE);
			break;
		case 0x14:   // VK_CAPITAL
			// Quirk: compares the layout to a key code, so always true.
			if (!m_bNumeric && m_nLayout != KEY_SYMBOLS)
			{
				m_nCol = 0;
				m_bCapsLock = keystroke[2] & 8;
				m_nRow = 1;
			}
			break;
		case 0x25:   // VK_LEFT
			if (m_pEvents != NULL)
			{
				m_pEvents->OnKey(virtualKey);
				break;
			}
			m_nCol = 7;
			m_nRow = 2;
			PressKey(KEY_LEFT);
			break;
		case 0x27:   // VK_RIGHT
			if (m_pEvents != NULL)
			{
				m_pEvents->OnKey(virtualKey);
				break;
			}
			m_nCol = 8;
			m_nRow = 2;
			PressKey(KEY_RIGHT);
			break;
		}
	}
	// No character typed: the remote's key code, if it's one.
	if (ch == 0)
	{
		if (m_pIR == NULL)
			return 0;
		WORD keyCode = m_pIR->wKeyCode;
		if (keyCode < 0x20 || keyCode > 0xA5)
			return 0;
		ch = (char)keyCode;
		if (ch == 0)
			return 0;
	}
	if (m_pEvents != NULL)
	{
		m_pEvents->OnChar((short)ch);
		return 0xF;
	}

	if (!m_bNumeric)
	{
		if (ch <= 0x1F)
			return 0;
		// Highlight the key (the character's upper case) if it's on this
		// layout.
		BOOL found = FALSE;
		KeyboardLayoutRows &rows = m_layouts[m_nLayout];
		for (UINT row = 0; row < m_nRows && !found; row++)
		{
			KeyboardRow &keys = rows[row];
			for (UINT col = 0; col < keys.size(); col++)
			{
				if ((unsigned)keys[col].code == (unsigned)ToUpper((wchar_t)ch))
				{
					m_nCol = col;
					m_nRow = row;
					found = TRUE;
					break;
				}
			}
		}
		m_bShift = keystroke[2] & 2;
		if (found && IsKeyDisabled())
			return 0;
		PressKey(ch);
		return 0;
	}

	// Numbers-only: digits and '.'.
	if (ch == '.' || ((unsigned char)keystroke[0] > 0x5F && (unsigned char)keystroke[0] < 0x6A) ||
	    (ch > '/' && ch < ':'))
	{
		KeyboardLayoutRows &rows = m_layouts[m_nLayout];
		for (UINT row = 0; row < m_nRows; row++)
		{
			KeyboardRow &keys = rows[row];
			for (UINT col = 0; col < keys.size(); col++)
			{
				if (keys[col].code == ch)
				{
					m_nRow = row;
					m_nCol = col;
					if (IsKeyDisabled())
						return 0;
					// Quirk: the typed character, not the key's (0 for the
					// number pad).
					PressKey(keystroke[1]);
					return 0;
				}
			}
		}
	}
	return 0;
}

// 0x000DE58D
// Action 0 (and 0x2E): the debug keyboard; anything else: the gamepad.
HRESULT CVirtualKeyboard::ProcessInput(int *action)
{
	if (*action == INPUT_NONE || *action == 0x2E)
		HandleKeystroke();
	else
		HandleAction(action);
	return S_OK;
}

// 0x00014242
std::string CVirtualKeyboard::GetResult()
{
	if (m_bCancelled || m_strText.size() == 0)
		return std::string();
	return StrFromWide(m_strText.c_str());
}

// 0x0001433E
void CVirtualKeyboard::SetTitle(const wchar_t *title)
{
	m_strTitle = title;
	if (m_strTitle.size() == 0)
		m_fTitleHeight = 0.0f;
}

// 0x00014302
void CVirtualKeyboard::SetText(const wchar_t *text)
{
	m_strText.erase(0, std::wstring::npos);
	m_strText = text;
	m_nCursor = (UINT)m_strText.size();
}

// 0x0001421F
const wchar_t *CVirtualKeyboard::GetText()
{
	if (m_bCancelled || m_strText.size() == 0)
		return L"";
	return m_strText.c_str();
}
