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
#define D3DFMT_A4R4G4B4 0x04
#define D3DFMT_DXT1 0x0C
#define D3DFMT_DXT3 0x0E
#define D3DFMT_LIN_R5G6B5 0x11
#define D3DFMT_LIN_A8R8G8B8 0x12
#define D3DFMT_LIN_X1R5G5B5 0x1C
#define D3DFMT_YUY2 0x24
#define D3DX_FILTER_NONE 1
#define D3DX_FILTER_TRIANGLE 4
#define D3DX_DEFAULT ((UINT)-1)

#define D3DRTYPE_TEXTURE 3
#define D3DLOCK_READONLY 0x80   // TODO: verify (LockRect flags 0x80 in the original)

#define D3DERR_INVALIDCALL ((HRESULT)0x8876086C)

#define D3DCLEAR_TARGET 0x000000F0
#define D3DCLEAR_ZBUFFER 0x00000001
#define D3DCLEAR_STENCIL 0x00000002

#define D3DFVF_XYZRHW 0x004
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_TEX1 0x100

#define D3DVSDE_DIFFUSE 3      // SetVertexDataColor register
#define D3DVSDE_VERTEX (-1)    // SetVertexData4f: the position
#define D3DVSDE_TEXCOORD0 9    // SetVertexData2f
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
	D3DTSS_RESULTARG = 20,
	D3DTSS_TEXTURETRANSFORMFLAGS = 21,
	D3DTSS_TEXCOORDINDEX = 26,   // TODO: verify the value (the original calls D3DDevice_SetTextureState_TexCoordIndex)
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
	D3DRS_SHADEMODE,
	D3DRS_ZBIAS,
	D3DRS_MULTISAMPLEMASK,
	D3DRS_COLORWRITEENABLE,
	D3DRS_AMBIENT,
	D3DRS_ZWRITEENABLE,
	D3DRS_SPECULARENABLE,
	D3DRS_LIGHTING,
	D3DRS_EDGEANTIALIAS,
	D3DRS_FOGENABLE,
	D3DRS_FOGCOLOR,
	D3DRS_FOGTABLEMODE,
	D3DRS_FOGSTART,
	D3DRS_FOGEND,
	D3DRS_FOGDENSITY,
	D3DRS_YUVENABLE,
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

struct IDirect3DVertexBuffer8
{
	ULONG Release();
	HRESULT Lock(UINT offset, UINT size, BYTE **data, DWORD flags);
};

struct IDirect3DIndexBuffer8
{
	ULONG Release();
	HRESULT Lock(UINT offset, UINT size, BYTE **data, DWORD flags);
};

#define D3DFMT_INDEX16 0x65   // TODO: verify

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

// D3DTRANSFORMSTATETYPE (Xbox)
#define D3DTS_VIEW 0
#define D3DTS_PROJECTION 1
#define D3DTS_WORLD 6
#define D3DCULL_NONE 0
#define D3DCULL_CW 0x900
#define D3DCULL_CCW 0x901
#define D3DTS_TEXTURE0 2
#define D3DFOG_LINEAR 3
#define D3DTTFF_DISABLE 0
#define D3DTTFF_COUNT2 2
#define D3DTSS_TCI_PASSTHRU 0
#define D3DTSS_TCI_CAMERASPACENORMAL 0x10000
#define D3DTADDRESS_CLAMPTOEDGE 5
#define D3DBLEND_ONE 1
#define D3DFVF_XYZ 0x002
#define D3DFVF_NORMAL 0x010
#define D3DFVF_TEX2 0x200
#define D3DSHADE_GOURAUD 0x1D01
#define D3DCOLORWRITEENABLE_ALL 0x01010101

struct D3DCOLORVALUE
{
	float r, g, b, a;
};
struct D3DMATERIAL8
{
	D3DCOLORVALUE Diffuse;
	D3DCOLORVALUE Ambient;
	D3DCOLORVALUE Specular;
	D3DCOLORVALUE Emissive;
	float Power;
};

