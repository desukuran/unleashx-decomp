// CXMViewer: the file manager's mesh viewer. See Viewers.h.
//
// An .xbg loads through the XDK's CXBMesh, its frames scaled to a unit
// sphere around the origin; an .xm is a raw dump (SXmHeader, vertices,
// indices) drawn as it is. The model sits 45 units in front of the camera
// in the skin's AMBIENT_CELLCOLOR; the right stick turns it, A/B/BACK
// close the viewer.
#include <cstdio>
#include <cstring>

#include "../app/XBApp.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../skin/SkinColors.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Viewers.h"

// The left stick's Y, summed every frame and never used. 0x0026CDDC
static float s_fUnusedSpin;

// Moves a frame's vertices by offset and scales them. (frame in ecx,
// offset in eax; NULL is no offset)
static HRESULT TransformFrameVertices(XBMESH_FRAME *frame, const float *offset, float scale)   // 0x000BE34E
{
	if (frame == NULL)
		return D3DERR_INVALIDCALL;
	float zero[3] = {0.0f, 0.0f, 0.0f};
	if (offset == NULL)
		offset = zero;
	float move[3] = {offset[0], offset[1], offset[2]};
	DWORD count = frame->dwNumVertices;
	UINT stride = D3DXGetFVFVertexSize(frame->dwFVF);
	BYTE *data;
	((IDirect3DVertexBuffer8 *)&frame->VB)->Lock(0, 0, &data, 0);
	for (; count != 0; count--)
	{
		float *v = (float *)data;
		v[0] = (v[0] + move[0]) * scale;
		v[1] = (v[1] + move[1]) * scale;
		v[2] = (v[2] + move[2]) * scale;
		data += stride;
	}
	return S_OK;
}

// The bounding sphere of a frame's vertices. (frame in eax)
static HRESULT ComputeFrameBounds(XBMESH_FRAME *frame, float *center, float *radius)   // 0x000BE3E9
{
	if (frame == NULL)
		return D3DERR_INVALIDCALL;
	DWORD count = frame->dwNumVertices;
	DWORD fvf = frame->dwFVF;
	BYTE *data;
	((IDirect3DVertexBuffer8 *)&frame->VB)->Lock(0, 0, &data, 0);
	return D3DXComputeBoundingSphere(data, count, fvf, center, radius);
}

// Scales a frame to the given radius (centred on the origin when center
// is set), then its children and the frames after it. Each frame is
// fitted on its own.
static HRESULT FitFrame(XBMESH_FRAME *frame, float size, BOOL center)   // 0x000BE41C
{
	float sphereCenter[3];
	float radius;
	HRESULT hr = ComputeFrameBounds(frame, sphereCenter, &radius);
	if (FAILED(hr))
		return hr;
	float scale = size / radius;
	float offset[3] = {0.0f, 0.0f, 0.0f};
	if (center)
	{
		offset[0] = 0.0f - sphereCenter[0];
		offset[1] = 0.0f - sphereCenter[1];
		offset[2] = 0.0f - sphereCenter[2];
	}
	TransformFrameVertices(frame, offset, scale);
	if (frame->pChild != NULL)
		FitFrame(frame->pChild, size, center);
	if (frame->pNext != NULL)
		FitFrame(frame->pNext, size, center);
	return S_OK;
}

// 0x000BE4E7 (this in esi)
CXMViewer::CXMViewer()
{
	m_pVB = NULL;
	m_pIB = NULL;
	memset(&m_header, 0, sizeof(m_header));
	m_bOpen = FALSE;
	m_bMessageShown = FALSE;
	m_pMessageBox = NULL;
	m_pFont = NULL;
	m_bInitialized = FALSE;
	m_bStayOpen = FALSE;
	m_bXbg = FALSE;
}

// 0x000BE6BB (this in eax)
CXMViewer::~CXMViewer()
{
	ReleaseModel();
	delete m_pMessageBox;
	m_pMessageBox = NULL;
}

// 0x000BE316 (this in esi)
void CXMViewer::ReleaseModel()
{
	if (m_pVB != NULL)
	{
		m_pVB->Release();
		m_pVB = NULL;
	}
	if (m_pIB != NULL)
	{
		m_pIB->Release();
		m_pIB = NULL;
	}
	m_mesh.Release();
}

