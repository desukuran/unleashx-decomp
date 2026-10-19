// CSlideShow: the picture slideshow (the screen saver's "slideshow" mode and
// the file manager's). See Screens.h.
#include <cstdlib>
#include <cstring>

#include "../common/StrUtil.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Texture.h"
#include "../system/FileOps.h"
#include "Screens.h"

// What ProcessInput leaves in *action for the keys it handled (and for no
// key at all). TODO: verify what the callers make of it
#define SLIDESHOW_HANDLED 0x30

// The transitions (m_nTransition); the others are a plain cross-fade.
enum
{
	TRANSITION_WIPE_RIGHT = 1,     // grows from the left edge
	TRANSITION_WIPE_DOWN = 2,      // grows from the top
	TRANSITION_WIPE_LEFT = 3,      // grows from the right edge
	TRANSITION_GROW = 4,           // grows from the top left corner
	TRANSITION_GROW_CENTER = 5,    // grows from the middle
	TRANSITION_SLIDE_RIGHT = 6,    // slides in from the left
	TRANSITION_SLIDE_UP = 7,       // slides up from the bottom
	TRANSITION_SPIN = 8,           // spins and zooms in
	TRANSITION_STRETCH_X = 9,
	TRANSITION_STRETCH_Y = 10,
};

// A fade takes 2 seconds.
#define FADE_PER_MS 0.0005f

// 0x000F5C9D (this in ebx)
CSlideShow::CSlideShow(const char *path)
{
	Reset();
	m_strPath = _strdup(path);   // (never freed)
	HANDLE thread = (HANDLE)_beginthreadex(NULL, 0x4000, ListThread, this, 0, NULL);
	if (thread == NULL || thread == INVALID_HANDLE_VALUE)
	{
		ListThread(this);
	}
	else
	{
		Sleep(5);
		CloseHandle(thread);
	}
}

// 0x000F5602
CSlideShow::~CSlideShow()
{
	// (The list thread isn't waited for.)
	m_strPath.erase();
	Stop();
	m_files.clear();
}

// 0x000F4EB4 (this in esi)
void CSlideShow::Reset()
{
	m_strPath.erase();
	m_pTextures[0] = NULL;
	m_pTextures[1] = NULL;
	m_pLoading = NULL;
	m_nTransition = 0;
	m_nCurrent = 0;
	m_nIndex = 0;
	m_bFading = FALSE;
	m_fX[0] = 0.0f;
	m_fX[1] = 0.0f;
	m_fY[0] = 0.0f;
	m_fY[1] = 0.0f;
	GetScreenSize(&m_fWidth[0], &m_fHeight[0]);
	m_hLoader = INVALID_HANDLE_VALUE;
	m_fWidth[1] = m_fWidth[0];
	m_fHeight[1] = m_fHeight[0];
	m_bStretch = FALSE;
	m_bReady = FALSE;
	m_nDelay = 5;
	m_bTransitions = TRUE;
	m_fFixedHeight = 0.0f;
	m_fFixedWidth = 0.0f;
	m_fFixedY = 0.0f;
	m_fFixedX = 0.0f;
	m_unknownB4[0] = 0;
	m_unknownB4[1] = 0;
	m_fScale[0] = 0.0f;
	m_fScale[1] = 0.0f;
	m_bPaused = FALSE;
	m_bSkip = FALSE;
	m_bOpen = TRUE;
}

// 0x000F4DD5 (seconds in eax)
void CSlideShow::SetDelay(DWORD seconds)
{
	if (seconds > 1000)
		seconds /= 1000;
	if (seconds > 2)
		m_nDelay = seconds;
}

// 0x000F4DF1
void CSlideShow::Show()
{
	if (m_bReady)
		m_timer.Start();
	m_bOpen = TRUE;
	m_timer.Start();
}

// 0x000F4E50 (this in esi)
void CSlideShow::Stop()
{
	if (m_hLoader != INVALID_HANDLE_VALUE)
	{
		ResumeThread(m_hLoader);
		WaitForSingleObject(m_hLoader, INFINITE);
		CloseHandle(m_hLoader);
		m_hLoader = INVALID_HANDLE_VALUE;
	}
	ReleaseTexture(m_pTextures[0]);
	ReleaseTexture(m_pTextures[1]);
	ReleaseTexture(m_pLoading);
}

