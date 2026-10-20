// The XDK sample framework's packed resources (.xpr files) as UnleashX links
// them. Xbox only. See app/XBApp.h.
#include "../app/XBApp.h"

#include <stdio.h>

#define XPR0_MAGIC_VALUE 0x30525058
#define XPR1_MAGIC_VALUE 0x31525058

// 0x00093025: the file name as given if it has a drive, else under
// g_strMediaPath; fails if it can't be opened. (src in esi, dst in edi)
HRESULT XBUtil_FindMediaFile(char *dst, const char *src)
{
	if (src == NULL || dst == NULL)
		return E_INVALIDARG;
	strcpy(dst, src);
	if (src[1] != ':')
		sprintf(dst, "%s%s", g_strMediaPath, src);
	HANDLE file = CreateFileA(dst, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		char message[340];
		sprintf(message, "XBUtil_FindMediaFile(): Could not find file [%s]\n", src);
		return 0x82000004;
	}
	CloseHandle(file);
	return S_OK;
}

// 0x000972B3 (this in eax)
CXBPackedResource::CXBPackedResource()
{
	m_pSysMemData = NULL;
	m_dwSysMemDataSize = 0;
	m_pVidMemData = NULL;
	m_dwVidMemDataSize = 0;
	m_pResourceTags = NULL;
	m_dwNumResourceTags = 0;
}

// 0x000972AE
CXBPackedResource::~CXBPackedResource()
{
	Destroy();
}

// XPR1 files carry their own tag table at the start of the system-memory
// part, with offsets to make into pointers.
static void FixupTags(CXBPackedResource *resource)
{
	DWORD *data = (DWORD *)resource->m_pSysMemData;
	resource->m_dwNumResourceTags = data[0];
	resource->m_pResourceTags = data + 1;
	XBRESOURCE *tags = (XBRESOURCE *)resource->m_pResourceTags;
	for (DWORD i = 0; i < resource->m_dwNumResourceTags; i++)
		tags[i].strName += (DWORD)resource->m_pSysMemData;
}

// 0x0009714B
HRESULT CXBPackedResource::Create(const char *fileName, DWORD numResourceTags, void *resourceTags)
{
	char path[512];
	if (FAILED(XBUtil_FindMediaFile(path, fileName)))
		return E_FAIL;
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY,
	                          NULL);
	if (file == INVALID_HANDLE_VALUE)
		return E_FAIL;
	XPR_HEADER header;
	DWORD read;
	ReadFile(file, &header, sizeof(header), &read, NULL);
	BOOL xpr1;
	if (header.dwMagic == XPR0_MAGIC_VALUE)
		xpr1 = FALSE;
	else if (header.dwMagic == XPR1_MAGIC_VALUE)
		xpr1 = TRUE;
	else
	{
		CloseHandle(file);
		return E_INVALIDARG;
	}
	m_dwSysMemDataSize = header.dwHeaderSize - sizeof(header);
	m_dwVidMemDataSize = header.dwTotalSize - header.dwHeaderSize;
	m_pSysMemData = new BYTE[m_dwSysMemDataSize];
	m_pVidMemData = (BYTE *)D3D_AllocContiguousMemory(m_dwVidMemDataSize, D3DTEXTURE_ALIGNMENT);
	ReadFile(file, m_pSysMemData, m_dwSysMemDataSize, &read, NULL);
	ReadFile(file, m_pVidMemData, m_dwVidMemDataSize, &read, NULL);
	CloseHandle(file);
	if (xpr1)
		FixupTags(this);
	if (numResourceTags != 0 || resourceTags != NULL)
	{
		m_pResourceTags = resourceTags;
		m_dwNumResourceTags = numResourceTags;
	}
	return S_OK;
}

// 0x00096F48: the same from an .xpr image in memory.
HRESULT CXBPackedResource::CreateFromMemory(const void *xpr, DWORD numResourceTags, void *resourceTags)
{
	if (xpr == NULL)
		return E_FAIL;
	const XPR_HEADER *header = (const XPR_HEADER *)xpr;
	BOOL xpr1;
	if (header->dwMagic == XPR0_MAGIC_VALUE)
		xpr1 = FALSE;
	else if (header->dwMagic == XPR1_MAGIC_VALUE)
		xpr1 = TRUE;
	else
		return E_INVALIDARG;
	// Quirk: the sizes are used but not stored.
	DWORD sysSize = header->dwHeaderSize - sizeof(XPR_HEADER);
	DWORD vidSize = header->dwTotalSize - header->dwHeaderSize;
	m_pSysMemData = new BYTE[sysSize];
	m_pVidMemData = (BYTE *)D3D_AllocContiguousMemory(vidSize, D3DTEXTURE_ALIGNMENT);
	memcpy(m_pSysMemData, (const BYTE *)xpr + sizeof(XPR_HEADER), sysSize);
	memcpy(m_pVidMemData, (const BYTE *)xpr + sizeof(XPR_HEADER) + sysSize, vidSize);
	if (xpr1)
		FixupTags(this);
	if (numResourceTags != 0 || resourceTags != NULL)
	{
		m_pResourceTags = resourceTags;
		m_dwNumResourceTags = numResourceTags;
	}
	return S_OK;
}

// 0x00097104
void CXBPackedResource::Destroy()
{
	if (m_pSysMemData != NULL)
		delete[] m_pSysMemData;
	m_pSysMemData = NULL;
	m_dwSysMemDataSize = 0;
	if (m_pVidMemData != NULL)
		D3D_FreeContiguousMemory(m_pVidMemData);
	m_pVidMemData = NULL;
	m_dwVidMemDataSize = 0;
	m_pResourceTags = NULL;
	m_dwNumResourceTags = 0;
}

// 0x00097134
HRESULT CXBPackedResource::GetResourceTags(DWORD *count, XBRESOURCE **tags)
{
	if (count != NULL)
		*count = m_dwNumResourceTags;
	if (tags != NULL)
		*tags = (XBRESOURCE *)m_pResourceTags;
	return S_OK;
}

// 0x0009726F: the named resource in the system-memory data, by the tag
// table (names without case).
static void *GetData(CXBPackedResource *resource, const char *name)
{
	XBRESOURCE *tags = (XBRESOURCE *)resource->m_pResourceTags;
	if (tags == NULL || name == NULL)
		return NULL;
	for (int i = 0; tags[i].strName != NULL; i++)
		if (_stricmp(name, tags[i].strName) == 0)
			return resource->m_pSysMemData + tags[i].dwOffset;
	return NULL;
}

// 0x0001C7E4: registered on first use (or, for a push buffer in system
// memory, made absolute), and AddRef'd. (resource in esi)
static D3DResource *RegisterResource(CXBPackedResource *packed, D3DResource *resource)
{
	if (resource == NULL || (resource->Common & D3DCOMMON_REFCOUNT_MASK) != 1)
		return resource;
	// A push buffer (bit 17 of the type) whose data is in system memory
	// (the top bit) is fixed up instead of registered.
	if ((resource->Common & 0x00020000) == 0 || (int)resource->Common >= 0)
		D3DResource_Register(resource, packed->m_pVidMemData);
	else
		resource->Data += (DWORD)packed->m_pSysMemData;
	D3DResource_AddRef(resource);
	return resource;
}

D3DResource *CXBPackedResource::GetResource(DWORD offset)
{
	return RegisterResource(this, (D3DResource *)(m_pSysMemData + offset));
}

// 0x0001C81B
IDirect3DTexture8 *CXBPackedResource::GetTexture(const char *name)
{
	return (IDirect3DTexture8 *)RegisterResource(this, (D3DResource *)GetData(this, name));
}
