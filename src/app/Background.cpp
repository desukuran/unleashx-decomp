// The 3D background (CUnleashX::RenderBackground): the globe (the sky
// sphere, textured with the cell wall, with fog), the skin's background
// image or video, and the orb (a reflective sphere showing the selected
// item's icon, inside the MS dashboard's ring mesh) that spins when the
// menu moves.
#include <cmath>

#include "../media/WMVideoPlayer.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "Screens.h"
#include "UnleashX.h"

// The orb's tilt (and two more angles): set by InitOrb.
static float s_fOrbYaw;      // 0x00279510
static float s_fOrbPitch;    // 0x00279514
static float s_fOrbRoll;     // 0x00279518 (cleared once)
static bool s_bOrbRollSet;   // 0x0027951C bit 0

#define ORB_RINGS 15
#define ORB_SEGMENTS 31
#define ORB_STEP 0.20943951f   // pi / 15

// The orb's vertex: position, normal (the same: a unit sphere), the
// environment map and the plain texture coordinates. 0x28 bytes.
struct SOrbVertex
{
	float x, y, z;           // +0x00
	float nx, ny, nz;        // +0x0C
	float envU, envV;        // +0x18
	float u, v;              // +0x20
};

// 0x000E0411
HRESULT CUnleashX::CreateOrbBuffer()
{
	if (m_pOrbVB != NULL)
	{
		m_pOrbVB->Release();
		m_pOrbVB = NULL;
	}
	m_nOrbVertices = ORB_RINGS * ORB_SEGMENTS * 2;
	// TODO: the usage/FVF/pool arguments (the original calls the
	// two-argument D3DDevice_CreateVertexBuffer2 with the length only).
	HRESULT hr = g_pd3dDevice->CreateVertexBuffer(m_nOrbVertices * sizeof(SOrbVertex), 0, 0, 0, &m_pOrbVB);
	if (FAILED(hr) || m_pOrbVB == NULL)
		return E_FAIL;
	return S_OK;
}

// Fills a vertex on the unit sphere, with environment map coordinates from
// its normal in view space.
static void SetOrbVertex(SOrbVertex *vertex, float x, float y, float z, float u, float v, const float *transform)
{
	vertex->x = vertex->nx = x;
	vertex->y = vertex->ny = y;
	vertex->z = vertex->nz = z;
	vertex->u = u;
	vertex->v = v;
	float transformed[4];
	D3DXVec3Transform(transformed, &vertex->nx, transform);
	float scale = 1.37f / D3DXVec4Length(transformed);
	vertex->envU = transformed[0] * scale + 0.5f;
	vertex->envV = 0.5f - transformed[1] * scale;
}

// 0x000E0662
// 15 rings from the top, each a strip of 31 vertex pairs around.
void CUnleashX::InitOrb()
{
	if (!s_bOrbRollSet)
	{
		s_bOrbRollSet = true;
		s_fOrbRoll = 0.0f;
	}
	s_fOrbYaw = 0.25840735f;
	s_fOrbPitch = 0.0f;
	float rotation[16];
	D3DXMatrixRotationYawPitchRoll(rotation, 0.25840735f, 0.0f, 0.0f);
	float transform[16];
	D3DXMatrixMultiply(transform, rotation, m_viewMatrix);

	BYTE *data = NULL;
	m_pOrbVB->Lock(0, 0, &data, 0);
	SOrbVertex *vertex = (SOrbVertex *)data;
	for (unsigned ring = 0; ring < ORB_RINGS; ring++)
	{
		float top = (float)ring * ORB_STEP;
		float bottom = (float)(ring + 1) * ORB_STEP;
		float topV = (float)ring * (1.0f / 15.0f);
		float bottomV = (float)(ring + 1) * (1.0f / 15.0f);
		for (unsigned segment = 0; segment < ORB_SEGMENTS; segment++)
		{
			float around = (float)segment * ORB_STEP;
			float u = 1.0f - (float)segment * (1.0f / 30.0f);
			SetOrbVertex(vertex++, sinf(around) * sinf(top), cosf(top), cosf(around) * sinf(top), u, topV,
			             transform);
			SetOrbVertex(vertex++, sinf(around) * sinf(bottom), cosf(bottom), cosf(around) * sinf(bottom), u,
			             bottomV, transform);
		}
	}
}

