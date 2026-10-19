// CScreenSaver: the screen saver. Text bounces around the screen (DVD-logo
// style, a sound on each bounce) while the screen dims; or a video loops,
// or a slideshow runs. The soundtrack pauses unless <ScreenSaver BGM> is
// on.
#include <cstdlib>
#include <cwchar>

#include "../audio/MusicManager.h"
#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../media/WMVideoPlayer.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "Screens.h"

// The text's position and direction (shared: the original keeps them in
// globals, so they carry over from one screen saver run to the next).
static float s_fTextX;            // 0x0027A364 (a random start, set once)
static float s_fTextY;            // 0x0027A360
static bool s_bTextXSet;          // 0x0027A368 bit 0
static bool s_bTextYSet;          // 0x0027A368 bit 1
static float s_fStepX = 1.0f;     // 0x0026C9A4
static float s_fStepY = 1.0f;     // 0x0026C9A0
static unsigned s_nTextFade;      // 0x00278D10 0..230 subtracted from the text's alpha

// 0x00139E2C
CScreenSaver::CScreenSaver()
{
	m_bActive = TRUE;
	m_bDimmed = FALSE;
	m_pFont = NULL;
	m_strText = L"";
	m_fFontScaleX = 1.0f;
	m_fFontScaleY = 1.0f;
	m_bInitialized = FALSE;
	m_pBounceSound = NULL;
	m_bSounds = TRUE;
	m_fNextMove = 0.0f;
	m_pSlideShow = NULL;
	m_bSlideShow = FALSE;
	m_pVideo = NULL;
	m_bVideo = FALSE;
	m_bMusicWasPlaying = FALSE;
	m_pSettings = CAppSettings::GetInstance();
}

// 0x00139D64
CScreenSaver::~CScreenSaver()
{
	m_pFont = NULL;
	m_pBounceSound = NULL;
	if (m_pVideo != NULL)
	{
		delete m_pVideo;
		m_pVideo = NULL;
	}
	if (m_pSlideShow != NULL)
	{
		delete m_pSlideShow;
		m_pSlideShow = NULL;
	}
}

