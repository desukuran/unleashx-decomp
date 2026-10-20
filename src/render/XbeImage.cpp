// An XBE's title image: the $$XTIMAGE section, either an XPR texture (an
// XPR header and the texture data 0x800 bytes in) or a DDS file. Loaded as
// a texture for menu icons and the FTP server's SITE TITLES, or saved next
// to the game for the game folder scan's items.xml cache.
#include "Texture.h"

#include <cstdlib>
#include <cstring>

#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../system/Storage.h"
#include "D3D.h"

// XBE image header fields, and the section headers it points to.
#define XBE_BASE_ADDRESS 0x104
#define XBE_SECTION_COUNT 0x11C
#define XBE_SECTION_HEADERS 0x120
#define XBE_DDS_MAGIC 0x20534444   // "DDS "

// Files under 10 KB have no title image to look for.
#define XBE_MIN_IMAGE_FILE 0x2800
// The section headers are looked for in the first 256 KB.
#define XBE_HEADER_READ 0x40000

// An XBE section header. 0x38 bytes.
struct SXbeSectionHeader
{
	DWORD flags;                 // +0x00
	DWORD virtualAddress;        // +0x04
	DWORD virtualSize;           // +0x08
	DWORD rawAddress;            // +0x0C file offset
	DWORD rawSize;               // +0x10
	DWORD nameAddress;           // +0x14
	DWORD nameReferenceCount;    // +0x18
	DWORD headReference;         // +0x1C
	DWORD tailReference;         // +0x20
	BYTE digest[20];             // +0x24
};

// The start of an XBE, read for its section headers. (Inlined into both
// readers.)
struct SXbeHeaders
{
	BYTE *data;                  // malloc'd, XBE_HEADER_READ bytes
	DWORD base;
	DWORD count;
	const SXbeSectionHeader *sections;
};

// FALSE when the file is too small (when its size can be read at all),
// can't be read or has no sections. data is left allocated when the read
// worked. Quirk: when the size can't be read the original goes on with an
// uninitialized size; here it counts as the most it reads.
static BOOL ReadXbeHeaders(HANDLE file, SXbeHeaders *xbe)
{
	xbe->data = NULL;
	BY_HANDLE_FILE_INFORMATION info;
	DWORD fileSize = XBE_HEADER_READ;
	if (GetFileInformationByHandle(file, &info))
	{
		fileSize = info.nFileSizeLow;
		if (fileSize < XBE_MIN_IMAGE_FILE)
			return FALSE;
	}
	SetFilePointer(file, 0, NULL, FILE_BEGIN);
	xbe->data = (BYTE *)malloc(XBE_HEADER_READ);
	if (xbe->data == NULL)
		return FALSE;
	DWORD read = 0;
	DWORD toRead = (int)fileSize < XBE_HEADER_READ + 1 ? fileSize : XBE_HEADER_READ;
	if (ReadFile(file, xbe->data, toRead, &read, NULL) != TRUE)
		return FALSE;
	xbe->base = *(DWORD *)(xbe->data + XBE_BASE_ADDRESS);
	xbe->count = *(DWORD *)(xbe->data + XBE_SECTION_COUNT);
	DWORD headers = *(DWORD *)(xbe->data + XBE_SECTION_HEADERS);
	if (xbe->count == 0 || headers == 0 || headers > fileSize)
		return FALSE;
	xbe->sections = (const SXbeSectionHeader *)(xbe->data + (headers - xbe->base));
	return TRUE;
}

// (The name's address isn't checked against what was read.)
static BOOL IsTitleImageSection(const SXbeHeaders &xbe, const SXbeSectionHeader &section)
{
	return memcmp(xbe.data + (section.nameAddress - xbe.base), "$$XTIMAGE", 10) == 0;
}

// The XPR header at the section's start; FALSE when it isn't one.
static BOOL ReadXprHeader(HANDLE file, SXbxHeader *header)
{
	memset(header, 0, sizeof(*header));
	DWORD count = 0;
	BOOL read = ReadFile(file, header, sizeof(*header), &count, NULL);
	return read && count == sizeof(*header) && (header->magic == XPR0_MAGIC || header->magic == XPR1_MAGIC) &&
	       header->headerSize == 0x800 && header->endMarker == 0xFFFFFFFF;
}

