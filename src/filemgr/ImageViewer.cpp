// CImageViewer: the file manager's picture viewer. See Viewers.h.
//
// One picture (bmp jpg png, or an .xbx), or every texture in an .xpr, in a
// window or over the whole screen. Left/right step through an .xpr's
// textures, a thumbstick click changes the zoom, START starts a slideshow
// of a picture's folder (CSlideShow), B/BACK close.
#include <cstring>
#include <cwchar>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Viewers.h"

int g_nImageCount = 0;       // 0x002761E4
int g_nLastImageIndex = 0;   // 0x0026C68C

// 0x00118E34 (this in esi)
CImageViewer::CImageViewer()
{
	m_bOpen = FALSE;
	m_bMessageShown = FALSE;
	m_pMessageBox = NULL;
	m_pFont = NULL;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_bInitialized = FALSE;
	m_unknown29B4 = 0;
	m_nCurrent = 0;
	m_bLoading = FALSE;
	m_pUnknown004 = NULL;
	m_pResourceTags = NULL;
	m_nZoom = ZOOM_WINDOW;
	memset(m_pTextures, 0, sizeof(m_pTextures));
	m_pSlideShow = NULL;
	m_bSlideShowAllowed = FALSE;
	g_fonts.Find("XboxDings", &m_pDingsFont);
}

// 0x00118DC1 (this in edi). Quirk: a slideshow still open is not freed.
CImageViewer::~CImageViewer()
{
	if (m_pUnknown004 != NULL)
		ReleaseTexture(m_pUnknown004);
	for (int i = 0; i < IMAGEVIEWER_MAX_IMAGES; i++)
	{
		if (m_pTextures[i] != NULL)
			ReleaseTexture(m_pTextures[i]);
	}
	m_xpr.Destroy();
	delete m_pMessageBox;
	m_pMessageBox = NULL;
}