// 0x00139DA4
HRESULT CScreenSaver::Init(CXBFont *font)
{
	if (m_bInitialized)
		return S_OK;
	m_pFont = font;
	if (font == NULL)
		g_fonts.Find("MainFont", &m_pFont);
	if (m_pFont != NULL)
	{
		m_fFontScaleX = m_pFont->m_fScaleFactorX;
		m_fFontScaleY = m_pFont->m_fScaleFactorY;
	}
	try
	{
		m_pBounceSound = g_soundEffects.Find("Scroll");
	}
	catch (...)
	{
	}
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x00139EB2
// Quirk: a video that fails to open (or a file of another type) becomes the
// text, so the screen saver shows its path.
void CScreenSaver::SetSource(const wchar_t *source)
{
	if (source == NULL || wcslen(source) == 0)
	{
		m_strText = L"";
		return;
	}
	if (m_strText.compare(source) == 0)
		return;
	std::string path = StrFromWide(source);
	if (FileExists(path.c_str()))
	{
		std::string extension = StrRight(path, 4);
		if (StrCmpNoCase(extension.c_str(), ".wmv") == 0 || StrCmpNoCase(extension.c_str(), ".xmv") == 0)
		{
			m_pVideo = new CWMVideoPlayer;
			if (m_pVideo != NULL)
			{
				m_pVideo->SetFont(m_pFont);
				m_pVideo->SetFullScreen(TRUE);
				m_pVideo->m_bUnknownA0 = FALSE;
				m_pVideo->SetAudio(TRUE);
				m_pVideo->Open(path.c_str());
				if (m_pVideo->IsValid())
				{
					m_bVideo = TRUE;
					m_strText = source;
					return;
				}
				m_strText = L"";
				m_pVideo->Close();
				delete m_pVideo;
				m_pVideo = NULL;
			}
		}
		else if (StrCmpNoCase(extension.c_str(), ".jpg") == 0 || StrCmpNoCase(extension.c_str(), ".png") == 0 ||
		         StrCmpNoCase(extension.c_str(), ".bmp") == 0)
		{
			// A single picture: a slideshow of one.
			delete m_pSlideShow;
			m_pSlideShow = new CSlideShow(path.c_str());
			if (m_pSlideShow != NULL)
			{
				m_bSlideShow = TRUE;
				m_pSlideShow->SetTransitions(TRUE);
				m_pSlideShow->SetDelay(5);
			}
			return;
		}
	}
	else if (DirectoryExists(path.c_str()))
	{
		if (m_pSlideShow == NULL)
			m_pSlideShow = new CSlideShow(path.c_str());
		if (m_pSlideShow != NULL)
		{
			m_bSlideShow = TRUE;
			m_pSlideShow->SetTransitions(TRUE);
			m_pSlideShow->SetDelay(5);
		}
		return;
	}
	m_strText = source;
}

// 0x0013A17A
void CScreenSaver::Start()
{
	m_bActive = TRUE;
	m_bDimmed = FALSE;
	m_timer.Start();
	m_fNextMove = 0.0f;
	if (m_bSlideShow)
		m_pSlideShow->Show();
	// The bounce sound only without soundtracks...
	m_bSounds = m_pSettings->m_audio.m_nSoundTrackMode == 0;
	if (g_pMusicManager != NULL && g_pMusicManager->IsPlaying())
	{
		m_bMusicWasPlaying = g_pMusicManager->GetState() == 2;   // playing, not paused
		if (!m_pSettings->m_prefs.m_bScreenSaverBGM)
			g_pMusicManager->Pause();
		else if (m_bMusicWasPlaying)
			m_bSounds = FALSE;   // ...and not over the music
	}
	if (m_pVideo != NULL)
	{
		if (!m_pVideo->IsValid())
			m_pVideo->Reopen(StrFromWide(m_strText.c_str()).c_str());
		// Quirk: the video's sound is on only with <ScreenSaver BGM> and no
		// music playing.
		m_pVideo->SetAudio(!m_bMusicWasPlaying && m_pSettings->m_prefs.m_bScreenSaverBGM);
	}
}

// 0x00139CE3
void CScreenSaver::CloseMedia()
{
	if (m_bSlideShow)
		m_pSlideShow->Stop();
	if (m_pVideo != NULL)
		m_pVideo->Close();
}

// 0x0013A26C
void CScreenSaver::Stop()
{
	if (m_bActive)
	{
		m_bActive = FALSE;
		CloseMedia();
		if (m_bMusicWasPlaying && !m_pSettings->m_prefs.m_bScreenSaverBGM && g_pMusicManager != NULL &&
		    g_pMusicManager->IsPlaying())
			g_pMusicManager->Resume();
	}
	m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
}

// 0x00139D04
HRESULT CScreenSaver::ProcessInput(int *action, SGamepadState **pad)
{
	if (*action != 0)
	{
		// (Unlike Stop, this doesn't resume the music.)
		m_bActive = FALSE;
		if (m_pFont != NULL)
			m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
		CloseMedia();
	}
	else if (m_bVideo)
	{
		m_pVideo->Update((BOOL *)action, 0);
	}
	else if (m_bSlideShow)
	{
		int none = 0;
		m_pSlideShow->ProcessInput(&none, pad);
	}
	return S_OK;
}

// 0x001398F7
HRESULT CScreenSaver::Render()
{
	if (m_bVideo)
	{
		m_pVideo->Render();
		return S_OK;
	}
	if (m_bSlideShow)
		return m_pSlideShow->Render();

	const SViewport &screen = m_pSettings->m_viewport;
	if (!s_bTextXSet)
	{
		s_bTextXSet = true;
		s_fTextX = (float)((unsigned)rand() % screen.Width);
	}
	if (!s_bTextYSet)
	{
		s_bTextYSet = true;
		s_fTextY = (float)((unsigned)rand() % screen.Height);
	}

	// The screen dims from 30 s to 35 s; after a minute the text starts
	// fading out too.
	if (!m_bDimmed)
	{
		if (m_timer.GetElapsedSeconds() > 30.0f)
		{
			DimScreen((m_timer.GetElapsedSeconds() - 30.0f) * (1.0f / 6.0f));
			if (m_timer.GetElapsedSeconds() > 35.0f)
				m_bDimmed = TRUE;
		}
		s_nTextFade = 0;
	}
	else
	{
		DimScreen(0.9f);
		if (m_timer.GetElapsedSeconds() > 60.0f && ++s_nTextFade >= 230)
			s_nTextFade = 230;
	}

	if (m_pFont == NULL || m_strText.size() == 0)
		return S_OK;
	try
	{
		// Twice the font size.
		float scale = 2.0f;
		ScaleToScreen(NULL, &scale, NULL, NULL, FALSE);   // TODO: verify (register arguments)
		m_pFont->SetScaleFactors(scale, scale);
		float width = 0.0f;
		float height = 0.0f;
		m_pFont->GetTextExtentN(m_strText.c_str(), (int)m_strText.size(), FALSE, &width, &height);

		// A step every 10 ms, bouncing off the screen edges (the text is
		// centered on its position).
		if (m_fNextMove - m_timer.GetElapsedMilliseconds() < 0.0f)
		{
			m_fNextMove = m_timer.GetElapsedMilliseconds() + 10.0f;
			s_fTextX += s_fStepX;
			s_fTextY += s_fStepY;
			float left = (float)screen.X + width * 0.5f;
			float right = (float)screen.Width + (float)screen.X - width * 0.5f;
			if (left >= s_fTextX || right < s_fTextX)
			{
				s_fStepX = -s_fStepX;
				s_fTextX = left >= s_fTextX ? left : right;
				if (m_bSounds)
					PlaySound(m_pBounceSound);
			}
			float top = (float)screen.Y + height * 0.5f;
			float bottom = (float)screen.Height + (float)screen.Y - height * 0.5f;
			if (top >= s_fTextY || bottom < s_fTextY)
			{
				s_fStepY = -s_fStepY;
				s_fTextY = top >= s_fTextY ? top : bottom;
				if (m_bSounds)
					PlaySound(m_pBounceSound);
			}
		}
		m_pFont->DrawText(s_fTextX, s_fTextY, 0xFFFFFFFF - (s_nTextFade << 24), m_strText.c_str(),
		                  XBFONT_CENTER_X | XBFONT_CENTER_Y | XBFONT_TRUNCATED, 0.0f);
		m_pFont->SetScaleFactors(m_fFontScaleX, m_fFontScaleY);
	}
	catch (...)
	{
	}
	return S_OK;
}
