// Built-in layouts used when a skin has no <Main> or no <MusicPlayer>.
#include <algorithm>

#include "../render/Fonts.h"
#include "../settings/AppSettings.h"
#include "../system/Drives.h"
#include "Skin.h"
#include "SkinColors.h"

static void AddText(std::vector<SSkinText> &texts, int left, int top, int width, int source, DWORD align)
{
	texts.push_back(MakeSkinTextPublic(left, top, width, 0, source, 0xFFFFFFFF, FALSE, 100, 100, 0, NULL, align));
}

// 0x000FCEA6
// The "[Default]" skin: no images, a column of system info on the left.
void CSkin::SetDefaultMainLayout()
{
	Free();
	m_bZipped = FALSE;
	SetDefaults();

	m_rcMenu.left = 330;
	m_rcMenu.top = 75;
	m_rcMenu.width = 290;
	m_rcMenu.height = 330;
	static const int kItemPos[5][2] = { {285, 96}, {310, 138}, {315, 180}, {310, 222}, {290, 264} };
	for (int i = 0; i < 5; i++)
	{
		m_itemPos[i].x = kItemPos[i][0];
		m_itemPos[i].y = kItemPos[i][1];
	}
	m_nItemPosCount = 5;
	m_bGlobe = TRUE;
	m_bOrb = TRUE;
	m_bFog = TRUE;
	m_bOwnMenuFont = FALSE;
	m_bOwnInfoFont = FALSE;
	g_fonts.Find("InfoFont", &m_pInfoFont);
	g_fonts.Find("MenuFont", (CXBFont **)&m_pMenuFont);

	float lineHeight = m_pInfoFont->m_fScaleFactorY * m_pInfoFont->m_fFontHeight + 3.0f;
	int line = (int)lineHeight;
	int y;

	AddText(m_mainTexts, 10, 10, 0, TEXT_DATETIME, SKIN_ALIGN_LEFT);
	y = line + 15;
	AddText(m_mainTexts, 10, y, 0, TEXT_C_FREEMB, SKIN_ALIGN_LEFT);
	y += 5 + line;
	AddText(m_mainTexts, 10, y, 0, TEXT_E_FREEMB, SKIN_ALIGN_LEFT);
	SDriveInfo *drive = GetDriveInfo('F');
	if (drive != NULL && IsDriveInfoMounted(drive))
	{
		y += 5 + line;
		AddText(m_mainTexts, 10, y, 0, TEXT_F_FREEMB, SKIN_ALIGN_LEFT);
		drive = GetDriveInfo('G');
		if (drive != NULL && IsDriveInfoMounted(drive))
		{
			y += 5 + line;
			AddText(m_mainTexts, 10, y, 0, TEXT_G_FREEMB, SKIN_ALIGN_LEFT);
		}
	}

	int bottom = (int)(470.0f - (lineHeight + lineHeight));
	AddText(m_mainTexts, 10, bottom, 0, TEXT_CPUTEMP, SKIN_ALIGN_LEFT);
	AddText(m_mainTexts, 10, bottom + line, 0, TEXT_MBTEMP, SKIN_ALIGN_LEFT);
	AddText(m_mainTexts, 630, bottom, 0, TEXT_IP, SKIN_ALIGN_RIGHT);
	AddText(m_mainTexts, 630, bottom + line, 0, TEXT_NUMCLIENT, SKIN_ALIGN_RIGHT);
	AddText(m_mainTexts, 630, 10, 0, TEXT_TRAY, SKIN_ALIGN_RIGHT);
	AddText(m_mainTexts, 630, line + 10, 0, TEXT_NICKNAME, SKIN_ALIGN_RIGHT);
	AddText(m_mainTexts, 10, 240, 250, TEXT_SONGNAME, SKIN_ALIGN_TRUNCATED);
	AddText(m_mainTexts, 10, line + 240, 250, TEXT_SONGTIME, SKIN_ALIGN_TRUNCATED);

	m_bFileManagerSafeBorder = FALSE;
	SetDefaultMusicPlayerLayout();
}

