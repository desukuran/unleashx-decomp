#include "Skin.h"

#include <cstring>
#include <cwchar>

#include "../media/WMVideoPlayer.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "SkinColors.h"

CSkin g_skin;                 // 0x00277968
BOOL g_bSkinGameIcon;         // 0x00277E70
static int g_nSkinUnknown;    // 0x00276244

// TODO: decompile the real constructor (a static initializer); this only
// gives the members a defined state.
CSkin::CSkin()
	: m_bZipped(FALSE), m_pFonts(NULL), m_pSettings(NULL), m_pEventSink(NULL), m_pBackground(NULL),
	  m_pOrbTexture(NULL), m_pCellwall(NULL), m_pBackgroundVideo(NULL), m_pPreviewVideo(NULL), m_pMenuFont(NULL),
	  m_pInfoFont(NULL), m_bOwnMenuFont(FALSE), m_bOwnInfoFont(FALSE), m_pMusicPlayerBackground(NULL),
	  m_pPlayListBackground(NULL), m_pButtonMap(NULL), m_pDialogBackground(NULL), m_pLoadingTexture(NULL),
	  m_bLoading(FALSE), m_hLoadThread(NULL), m_bChanged(FALSE), m_bLoaded(FALSE), m_pFileManagerBackground(NULL),
	  m_pLoadingBackground(NULL)
{
}

// 0x000F9852
CSkin::~CSkin()
{
	Free();
	if (m_hLoadThread != NULL && m_hLoadThread != (HANDLE)-1)
	{
		WaitForSingleObject(m_hLoadThread, INFINITE);
		CloseHandle(m_hLoadThread);
		m_hLoadThread = NULL;
	}
	ReleaseTexture(m_pLoadingTexture);
}

// 0x000F9166
void CSkin::SetDefaults()
{
	m_pSettings->m_nSkinHeight = 480;
	m_pSettings->m_nSkinWidth = 640;
	m_unknown004 = 0;

	// Everything from the menu rectangle up to the game icon.
	memset(&m_rcMenu, 0, (BYTE *)&m_rcGameIcon - (BYTE *)&m_rcMenu + sizeof(m_rcGameIcon));
	m_rcMenu.top = 75;
	m_rcMenu.left = 340;
	m_rcMenu.width = 300;
	m_rcMenu.height = 330;
	m_bFixed = FALSE;
	m_bRotate = FALSE;
	m_bScroll = FALSE;
	m_dwBackColor = g_dwSkinBackColor;
	m_dwBorderColor = g_dwSkinBorderColor;
	m_bSmooth = TRUE;
	m_nAlign = 0;
	m_bShowIcon = TRUE;
	m_bShowArrows = TRUE;
	m_bPreview = FALSE;
	m_bPreviewAudio = TRUE;
	m_bPreviewMusic = FALSE;
	m_bHorizontal = FALSE;
	m_nItemPosCount = 0;
	m_nMaxItem = 0;
	m_unknown508 = 0;
	m_fGameIconOpacity = 1.0f;
	m_bGlobe = TRUE;
	m_bOrb = TRUE;
	m_bFog = TRUE;
	m_bOwnMenuFont = FALSE;
	m_bOwnInfoFont = FALSE;

	memset(m_szAuthor, 0, sizeof(m_szAuthor));
	m_strSkinFile.clear();
	swprintf(m_szName, 0x50, L"%ls", L"Default");
	memset(m_szVersion, 0, sizeof(m_szVersion));

	g_nSkinUnknown = 0;
	m_bLoadingShowInfo = TRUE;
	m_fMenuFontScaleX = 1.0f;
	m_fMenuFontScaleY = 1.0f;
	m_bFileManagerSafeBorder = TRUE;
	m_bFileManagerDrawBorder = FALSE;
	m_bSortByZIndex = FALSE;
	m_pMusicPlayerBackground = NULL;
	m_pPlayListBackground = NULL;
	m_pButtonMap = NULL;
	m_pDialogBackground = NULL;

	m_rcMusicPlayer.width = 410;
	m_rcPlayList.width = 410;
	m_rcMusicPlayer.left = 115;
	m_rcPlayList.left = 115;
	m_nPlayListOffsetX = 5;
	m_nPlayListOffsetX2 = 0;
	m_nPlayListOffsetY = 5;
	m_nPlayListOffsetY2 = 0;
	m_nDialogOffsetX = 5;
	m_nDialogOffsetX2 = 0;
	m_nDialogOffsetY = 5;
	m_nDialogOffsetY2 = 0;
	m_bMusicPlayerRelative = FALSE;
	m_rcMusicPlayer.top = 40;
	m_rcMusicPlayer.height = 135;
	m_rcPlayList.top = 176;
	m_rcPlayList.height = 264;

	// No music-player buttons yet.
	int *buttons = &m_nButtonPrev;
	for (int i = 0; i < 11; i++)
		buttons[i] = 0xFF;
}