// The orb's position and spin.
static float s_fAspect;            // 0x0027A36C 1, or 640 / the screen width when wider
static bool s_bAspectSet;          // 0x0027A370 bit 0
static float s_fOrbX;              // 0x00278D30 -12 (set on first use)
static float s_fOrbY;              // 0x00278D2C 6.5
static float s_fOrbAngle;          // 0x00278D28 its turn about the view axis
static float s_fSpinFrames;        // 0x00278D24 the fast spin's countdown
static int s_nSpinItem;            // 0x00278D20 the selection the spin was for
static float s_fSpinDirection = -1.0f;   // 0x0026CBF0 the slow turn's direction
static float s_fLastSpin = 1.0f;   // 0x0026CBF4
static float s_fSpinStep = 0.0087266462f;   // 0x0026CBF8 half a degree

#define FVF_MESH (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)   // 0x112
#define FVF_ORB (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2)    // 0x212

// The orb turns half a degree a frame in the direction the menu moved
// (g_fGlobeSpin, set by FrameMove), for 30 frames counted down every 5 ms
// or more; then slowly (1/45 of that) the same way.
static void UpdateOrbSpin(int selected)
{
	float delta;
	if (g_fGlobeSpin == 0.0f)
	{
		s_fSpinFrames = 0.0f;
		delta = s_fSpinStep * (1.0f / 45.0f) * s_fSpinDirection;
	}
	else
	{
		if (s_nSpinItem == selected)
		{
			s_fSpinDirection = g_fGlobeSpin;
			g_fGlobeSpin = 0.0f;
		}
		else
		{
			if (s_fSpinFrames == 0.0f || s_fLastSpin != g_fGlobeSpin)
			{
				// A new move: what's left of the last one first.
				s_fOrbAngle += s_fSpinStep * s_fSpinFrames * g_fGlobeSpin;
				s_fSpinFrames = 30.0f;
			}
			s_fLastSpin = g_fGlobeSpin;
			if (GetTickCount() - g_dwGlobeSpinTime > 5)
			{
				s_fSpinFrames -= 1.0f;
				g_dwGlobeSpinTime = GetTickCount();
			}
			if (s_fSpinFrames < 0.0f)
				s_fSpinFrames = 0.0f;
			if (s_fSpinFrames == 0.0f)
			{
				s_fSpinDirection = g_fGlobeSpin;
				g_fGlobeSpin = 0.0f;
				s_nSpinItem = selected;
			}
		}
		delta = s_fSpinStep * g_fGlobeSpin;
	}
	s_fOrbAngle += delta;
}

