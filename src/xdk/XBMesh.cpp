// The XDK sample framework's mesh (CXBMesh, .xbg files) as UnleashX links
// it. Xbox only. See app/XBApp.h.
#include "../app/XBApp.h"

HRESULT XBUtil_FindMediaFile(char *dst, const char *src);   // XBResource.cpp

#define XBG_MAGIC 0x02474258   // "XBG\2"

// 0x00097536 (this in eax)
CXBMesh::CXBMesh()
{
	m_pAllocatedSysMem = NULL;
	m_pAllocatedVidMem = NULL;
	m_pMeshFrames = NULL;
	m_dwNumFrames = 0;
	m_dwRefCount = 1;
}

// 0x00097801: releases the subsets' textures and the data.
CXBMesh::~CXBMesh()
{
	for (DWORD i = 0; i < m_dwNumFrames; i++)
	{
		XBMESH_FRAME *frame = &m_pMeshFrames[i];
		for (DWORD j = 0; j < frame->dwNumSubsets; j++)
		{
			if (frame->pSubsets[j].pTexture != NULL)
			{
				frame->pSubsets[j].pTexture->Release();
				frame->pSubsets[j].pTexture = NULL;
			}
		}
	}
	if (m_pAllocatedSysMem != NULL)
		delete[] (BYTE *)m_pAllocatedSysMem;
	if (m_pAllocatedVidMem != NULL)
		D3D_FreeContiguousMemory(m_pAllocatedVidMem);
}

// 0x00097F4B: the frames (with their offsets made pointers) in system
// memory, the vertex and index data in contiguous memory.
HRESULT CXBMesh::Create(const char *fileName, CXBPackedResource *resource)
{
	char path[512];
	if (FAILED(XBUtil_FindMediaFile(path, fileName)))
		return E_FAIL;
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
	                          NULL);
	if (file == INVALID_HANDLE_VALUE)
		return E_FAIL;
	DWORD read, magic;
	ReadFile(file, &magic, 4, &read, NULL);
	if (magic != XBG_MAGIC)
	{
		CloseHandle(file);
		return E_FAIL;
	}
	DWORD numFrames, sysMemSize, vidMemSize;
	ReadFile(file, &numFrames, 4, &read, NULL);
	ReadFile(file, &sysMemSize, 4, &read, NULL);
	ReadFile(file, &vidMemSize, 4, &read, NULL);
	m_pAllocatedSysMem = new BYTE[sysMemSize];
	ReadFile(file, m_pAllocatedSysMem, sysMemSize, &read, NULL);
	m_pAllocatedVidMem = D3D_AllocContiguousMemory(vidMemSize, D3DVERTEXBUFFER_ALIGNMENT);
	ReadFile(file, m_pAllocatedVidMem, vidMemSize, &read, NULL);
	CloseHandle(file);

	// The offsets in the file count from its start, 16 bytes before the
	// frames.
	m_pMeshFrames = (XBMESH_FRAME *)m_pAllocatedSysMem;
	m_dwNumFrames = numFrames;
	DWORD base = (DWORD)m_pMeshFrames - 16;
	for (DWORD i = 0; i < m_dwNumFrames; i++)
	{
		XBMESH_FRAME *frame = &m_pMeshFrames[i];
		if (frame->pChild != NULL)
			frame->pChild = (XBMESH_FRAME *)((DWORD)frame->pChild + base);
		if (frame->pNext != NULL)
			frame->pNext = (XBMESH_FRAME *)((DWORD)frame->pNext + base);
		if (frame->pSubsets != NULL)
			frame->pSubsets = (XBMESH_SUBSET *)((DWORD)frame->pSubsets + base);
		if (frame->dwNumIndices != 0)
			frame->IB.Data += base;
		if (frame->dwNumVertices != 0)
			D3DResource_Register(&frame->VB, m_pAllocatedVidMem);
	}
	if (resource != NULL)
	{
		for (DWORD i = 0; i < m_dwNumFrames; i++)
		{
			XBMESH_FRAME *frame = &m_pMeshFrames[i];
			for (DWORD j = 0; j < frame->dwNumSubsets; j++)
				frame->pSubsets[j].pTexture = resource->GetTexture(frame->pSubsets[j].strTexture);
		}
	}
	return S_OK;
}

// 0x00097521
HRESULT CXBMesh::Render(DWORD flags)
{
	if (m_pMeshFrames != NULL)
		RenderFrame(m_pMeshFrames, flags);
	return S_OK;
}

// 0x00097759
HRESULT CXBMesh::RenderFrame(XBMESH_FRAME *frame, DWORD flags)
{
	D3DXMATRIX world, frameWorld;
	D3DDevice_GetTransform(D3DTS_WORLD, &world);
	D3DXMatrixMultiply(&frameWorld, (D3DXMATRIX *)frame->matTransform, &world);
	D3DDevice_SetTransform(D3DTS_WORLD, &frameWorld);
	if (frame->dwNumSubsets != 0)
	{
		RenderFrameCallback((DWORD)(frame - m_pMeshFrames), frame, flags);
		RenderMesh((XBMESH_DATA *)&frame->VB, flags);
	}
	if (frame->pChild != NULL)
		RenderFrame(frame->pChild, flags);
	D3DDevice_SetTransform(D3DTS_WORLD, &world);
	if (frame->pNext != NULL)
		RenderFrame(frame->pNext, flags);
	return S_OK;
}

// 0x000975E9: flags XBMESH_OPAQUEONLY (1), XBMESH_ALPHAONLY (2),
// XBMESH_NOMATERIALS (0x10), XBMESH_NOTEXTURES (0x20), XBMESH_NOFVF (0x40).
HRESULT CXBMesh::RenderMesh(XBMESH_DATA *mesh, DWORD flags)
{
	if (mesh->dwNumVertices == 0)
		return S_OK;
	D3DDevice_SetStreamSource(0, (D3DVertexBuffer *)&mesh->VB, mesh->dwVertexSize);
	D3DDevice_SetIndices((D3DIndexBuffer *)&mesh->IB, 0);
	if ((flags & 0x40) == 0)
		D3DDevice_SetVertexShader(mesh->dwFVF);
	for (DWORD i = 0; i < mesh->dwNumSubsets; i++)
	{
		XBMESH_SUBSET *subset = &mesh->pSubsets[i];
		float alpha = subset->mtrl.Diffuse.a;
		BOOL draw = FALSE;
		if ((flags & 2) == 0 && ((flags & 0x10) != 0 || alpha >= 1.0f))
			draw = TRUE;
		if ((flags & 1) == 0 && (flags & 0x10) == 0 && alpha < 1.0f)
			draw = TRUE;
		if (!draw)
			continue;
		if ((flags & 0x10) == 0)
			D3DDevice_SetMaterial(&subset->mtrl);
		if ((flags & 0x20) == 0)
			D3DDevice_SetTexture(0, subset->pTexture);
		if (!RenderMeshCallback(i, subset, flags))
			continue;
		DWORD primitives = mesh->dwPrimType == D3DPT_TRIANGLESTRIP ? subset->dwIndexCount - 2 : subset->dwIndexCount / 3;
		D3DDevice::DrawIndexedPrimitive((D3DPRIMITIVETYPE)mesh->dwPrimType, 0, 0, subset->dwIndexStart, primitives);
	}
	if ((flags & 0x20) == 0)
		D3DDevice_SetTexture(0, NULL);
	D3DDevice_SetStreamSource(0, NULL, 0);
	D3DDevice_SetIndices(NULL, 0);
	return S_OK;
}