// 0x000FA219
// Music player made of shapes in the color scheme's MM* colors.
void CSkin::SetDefaultMusicPlayerLayout()
{
	m_musicOrder.clear();
	int *buttons = &m_nButtonPrev;
	for (int i = 0; i < 11; i++)
		buttons[i] = i;

	CXBFont *font = NULL;
	FindFont("InfoFont", &font);

	const SColorScheme &c = g_colorScheme;
	const SSkinShape kShapes[] = {
		// type, direction, left, top, width, height, unknown18, border, back
		{SHAPE_ROUNDEDBOX, 1, 120, 45, 400, 90, 2, c.MM_BORDERCOLOR, c.MMLightBackColor},
		{SHAPE_ROUNDEDBOX, 1, 120, 105, 400, 30, 1, c.MM_BORDERCOLOR, c.MMDarkBackColor},
		{SHAPE_STOPSTATUS, 1, 128, 55, 10, 10, 1, c.MMOffForeColor, c.MMForeColor},
		{SHAPE_PLAYSTATUS, 4, 128, 70, 10, 10, 1, c.MMOffForeColor, c.MMForeColor},
		{SHAPE_PAUSESTATUS, 1, 128, 85, 10, 10, 1, c.MMOffForeColor, c.MMForeColor},
		{SHAPE_PREV, 1, 120, 140, 30, 30, 1, 0, 0},
		{SHAPE_PLAY, 1, 150, 140, 30, 30, 1, 0, 0},
		{SHAPE_PAUSE, 1, 180, 140, 30, 30, 1, 0, 0},
		{SHAPE_STOP, 1, 210, 140, 30, 30, 1, 0, 0},
		{SHAPE_NEXT, 1, 240, 140, 30, 30, 1, 0, 0},
		{SHAPE_PREVTRACK, 1, 270, 140, 30, 30, 1, 0, 0},
		{SHAPE_NEXTTRACK, 1, 300, 140, 30, 30, 1, 0, 0},
		{SHAPE_RANDOM, 1, 330, 140, 30, 15, 1, 0, 0},
		{SHAPE_MUTE, 1, 330, 155, 30, 15, 1, 0, 0},
		{SHAPE_VOLUME, 1, 370, 155, 145, 15, 1, 0, 0},
		{SHAPE_PROGRESS, 1, 115, 126, 410, 15, 1, 0, 0},
		{0, 1, 395, 48, 120, 54, 1, c.MM_BORDERCOLOR, c.MMDarkBackColor},
		{SHAPE_METER, 1, 395, 48, 120, 54, 1, c.MMDarkBarColor, c.MMLightBarColor},
		{0, 1, 395, 54, 120, 6, 1, c.MM_BORDERCOLOR, 0},
		{0, 1, 395, 66, 120, 6, 1, c.MM_BORDERCOLOR, 0},
		{0, 1, 395, 72, 120, 6, 1, c.MM_BORDERCOLOR, 0},
		{0, 1, 395, 84, 120, 6, 1, c.MM_BORDERCOLOR, 0},
		{0, 1, 395, 96, 120, 6, 1, c.MM_BORDERCOLOR, 0},
		{0, 1, 395, 48, 120, 54, 1, c.MM_BORDERCOLOR, 0},
	};
	// Draw order: the volume meter's frame (shapes 18-22) above it, the
	// outline last.
	for (size_t i = 0; i < sizeof(kShapes) / sizeof(kShapes[0]); i++)
	{
		m_musicShapes.push_back(kShapes[i]);
		int zIndex = i < 18 ? 0 : (i < 23 ? 2 : 3);
		SSkinDrawItem item = { zIndex, (int)m_musicShapes.size() - 1, 1 };
		m_musicOrder.push_back(item);
	}

	DWORD fore = c.MMForeColor;
	const DWORD centerY = SKIN_ALIGN_CENTER_Y;
	m_musicTexts.push_back(MakeSkinTextPublic(140, 55, 110, 40, TEXT_SONGTIME, fore, FALSE, 200, 200, 0, font,
	                                          centerY | 0x10));
	m_musicTexts.push_back(MakeSkinTextPublic(265, 55, 68, 15, TEXT_SONGBITRATE, fore, FALSE, 75, 75, 0, font, centerY));
	m_musicTexts.push_back(MakeSkinTextPublic(332, 55, 68, 15, TEXT_SONGFREQUENCY, fore, FALSE, 75, 75, 0, font, centerY));
	m_musicTexts.push_back(MakeSkinTextPublic(265, 75, 68, 15, TEXT_SONGLENGTH, fore, FALSE, 75, 75, 0, font, centerY));
	m_musicTexts.push_back(MakeSkinTextPublic(332, 75, 68, 15, TEXT_SONGCHANNEL, fore, FALSE, 75, 75, 0, font, centerY));
	m_musicTexts.push_back(MakeSkinTextPublic(120, 105, 400, 30, TEXT_SONGTRACK, fore, FALSE, 100, 100, 2, font, 0));

	for (size_t i = 0; i < m_musicTexts.size(); i++)
	{
		SSkinDrawItem item = { 5, (int)i, 2 };
		m_musicOrder.push_back(item);
	}
	std::sort(m_musicOrder.begin(), m_musicOrder.end(), CompareSkinZIndex);   // 0x000274D4
}
