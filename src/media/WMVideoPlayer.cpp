// CWMVideoPlayer: WMV/XMV videos through the XDK samples' players. See
// WMVideoPlayer.h.
#include "WMVideoPlayer.h"

#include <cstring>
#include <cwchar>

#include "../app/Screens.h"
#include "../audio/AudioDevice.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"

int g_nOverlayUsers;   // 0x0027765C

// The action ProcessInput leaves in modal mode. TODO: verify what 48 is
#define INPUT_VIDEO_HANDLED 0x30

// The caption height, worked out once and never used. (0x0027A384)
static float s_fCaptionHeight;
static bool s_bCaptionHeightSet;

// 0x0009D28C
CWMVideoPlayer::CWMVideoPlayer()
{
	m_unknown64 = -1;
	m_pTextures[0] = NULL;
	m_pTextures[1] = NULL;
	m_pSurfaces[0] = NULL;
	m_pSurfaces[1] = NULL;
	m_bFrameReady = FALSE;
	m_bOpen = FALSE;
	m_bModal = TRUE;
	m_bFullScreen = FALSE;
	m_bMessageShown = FALSE;
	m_bPlaying = TRUE;
	m_pMessageBox = NULL;
	m_fLineHeight = 0.0f;
	m_bInitialized = FALSE;
	m_pWmv = NULL;
	m_pXmv = NULL;
	m_nFrames = 0;
	m_unknown5C = 0;
	m_pTitleFont = NULL;
	m_pFont = NULL;
	m_bAudio = true;
	m_bLoop = true;
	m_bXmv = FALSE;
	m_bCustomRect = FALSE;
	m_rcScreen.left = 0;
	m_rcScreen.top = 0;
	m_rcScreen.right = 0;
	m_rcScreen.bottom = 0;
	m_bReady = TRUE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x0009C821
CWMVideoPlayer::~CWMVideoPlayer()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	if (!m_bXmv || m_pWmv != NULL)
	{
		for (int i = 0; i < 2; i++)
		{
			if (m_pSurfaces[i] != NULL)
			{
				m_pSurfaces[i]->Release();
				m_pSurfaces[i] = NULL;
			}
			if (m_pTextures[i] != NULL)
			{
				m_pTextures[i]->Release();
				m_pTextures[i] = NULL;
			}
		}
		if (m_pWmv != NULL)
		{
			m_pWmv->CloseFile();
			delete m_pWmv;
			m_pWmv = NULL;
			g_nOverlayUsers--;
			if (m_bModal || g_nOverlayUsers == 0)
			{
				g_pd3dDevice->BlockUntilVerticalBlank();
				g_pd3dDevice->EnableOverlay(FALSE);
			}
		}
	}
	if (m_pXmv != NULL)
	{
		m_pXmv->Destroy();
		delete m_pXmv;
		m_pXmv = NULL;
		g_nOverlayUsers--;
		if (m_bModal || g_nOverlayUsers == 0)
		{
			g_pd3dDevice->BlockUntilVerticalBlank();
			g_pd3dDevice->EnableOverlay(FALSE);
		}
	}
}

// 0x0009C951
// (Quirk: without a dialog font in the settings the title font is the
// font passed, NULL when "SmallFont" was looked up.)
BOOL CWMVideoPlayer::Init(CXBFontEx *font)
{
	if (m_bInitialized)
		return S_OK;
	if (font == NULL)
		g_fonts.Find("SmallFont", &m_pFont);
	else
		m_pFont = (CXBFont *)font;
	if (m_pFont == NULL)
		return E_FAIL;
	m_fLineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
	m_pTitleFont = m_pSettings->m_pDialogFont;
	if (m_pTitleFont == NULL)
		m_pTitleFont = (CXBFont *)font;
	m_fWindowY = 20.0f;
	m_fWindowX = 30.0f;
	m_fWindowWidth = 580.0f;
	m_fWindowHeight = 440.0f;
	ScaleToScreen(&m_fWindowX, &m_fWindowWidth, &m_fWindowY, &m_fWindowHeight, TRUE);
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x0009BB70
void CWMVideoPlayer::SetRect(int top, int left, int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		m_bCustomRect = FALSE;
		return;
	}
	m_rcDest.top = top;
	m_rcDest.left = left;
	m_bCustomRect = TRUE;
	// (A size, not a corner.)
	m_rcDest.right = width;
	m_rcDest.bottom = height;
}

