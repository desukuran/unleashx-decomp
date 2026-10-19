// The 3D background: the orb (a reflective sphere, environment-mapped from
// the view) and its vertex buffer.
#include <cmath>

#include "../render/D3D.h"
#include "../render/Texture.h"
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