// 0x000F5BA1
unsigned __stdcall CSlideShow::ListThread(void *param)
{
	CSlideShow *show = (CSlideShow *)param;
	if (show == NULL)
		return 1;
	Sleep(100);
	show->m_timer.Stop();
	show->m_files.clear();

	SYSTEMTIME now;
	GetLocalTime(&now);
	srand(GetTickCount() + (now.wHour * 60 + now.wMinute) * 60 + now.wMilliseconds + now.wSecond);

	// Everything under the folder (subfolders and other files too: the
	// loader skips what isn't a picture), in random order.
	std::vector<SFileListItem> found;
	CollectFiles(show->m_strPath.c_str(), &found);
	while (found.size() != 0)
	{
		UINT pick = (UINT)rand() % (UINT)found.size();
		show->m_files.push_back(found[pick]);
		found.erase(found.begin() + pick);
	}
	found.clear();
	show->m_bReady = TRUE;
	Sleep(100);
	show->Show();
	return 0;
}

// 0x000F527E
unsigned __stdcall CSlideShow::LoaderThread(void *param)
{
	CSlideShow *show = (CSlideShow *)param;
	if (show == NULL)
		return 0;
	if (show->m_files.size() == 0)
		return 1;
	show->m_timer.Reset();
	show->m_timer.Stop();
	int next = show->m_nCurrent ^ 1;
	srand(GetTickCount());
	ReleaseTexture(show->m_pLoading);

	// Quirk: the first round starts at the second picture; the first one
	// comes up when the index wraps (which also ends the search).
	do
	{
		show->m_nIndex = (show->m_nIndex + 1) % show->m_files.size();
		SFileListItem &item = show->m_files[show->m_nIndex];
		std::string extension = StrRight(item.path, 4);
		if (StrCmpNoCase(extension.c_str(), ".jpg") == 0 || StrCmpNoCase(extension.c_str(), ".png") == 0 ||
		    StrCmpNoCase(extension.c_str(), ".bmp") == 0)
		{
			SImageInfo info;
			memset(&info, 0, sizeof(info));
			if (LoadTextureFromFile(item.path.c_str(), g_pd3dDevice, &show->m_pLoading, 1, 1, &info))
			{
				ReleaseTexture(show->m_pTextures[next]);
				show->m_pLoading->AddRef();
				show->m_pTextures[next] = show->m_pLoading;

				if (show->m_fFixedHeight > 0.0f && show->m_fFixedWidth > 0.0f)
				{
					float x = show->m_fFixedX;
					float y = show->m_fFixedY;
					float width = show->m_fFixedWidth;
					float height = show->m_fFixedHeight;
					ScaleToScreen(&x, &width, &y, &height, FALSE);
					show->m_fWidth[next] = width;
					show->m_fHeight[next] = height;
					show->m_fX[next] = x;
					show->m_fY[next] = y;
				}
				else
				{
					float screenWidth, screenHeight;
					GetScreenSize(&screenWidth, &screenHeight);
					if (show->m_bStretch)
					{
						show->m_fWidth[next] = screenWidth;
						show->m_fHeight[next] = screenHeight;
					}
					else
					{
						// As wide as the screen (small pictures are scaled
						// up too), unless that makes it too tall.
						float width = (float)(UINT)info.width;
						float height = (float)(UINT)info.height;
						show->m_fWidth[next] = width;
						show->m_fHeight[next] = height;
						if (width > screenWidth || screenWidth > width)
						{
							show->m_fWidth[next] = screenWidth;
							show->m_fHeight[next] = height * screenWidth / width;
						}
						if (show->m_fHeight[next] > screenHeight)
						{
							show->m_fWidth[next] = width * screenHeight / height;
							show->m_fHeight[next] = screenHeight;
						}
					}
					// Centered.
					float originX, originY;
					GetScreenOrigin(&originX, &originY);
					float dx = (screenWidth - show->m_fWidth[next]) * 0.5f;
					if (dx < 0.0f)
						dx = 0.0f;
					float dy = (screenHeight - show->m_fHeight[next]) * 0.5f;
					show->m_fX[next] = dx + originX;
					if (dy < 0.0f)
						dy = 0.0f;
					show->m_fY[next] = dy + originY;
				}

				show->m_nCurrent = next;
				show->m_bFading = TRUE;
				ReleaseTexture(show->m_pLoading);
				show->m_nTransition = rand() % 12;
				break;
			}
		}
		Sleep(5);
	} while (show->m_nIndex != 0);

	SwitchToThread();
	Sleep(100);
	show->m_timer.Start();
	return 0;
}

// 0x000F563B (this in edi)
void CSlideShow::LoadNext()
{
	if (m_hLoader != INVALID_HANDLE_VALUE)
	{
		// Still loading the last one.
		if (WaitForSingleObject(m_hLoader, 100) != WAIT_OBJECT_0)
		{
			m_timer.Reset();
			return;
		}
		CloseHandle(m_hLoader);
		m_hLoader = INVALID_HANDLE_VALUE;
	}
	m_timer.Reset();
	m_timer.Stop();
	m_hLoader = (HANDLE)_beginthreadex(NULL, 0, LoaderThread, this, 0, NULL);
	if (m_hLoader != NULL && m_hLoader != INVALID_HANDLE_VALUE)
	{
		Sleep(5);
		return;
	}
	LoaderThread(this);
	m_hLoader = INVALID_HANDLE_VALUE;
}