// 0x0009BDF1
// Full screen: the screen area. Otherwise (unless SetRect gave a size)
// the video fitted into the window, centred: as wide as the window
// allows, then lower if it doesn't fit above the two text lines.
// (Quirks: the height is checked against three lines but cut to six; and
// the result is a RECT where windowed drawing reads a size.)
void CWMVideoPlayer::SetFullScreen(BOOL fullScreen)
{
	m_bFullScreen = fullScreen;
	if (fullScreen)
	{
		const SViewport &screen = m_pSettings->m_viewport;
		m_rcScreen.left = screen.X;
		m_rcScreen.top = screen.Y;
		m_rcScreen.right = screen.Width + m_rcScreen.left;
		m_rcScreen.bottom = screen.Height + m_rcScreen.top;
		return;
	}
	if (m_bCustomRect)
		return;
	float videoWidth = (float)m_rcSource.right;
	float videoHeight = (float)m_rcSource.bottom;
	float maxWidth = m_fWindowWidth - 30.0f;
	float width = videoWidth;
	float height = videoHeight;
	if (videoWidth > maxWidth || maxWidth > videoWidth)
	{
		height = videoHeight * maxWidth / videoWidth;
		width = maxWidth;
	}
	if (m_pFont != NULL)
	{
		float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
		if (height > (m_fWindowHeight - lineHeight * 3.0f) - 20.0f)
		{
			height = (m_fWindowHeight - lineHeight * 6.0f) - 20.0f;
			width = videoWidth * height / videoHeight;
		}
	}
	m_rcDest.left = (int)((m_fWindowWidth - width) * 0.5f + m_fWindowX);
	m_rcDest.top = (int)((m_fWindowHeight - height) * 0.5f + m_fWindowY);
	m_rcDest.right = (int)width + m_rcDest.left;
	m_rcDest.bottom = (int)height + m_rcDest.top;
}

// 0x0009BF4C
BOOL CWMVideoPlayer::IsOpen()
{
	return m_bPlaying || (m_bOpen && m_bXmv && m_pXmv->m_bPlaying);
}

// 0x0009BF69
BOOL CWMVideoPlayer::HasAudio()
{
	if (m_bXmv && m_pXmv != NULL)
		return m_pXmv->m_bHasAudio;
	if (m_pWmv != NULL)
		return m_pWmv->m_bHasAudio;
	return FALSE;
}

// 0x0009BF89
void CWMVideoPlayer::SetAudio(BOOL enable)
{
	if (m_bAudio == ((BYTE)enable != 0))
		return;
	m_bAudio = (BYTE)enable != 0;
	if (m_bXmv && m_pXmv != NULL)
		m_pXmv->m_bAudio = m_bAudio;
	if (m_pWmv != NULL)
		m_pWmv->m_bAudio = m_bAudio;
}

// 0x0009C1F3
void CWMVideoPlayer::SetLoop(BOOL loop)
{
	if (m_bLoop == ((BYTE)loop != 0))
		return;
	m_bLoop = (BYTE)loop != 0;
	if (m_bXmv && m_pXmv != NULL)
		m_pXmv->SetLoop(m_bLoop);
	if (m_pWmv != NULL)
		m_pWmv->m_nLoops = m_bLoop ? -1 : 1;
}

// 0x0009CA13
void CWMVideoPlayer::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (!m_bModal)
		return;
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
		}
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageShown = TRUE;
	}
}

// 0x0009C224
void CWMVideoPlayer::Close()
{
	if (m_pWmv != NULL)
		m_pWmv->CloseFile();
	if (m_pXmv != NULL)
		m_pXmv->Destroy();
	m_strFile.clear();
	m_bFrameReady = FALSE;
	m_bOpen = FALSE;
	if (m_bModal || g_nOverlayUsers == 0)
	{
		g_pd3dDevice->BlockUntilVerticalBlank();
		g_pd3dDevice->EnableOverlay(FALSE);
	}
}

