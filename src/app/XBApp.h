// The application framework UnleashX is built on: CXBApplicationEx (the
// XDK sample framework as extended by XBMC), CXBStopWatch, CXBPackedResource
// and CXBMesh. Library code: only what UnleashX uses is declared here.
#pragma once

#include "../common/Platform.h"

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
	BYTE m_defaultGamepad[0xE4];              // +0x0A8 XBGAMEPAD; +0xB0/+0xB1 are the black/white
	                                          //        analog buttons, read at startup
	BYTE m_defaultIR[0x8C];                   // +0x18C XBIR_REMOTE TODO: verify size
	DWORD m_dwScreenWidth;                    // +0x218 640, 720, 1280, 1920
	DWORD m_dwScreenHeight;                   // +0x21C 480, 576, 720, 1080
};

extern CXBApplicationEx *g_pApplication;     // 0x00277660 set by the constructor

// High-resolution timer. 0x20 bytes.
class CXBStopWatch
{
public:
	CXBStopWatch();                           // 0x00094658
	void Stop();                              // 0x00094606

	float m_fSecsPerTick;
	BYTE m_state[0x1C];
};

// A packed resource (.xpr) loaded from the XBE. 0x18 bytes.
class CXBPackedResource
{
public:
	CXBPackedResource();                      // 0x000972B3
	// Texture of the named resource, or NULL.
	IDirect3DTexture8 *GetTexture(const char *name);   // 0x0001C81B

	BYTE *m_pSysMemData;
	DWORD m_dwSysMemDataSize;
	BYTE *m_pVidMemData;
	DWORD m_dwVidMemDataSize;
	void *m_pResourceTags;
	DWORD m_dwNumResourceTags;
};

// 0x20 bytes; CXBMeshEx only replaces the vtable.
class CXBMesh
{
public:
	CXBMesh();                                // 0x00097536
	virtual ~CXBMesh() {}

	BYTE m_data[0x1C];
};
