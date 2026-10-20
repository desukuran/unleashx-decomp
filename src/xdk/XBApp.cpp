// The XDK sample framework's application object, stopwatch and small
// helpers as UnleashX links them (the main loop, Run(), is in
// app/XBAppRun.cpp). Xbox only. See app/XBApp.h.
#include "../app/XBApp.h"

#include "../input/XBInput.h"
#include "../render/Texture.h"

CXBApplicationEx *g_pApplication;          // 0x00277660
IDirect3DDevice8 *g_pd3dDevice;            // 0x00277664
SPresentParameters *g_pPresentParams;      // 0x00277668
DWORD g_dwVideoFlags;                      // 0x0027766C

// 0x0009A26F (this in eax)
CXBApplicationEx::CXBApplicationEx()
{
	g_pApplication = this;
	m_pD3D = NULL;
	m_pd3dDevice = NULL;
	m_pDepthBuffer = NULL;
	m_pBackBuffer = NULL;
	m_bPaused = FALSE;
	m_fTime = 0.0f;
	m_fElapsedTime = 0.0f;
	m_fAppTime = 0.0f;
	m_fElapsedAppTime = 0.0f;
	m_fFPS = 0.0f;
	m_strFrameRate[0] = 0;
	m_dwScreenWidth = 640;
	m_dwScreenHeight = 480;
	m_pInputDeviceTypes = NULL;
	m_dwNumInputDeviceTypes = 0;
}

// 0x0009B270: a 640x480 32-bit device with a 24-bit depth buffer, no vsync
// wait, then the input devices and the app's Initialize().
HRESULT CXBApplicationEx::Create()
{
	m_pD3D = Direct3DCreate8(0);
	if (m_pD3D == NULL)
		return E_FAIL;
	g_dwVideoFlags = XGetVideoFlags();
	memset(&m_d3dpp, 0, sizeof(m_d3dpp));
	m_d3dpp.Flags = D3DPRESENTFLAG_INTERLACED;
	m_d3dpp.BackBufferWidth = 640;
	m_d3dpp.BackBufferHeight = 480;
	m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	m_d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	m_d3dpp.BackBufferCount = 1;
	m_d3dpp.EnableAutoDepthStencil = TRUE;
	m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	HRESULT hr = Direct3D_CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
	                                   XDK_CAST(D3DPRESENT_PARAMETERS *, &m_d3dpp), &g_pd3dDevice);
	if (FAILED(hr))
		return hr;
	m_pd3dDevice = g_pd3dDevice;
	g_pPresentParams = &m_d3dpp;
	m_pDepthBuffer = D3DDevice_GetDepthStencilSurface2();
	m_pBackBuffer = D3DDevice_GetBackBuffer2(0);

	XInitDevices(m_dwNumInputDeviceTypes, (XDEVICE_PREALLOC_TYPE *)m_pInputDeviceTypes);
	hr = XBInput_CreateGamepads(&m_pGamepads);
	if (FAILED(hr))
		return hr;
	XBInput_CreateIR_Remotes(&m_pIRRemotes);
	XBInput_InitDebugKeyboard((SKeyboardState **)&m_pKeystroke);
	XBInput_InitDebugMouse(&m_pMice);

	hr = Initialize();
	if (FAILED(hr))
		return hr;
	// The Direct3D object isn't needed any more (the sample released it).
	m_pD3D = NULL;
	return S_OK;
}

// ---- CXBStopWatch ----

// 0x000945A4: QueryPerformanceCounter.
static LONGLONG GetTicks()
{
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return ticks.QuadPart;
}

// 0x00094658 (also 0x0002B58D; this in esi)
CXBStopWatch::CXBStopWatch()
{
	m_nStartTick = 0;
	m_nElapsedTicks = 0;
	m_bRunning = FALSE;
	m_fSecsPerTick = 0.0f;
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	m_fSecsPerTick = 1.0f / (float)frequency.QuadPart;
}

// 0x0009462A
void CXBStopWatch::Start()
{
	m_nElapsedTicks = 0;
	Resume();
}

// 0x00094645
void CXBStopWatch::Resume()
{
	m_nStartTick = GetTicks();
	m_bRunning = TRUE;
}

// 0x00094606
void CXBStopWatch::Stop()
{
	if (!m_bRunning)
		return;
	m_nElapsedTicks += GetTicks() - m_nStartTick;
	m_nStartTick = 0;
	m_bRunning = FALSE;
}

// 0x000945ED
void CXBStopWatch::Reset()
{
	m_nElapsedTicks = 0;
	if (m_bRunning)
		m_nStartTick = GetTicks();
}

// 0x000945E9
BOOL CXBStopWatch::IsRunning() const
{
	return m_bRunning;
}

// 0x000945BA
float CXBStopWatch::GetElapsedSeconds()
{
	LONGLONG ticks = m_nElapsedTicks;
	if (m_bRunning)
		ticks += GetTicks() - m_nStartTick;
	return (float)ticks * m_fSecsPerTick;
}

// 0x0009468D
float CXBStopWatch::GetElapsedMilliseconds()
{
	return GetElapsedSeconds() * 1000.0f;
}

// 0x0002B5C2: the same with GetTickCount.
void CXBStopWatch::StartTicks()
{
	m_nElapsedTicks = 0;
	m_nStartTick = GetTickCount();
	m_bRunning = TRUE;
}

// 0x0002B5DE
void CXBStopWatch::StopTicks()
{
	if (!m_bRunning)
		return;
	m_nElapsedTicks += (LONGLONG)GetTickCount() - m_nStartTick;
	m_nStartTick = 0;
	m_bRunning = FALSE;
}

// 0x0002B604: milliseconds. Quirk: 0 once stopped (the time counted while
// running isn't returned then).
float CXBStopWatch::GetElapsedTicks()
{
	if (!m_bRunning)
		return 0.0f;
	return (float)(m_nElapsedTicks + (LONGLONG)GetTickCount() - m_nStartTick);
}

// ---- XBUtil ----

char g_strMediaPath[MAX_PATH] = "D:\\Media\\";   // 0x0026BC78

// 0x000930A7
void XBUtil_SetMediaPath(const char *path)
{
	strcpy(g_strMediaPath, path);
}

// 0x00092EEF: diffuse and ambient set to the color.
void XBUtil_InitMaterial(D3DMATERIAL8 *material, float r, float g, float b, float a)
{
	memset(material, 0, sizeof(*material));
	material->Diffuse.r = material->Ambient.r = r;
	material->Diffuse.g = material->Ambient.g = g;
	material->Diffuse.b = material->Ambient.b = b;
	material->Diffuse.a = material->Ambient.a = a;
}