// 0x0009CAA9
// The player for the file's kind, the file, the frame textures and the
// overlay. Returns 0 when it opened.
unsigned __stdcall CWMVideoPlayer::OpenThread(void *param)
{
	CWMVideoPlayer *self = (CWMVideoPlayer *)param;
	if (self == NULL)
		return 0;
	self->m_bMessageShown = FALSE;
	std::string error;
	self->m_bOpen = FALSE;
	if (self->m_pWmv != NULL)
		self->m_pWmv->CloseFile();
	if (self->m_pXmv != NULL)
		self->m_pXmv->Destroy();
	BOOL opened = FALSE;
	if (!self->m_bXmv)
	{
		if (self->m_pWmv == NULL)
		{
			self->m_pWmv = new CWmvSamplePlayer;
			if (self->m_pWmv == NULL)
				error = "Unable to create WMVPlayer Object. Out of memory?";
			else
			{
				self->m_pWmv->Initialize(CAudioDevice::GetInstance()->GetDirectSound());
				self->m_pWmv->m_nLoops = self->m_bLoop ? -1 : 1;
				self->m_pWmv->m_bAudio = self->m_bAudio;
			}
		}
	}
	else if (self->m_pXmv == NULL)
	{
		self->m_pXmv = new CXMVPlayer;
		if (self->m_pXmv == NULL)
			error = "Unable to create XMVPlayer Object. Out of memory?";
	}
	if (error.size() != 0)
	{
		self->ShowMessage(error.c_str(), L"ERROR", 0);
		self->m_bReady = TRUE;
		return 1;
	}

	self->m_bYuv = TRUE;
	if (!self->m_bXmv)
	{
		self->m_pWmv->m_nLoops = self->m_bLoop ? -1 : 1;
		self->m_pWmv->m_bAudio = self->m_bAudio;
		if (SUCCEEDED(self->m_pWmv->OpenFile(self->m_strFile.c_str(),
		                                     self->m_bYuv ? D3DFMT_YUY2 : D3DFMT_LIN_A8R8G8B8)))
		{
			opened = TRUE;
		}
		else
		{
			StrFormat(error, "Failed to open '%s'. File may be corrupt or damaged.", self->m_strFile.c_str());
			self->ShowMessage(error.c_str(), L"ERROR", 0);
			self->m_pWmv->CloseFile();
			delete self->m_pWmv;
			self->m_pWmv = NULL;
		}
	}
	else
	{
		self->m_bYuv = TRUE;
		self->m_pXmv->SetLoop(self->m_bLoop);
		self->m_pXmv->m_bAudio = self->m_bAudio;
		if (SUCCEEDED(self->m_pXmv->OpenFile(self->m_strFile.c_str(), D3DFMT_YUY2, TRUE)))
		{
			opened = TRUE;
			if (self->m_bModal)
				self->m_pXmv->EnableOverlays();
		}
		else
		{
			StrFormat(error, "Failed to open '%s'. Unsupported file format.", self->m_strFile.c_str());
			self->ShowMessage(error.c_str(), L"ERROR", 0);
			self->m_pXmv->Destroy();
			delete self->m_pXmv;
			self->m_pXmv = NULL;
		}
	}
	if (!opened)
	{
		self->m_bReady = TRUE;
		return 1;
	}

	if (!self->m_bXmv)
	{
		// Two frame textures to alternate between.
		self->m_pWmv->GetVideoInfo(&self->m_info);
		for (int i = 0; i < 2; i++)
		{
			g_pd3dDevice->CreateTexture(self->m_info.width, self->m_info.height, 1, 0,
			                            self->m_bYuv ? D3DFMT_YUY2 : D3DFMT_A8R8G8B8, 0, &self->m_pTextures[i]);
			self->m_pTextures[i]->GetSurfaceLevel(0, &self->m_pSurfaces[i]);
		}
		self->m_pWmv->m_nLoops = self->m_bLoop ? -1 : 1;
		self->m_pWmv->m_bAudio = self->m_bAudio;
	}
	else
	{
		memset(&self->m_info, 0, sizeof(self->m_info));
		self->m_info.width = self->m_pXmv->m_dwWidth;
		self->m_info.height = self->m_pXmv->m_dwHeight;
		self->m_pXmv->SetLoop(self->m_bLoop);
		self->m_pXmv->m_bAudio = self->m_bAudio;
	}
	self->m_rcSource.right = self->m_info.width;
	self->m_rcSource.bottom = self->m_info.height;
	self->m_nCurrent = 0;
	self->m_rcSource.left = 0;
	self->m_rcSource.top = 0;
	self->SetFullScreen(self->m_bFullScreen);
	self->m_unknown64 = -1;
	g_nOverlayUsers++;
	self->m_unknown5C = 0;
	self->m_nFrames = 0;
	if (self->m_bModal || g_nOverlayUsers == 1)
		g_pd3dDevice->EnableOverlay(TRUE);
	self->m_bReady = TRUE;
	self->m_bOpen = TRUE;
	return 0;
}