// 0x001185D2. (The window is not scaled to the screen area.)
BOOL CImageViewer::Init(CXBFontEx *font)
{
	if (m_bInitialized)
		return S_OK;
	if (font == NULL)
		return E_FAIL;
	m_fY = 40.0f;
	m_fHeight = 400.0f;
	m_fWidth = 420.0f;
	m_pFont = font;
	m_fX = 110.0f;
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x00119217 (this in edi)
void CImageViewer::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
		else
		{
			m_pMessageBox->SetButtonText(MSGBOX_OK, GetString(STR_OK));
			m_pMessageBox->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
			m_pMessageBox->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
			m_pMessageBox->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
			m_pMessageBox->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
			m_pMessageBox->SetButtonText(MSGBOX_YES, GetString(STR_YES));
			m_pMessageBox->SetButtonText(MSGBOX_NO, GetString(STR_NO));
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// An .xbx is an .xpr holding one texture; anything that doesn't load as
// one is tried as a picture file. Quirk: a picture loaded that way isn't
// counted, so the window says "(No Image)". (this in eax)
HRESULT CImageViewer::LoadXbx(const char *path)   // 0x0011863F
{
	memset(&m_imageInfo, 0, sizeof(m_imageInfo));
	if (FAILED(m_xpr.Create(path, 1, NULL)))
	{
		if (!LoadTextureFromFile(path, g_pd3dDevice, &m_pTextures[0], 1, 1, &m_imageInfo))
			return E_FAIL;
		return S_OK;
	}
	m_pTextures[0] = (IDirect3DTexture8 *)m_xpr.GetResource(0);
	if (m_pTextures[0] == NULL)
		return E_FAIL;
	m_nCurrent = 0;
	m_dwTypes[0] = D3DRTYPE_TEXTURE;
	g_nImageCount = 1;
	MultiByteToWideChar(CP_ACP, 0, m_strPath.c_str(), (int)m_strPath.size(), m_szNames[0], 0x104);
	return S_OK;
}

// (this in eax)
HRESULT CImageViewer::LoadImage(const char *path)   // 0x00118EE8
{
	if (m_pUnknown004 != NULL)
		ReleaseTexture(m_pUnknown004);
	for (int i = 0; i < g_nImageCount; i++)
	{
		if (m_pTextures[i] != NULL)
			ReleaseTexture(m_pTextures[i]);
	}
	m_xpr.Destroy();
	g_nImageCount = 0;
	m_pResourceTags = NULL;
	memset(m_pTextures, 0, sizeof(m_pTextures));
	memset(m_szNames, 0, sizeof(m_szNames));
	memset(&m_imageInfo, 0, sizeof(m_imageInfo));

	std::string extension = StrToLower(path);
	int dot = StrReverseFind(extension, ".");
	if (dot != -1)
	{
		extension = StrMid(extension, dot + 1, (int)extension.size() - (dot + 1));
		if (StrCmpNoCase(extension.c_str(), "xbx") == 0)
			return LoadXbx(path);
		if (StrCmpNoCase(extension.c_str(), "jpg") == 0 || StrCmpNoCase(extension.c_str(), "png") == 0 ||
		    StrCmpNoCase(extension.c_str(), "bmp") == 0)
		{
			if (!LoadTextureFromFile(path, g_pd3dDevice, &m_pTextures[0], 1, 1, &m_imageInfo))
				return E_FAIL;
			m_nCurrent = 0;
			m_dwTypes[0] = D3DRTYPE_TEXTURE;
			g_nImageCount = 1;
			MultiByteToWideChar(CP_ACP, 0, m_strPath.c_str(), (int)m_strPath.size(), m_szNames[0], 0x104);
			return S_OK;
		}
	}

	// Anything else is an .xpr: its named resources (the textures among
	// them), or without names the first resource. Quirk: there's no limit
	// on the count, but room for 20; and each texture gets an extra
	// reference from the type check.
	if (FAILED(m_xpr.Create(path, 0, NULL)))
		return E_FAIL;
	DWORD count;
	m_xpr.GetResourceTags(&count, &m_pResourceTags);
	if (count == 0)
	{
		m_pTextures[0] = (IDirect3DTexture8 *)m_xpr.GetResource(0);
		if (m_pTextures[0] == NULL)
			return E_FAIL;
		m_nCurrent = 0;
		m_dwTypes[0] = D3DRTYPE_TEXTURE;
		g_nImageCount = 1;
		MultiByteToWideChar(CP_ACP, 0, m_strPath.c_str(), (int)m_strPath.size(), m_szNames[0], 0x104);
		return S_OK;
	}
	for (int i = 0; i < (int)count; i++)
	{
		D3DResource *resource = m_xpr.GetResource(m_pResourceTags[i].dwOffset);
		if (D3DResource_GetType(resource) == D3DRTYPE_TEXTURE)
		{
			m_pTextures[i] = (IDirect3DTexture8 *)m_xpr.GetResource(m_pResourceTags[i].dwOffset);
			m_dwTypes[i] = D3DRTYPE_TEXTURE;
			const char *name = m_pResourceTags[i].strName;
			MultiByteToWideChar(CP_ACP, 0, name, (int)strlen(name), m_szNames[i], 0x104);
		}
		else
		{
			m_dwTypes[i] = D3DResource_GetType(resource);
		}
	}
	g_nImageCount = (int)count;
	return S_OK;
}

// 0x0011935E (path in eax, this on the stack)
BOOL CImageViewer::Open(const char *path)
{
	m_bSlideShowAllowed = FALSE;
	if (path == NULL || strlen(path) == 0)
	{
		m_bOpen = FALSE;
		return FALSE;
	}
	m_strPath = path;
	if (FAILED(LoadImage(m_strPath.c_str())))
	{
		std::string message;
		StrFormat(message, "The file '%s' is not a valid image file.", m_strPath.c_str());
		ShowMessage(message.c_str(), L"Error", MSGBOX_BUTTONS_OK);
		return FALSE;
	}
	std::string extension = StrRight(std::string(path), 4);
	if (StrCmpNoCase(extension.c_str(), ".jpg") == 0 || StrCmpNoCase(extension.c_str(), ".png") == 0 ||
	    StrCmpNoCase(extension.c_str(), ".bmp") == 0)
		m_bSlideShowAllowed = TRUE;
	return TRUE;
}

// 0x001194AE
HRESULT CImageViewer::ProcessInput(int *action, SGamepadState **pad)
{
	// (The message box's buttons fall through to the checks below.)
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
	}
	if (m_pSlideShow != NULL)
	{
		if (m_pSlideShow->IsOpen())
		{
			m_pSlideShow->ProcessInput(action, pad);
			return S_OK;
		}
		delete m_pSlideShow;
		m_pSlideShow = NULL;
	}
	int count = g_nImageCount;
	if (!m_bLoading)
	{
		switch (*action)
		{
		case INPUT_START:
			if (!m_bSlideShowAllowed)
				break;
			if (m_pSlideShow == NULL)
			{
				m_pSlideShow = new CSlideShow(GetPathDirectory(m_strPath.c_str()).c_str());
				if (m_pSlideShow != NULL)
				{
					m_pSlideShow->SetTransitions(TRUE);
					m_pSlideShow->SetDelay(5);
				}
			}
			else
			{
				// (Can't happen: an open slideshow took the input above.)
				m_pSlideShow->Show();
			}
			break;

		case INPUT_B:
		case INPUT_BACK:
			*action = INPUT_NONE;
			m_bOpen = FALSE;
			break;

		case INPUT_LEFT:
			if (g_nImageCount > 1)
			{
				if (m_nCurrent == 0)
					m_nCurrent = g_nImageCount - 1;
				else
					m_nCurrent--;
			}
			break;

		case INPUT_RIGHT:
			if (g_nImageCount > 1)
			{
				m_nCurrent++;
				if ((unsigned)m_nCurrent >= (unsigned)count)
					m_nCurrent = 0;
			}
			break;

		case INPUT_LTHUMB_CLICK:
		case INPUT_RTHUMB_CLICK:
			m_nZoom = (m_nZoom + 1) % 3;
			break;
		}
	}
	if (*action == INPUT_NONE && g_nLastImageIndex != m_nCurrent)
		g_nLastImageIndex = m_nCurrent;
	return S_OK;
}

// 0x001186F3
BOOL CImageViewer::Render()
{
	if (m_pSlideShow != NULL)
	{
		m_pSlideShow->Render();
		return S_OK;
	}
	DimScreen(0.3f);
	float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, 10.0f, 10.0f, lineHeight * 1.5f, g_colorScheme.DIALOG_BORDERCOLOR,
	                g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
	                g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
	if (m_bMessageShown)
	{
		m_pMessageBox->Render();
		return S_OK;
	}
	if (g_nImageCount == 0)
	{
		m_pFont->DrawText(m_fWidth * 0.5f + m_fX, m_fHeight * 0.5f + m_fY, g_colorScheme.DIALOG_TEXTCOLOR,
		                  L"(No Image)", XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
		return S_OK;
	}
	if (m_bLoading)
	{
		m_pFont->DrawText(150.0f, 200.0f, 0xFFFF0000, L"Loading resources from XPR...", 0, 0.0f);
		// Quirk: (x + width) / 2, not the window's middle.
		m_pFont->DrawText((m_fWidth + m_fX) * 0.5f, (m_fHeight + m_fY) * 0.5f, g_colorScheme.DIALOG_TEXTCOLOR,
		                  L"Loading Image...", XBFONT_CENTER_X | XBFONT_CENTER_Y, 0.0f);
		return S_OK;
	}

	wchar_t caption[260];
	wcscpy(caption, L"");
	int current = m_nCurrent;
	float captionX = m_fX + 10.0f;
	if (m_dwTypes[current] != D3DRTYPE_TEXTURE)
	{
		swprintf(caption, 260, L"%ls is not a Texture ( %d of %d)", m_szNames[current], current + 1, g_nImageCount);
	}
	else if (m_pTextures[current] == NULL)
	{
		if (m_pResourceTags == NULL)
			return S_OK;
		swprintf(caption, 260, L"%ls is not a Texture", m_szNames[current]);
	}
	else
	{
		D3DSURFACE_DESC desc;
		Get2DSurfaceDesc((D3DResource *)m_pTextures[current], 0, &desc);
		int imageWidth = (int)desc.Width;
		int imageHeight = (int)desc.Height;
		if (m_imageInfo.width != 0)
		{
			// The file's own size (the texture may be padded).
			imageWidth = m_imageInfo.width;
			imageHeight = m_imageInfo.height;
		}
		float width = (float)(unsigned)imageWidth;
		float height = (float)(unsigned)imageHeight;

		if (m_nZoom == ZOOM_ACTUAL)
		{
			float screenWidth, screenHeight, originX, originY;
			GetScreenSize(&screenWidth, &screenHeight);
			GetScreenOrigin(&originX, &originY);
			if (screenWidth < width || screenHeight < height)
			{
				float aspect = width / height;
				if (screenWidth < width)
				{
					height = screenWidth / aspect;
					width = screenWidth;
				}
				if (screenHeight < height)
				{
					width = aspect * screenHeight;
					height = screenHeight;
				}
			}
			DrawTexture(m_pTextures[current], (screenWidth - width) * 0.5f + originX,
			            (screenHeight - height) * 0.5f + originY, width, height, 1.0f, NULL, 0.0f, NULL);
			return S_OK;
		}
		if (m_nZoom != ZOOM_WINDOW)
		{
			// ZOOM_FULLSCREEN: the screen's size, placed from the window
			// so that it covers the screen.
			GetScreenSize(&width, &height);
			DrawTexture(m_pTextures[current], (m_fWidth - width) * 0.5f + m_fX, (m_fHeight - height) * 0.5f + m_fY,
			            width, height, 1.0f, NULL, 0.0f, NULL);
			return S_OK;
		}

		// ZOOM_WINDOW: scaled to the window's width (up or down), then
		// down to its height. Quirk: the height check leaves room for 3
		// lines, the shrink for 6.
		float available = m_fWidth - 30.0f;
		float aspect = width / height;
		if (available < width || width < available)
		{
			height = available / aspect;
			width = available;
		}
		if ((m_fHeight - lineHeight * 3.0f) - 20.0f < height)
		{
			height = (m_fHeight - lineHeight * 6.0f) - 20.0f;
			width = aspect * height;
		}
		DrawTexture(m_pTextures[current], (m_fWidth - width) * 0.5f + m_fX, (m_fHeight - height) * 0.5f + m_fY,
		            width, height, 1.0f, NULL, 0.0f, NULL);
		if (m_pResourceTags == NULL)
			swprintf(caption, 260, L"%ls (%d x %d)", m_szNames[current], imageWidth, imageHeight);
		else
			swprintf(caption, 260, L"%ls (%d x %d) %d of %d", m_szNames[current], imageWidth, imageHeight,
			         current + 1, g_nImageCount);
		if (m_bSlideShowAllowed && m_pDingsFont != NULL)
		{
			// XboxDings 'G': START plays a slideshow.
			const wchar_t sign[2] = {L'G', 0};
			m_pDingsFont->DrawText(captionX, (m_fHeight + m_fY) - lineHeight, g_colorScheme.DIALOG_TEXTCOLOR, sign,
			                       XBFONT_CENTER_Y, m_fWidth - 20.0f);
			captionX = m_pDingsFont->GetTextWidth(sign) + captionX;
		}
	}
	m_pFont->DrawText(captionX, (m_fHeight + m_fY) - lineHeight, g_colorScheme.DIALOG_TEXTCOLOR, caption,
	                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 20.0f);
	return S_OK;
}
