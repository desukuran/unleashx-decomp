// The application framework UnleashX is built on: CXBApplicationEx (the
// XDK sample framework as extended by XBMC), CXBStopWatch, CXBPackedResource
// and CXBMesh. Library code: only what UnleashX uses is declared here.
#pragma once

#include "../common/Platform.h"
#include "../input/XBInput.h"
#include "../render/D3D.h"

struct IDirect3DDevice8;
struct IDirect3DTexture8;

// D3DPRESENT_PARAMETERS (XDK), 0x44 bytes
struct SPresentParameters
{
	DWORD BackBufferWidth;              // +0x00
	DWORD BackBufferHeight;             // +0x04
	DWORD BackBufferFormat;             // +0x08
	DWORD BackBufferCount;              // +0x0C
	DWORD MultiSampleType;              // +0x10
	DWORD SwapEffect;                   // +0x14
	HANDLE hDeviceWindow;               // +0x18
	BOOL Windowed;                      // +0x1C
	BOOL EnableAutoDepthStencil;        // +0x20
	DWORD AutoDepthStencilFormat;       // +0x24
	DWORD Flags;                        // +0x28 D3DPRESENTFLAG_*
	DWORD FullScreen_RefreshRateInHz;   // +0x2C
	DWORD FullScreen_PresentationInterval;   // +0x30
	void *BufferSurfaces[3];            // +0x34
	void *DepthStencilSurface;          // +0x40
};

#define D3DPRESENTFLAG_WIDESCREEN 0x10
#define D3DPRESENTFLAG_INTERLACED 0x20
#define D3DPRESENTFLAG_PROGRESSIVE 0x40
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000
#define D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE 0x80000001
#define D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN 0x3223

// 0x220 bytes. Vtable: scalar deleting destructor, then the four overrides.
class CXBApplicationEx
{
public:
	CXBApplicationEx();                       // 0x0009A26F
	virtual ~CXBApplicationEx() {}
	virtual HRESULT Initialize() = 0;
	virtual HRESULT FrameMove() = 0;
	virtual HRESULT Render() = 0;
	virtual HRESULT Cleanup() = 0;

	HRESULT Create();                         // 0x0009B270
	int Run();                                // 0x0009A514

	SPresentParameters m_d3dpp;               // +0x004
	void *m_pD3D;                             // +0x048 TODO: verify
	IDirect3DDevice8 *m_pd3dDevice;           // +0x04C
	void *m_pBackBuffer;                      // +0x050
	void *m_pDepthBuffer;                     // +0x054
	float m_fTime;                            // +0x058
	float m_fElapsedTime;                     // +0x05C
	float m_fAppTime;                         // +0x060
	float m_fElapsedAppTime;                  // +0x064
	BOOL m_bPaused;                           // +0x068
	float m_fFPS;                             // +0x06C
	wchar_t m_strFrameRate[20];               // +0x070
	BYTE m_unknown098[0x10];                  // +0x098
	XBGAMEPAD m_defaultGamepad;               // +0x0A8 all four gamepads merged (UpdateGamepads)
	DWORD m_unknown188;                       // +0x188
	SIRRemote m_defaultIR;                    // +0x18C the remote in use
	SKeystroke *m_pKeystroke;                 // +0x210 the debug keyboard's last key
	DWORD m_unknown214;                       // +0x214
	DWORD m_dwScreenWidth;                    // +0x218 640, 720, 1280, 1920
	DWORD m_dwScreenHeight;                   // +0x21C 480, 576, 720, 1080
};

extern CXBApplicationEx *g_pApplication;     // 0x00277660 set by the constructor
extern SPresentParameters *g_pPresentParams; // 0x00277668 &g_pApplication->m_d3dpp, set by Create()
extern DWORD g_dwVideoFlags;                 // 0x0027766C XGetVideoFlags(), set by Create()

// High-resolution timer. 0x20 bytes.
class CXBStopWatch
{
public:
	CXBStopWatch();                           // 0x00094658
	void Start();                             // 0x0009462A
	// Start without clearing the time counted so far (the tail of Start).
	void Resume();                            // 0x00094645 (this in esi)
	void Stop();                              // 0x00094606
	void Reset();                             // 0x000945ED
	BOOL IsRunning() const;                   // 0x000945E9
	float GetElapsedSeconds();                // 0x000945BA
	float GetElapsedMilliseconds();           // 0x0009468D
	// The same with GetTickCount (milliseconds). (this in esi)
	void StartTicks();                        // 0x0002B5C2
	void StopTicks();                         // 0x0002B5DE
	float GetElapsedTicks();                  // 0x0002B604