// (The start of Open and Reopen, which waits for an open in progress.)
// Quirk: a name under 8 characters or without a .wmv/.xmv extension is
// refused without m_bReady being set again, so the next open waits
// forever.
BOOL CWMVideoPlayer::PrepareOpen(const char * /*fileName*/)
{
	while (!m_bReady)
		Sleep(5);
	m_bOpen = FALSE;
	m_bFrameReady = FALSE;
	if (m_pWmv != NULL)
	{
		m_pWmv->CloseFile();
		delete m_pWmv;
		m_pWmv = NULL;
	}
	if (m_pXmv != NULL)
	{
		m_pXmv->Destroy();
		delete m_pXmv;
		m_pXmv = NULL;
	}
	return TRUE;
}

// Releases the frame surfaces (and, for WMV, the textures: XMV's belong to
// its player).
static void ReleaseFrames(CWMVideoPlayer *player)
{
	for (int i = 0; i < 2; i++)
	{
		if (player->m_pSurfaces[i] != NULL)
		{
			player->m_pSurfaces[i]->Release();
			player->m_pSurfaces[i] = NULL;
		}
		if (!player->m_bXmv && player->m_pTextures[i] != NULL)
		{
			player->m_pTextures[i]->Release();
			player->m_pTextures[i] = NULL;
		}
	}
}

// The file's kind from its extension. FALSE (and the name cleared) for
// anything but .wmv and .xmv.
static BOOL CheckVideoName(std::string &name)
{
	StrTrimRight(StrTrimLeft(name));
	if (name.size() < 8 ||
	    (StrCmpNoCase(StrRight(name, 4), ".wmv") != 0 && StrCmpNoCase(StrRight(name, 4), ".xmv") != 0))
	{
		name.clear();
		return FALSE;
	}
	return TRUE;
}

// 0x0009CE46
BOOL CWMVideoPlayer::Open(const char *fileName)
{
	PrepareOpen(fileName);
	ReleaseFrames(this);
	if (fileName == NULL || strlen(fileName) == 0)
	{
		m_bFrameReady = FALSE;
		return FALSE;
	}
	if (!FileExists(fileName))
	{
		if (m_bModal)
			ShowMessage("File does not exists.", L"ERROR", 0);
		m_bFrameReady = FALSE;
		return FALSE;
	}
	m_bPlaying = TRUE;
	m_bReady = FALSE;
	m_strFile = fileName;
	if (!CheckVideoName(m_strFile))
		return FALSE;
	m_bXmv = StrCmpNoCase(StrRight(m_strFile, 4), ".xmv") == 0;
	m_nCurrent = 0;
	return OpenThread(this) == 0;
}

// 0x0009D050
void CWMVideoPlayer::Reopen(const char *fileName)
{
	PrepareOpen(fileName);
	if (fileName == NULL || strlen(fileName) == 0)
	{
		m_bFrameReady = FALSE;
		return;
	}
	if (!FileExists(fileName))
	{
		if (m_bModal)
			ShowMessage("File does not exists.", L"ERROR", 0);
		m_bFrameReady = FALSE;
		return;
	}
	m_bPlaying = TRUE;
	m_bReady = FALSE;
	m_strFile = fileName;
	if (!CheckVideoName(m_strFile))
		return;
	if (!m_bXmv)
		ReleaseFrames(this);
	m_bXmv = StrCmpNoCase(StrRight(m_strFile, 4), ".xmv") == 0;
	m_nCurrent = 0;
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, OpenThread, this, 0, NULL);
	if (thread == NULL)
	{
		OutputDebugStringA("OpenFileThread creation failed.\r\n");
	}
	else
	{
		OutputDebugStringA("OpenFileThread creation succeeded.\r\n");
		Sleep(5);
		CloseHandle(thread);
	}
}