struct IDirect3DDevice8
{
	HRESULT Reset(SPresentParameters *params);
	HRESULT SetViewport(const D3DVIEWPORT8 *viewport);
	HRESULT GetViewport(D3DVIEWPORT8 *viewport);
	HRESULT SetTransform(DWORD state, const float *matrix);
	HRESULT SetMaterial(const D3DMATERIAL8 *material);
	HRESULT BlockUntilVerticalBlank();
	HRESULT Swap(DWORD flags);
	HRESULT SetSoftDisplayFilter(BOOL enable);
	HRESULT SetFlickerFilter(DWORD level);
	HRESULT CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, DWORD pool,
	                      IDirect3DTexture8 **texture);
	HRESULT SetTexture(DWORD stage, IDirect3DTexture8 *texture);
	HRESULT SetStreamSource(UINT stream, IDirect3DVertexBuffer8 *buffer, UINT stride);
	HRESULT DrawVertices(DWORD primitiveType, UINT startVertex, UINT vertexCount);
	HRESULT SetIndices(IDirect3DIndexBuffer8 *buffer, UINT baseVertexIndex);
	HRESULT CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT format, DWORD pool, IDirect3DIndexBuffer8 **buffer);
	// (Inline on the Xbox: they turn the primitive count into a vertex
	// count and call DrawIndexedVertices / DrawVertices.)
	HRESULT DrawIndexedPrimitive(DWORD primitiveType, UINT minIndex, UINT numVertices, UINT startIndex,
	                             UINT primitiveCount);
	HRESULT DrawPrimitive(DWORD primitiveType, UINT startVertex, UINT primitiveCount);
	HRESULT SetVertexShaderConstant(int reg, const void *data, DWORD count);
	HRESULT CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, DWORD pool, IDirect3DVertexBuffer8 **buffer);
	HRESULT SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value);
	HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value);
	HRESULT SetVertexShader(DWORD handle);
	HRESULT SetPixelShader(DWORD handle);
	HRESULT DrawVerticesUP(DWORD primitiveType, UINT vertexCount, const void *vertices, UINT stride);
	HRESULT GetDisplayMode(D3DDISPLAYMODE *mode);
	BOOL IsBusy();                                // D3DDevice_IsBusy
	HRESULT GetBackBuffer(int index, DWORD type, IDirect3DSurface8 **surface);
	// A surface in system memory. (D3DDevice_CreateImageSurface, 0x00144840)
	HRESULT CreateImageSurface(UINT width, UINT height, D3DFORMAT format, IDirect3DSurface8 **surface);
	HRESULT SetScissors(DWORD count, BOOL exclusive, const D3DRECT *rects);
	HRESULT Clear(DWORD count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil);
	HRESULT Present(const void *, const void *, void *, void *);
	HRESULT CopyRects(IDirect3DSurface8 *source, const void *sourceRects, UINT count,
	                  IDirect3DSurface8 *destination, const void *destPoints);
	// immediate-mode drawing (Xbox extension)
	HRESULT Begin(DWORD primitiveType);
	HRESULT End();
	HRESULT SetVertexData2f(int reg, float a, float b);
	HRESULT SetVertexData4f(int reg, float a, float b, float c, float d);
	HRESULT SetVertexDataColor(int reg, D3DCOLOR color);
	// The video overlay (Xbox extension).
	HRESULT EnableOverlay(BOOL enable);
	HRESULT UpdateOverlay(IDirect3DSurface8 *surface, const RECT *source, const RECT *destination, BOOL colorKey,
	                      D3DCOLOR colorKeyColor);
};

// The surface the previous program (or the boot animation) left on screen.
IDirect3DSurface8 *AvGetSavedDataAddress();

DWORD D3DResource_GetType(D3DResource *resource);
HRESULT D3DXLoadSurfaceFromSurface(IDirect3DSurface8 *destination, const void *destPalette, const void *destRect,
                                   IDirect3DSurface8 *source, const void *sourcePalette, const void *sourceRect,
                                   DWORD filter, D3DCOLOR colorKey);
void Get2DSurfaceDesc(D3DResource *resource, DWORD level, D3DSURFACE_DESC *desc);

HRESULT D3DXCreateTexture(IDirect3DDevice8 *device, UINT width, UINT height, UINT mipLevels, DWORD usage,
                          D3DFORMAT format, DWORD pool, IDirect3DTexture8 **texture);
// info: D3DXIMAGE_INFO (SImageInfo, Texture.h).
HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice8 *device, const void *data, UINT size, UINT width,
                                            UINT height, UINT mipLevels, DWORD usage, D3DFORMAT format, DWORD pool,
                                            DWORD filter, DWORD mipFilter, D3DCOLOR colorKey, void *info,
                                            void *palette, IDirect3DTexture8 **texture);
HRESULT D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface8 *destination, const void *destPalette,
                                        const void *destRect, const void *data, UINT size, const void *srcRect,
                                        DWORD filter, D3DCOLOR colorKey, void *info);   // 0x00153B71
// TRUE: DXT2/DXT4 data loads as DXT3/DXT5. 0x001536E0
void D3DXSetDXT3DXT5(BOOL set);
BOOL XGIsSwizzledFormat(D3DFORMAT format);
HRESULT D3DXCreateTextureFromFileA(IDirect3DDevice8 *device, const char *file,
                                   IDirect3DTexture8 **texture);   // 0x001542A1
// D3DX math (matrices are 4x4 floats).
float *D3DXMatrixLookAtLH(float *out, const float *eye, const float *at, const float *up);   // 0x0003E5F3
float *D3DXMatrixPerspectiveFovLH(float *out, float fovY, float aspect, float zNear, float zFar);   // 0x0003E6DE
float *D3DXMatrixRotationYawPitchRoll(float *out, float yaw, float pitch, float roll);         // 0x0003E5BA
float *D3DXMatrixMultiply(float *out, const float *a, const float *b);                      // 0x0003E25B
// v (x, y, z, 1) times m, into a 4-vector.
float *D3DXVec3Transform(float *out, const float *v, const float *m);                       // 0x0003E19C
float *D3DXMatrixTranslation(float *out, float x, float y, float z);                         // 0x0003E400
float *D3DXMatrixScaling(float *out, float x, float y, float z);                             // 0x0003E3A4
float *D3DXMatrixIdentity(float *out);                                                      // 0x00020B0E (out in eax)
float *D3DXMatrixTranspose(float *out, const float *m);                                     // 0x0003E359
float D3DXVec4Length(const float *v);
UINT D3DXGetFVFVertexSize(DWORD fvf);                                                       // 0x00154515
HRESULT D3DXComputeBoundingSphere(const void *vertices, DWORD numVertices, DWORD fvf, float *center,
                                  float *radius);                                           // 0x001543F5                                                       // 0x0002A76A
// XDK sample framework: where the XBUtil loaders look for media ("D:\\Media\\").
extern char g_strMediaPath[];                                                              // 0x0026BC78
void XBUtil_SetMediaPath(const char *path);   // 0x000930A7 (path in eax)
// XDK sample framework: diffuse and ambient set to the color. (material in edx)
void XBUtil_InitMaterial(D3DMATERIAL8 *material, float r, float g, float b, float a);       // 0x00092EEF
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
