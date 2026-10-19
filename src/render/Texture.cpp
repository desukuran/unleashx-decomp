// Texture loading and screen captures. See Texture.h.
#include "Texture.h"

#include <cstdlib>
#include <cstring>

#include "../app/UnleashX.h"
#include "../app/XBApp.h"
#include "../settings/AppSettings.h"
#include "../system/FileIO.h"
#include "D3D.h"

// The display mode, cached by GetDisplaySize. 0x00278D9C
static D3DDISPLAYMODE s_displayMode;

// (inlined everywhere)
void ReleaseTexture(IDirect3DTexture8 *&texture)
{
	if (texture != NULL)
	{
		texture->Release();
		texture = NULL;
	}
}

// 0x0009F14B (width in esi, height in edi)
void GetDisplaySize(float *width, float *height, BOOL refresh)
{
	if (refresh || s_displayMode.Height == 0)
		g_pd3dDevice->GetDisplayMode(&s_displayMode);
	if (width != NULL)
		*width = (float)s_displayMode.Width;
	if (height != NULL)
		*height = (float)s_displayMode.Height;
}

// Shrinks width x height (keeping the aspect) until it fits limitWidth x
// limitHeight. (inlined twice)
static void FitInside(int &width, int &height, unsigned limitWidth, unsigned limitHeight)
{
	if ((unsigned)width <= limitWidth && (unsigned)height <= limitHeight)
		return;
	float aspect = (float)(unsigned)width / (float)(unsigned)height;
	if (limitWidth < (unsigned)width)
	{
		width = (int)limitWidth;
		height = (int)((float)limitWidth / aspect);
	}
	if (limitHeight < (unsigned)height)
	{
		height = (int)limitHeight;
		width = (int)((float)limitHeight * aspect);
	}
}

// 0x0009ECB3 (path in ecx)
BOOL LoadTextureFromFile(const char *path, IDirect3DDevice8 *device, IDirect3DTexture8 **texture, int width, int height,
                         SImageInfo *info)
{
	if (texture == NULL)
		return FALSE;
	*texture = NULL;
	if (device == NULL || path == NULL)
		return FALSE;
	IFile *file = CreateFileObject(path);
	if (file == NULL)
		return FALSE;
	if (file->OpenRead(path, 0x1000, FALSE))
	{
		DWORD size = file->GetSize();
		char *data = (char *)malloc(size);
		if (data != NULL)
		{
			DWORD read = file->Read(data, size);
			if (read != 0 && data[0] != 0)
			{
				// (The info is also what each try asks for: D3DX fills
				// it in, and its format is passed on.)
				SImageInfo image;
				memset(&image, 0, sizeof(image));
				if (read <= 153600 || !g_pSettings->m_video.m_bTextureCompression)
				{
					image.format = D3DFMT_A8R8G8B8;
				}
				else
				{
					D3DXSetDXT3DXT5(TRUE);
					image.format = D3DFMT_DXT3;
				}
				image.width = g_pSettings->m_nSkinWidth;
				image.height = g_pSettings->m_nSkinHeight;
				// 1: its own size.
				HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(g_pd3dDevice, data, read, D3DX_DEFAULT, D3DX_DEFAULT, 1, 0,
				                                                 image.format, 1, D3DX_DEFAULT, D3DX_FILTER_NONE, 0,
				                                                 &image, NULL, texture);
				if (hr != S_OK)
				{
					// 2: no bigger than the display mode.
					D3DDISPLAYMODE mode;
					g_pd3dDevice->GetDisplayMode(&mode);
					if ((unsigned)width < 2 || (unsigned)height < 2)
					{
						width = image.width;
						height = image.height;
					}
					FitInside(width, height, mode.Width, mode.Height);
					hr = D3DXCreateTextureFromFileInMemoryEx(g_pd3dDevice, data, read, width, height, 1, 0, image.format, 1,
					                                         D3DX_DEFAULT, D3DX_FILTER_NONE, 0, &image, NULL, texture);
					if (hr != S_OK)
					{
						// 3: 16-bit, no bigger than the TV (720 wide for
						// widescreen, 576 high for PAL), through a surface.
						D3DXSetDXT3DXT5(TRUE);
						image.format = D3DFMT_DXT3;
						unsigned limitWidth = g_pSettings->m_video.m_bWidescreen ? 720 : 640;
						unsigned limitHeight = g_pSettings->m_video.m_bPAL ? 576 : 480;
						FitInside(width, height, limitWidth, limitHeight);
						image.format = D3DFMT_A4R4G4B4;
						hr = D3DXCreateTextureFromFileInMemoryEx(g_pd3dDevice, data, read, width, height, 1, 0,
						                                         D3DFMT_A4R4G4B4, 1, D3DX_DEFAULT, D3DX_FILTER_NONE, 0,
						                                         &image, NULL, texture);
						if (hr != S_OK)
						{
							if (XGIsSwizzledFormat(image.format))
							{
								image.width = 1024;
								image.height = 512;
							}
							IDirect3DSurface8 *surface = NULL;
							g_pd3dDevice->CreateImageSurface(image.width, image.height, image.format, &surface);
							hr = surface != NULL ? S_OK : E_OUTOFMEMORY;
							if (hr == S_OK)
							{
								hr = D3DXLoadSurfaceFromFileInMemory(surface, NULL, NULL, data, read, NULL, D3DX_DEFAULT, 0,
								                                     &image);
								if (hr == S_OK &&
								    SUCCEEDED(D3DXCreateTexture(g_pd3dDevice, width, height, 1, 0, image.format, 0, texture)))
								{
									IDirect3DSurface8 *level = NULL;
									(*texture)->GetSurfaceLevel(0, &level);
									D3DXLoadSurfaceFromSurface(level, NULL, NULL, surface, NULL, NULL, D3DX_FILTER_TRIANGLE, 0);
									level->Release();
								}
								surface->Release();
							}
						}
					}
				}
				if (info != NULL && *texture != NULL)
					*info = image;
			}
			free(data);
		}
		file->Close();
	}
	delete file;
	return *texture != NULL;
}

