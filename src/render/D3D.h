// Direct3D 8 (Xbox). On the Xbox this is <xtl.h>; on other hosts a minimal
// declaration-only stand-in so render code can be syntax-checked. Only what
// the recovered code uses is declared. Note the Xbox enum values differ from
// PC Direct3D 8 (texture stage states, blend factors).
#pragma once

#include "../common/Platform.h"

#ifndef _XBOX

typedef DWORD D3DFORMAT;
typedef DWORD D3DCOLOR;

#define D3DFMT_X1R5G5B5 0x03
#define D3DFMT_A8R8G8B8 0x06
#define D3DFMT_DXT1 0x0C
#define D3DFMT_LIN_R5G6B5 0x11
#define D3DFMT_LIN_A8R8G8B8 0x12
#define D3DX_FILTER_NONE 1

#define D3DRTYPE_TEXTURE 3
#define D3DLOCK_READONLY 0x80   // TODO: verify (LockRect flags 0x80 in the original)

#define D3DCLEAR_TARGET 0x000000F0
#define D3DCLEAR_ZBUFFER 0x00000001
#define D3DCLEAR_STENCIL 0x00000002

#define D3DFVF_XYZRHW 0x004
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_TEX1 0x100

#define D3DVSDE_DIFFUSE 3      // SetVertexDataColor register
#define D3DVSDE_VERTEX (-1)    // SetVertexData4f: the position
#define D3DFILL_SOLID 0x1B02
// D3DTEXTUREFILTERTYPE
#define D3DTEXF_NONE 0
#define D3DTEXF_LINEAR 2

// D3DPRIMITIVETYPE (Xbox)
#define D3DPT_LINELIST 2
#define D3DPT_LINELOOP 3
#define D3DPT_TRIANGLELIST 5
#define D3DPT_TRIANGLESTRIP 6
#define D3DPT_TRIANGLEFAN 7
#define D3DPT_QUADLIST 8
#define D3DPT_POLYGON 10

// D3DTEXTURESTAGESTATETYPE (Xbox order)
enum D3DTEXTURESTAGESTATETYPE
{
	D3DTSS_ADDRESSU = 0,
	D3DTSS_ADDRESSV = 1,
	D3DTSS_ADDRESSW = 2,
	D3DTSS_MAGFILTER = 3,
	D3DTSS_MINFILTER = 4,
	D3DTSS_MIPFILTER = 5,
	D3DTSS_COLOROP = 12,
	D3DTSS_COLORARG0 = 13,
	D3DTSS_COLORARG1 = 14,
	D3DTSS_COLORARG2 = 15,
	D3DTSS_ALPHAOP = 16,
	D3DTSS_ALPHAARG0 = 17,
	D3DTSS_ALPHAARG1 = 18,
	D3DTSS_ALPHAARG2 = 19,
};

// D3DTEXTUREOP (Xbox)
#define D3DTOP_DISABLE 1
#define D3DTOP_SELECTARG1 2
#define D3DTOP_SELECTARG2 3
#define D3DTOP_MODULATE 4

// D3DTA_*
#define D3DTA_DIFFUSE 0
#define D3DTA_CURRENT 1
#define D3DTA_TEXTURE 2
#define D3DTA_TFACTOR 3

#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_CLAMP 3

// D3DRENDERSTATETYPE: the names are what matters here; the values are opaque.
enum D3DRENDERSTATETYPE
{
	D3DRS_ALPHATESTENABLE,
	D3DRS_ALPHABLENDENABLE,
	D3DRS_SRCBLEND,
	D3DRS_DESTBLEND,
	D3DRS_ZENABLE,
	D3DRS_TEXTUREFACTOR,
	D3DRS_LINEWIDTH,
	D3DRS_CULLMODE,
	D3DRS_MULTISAMPLEANTIALIAS,
	D3DRS_FILLMODE,
	D3DRS_DITHERENABLE,
};

// D3DBLEND (Xbox uses the GL values)
#define D3DBLEND_SRCCOLOR 0x300
#define D3DBLEND_SRCALPHA 0x302
#define D3DBLEND_INVSRCALPHA 0x303

struct D3DSURFACE_DESC
{
	D3DFORMAT Format;
	DWORD Type;
	DWORD Usage;
	DWORD Size;
	DWORD MultiSampleType;
	DWORD Width;
	DWORD Height;
};