// 0x0009C26C
HRESULT CWMVideoPlayer::ProcessInput(int *action, SGamepadState ** /*pad*/)
{
	if (!m_bModal)
	{
		// The intro movie can be skipped.
		if (!m_bLoop && (*action == INPUT_A || *action == INPUT_START))
		{
			*action = INPUT_NONE;
			Close();
			return S_OK;
		}
	}
	else
	{
		if (m_bMessageShown)
		{
			m_pMessageBox->ProcessInput(action);
			m_bMessageShown = m_pMessageBox->IsOpen();
			return S_OK;
		}
		int input = *action;
		if (input == INPUT_A || input == INPUT_B || input == INPUT_BLACK || input == INPUT_BACK)
		{
			m_bPlaying = FALSE;
			if (m_bXmv && m_pXmv != NULL)
				m_pXmv->Destroy();
			if (m_pWmv != NULL)
				m_pWmv->CloseFile();
			*action = INPUT_NONE;
			return S_OK;
		}
		if (input == INPUT_RTHUMB_CLICK || input == INPUT_IR_DISPLAY)
			SetFullScreen(!m_bFullScreen);
		*action = INPUT_VIDEO_HANDLED;
	}

	if (!m_bReady)
		return S_OK;
	if (!m_bOpen)
	{
		if (!m_bModal)
			m_bPlaying = FALSE;
		return E_FAIL;
	}
	if (!m_bPlaying)
	{
		m_bFrameReady = FALSE;
		return S_OK;
	}
	if (!m_bXmv)
	{
		if (m_pWmv != NULL)
			m_bFrameReady = m_pWmv->IsFrameDue();
	}
	else if (m_pXmv != NULL)
	{
		m_bFrameReady = m_pXmv->m_bPlaying;
		if (!m_bFrameReady)
		{
			m_bPlaying = FALSE;
			m_pXmv->Destroy();
			return S_OK;
		}
	}
	if (!m_bFrameReady)
		return S_OK;
	m_nFrames++;
	if (!m_bXmv)
	{
		if (m_pWmv->m_bFrameReady)
			m_pWmv->CopyFrame(m_pTextures[m_nCurrent]);
		else
			m_bFrameReady = FALSE;
		if (m_pWmv->DecodeNext() == 1)
			m_bPlaying = FALSE;
	}
	else
	{
		IDirect3DTexture8 *frame = m_bModal ? m_pXmv->AdvanceFrameForOverlays() : m_pXmv->AdvanceFrameForTexturing();
		m_pTextures[m_nCurrent] = frame;
		m_bFrameReady = frame != NULL;
		if (frame != NULL && m_bModal)
		{
			if (m_pSurfaces[m_nCurrent] != NULL)
				m_pSurfaces[m_nCurrent]->Release();
			frame->GetSurfaceLevel(0, &m_pSurfaces[m_nCurrent]);
		}
	}
	return S_OK;
}

// 0x0009BFAF
// A quad with the frame (texel coordinates on a linear texture): the
// screen area when full screen, else x, y, width, height in skin
// coordinates.
void CWMVideoPlayer::DrawFrame(IDirect3DTexture8 *texture, int x, int y, int width, int height)
{
	if (texture == NULL)
		return;
	g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	g_pd3dDevice->SetTexture(0, texture);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	if (m_bYuv)
		g_pd3dDevice->SetRenderState(D3DRS_YUVENABLE, TRUE);
	float left;
	float top;
	float right;
	float bottom;
	if (!m_bFullScreen)
	{
		float fx = (float)x;
		float fy = (float)y;
		float fw = (float)width;
		float fh = (float)height;
		ScaleToScreen(&fx, &fw, &fy, &fh, FALSE);
		left = fx;
		top = fy;
		right = fw + fx;
		bottom = fy + fh;
	}
	else
	{
		const SViewport &screen = m_pSettings->m_viewport;
		left = (float)(DWORD)screen.X;
		top = (float)(DWORD)screen.Y;
		right = (float)(DWORD)screen.Width + left;
		bottom = top + (float)(DWORD)screen.Height;
	}
	float u = (float)m_rcSource.right;
	float v = (float)m_rcSource.bottom;
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_pd3dDevice->Begin(D3DPT_QUADLIST);
	g_pd3dDevice->SetVertexData2f(D3DVSDE_TEXCOORD0, 0.0f, v);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, bottom, 0.0f, 1.0f);
	g_pd3dDevice->SetVertexData2f(D3DVSDE_TEXCOORD0, 0.0f, 0.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, left, top, 0.0f, 1.0f);
	g_pd3dDevice->SetVertexData2f(D3DVSDE_TEXCOORD0, u, 0.0f);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, top, 0.0f, 1.0f);
	g_pd3dDevice->SetVertexData2f(D3DVSDE_TEXCOORD0, u, v);
	g_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, right, bottom, 0.0f, 1.0f);
	g_pd3dDevice->End();
	if (m_bYuv)
		g_pd3dDevice->SetRenderState(D3DRS_YUVENABLE, FALSE);
}