// 0x0009F19C (texture in eax)
HRESULT CaptureBackBuffer(IDirect3DTexture8 **texture, BOOL backBuffer)
{
	HRESULT hr = E_FAIL;
	ReleaseTexture(*texture);
	IDirect3DSurface8 *surface = NULL;
	g_pd3dDevice->GetBackBuffer(backBuffer ? 0 : -1, 0, &surface);
	D3DSURFACE_DESC desc;
	surface->GetDesc(&desc);
	if (desc.Width != 0 && desc.Height != 0)
	{
		D3DDISPLAYMODE mode;
		g_pd3dDevice->GetDisplayMode(&mode);
		g_pd3dDevice->CreateTexture(640, 480, 1, 0, mode.Format, 0, texture);
		if (*texture != NULL)
		{
			IDirect3DSurface8 *level = NULL;
			(*texture)->GetSurfaceLevel(0, &level);
			if (level != NULL)
			{
				D3DXLoadSurfaceFromSurface(level, NULL, NULL, surface, NULL, NULL, D3DX_FILTER_NONE, 0);
				level->Release();
				hr = S_OK;
			}
		}
	}
	surface->Release();
	return hr;
}

// 0x0009F23D
HRESULT CaptureSavedScreen(IDirect3DTexture8 **texture, IDirect3DDevice8 * /*device*/)
{
	IDirect3DSurface8 *saved = AvGetSavedDataAddress();
	if (saved == NULL)
		return S_OK;
	D3DSURFACE_DESC desc;
	saved->GetDesc(&desc);
	if (desc.Width != 0 && desc.Height != 0)
	{
		D3DDISPLAYMODE mode;
		g_pd3dDevice->GetDisplayMode(&mode);
		g_pd3dDevice->CreateTexture(640, 480, 1, 0, mode.Format, 0, texture);
		IDirect3DSurface8 *level = NULL;
		(*texture)->GetSurfaceLevel(0, &level);
		D3DXLoadSurfaceFromSurface(level, NULL, NULL, saved, NULL, NULL, D3DX_FILTER_TRIANGLE, 0);
		level->Release();
	}
	return S_OK;
}