// 0x0009FAAF
BOOL LoadXbeIcon(const char *xbePath, IDirect3DTexture8 **texture)
{
	if (texture == NULL)
		return FALSE;
	*texture = NULL;
	BOOL result = FALSE;
	HANDLE file = CreateFileA(xbePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	SXbeHeaders xbe;
	if (!ReadXbeHeaders(file, &xbe))
	{
		CloseHandle(file);
		free(xbe.data);
		return FALSE;
	}

	// Unlike ExtractXbeTitleImage, an image section that isn't usable
	// doesn't end the search.
	for (DWORD i = 0; i < xbe.count; i++)
	{
		const SXbeSectionHeader &section = xbe.sections[i];
		if (!IsTitleImageSection(xbe, section))
			continue;
		if (SetFilePointer(file, section.rawAddress, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			break;
		SXbxHeader header;
		if (ReadXprHeader(file, &header))
		{
			// An XPR: the data straight into a new texture (an empty
			// size is taken as 128x128 DXT1).
			D3DSURFACE_DESC desc;
			Get2DSurfaceDesc(&header.texture, 0, &desc);
			if (desc.Height == 0)
			{
				desc.Height = 128;
				desc.Width = 128;
				desc.Format = D3DFMT_DXT1;
			}
			if (FAILED(g_pd3dDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, 0, texture)))
				break;
			D3DLOCKED_RECT locked;
			(*texture)->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
			SetFilePointer(file, section.rawAddress + 0x800, NULL, FILE_BEGIN);
			DWORD size = header.totalSize - header.headerSize;
			DWORD count = 0;
			if (ReadFile(file, locked.pBits, size, &count, NULL) && count == size)
			{
				result = TRUE;
				break;
			}
			// Quirk: the texture stays in *texture (unreleased, and
			// overwritten by a later section) and the search goes on.
		}
		else if (header.magic == XBE_DDS_MAGIC)
		{
			// A DDS file, loaded from memory.
			if (SetFilePointer(file, section.rawAddress, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
			{
				BYTE *dds = (BYTE *)malloc(section.rawSize + 0x800);
				if (dds != NULL)
				{
					DWORD count;
					SImageInfo info;
					if (ReadFile(file, dds, section.rawSize, &count, NULL) && g_pd3dDevice != NULL &&
					    D3DXCreateTextureFromFileInMemoryEx(g_pd3dDevice, dds, section.rawSize, D3DX_DEFAULT,
					                                        D3DX_DEFAULT, 0, 0, (D3DFORMAT)D3DX_DEFAULT, 1,
					                                        D3DX_DEFAULT, 1, 0, XDK_CAST(D3DXIMAGE_INFO *, &info), NULL, texture) == S_OK)
						result = TRUE;
					free(dds);
				}
			}
			break;
		}
	}
	free(xbe.data);
	CloseHandle(file);
	return result;
}

// 0x000A4225
BOOL SaveTextureToXpr(const char *path, IDirect3DTexture8 *texture)
{
	if (path == NULL || texture == NULL)
		return FALSE;
	D3DSURFACE_DESC desc;
	Get2DSurfaceDesc((D3DResource *)texture, 0, &desc);
	if (desc.Height == 0 || desc.Width == 0)
		return FALSE;
	// Quirk: this first level is fetched for nothing and never released.
	IDirect3DSurface8 *unused = NULL;
	texture->GetSurfaceLevel(0, &unused);

	IDirect3DSurface8 *copy = NULL;
	g_pd3dDevice->CreateImageSurface(desc.Width, desc.Height, desc.Format, &copy);
	if (copy == NULL)
		return FALSE;
	IDirect3DSurface8 *level = NULL;
	texture->GetSurfaceLevel(0, &level);
	HRESULT hr = D3DXLoadSurfaceFromSurface(copy, NULL, NULL, level, NULL, NULL, D3DX_DEFAULT, 0);
	if (level != NULL)
		level->Release();
	if (FAILED(hr))
	{
		copy->Release();
		return FALSE;
	}

	std::string dir = path;
	int slash = StrReverseFind(dir, "\\", -1);
	if (slash != -1)
		dir = StrLeft(dir, slash);
	if (DirectoryExists(dir.c_str()) || CreateDirectoryTree(dir.c_str()))
		hr = XGWriteSurfaceOrTextureToXPR(copy, path, TRUE);
	else
		hr = E_FAIL;
	copy->Release();
	return SUCCEEDED(hr);
}

// 0x000A4D10
BOOL ExtractXbeTitleImage(const char *xbePath, const char *outFile)
{
	// (The original runs this in a loop left over from searching the file
	// from its end; every way through it ends after the first pass. It
	// also copies an uninitialized buffer into an unused string.)
	BOOL result = FALSE;
	HANDLE file = CreateFileA(xbePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	SXbeHeaders xbe;
	if (ReadXbeHeaders(file, &xbe))
	{
		for (DWORD i = 0; i < xbe.count; i++)
		{
			const SXbeSectionHeader &section = xbe.sections[i];
			if (!IsTitleImageSection(xbe, section))
				continue;
			if (SetFilePointer(file, section.rawAddress, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
				break;
			SXbxHeader header;
			if (ReadXprHeader(file, &header))
			{
				// An XPR: into a texture, then written out as an .xpr.
				D3DSURFACE_DESC desc;
				Get2DSurfaceDesc(&header.texture, 0, &desc);
				if (desc.Height == 0 || desc.Width == 0)
				{
					desc.Height = 128;
					desc.Width = 128;
					desc.Format = D3DFMT_DXT1;
				}
				IDirect3DTexture8 *texture = NULL;
				if (FAILED(g_pd3dDevice->CreateTexture(desc.Width, desc.Height, 1, 0, desc.Format, 0, &texture)))
					break;
				D3DLOCKED_RECT locked;
				texture->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
				SetFilePointer(file, section.rawAddress + 0x800, NULL, FILE_BEGIN);
				DWORD size = header.totalSize - header.headerSize;
				DWORD count = 0;
				// Quirk: a short read leaks the texture.
				if (ReadFile(file, locked.pBits, size, &count, NULL) && count == size)
				{
					result = SaveTextureToXpr(outFile, texture);
					texture->Release();
				}
			}
			else if (header.magic == XBE_DDS_MAGIC && !FileExists(outFile) &&
			         SetFilePointer(file, section.rawAddress, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER)
			{
				// A DDS: copied out as it is, kept only if D3DX can load it.
				BYTE *dds = (BYTE *)malloc(section.rawSize + 0x800);
				DWORD count;
				if (dds != NULL && ReadFile(file, dds, section.rawSize, &count, NULL))
				{
					// (Without a backslash the file's own name is created
					// as a folder.)
					std::string dir = outFile;
					int slash = StrReverseFind(dir, "\\", -1);
					if (slash != -1)
						dir = StrLeft(dir, slash);
					CreateDirectoryTree(dir.c_str());
					HANDLE out = CreateFileA(outFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
					                         FILE_ATTRIBUTE_NORMAL, NULL);
					if (out != NULL && out != INVALID_HANDLE_VALUE)
					{
						DWORD written = 0;
						BOOL ok = WriteFile(out, dds, section.rawSize, &written, NULL) && written == section.rawSize;
						CloseHandle(out);
						if (ok)
						{
							IDirect3DTexture8 *check = NULL;
							SImageInfo info;
							if (g_pd3dDevice == NULL ||
							    D3DXCreateTextureFromFileExA(g_pd3dDevice, outFile, D3DX_DEFAULT, D3DX_DEFAULT, 0, 0,
							                                 (D3DFORMAT)D3DX_DEFAULT, 1, D3DX_DEFAULT, 1, 0, XDK_CAST(D3DXIMAGE_INFO *, &info),
							                                 NULL, &check) == S_OK)
							{
								result = TRUE;
								if (check != NULL)
									check->Release();
							}
							else
								DeleteFileA(outFile);
						}
					}
				}
				free(dds);
			}
			// The first image section decides, usable or not.
			break;
		}
	}
	free(xbe.data);
	CloseHandle(file);
	return result;
}