// 0x0009C436
BOOL CWMVideoPlayer::Render()
{
	if (!m_bInitialized)
		return S_OK;
	if (m_bModal)
	{
		if (m_bReady && !m_bMessageShown && m_pWmv == NULL && m_pXmv == NULL)
		{
			m_bPlaying = FALSE;
			return E_FAIL;
		}
		if (m_bModal && m_bMessageShown)
			m_pMessageBox->Render();
	}
	if (!m_bReady || !m_bOpen)
		return S_OK;

	if (m_bModal && !m_bFullScreen)
	{
		// The window: the file's name and its size (and bit rate).
		float lineHeight = m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight;
		DimScreen(0.3f);
		if (!s_bCaptionHeightSet)
		{
			s_bCaptionHeightSet = true;
			s_fCaptionHeight = m_pTitleFont->m_fScaleFactorY * m_pTitleFont->m_fFontHeight + 10.0f + 2.0f;
		}
		DrawWindowFrame(m_fWindowX, m_fWindowY, m_fWindowWidth, m_fWindowHeight, 10.0f, 10.0f,
		                m_pFont->m_fScaleFactorY * m_pFont->m_fFontHeight * 1.5f, g_colorScheme.DIALOG_BORDERCOLOR,
		                g_colorScheme.DIALOG_BACKCOLOR, g_colorScheme.DIALOG_INNER_BORDERCOLOR,
		                g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 1);
		wchar_t name[260];
		MultiByteToWideChar(CP_ACP, 0, m_strFile.c_str(), (int)strlen(m_strFile.c_str()), name, 0x104);
		m_pTitleFont->DrawText(m_fWindowX + 10.0f + 2.0f,
		                       (m_pTitleFont->m_fScaleFactorY * m_pTitleFont->m_fFontHeight + 10.0f) * 0.5f + m_fWindowY,
		                       g_colorScheme.CAPTION_TEXTCOLOR, name, XBFONT_CENTER_Y | XBFONT_TRUNCATED,
		                       (m_fWindowWidth - 10.0f * 2.0f) - 4.0f);
		if (m_bOpen)
		{
			wchar_t text[260];
			if (!m_bXmv)
				swprintf(text, 260, L"%d x %d, %dkbps", m_info.width, m_info.height, m_info.bitRate >> 10);
			else
				swprintf(text, 260, L"%d x %d", m_info.width, m_info.height);
			m_pFont->DrawText(m_fWindowWidth * 0.5f + m_fWindowX, (m_fWindowHeight + m_fWindowY) - lineHeight,
			                  g_colorScheme.DIALOG_TEXTCOLOR, text, XBFONT_CENTER_X | XBFONT_CENTER_Y | XBFONT_TRUNCATED,
			                  m_fWindowWidth - 20.0f);
		}
	}

	if (!m_bFrameReady)
	{
		// No new frame: the last one again (windowed WMV only).
		if (!m_bXmv && !m_bModal)
		{
			const RECT &rect = m_bFullScreen ? m_rcScreen : m_rcDest;
			DrawFrame(m_pTextures[m_nCurrent ^ 1], rect.left, rect.top, rect.right, rect.bottom);
		}
		return S_OK;
	}
	if (m_bFullScreen)
	{
		// The screen area may have changed.
		const SViewport &screen = m_pSettings->m_viewport;
		if (m_rcScreen.right != m_rcScreen.left + (LONG)screen.Width ||
		    m_rcScreen.bottom != (LONG)screen.Height + m_rcScreen.top)
		{
			m_rcScreen.left = screen.X;
			m_rcScreen.top = screen.Y;
			m_rcScreen.right = m_rcScreen.left + screen.Width;
			m_rcScreen.bottom = screen.Height + m_rcScreen.top;
		}
	}
	const RECT &rect = m_bFullScreen ? m_rcScreen : m_rcDest;
	if (!m_bModal)
		DrawFrame(m_pTextures[m_nCurrent], rect.left, rect.top, rect.right, rect.bottom);
	else
		g_pd3dDevice->UpdateOverlay(m_pSurfaces[m_nCurrent], &m_rcSource, &rect, FALSE, 0);
	m_nCurrent ^= 1;
	m_bFrameReady = FALSE;
	return S_OK;
}