// 0x000E0E0F
// (The original writes most render and texture stage states straight into
// D3D's state cache.)
HRESULT CUnleashX::RenderBackground()
{
	IDirect3DDevice8 *device = g_pd3dDevice;
	if (!s_bAspectSet)
	{
		s_bAspectSet = true;
		if (m_dwScreenWidth <= 640)
			s_fAspect = 1.0f;
		else
			s_fAspect = 640.0f / (float)(unsigned)g_pSettings->m_viewport.Width;
	}
	if (s_fOrbX == 0.0f)
	{
		s_fOrbY = 6.5f;
		s_fOrbX = -12.0f;
	}

	if (g_skin.m_bGlobe)
	{
		// The globe: the sky sphere in the cell color, blended, without
		// writing depth.
		device->SetVertexShader(FVF_MESH);
		device->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
		device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_AMBIENT, g_colorScheme.AMBIENT_SKYCOLOR);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_DITHERENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		device->SetRenderState(D3DRS_TEXTUREFACTOR, g_colorScheme.AMBIENT_CELLCOLOR);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		device->SetRenderState(D3DRS_ZENABLE, TRUE);
		// The skin's cell wall, else the MS dashboard's, else our own.
		IDirect3DTexture8 *wall = g_skin.m_pCellwall;
		if (wall == NULL)
			wall = m_pDashCellWall != NULL ? m_pDashCellWall : m_pCellWall;
		device->SetTexture(0, wall);
		device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		device->SetRenderState(D3DRS_EDGEANTIALIAS, TRUE);
		device->SetTransform(D3DTS_WORLD, m_globeMatrix);
		m_outerSphereMesh.Render(0x30);
		device->SetRenderState(D3DRS_EDGEANTIALIAS, FALSE);

		// Back to the plain view, with linear fog from 5 to 100.
		D3DXMatrixIdentity(m_worldMatrix);
		device->SetVertexShader(FVF_MESH);
		device->SetTransform(D3DTS_WORLD, m_worldMatrix);
		device->SetTransform(D3DTS_VIEW, m_viewMatrix);
		device->SetTransform(D3DTS_PROJECTION, m_projMatrix);
		device->SetRenderState(D3DRS_AMBIENT, g_colorScheme.AMBIENT_SKYCOLOR);
		device->SetRenderState(D3DRS_FOGENABLE, g_skin.m_bFog);
		device->SetRenderState(D3DRS_FOGCOLOR, g_colorScheme.AMBIENT_FOGCOLOR);
		device->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
		float fogStart = 5.0f;
		float fogEnd = 100.0f;
		device->SetRenderState(D3DRS_FOGSTART, *(DWORD *)&fogStart);
		device->SetRenderState(D3DRS_FOGEND, *(DWORD *)&fogEnd);
		// Quirk: the DWORD 1, not 1.0f (unused with linear fog).
		device->SetRenderState(D3DRS_FOGDENSITY, 1);
	}

	// The skin's background image or video.
	try
	{
		if (g_skin.m_pBackground != NULL)
			DrawTextureFullScreen(g_skin.m_pBackground, 1.0f);
		else if (g_skin.m_pBackgroundVideo != NULL)
			g_skin.m_pBackgroundVideo->Render();
	}
	catch (...)
	{
	}

	// TODO: what the shader constants (registers 20 and 24) are for.
	float transposed[16];
	D3DXMatrixTranspose(transposed, (const float *)m_unknown980);
	device->SetVertexShaderConstant(0x74 - 96, transposed, 4);
	D3DXMatrixTranspose(transposed, m_projMatrix);
	device->SetVertexShaderConstant(0x78 - 96, transposed, 4);

	if (g_skin.m_bOrb)
	{
		// The ring (CrossHair.xbg): CellWall added in grey through the
		// camera-space normals.
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
		device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
		device->SetRenderState(D3DRS_ZENABLE, TRUE);
		device->SetRenderState(D3DRS_AMBIENT, g_colorScheme.AMBIENT_SKYCOLOR);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		device->SetRenderState(D3DRS_TEXTUREFACTOR, 0x8C8C8C8C);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		device->SetTexture(0, m_pCellWall);
		// (The original leaves the rest of the matrix uninitialized.)
		float texture[16] = {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.0f, 0.0f,
		                     0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f};
		device->SetTransform(D3DTS_TEXTURE0, texture);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL);
		float world[16];
		D3DXMatrixIdentity(world);
		UpdateOrbSpin(m_menu.m_nSelected);
		BuildWorldMatrix(world, s_fOrbX, s_fOrbY, 38.0f, s_fAspect * 0.36f, 0.38f, 0.38f, 0.0f, 0.18f, s_fOrbAngle);
		device->SetTransform(D3DTS_WORLD, world);
		device->SetVertexShader(FVF_MESH);
		m_crossHairMesh.Render(0x30);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
		device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMPTOEDGE);
		device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMPTOEDGE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

		// The orb itself: in the menu the selected item's icon (else the
		// skin's orb image, else ours).
		IDirect3DTexture8 *texture0;
		IDirect3DTexture8 *icon = m_menu.m_pIconTexture;
		if (m_nMode == MODE_MENU)
		{
			if (g_skin.m_pOrbTexture != NULL)
				texture0 = icon != NULL ? icon : g_skin.m_pOrbTexture;
			else
				texture0 = icon != NULL ? icon : m_pOrb;
		}
		else
		{
			texture0 = g_skin.m_pOrbTexture != NULL ? g_skin.m_pOrbTexture : m_pOrb;
		}
		device->SetTexture(0, texture0);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetVertexShader(FVF_ORB);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
		device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		D3DXMatrixIdentity(world);
		BuildWorldMatrix(world, s_fOrbX, s_fOrbY, 40.0f, s_fAspect * 9.0f, 9.0f, 9.0f, 0.0f, 0.0f, 0.01f);
		device->SetTransform(D3DTS_WORLD, world);
		device->SetStreamSource(0, m_pOrbVB, sizeof(SOrbVertex));
		device->DrawVertices(D3DPT_TRIANGLESTRIP, 0, m_nOrbVertices);
	}

	// <Main><GameIcon>: the selected item's icon.
	IDirect3DTexture8 *icon = m_menu.m_pIconTexture;
	if (g_bSkinGameIcon && icon != NULL)
	{
		const SSkinRect &rect = g_skin.m_rcGameIcon;
		float x = (float)rect.left;
		float y = (float)rect.top;
		float width = (float)rect.width;
		float height = (float)rect.height;
		ScaleToScreen(&x, &width, &y, &height, FALSE);
		DrawTexture(icon, x, y, width, height, g_skin.m_fGameIconOpacity, NULL, 0.0f, NULL);
	}

	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	return S_OK;
}