// 0x000BDF7D
BOOL CXMViewer::Init(CXBFontEx *font)
{
	if (m_bInitialized)
		return S_OK;
	if (font == NULL)
		return E_FAIL;
	m_fUnknown70 = 40.0f;
	m_pFont = font;
	m_fUnknown74 = 30.0f;
	GetScreenSize(&m_fScreenWidth, &m_fScreenHeight);
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x000BE6F8 (this in edi)
void CXMViewer::ShowMessage(const char *text, const wchar_t *title, int buttons)
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

// 0x000BE815 (this in ebx)
BOOL CXMViewer::Open(const char *path)
{
	if (path == NULL || strlen(path) == 0)
	{
		m_bOpen = FALSE;
		return FALSE;
	}
	m_strPath = path;
	ReleaseModel();
	D3DXMatrixIdentity(m_matWorld);
	std::string message;

	std::string extension = StrRight(m_strPath, 3);
	StrMakeLower(extension);
	if (extension.compare("xbg") == 0)
	{
		if (SUCCEEDED(m_mesh.Create(m_strPath.c_str(), NULL)))
		{
			m_bXbg = TRUE;
			for (DWORD i = 0; i < m_mesh.m_dwNumFrames; i++)
				FitFrame(&m_mesh.m_pMeshFrames[i], 1.0f, TRUE);
			return TRUE;
		}
		StrFormat(message, "The file '%s' is not a valid mesh file.", m_strPath.c_str());
		ShowMessage(message.c_str(), L"Error", MSGBOX_BUTTONS_OK);
		m_bXbg = FALSE;
		return FALSE;
	}

	// An .xm (or anything else that isn't an .xbg). Quirk: the buffers
	// aren't checked, and one that failed to fill stays (an index buffer
	// read short still gets drawn).
	FILE *file = fopen(path, "rb");
	if (file == NULL)
	{
		StrFormat(message, "The file '%s' is not a valid mesh file.", m_strPath.c_str());
		ShowMessage(message.c_str(), L"Error", MSGBOX_BUTTONS_OK);
		return FALSE;
	}
	memset(&m_header, 0, sizeof(m_header));
	if (fread(&m_header, 1, sizeof(m_header), file) != sizeof(m_header))
	{
		fclose(file);
		StrFormat(message, "The file '%s' is not a valid mesh file.", m_strPath.c_str());
		ShowMessage(message.c_str(), L"Error", MSGBOX_BUTTONS_OK);
		return FALSE;
	}
	// Quirk: only points and lines; triangle meshes are refused.
	if ((int)m_header.primitiveType > 0 && (int)m_header.primitiveType < 5)
	{
		UINT vertexBytes = m_header.vertexSize * m_header.vertexCount;
		g_pd3dDevice->CreateVertexBuffer(vertexBytes, 0, 0, 0, &m_pVB);
		BYTE *data;
		m_pVB->Lock(0, 0, &data, 0);
		if (fread(data, 1, vertexBytes, file) == vertexBytes)
		{
			g_pd3dDevice->CreateIndexBuffer(m_header.indexCount * 2, 0, D3DFMT_INDEX16, 0, &m_pIB);
			m_pIB->Lock(0, 0, &data, 0);
			if (fread(data, 1, m_header.indexCount * 2, file) == m_header.indexCount * 2)
			{
				fclose(file);
				return TRUE;
			}
		}
	}
	fclose(file);
	// (Sic.)
	StrFormat(message, "Unable to The file '%s'. It may not be a valid mesh file.", m_strPath.c_str());
	ShowMessage(message.c_str(), L"Error", MSGBOX_BUTTONS_OK);
	return FALSE;
}

// 0x000BE21B
HRESULT CXMViewer::ProcessInput(int *action, SGamepadState **pad)
{
	// (The message box's buttons fall through to the checks below.)
	if (m_bMessageShown)
	{
		m_pMessageBox->ProcessInput(action);
		m_bMessageShown = m_pMessageBox->IsOpen();
	}
	if (pad != NULL)
	{
		// The right stick turns the model up to 90 degrees each way.
		SGamepadState *state = *pad;
		float x2 = state != NULL ? state->fX2 : 0.0f;
		float y2 = state != NULL ? state->fY2 : 0.0f;
		float y1 = state != NULL ? state->fY1 : 0.0f;
		s_fUnusedSpin += y1;
		BuildWorldMatrix(m_matWorld, 0.0f, 0.0f, 45.0f, 1.0f, 1.0f, 1.0f, 0.0f - y2 * 1.5707964f,
		                 0.0f - x2 * 1.5707964f, 0.0f);
	}
	if (!m_bStayOpen)
	{
		if (*action == INPUT_A || *action == INPUT_B || *action == INPUT_BACK)
		{
			*action = INPUT_NONE;
			m_bOpen = FALSE;
		}
	}
	return S_OK;
}

// 0x000BE52C
BOOL CXMViewer::Render()
{
	DimScreen(0.3f);
	if (m_bMessageShown)
	{
		// (Drawn before the model, so the model covers it.)
		m_pMessageBox->Render();
	}
	else if (!m_bXbg && (m_pVB == NULL || m_pIB == NULL))
	{
		m_bOpen = FALSE;
		return E_FAIL;
	}
	IDirect3DDevice8 *device = g_pd3dDevice;
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetRenderState(D3DRS_AMBIENT, g_colorScheme.AMBIENT_SKYCOLOR);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	// TODO: verify the stage states (inlined writes to the deferred state
	// table).
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.AMBIENT_CELLCOLOR);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	device->SetTransform(D3DTS_WORLD, m_matWorld);
	if (m_bXbg)
	{
		m_mesh.Render(0x20);
	}
	else
	{
		// Quirk: no vertex shader is set (the FVF in the header goes
		// unused), and after the indexed draw the vertices are drawn again
		// with the vertex count taken as a primitive count.
		device->SetStreamSource(0, m_pVB, m_header.vertexSize);
		device->SetIndices(m_pIB, 0);
		device->DrawIndexedPrimitive(m_header.primitiveType, 0, 0, 0, m_header.primitiveCount);
		device->DrawPrimitive(m_header.primitiveType, 0, m_header.vertexCount);
	}
	return S_OK;
}