// 0x0009AEB8
// The captured screen (a linear texture, texel coordinates) over the
// width x height display at alpha. Afterwards it puts back modulate, wrap
// and no blending, then turns texturing off.
void DrawSavedScreen(IDirect3DTexture8 *texture, DWORD width, DWORD height, float alpha)
{
	struct STexelVertex
	{
		float x, y, z, rhw;
		float u, v;
	};
	float w = (float)(int)width;
	float h = (float)(int)height;
	STexelVertex v[4] = {
		{-0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f},
		{w - 0.5f, -0.5f, 1.0f, 0.0f, w, 0.0f},
		{w - 0.5f, h - 0.5f, 1.0f, 0.0f, w, h},
		{-0.5f, h - 0.5f, 1.0f, 0.0f, 0.0f, h},
	};

	g_pd3dDevice->SetTexture(0, texture);
	g_pd3dDevice->SetPixelShader(0);
	// (The original writes these into D3D's texture stage state cache.)
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, (DWORD)(int)(alpha * 255.0f) << 24);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	g_pd3dDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
	g_pd3dDevice->DrawVerticesUP(D3DPT_QUADLIST, 4, v, sizeof(STexelVertex));

	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	g_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF000000);
	// Then texturing off again (both stages disabled).
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	g_pd3dDevice->SetTexture(0, NULL);
}

// 0x0009F909
bool LoadXprTexture(const char *path, IDirect3DTexture8 **texture)
{
	if (texture == NULL)
		return false;
	*texture = NULL;
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return *texture != NULL;
	SXbxHeader header;
	memset(&header, 0, sizeof(header));
	DWORD read;
	if (!ReadFile(file, &header, sizeof(header), &read, NULL))
	{
		CloseHandle(file);
		return false;
	}
	BOOL ok = read == sizeof(header) && (header.magic == 0x30525058 || header.magic == 0x31525058) &&
	          header.headerSize == 0x800 && header.endMarker == 0xFFFFFFFF && header.texture.Common < 0x828480 &&
	          D3DResource_GetType(&header.texture) == D3DRTYPE_TEXTURE;
	if (ok)
	{
		D3DSURFACE_DESC desc;
		Get2DSurfaceDesc(&header.texture, 0, &desc);
		ok = SUCCEEDED(D3DXCreateTexture(g_pd3dDevice, desc.Width, desc.Height, 1, 0, desc.Format, 0, texture));
		if (ok)
		{
			// The data straight into the texture. (It's never unlocked.)
			D3DLOCKED_RECT locked;
			(*texture)->LockRect(0, &locked, NULL, D3DLOCK_READONLY);
			DWORD dataSize = header.totalSize - header.headerSize;
			SetFilePointer(file, 0x800, NULL, FILE_BEGIN);
			ok = ReadFile(file, locked.pBits, dataSize, &read, NULL);
			ok &= read == dataSize;
			if (ok)
			{
				// (Onto itself, with black as the color key.)
				IDirect3DSurface8 *level = NULL;
				(*texture)->GetSurfaceLevel(0, &level);
				D3DXLoadSurfaceFromSurface(level, NULL, NULL, level, NULL, NULL, D3DX_FILTER_NONE, 0xFF000000);
				level->Release();
				CloseHandle(file);
				return *texture != NULL;
			}
		}
		// Quirk: a texture that failed to fill is dropped, not released.
		*texture = NULL;
	}
	CloseHandle(file);
	return false;
}

// 0x000A2371 (path in eax)
BOOL LoadXprFileTexture(const char *path, IDirect3DTexture8 **texture)
{
	if (texture == NULL)
		return FALSE;
	*texture = NULL;
	BOOL ok = FALSE;
	CXBPackedResource xpr;
	if (SUCCEEDED(xpr.Create(path, 1, NULL)))
	{
		IDirect3DTexture8 *source = (IDirect3DTexture8 *)xpr.GetResource(0);
		if (source != NULL)
		{
			D3DSURFACE_DESC desc;
			Get2DSurfaceDesc((D3DResource *)source, 0, &desc);
			if (SUCCEEDED(D3DXCreateTexture(g_pd3dDevice, desc.Width, desc.Height, 1, 0, desc.Format, 0, texture)))
			{
				IDirect3DSurface8 *from = NULL;
				IDirect3DSurface8 *to = NULL;
				source->GetSurfaceLevel(0, &from);
				(*texture)->GetSurfaceLevel(0, &to);
				D3DXLoadSurfaceFromSurface(to, NULL, NULL, from, NULL, NULL, D3DX_FILTER_NONE, 0xFF000000);
				to->Release();
				from->Release();
				ok = TRUE;
			}
		}
		xpr.Destroy();
	}
	return ok;
}