struct D3DLOCKED_RECT
{
	int Pitch;
	void *pBits;
};

struct D3DDISPLAYMODE
{
	DWORD Width;
	DWORD Height;
	DWORD RefreshRate;
	DWORD Flags;
	D3DFORMAT Format;
};

struct D3DRECT
{
	long x1, y1, x2, y2;
};

// The resource header every D3D object starts with (D3DResource). 0x14
// bytes for a texture: Common, Data, Lock, Format, Size.
struct D3DResource
{
	DWORD Common;
	DWORD Data;
	DWORD Lock;
};

struct IDirect3DSurface8
{
	ULONG AddRef();
	ULONG Release();
	HRESULT GetDesc(D3DSURFACE_DESC *desc);
};

struct IDirect3DTexture8
{
	ULONG AddRef();
	ULONG Release();
	HRESULT GetLevelDesc(UINT level, D3DSURFACE_DESC *desc);
	HRESULT GetSurfaceLevel(UINT level, IDirect3DSurface8 **surface);
	HRESULT LockRect(UINT level, D3DLOCKED_RECT *locked, const void *rect, DWORD flags);
};

struct D3DVIEWPORT8
{
	DWORD X;
	DWORD Y;
	DWORD Width;
	DWORD Height;
	float MinZ;
	float MaxZ;
};

struct SPresentParameters;   // D3DPRESENT_PARAMETERS, see app/XBApp.h

struct IDirect3DDevice8
{
	HRESULT Reset(SPresentParameters *params);
	HRESULT SetViewport(const D3DVIEWPORT8 *viewport);
	HRESULT Swap(DWORD flags);
	HRESULT SetSoftDisplayFilter(BOOL enable);
	HRESULT SetFlickerFilter(DWORD level);
	HRESULT CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, DWORD pool,
	                      IDirect3DTexture8 **texture);
	HRESULT SetTexture(DWORD stage, IDirect3DTexture8 *texture);
	HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value);
	HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value);
	HRESULT SetVertexShader(DWORD handle);
	HRESULT SetPixelShader(DWORD handle);
	HRESULT DrawVerticesUP(DWORD primitiveType, UINT vertexCount, const void *vertices, UINT stride);
	HRESULT GetDisplayMode(D3DDISPLAYMODE *mode);
	HRESULT GetBackBuffer(int index, DWORD type, IDirect3DSurface8 **surface);
	HRESULT SetScissors(DWORD count, BOOL exclusive, const D3DRECT *rects);
	HRESULT Clear(DWORD count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
	HRESULT Present(const void *, const void *, void *, void *);
	HRESULT CopyRects(IDirect3DSurface8 *source, const void *sourceRects, UINT count,
	                  IDirect3DSurface8 *destination, const void *destPoints);
	// immediate-mode drawing (Xbox extension)
	HRESULT Begin(DWORD primitiveType);
	HRESULT End();
	HRESULT SetVertexData4f(int reg, float a, float b, float c, float d);
	HRESULT SetVertexDataColor(int reg, D3DCOLOR color);
};

// The surface the previous program (or the boot animation) left on screen.
IDirect3DSurface8 *AvGetSavedDataAddress();

DWORD D3DResource_GetType(D3DResource *resource);
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface8 *destination, const void *destPalette, const void *destRect,
                                   IDirect3DSurface8 *source, const void *sourcePalette, const void *sourceRect,
                                   DWORD filter, D3DCOLOR colorKey);
void Get2DSurfaceDesc(D3DResource *resource, DWORD level, D3DSURFACE_DESC *desc);

HRESULT D3DXCreateTextureFromFileExA(IDirect3DDevice8 *device, const char *file, UINT width, UINT height,
                                     UINT mipLevels, DWORD usage, D3DFORMAT format, DWORD pool, DWORD filter,
                                     DWORD mipFilter, D3DCOLOR colorKey, void *srcInfo, void *palette,
                                     IDirect3DTexture8 **texture);
#endif

// XPR file header (XPR0/XPR1): the resource headers follow, the data starts
// at headerSize.
struct XPR_HEADER
{
	DWORD dwMagic;        // 'XPR0' or 'XPR1'
	DWORD dwTotalSize;
	DWORD dwHeaderSize;
};
#define XPR0_MAGIC 0x30525058
#define XPR1_MAGIC 0x31525058