	float m_fSecsPerTick;
	BYTE m_state[0x1C];
};

// A named resource in an .xpr (XDK XBRESOURCE).
struct XBRESOURCE
{
	char *strName;
	DWORD dwOffset;
};

// A packed resource (.xpr) loaded from the XBE. 0x18 bytes.
class CXBPackedResource
{
public:
	CXBPackedResource();                      // 0x000972B3
	~CXBPackedResource();                     // 0x000972AE
	// Loads an .xpr file. (this on the stack, fileName in eax)
	HRESULT Create(const char *fileName, DWORD numResourceTags = 0, void *resourceTags = NULL);   // 0x0009714B
	void Destroy();                           // 0x00097104
	// Texture of the named resource, or NULL.
	IDirect3DTexture8 *GetTexture(const char *name);   // 0x0001C81B
	// The resource at offset in the system-memory data, registered and
	// AddRef'd (each call adds a reference). (this in ecx, the resource
	// pointer in esi)
	D3DResource *GetResource(DWORD offset);   // 0x0001C7E4
	// (count in eax, tags in edx, this in ecx)
	HRESULT GetResourceTags(DWORD *count, XBRESOURCE **tags);   // 0x00097134

	BYTE *m_pSysMemData;
	DWORD m_dwSysMemDataSize;
	BYTE *m_pVidMemData;
	DWORD m_dwVidMemDataSize;
	void *m_pResourceTags;
	DWORD m_dwNumResourceTags;
};

// XDK sample framework mesh data (XBMesh.h). A subset of a frame's mesh.
struct XBMESH_SUBSET
{
	D3DMATERIAL8 mtrl;                // +0x00
	IDirect3DTexture8 *pTexture;      // +0x44
	char strTexture[64];              // +0x48
	DWORD dwVertexStart;              // +0x88
	DWORD dwVertexCount;              // +0x8C
	DWORD dwIndexStart;               // +0x90
	DWORD dwIndexCount;               // +0x94
};

// A frame of an .xbg mesh. 0xC0 bytes in the array.
struct XBMESH_FRAME
{
	float matTransform[16];           // +0x00
	D3DResource VB;                   // +0x40 the D3DVertexBuffer header itself
	DWORD dwNumVertices;              // +0x4C
	D3DResource IB;                   // +0x50
	DWORD dwNumIndices;               // +0x5C
	DWORD dwFVF;                      // +0x60
	DWORD dwVertexSize;               // +0x64
	DWORD dwPrimType;                 // +0x68
	DWORD dwNumSubsets;               // +0x6C
	XBMESH_SUBSET *pSubsets;          // +0x70
	char strName[64];                 // +0x74
	XBMESH_FRAME *pChild;             // +0xB4
	XBMESH_FRAME *pNext;              // +0xB8
	DWORD m_padding;                  // +0xBC
};

// 0x18 bytes; CXBMeshEx only replaces the vtable.
class CXBMesh
{
public:
	CXBMesh();                                // 0x00097536
	// Loads an .xbg file; its textures come from resource (if given).
	// (this on the stack, fileName in eax) TODO: decompile
	HRESULT Create(const char *fileName, CXBPackedResource *resource);   // 0x00097F4B
	// Draws the frames; flags XBMESH_* (0x30: no materials or textures, the
	// caller sets them). (this in ecx)
	HRESULT Render(DWORD flags);                                        // 0x00097521
	// (Inlined as written: it can't reach 0, so it only counts down.)
	void Release()
	{
		if (m_dwRefCount > 1 && --m_dwRefCount == 0)
			delete this;
	}
	virtual ~CXBMesh() {}

	void *m_pAllocatedSysMem;                 // +0x04
	void *m_pAllocatedVidMem;                 // +0x08
	XBMESH_FRAME *m_pMeshFrames;              // +0x0C
	DWORD m_dwNumFrames;                      // +0x10
	DWORD m_dwRefCount;                       // +0x14
};