static void FreeTexts(std::vector<SSkinText> &texts)
{
	for (size_t i = 0; i < texts.size(); i++)
	{
		if (texts[i].text != NULL)
		{
			delete[] texts[i].text;
			texts[i].text = NULL;
		}
	}
	texts.clear();
}

static void FreeImages(std::vector<SSkinImage> &images)
{
	for (size_t i = 0; i < images.size(); i++)
		ReleaseTexture(images[i].texture);
	images.clear();
}

// 0x000F9372
void CSkin::Free()
{
	m_strIntroMovie.clear();
	m_strLoadingPreview.clear();
	if (m_pBackgroundVideo != NULL)
	{
		delete m_pBackgroundVideo;
		m_pBackgroundVideo = NULL;
	}
	if (m_pPreviewVideo != NULL)
	{
		delete m_pPreviewVideo;
		m_pPreviewVideo = NULL;
	}
	ReleaseTexture(m_pBackground);
	ReleaseTexture(m_pOrbTexture);
	ReleaseTexture(m_pCellwall);
	ReleaseTexture(m_pFileManagerBackground);
	m_strFileManagerBackground.clear();
	ReleaseTexture(m_pLoadingBackground);
	m_strLoadingBackground.clear();

	m_mainShapes.clear();
	FreeTexts(m_mainTexts);
	FreeImages(m_mainImages);

	if (m_bOwnMenuFont && m_pMenuFont != NULL)
		delete m_pMenuFont;   // CXBFont::Destroy + delete (0x00098FFE, 0x000269E1)
	m_bOwnMenuFont = FALSE;
	m_pMenuFont = NULL;
	if (m_bOwnInfoFont && m_pInfoFont != NULL)
		delete m_pInfoFont;

	m_musicShapes.clear();
	FreeTexts(m_musicTexts);
	FreeImages(m_musicImages);

	if (m_pFonts != NULL)
	{
		delete m_pFonts;
		m_pFonts = NULL;
	}

	ReleaseTexture(m_pMusicPlayerBackground);
	ReleaseTexture(m_pPlayListBackground);
	ReleaseTexture(m_pButtonMap);
	ReleaseTexture(m_pDialogBackground);
	// Also resets the music player / playlist / dialog layout.
	memset(&m_pMusicPlayerBackground, 0, (BYTE *)&m_bMusicPlayerRelative - (BYTE *)&m_pMusicPlayerBackground + 4);

	// (The original repeats the music-player cleanup here; it's a no-op by now.)
	m_mainOrder.clear();
	m_musicOrder.clear();
	m_bOwnInfoFont = FALSE;
	m_pInfoFont = NULL;
	m_bColorScheme = FALSE;
}

// 0x000F8E2B
// Called each frame: reports a finished background load, runs the preview
// timer and lets the skin's videos decode.
HRESULT CSkin::PollLoad(BOOL *busy)
{
	if (!m_bChanged)
	{
		if (m_bLoading)
		{
			*busy = FALSE;
			return S_OK;
		}
	}
	else
	{
		if (m_pEventSink != NULL)
			m_pEventSink->OnSkinLoaded();
		m_bChanged = FALSE;
		m_bLoading = FALSE;
		*busy = FALSE;
	}

	if (m_bPreview)
	{
		if (m_dwPreviewTick != 0 && m_dwPreviewTick < GetTickCount())
		{
			m_dwPreviewTick = 0;
			if (m_pEventSink != NULL)
				m_pEventSink->OnPreviewTimer();
		}
		if (m_pPreviewVideo != NULL && m_bPreviewPlaying)
			m_pPreviewVideo->Update(busy, 0);
	}
	if (m_pBackgroundVideo != NULL)
		m_pBackgroundVideo->Update(busy, 0);
	return S_OK;
}