// 0x000F4F6C
BOOL CSlideShow::Render()
{
	DimScreen(1.0f);
	if (!m_bFading || !m_bTransitions)
	{
		int i = m_nCurrent;
		DrawTexture(m_pTextures[i], m_fX[i], m_fY[i], m_fWidth[i], m_fHeight[i], 1.0f, NULL, 0.0f, NULL);
		g_pd3dDevice->BlockUntilVerticalBlank();
		return FALSE;
	}

	// t goes from 1 to 0 as the new picture fades in.
	int previous = m_nCurrent ^ 1;
	float t = 1.0f - m_timer.GetElapsedMilliseconds() * FADE_PER_MS;
	if (t <= 0.0f)
	{
		t = 0.0f;
		m_bFading = FALSE;
		m_timer.Start();
	}
	float previousAlpha = t < 0.05f ? 0.05f : t;

	int i = m_nCurrent;
	float x = m_fX[i];
	float y = m_fY[i];
	float width = m_fWidth[i];
	float height = m_fHeight[i];
	float angle = 0.0f;
	const float *scale = NULL;
	switch (m_nTransition)
	{
	case TRANSITION_WIPE_DOWN:
		height = (1.0f - t) * m_fHeight[i];
		break;
	case TRANSITION_WIPE_LEFT:
		x = m_fWidth[i] * t + m_fX[i];
		// fall through
	case TRANSITION_WIPE_RIGHT:
		width = (1.0f - t) * m_fWidth[i];
		break;
	case TRANSITION_GROW_CENTER:
		y = m_fHeight[i] * t * 0.5f + m_fY[i];
		x = m_fWidth[i] * t * 0.5f + m_fX[i];
		// fall through
	case TRANSITION_GROW:
		width = (1.0f - t) * m_fWidth[i];
		height = m_fHeight[i] * (1.0f - t);
		break;
	case TRANSITION_SLIDE_RIGHT:
		x = m_fX[i] - (m_fWidth[i] + m_fX[i]) * t;
		break;
	case TRANSITION_SLIDE_UP:
		y = (m_fHeight[i] + m_fY[i]) * t + m_fY[i];
		break;
	case TRANSITION_SPIN:
		angle = (1.0f - t) * 360.0f;
		m_fScale[1] = 1.0f - t;
		m_fScale[0] = 1.0f - t;
		scale = m_fScale;
		break;
	case TRANSITION_STRETCH_X:
		m_fScale[1] = 1.0f;
		m_fScale[0] = 1.0f - t;
		scale = m_fScale;
		break;
	case TRANSITION_STRETCH_Y:
		m_fScale[0] = 1.0f;
		m_fScale[1] = 1.0f - t;
		scale = m_fScale;
		break;
	}
	DrawTexture(m_pTextures[previous], m_fX[previous], m_fY[previous], m_fWidth[previous], m_fHeight[previous],
	            previousAlpha, NULL, 0.0f, NULL);
	DrawTexture(m_pTextures[i], x, y, width, height, 1.0f - t, NULL, angle, scale);
	g_pd3dDevice->BlockUntilVerticalBlank();
	return FALSE;
}

// 0x000F56B7
HRESULT CSlideShow::ProcessInput(int *action, SGamepadState ** /*pad*/)
{
	if (m_bReady && !m_bPaused)
	{
		if (m_bSkip || m_timer.GetElapsedSeconds() > (float)m_nDelay)
		{
			m_bSkip = FALSE;
			LoadNext();
		}
	}
	Sleep(m_bFading ? 50 : 10);

	int pressed = *action;
	*action = SLIDESHOW_HANDLED;
	switch (pressed)
	{
	case INPUT_A:
	case INPUT_START:
		m_bPaused = !m_bPaused;
		break;
	case INPUT_B:
	case INPUT_BACK:
		m_bOpen = FALSE;
		break;
	case INPUT_Y:
		SetTransitions(!m_bTransitions);
		break;
	case INPUT_UP:
		SetDelay(m_nDelay + 1);
		break;
	case INPUT_DOWN:
		SetDelay(m_nDelay - 1);
		break;
	case INPUT_LEFT:
		// Back one (the loader steps forward first).
		if (m_nIndex < 2)
			break;
		m_nIndex -= 2;
		m_bSkip = TRUE;
		m_bFading = FALSE;
		break;
	case INPUT_RIGHT:
		m_bSkip = TRUE;
		m_bFading = FALSE;
		break;
	default:
		if (pressed != INPUT_NONE)
			*action = pressed;
		break;
	}
	return S_OK;
}
