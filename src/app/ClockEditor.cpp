// CClockEditor: "Clock Settings", the date/time pop-up (Action="SetClock",
// the cold boot clock prompt, the Date & Time page). Six spinners: the date
// in the order of the Xbox language, then hour, minute and AM/PM.
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../render/D3D.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../system/SystemControl.h"
#include "Screens.h"

// 0x0026A800 (index 0 unused)
static const wchar_t s_monthNames[13][10] = {
	L"", L"January", L"February", L"March", L"April", L"May", L"June",
	L"July", L"August", L"September", L"October", L"November", L"December",
};

// The day spinner's names need redoing for the selected month and year.
// Cleared when the month (or the year with February selected) is changed;
// redone when the day spinner gets the focus.
static BOOL s_bDaysValid = TRUE;   // 0x0026CDC0

// 0x000BECCD
static BOOL IsLeapYear(unsigned year)
{
	if ((year & 3) != 0)
		return FALSE;
	if (year % 100 != 0)
		return TRUE;
	return year % 400 == 0;
}

// 0x000BECF5 (month in eax; the first stack argument is unused)
static int DaysInMonth(int month, unsigned year)
{
	switch (month)
	{
	case 1:
	case 3:
	case 5:
	case 7:
	case 8:
	case 10:
	case 12:
		return 31;
	case 2:
		return IsLeapYear(year) ? 29 : 28;
	default:
		return 30;
	}
}

// Day spinner: items from days on blank, 29..days named (the spinner skips
// the blank ones), and the selection moved into the month.
static void SetDaysInMonth(CUpDown *spinner, int days)
{
	for (int i = days; i < 31; i++)
		spinner->SetItemText(i, L"");
	for (unsigned i = 29; i <= (unsigned)days; i++)
	{
		wchar_t text[6];
		swprintf(text, 6, L"%d", i);
		spinner->SetItemText(i - 1, text);
	}
	int last = days - 1;
	if (spinner->m_nSelected > last && last >= 0 && last < (int)spinner->m_items.size())
		spinner->m_nSelected = last;
}

static void AddNumbers(CUpDown *spinner, int first, int last)
{
	for (int i = first; i <= last; i++)
	{
		wchar_t text[6];
		swprintf(text, 6, L"%d", i);
		spinner->AddItem(text);
	}
}

// Days 1..days, then blanks up to 31 items.
static void AddDays(CUpDown *spinner, int days)
{
	AddNumbers(spinner, 1, days);
	if (days + 1 < 32)
	{
		for (int n = 32 - (days + 1); n != 0; n--)
			spinner->AddItem(L"");
	}
}

// 0x000C66A7
CClockEditor::CClockEditor()
{
	m_nMonthSpinner = 0;
	m_bInitialized = FALSE;
	m_pFont = NULL;
	m_pButtonFont = NULL;
	m_pSoundMove = NULL;
	m_bOpen = FALSE;
	m_pSoundAccept = NULL;
	m_pSoundBack = NULL;
}

// 0x000C147B
CClockEditor::~CClockEditor()
{
	m_pSoundMove = NULL;
	m_pFont = NULL;
	m_pButtonFont = NULL;
	m_pSoundAccept = NULL;
	m_pSoundBack = NULL;
}

// 0x000C1FAC
HRESULT CClockEditor::Init(CXBFontEx *font)
{
	g_fonts.Find("MainFont", &m_pFont);
	g_fonts.Find("XboxDings", &m_pButtonFont);
	if (m_pFont == NULL)
		m_pFont = font;
	if (m_bInitialized || m_pFont == NULL)
		return E_FAIL;

	m_nFocus = 0;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	m_pFont->GetTextWidth(L"WW");   // (unused)
	SYSTEMTIME now;
	GetLocalTime(&now);
	int days = DaysInMonth(now.wMonth, now.wYear);
	swprintf(m_szDateSeparator, 2, L"%s", L"/");

	// The date order. Years are 2000-2099.
	switch (XGetLanguage())
	{
	case XC_LANGUAGE_JAPANESE:
	case XC_LANGUAGE_KOREAN:
	case XC_LANGUAGE_TCHINESE:
		// y/m/d
		m_nYearSpinner = 0;
		m_nMonthSpinner = 1;
		m_nDaySpinner = 2;
		AddNumbers(&m_spinners[0], 2000, 2099);
		AddNumbers(&m_spinners[1], 1, 12);
		AddDays(&m_spinners[2], days);
		break;

	case XC_LANGUAGE_GERMAN:
	case XC_LANGUAGE_ITALIAN:
		// d.m.y
		swprintf(m_szDateSeparator, 2, L"%s", L".");
		// fall through
	case XC_LANGUAGE_FRENCH:
	case XC_LANGUAGE_SPANISH:
	case XC_LANGUAGE_PORTUGUESE:
		// d/m/y
		m_nDaySpinner = 0;
		m_nMonthSpinner = 1;
		m_nYearSpinner = 2;
		AddDays(&m_spinners[0], days);
		AddNumbers(&m_spinners[1], 1, 12);
		AddNumbers(&m_spinners[2], 2000, 2099);
		break;

	default:
		// m/d/y
		m_nMonthSpinner = 0;
		m_nDaySpinner = 1;
		m_nYearSpinner = 2;
		AddNumbers(&m_spinners[0], 1, 12);
		AddDays(&m_spinners[1], days);
		AddNumbers(&m_spinners[2], 2000, 2099);
		break;
	}
	for (int i = 1; i < 13; i++)
	{
		wchar_t text[6];
		swprintf(text, 6, L"%02d", i);
		m_spinners[SPINNER_HOUR].AddItem(text);
	}
	for (int i = 0; i < 60; i++)
	{
		wchar_t text[6];
		swprintf(text, 6, L"%02d", i);
		m_spinners[SPINNER_MINUTE].AddItem(text);
	}
	m_spinners[SPINNER_AMPM].AddItem(L"AM");
	m_spinners[SPINNER_AMPM].AddItem(L"PM");

	// The layout, centered (Show redoes it with the font scale).
	float y = lineHeight + 80.0f;
	ScaleToScreen(NULL, NULL, &y, NULL, FALSE);
	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float width = m_pFont->GetTextWidth(L"// :   ") + 8.0f;
	for (int i = 0; i < 6; i++)
	{
		m_spinners[i].Create(NULL, 100.0f, y, 0.0f, 0.0f);
		width = m_spinners[i].m_fWidth + width;
	}
	m_spinners[0].m_bFocused = TRUE;
	D3DDISPLAYMODE mode;
	g_pd3dDevice->GetDisplayMode(&mode);
	float x = ((float)mode.Width - width) * 0.5f;
	m_fPositions[0] = x;
	m_spinners[0].m_fX = x;
	static const wchar_t *const separators[5] = {NULL, NULL, L"  ", L":", L" "};
	for (int i = 0; i < 5; i++)
	{
		m_fPositions[i * 2 + 1] = m_fPositions[i * 2] + m_spinners[i].m_fWidth + 2.0f;
		const wchar_t *separator = separators[i] != NULL ? separators[i] : m_szDateSeparator;
		m_fPositions[i * 2 + 2] = m_pFont->GetTextWidth(separator) + m_fPositions[i * 2 + 1] + 2.0f;
		m_spinners[i + 1].m_fX = m_fPositions[i * 2 + 2];
	}
	m_fY = 40.0f;
	m_fX = m_fPositions[0] - 30.0f;
	m_fWidth = width + 60.0f;
	m_fHeight = m_spinners[0].m_fHeight + y + 80.0f;

	m_pSoundMove = g_soundEffects.Find("Scroll");
	m_pSoundAccept = g_soundEffects.Find("Select");
	m_pSoundBack = g_soundEffects.Find("Back");
	Show();
	m_bInitialized = TRUE;
	return S_OK;
}

// 0x000C0B13
void CClockEditor::Show()
{
	m_bOpen = TRUE;
	SYSTEMTIME now;
	GetLocalTime(&now);

	CUpDown *spinner = &m_spinners[m_nMonthSpinner];
	int value = now.wMonth - 1;
	if (value >= 0 && value < (int)spinner->m_items.size())
		spinner->m_nSelected = value;
	spinner = &m_spinners[m_nDaySpinner];
	value = now.wDay - 1;
	if (value >= 0 && value < (int)spinner->m_items.size())
		spinner->m_nSelected = value;
	spinner = &m_spinners[m_nYearSpinner];
	value = now.wYear - 2000;
	if (value >= 0 && value < (int)spinner->m_items.size())
		spinner->m_nSelected = value;

	// 12-hour clock. (Before noon the AM/PM spinner is left as it was.)
	CUpDown *hour = &m_spinners[SPINNER_HOUR];
	if (now.wHour >= 12)
	{
		value = now.wHour > 12 ? now.wHour - 13 : now.wHour - 1;
		if (value >= 0 && value < (int)hour->m_items.size())
			hour->m_nSelected = value;
		if ((int)m_spinners[SPINNER_AMPM].m_items.size() > 1)
			m_spinners[SPINNER_AMPM].m_nSelected = 1;
	}
	else if (now.wHour > 0)
	{
		value = now.wHour - 1;
		if (value < (int)hour->m_items.size())
			hour->m_nSelected = value;
	}
	else if ((int)hour->m_items.size() > 11)
	{
		hour->m_nSelected = 11;
	}
	value = now.wMinute;
	if (value < (int)m_spinners[SPINNER_MINUTE].m_items.size())
		m_spinners[SPINNER_MINUTE].m_nSelected = value;

	SetDaysInMonth(&m_spinners[m_nDaySpinner], DaysInMonth(now.wMonth, now.wYear));
	// (The hour is the 24-hour one here.)
	swprintf(m_szTime, 30, L"%s %d, %d  %02d:%02d %s", s_monthNames[now.wMonth], now.wDay, now.wYear, now.wHour,
	         now.wMinute, m_spinners[SPINNER_AMPM].GetText());

	// The layout, centered.
	m_fY = 40.0f;
	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	ScaleToScreen(NULL, &scaleX, &m_fY, &scaleY, FALSE);
	if (1.0f > scaleX)
		scaleX = 1.0f;
	if (1.0f > scaleY)
		scaleY = 1.0f;
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float spinnerY = lineHeight + scaleY * 40.0f + m_fY;
	float width = m_pFont->GetTextWidth(L"// :   ") + 8.0f;
	for (int i = 0; i < 6; i++)
	{
		m_spinners[i].m_fY = spinnerY;
		width = m_spinners[i].GetBoxWidth() + width;
	}
	float screenWidth;
	GetScreenSize(&screenWidth, NULL);
	m_fPositions[0] = (screenWidth - width) * 0.5f + (float)g_pSettings->m_viewport.X;
	m_spinners[0].m_fX = m_fPositions[0];
	static const wchar_t *const separators[5] = {NULL, NULL, L"  ", L":", L" "};
	for (int i = 0; i < 5; i++)
	{
		m_fPositions[i * 2 + 1] = m_spinners[i].GetBoxWidth() + m_fPositions[i * 2] + 2.0f;
		const wchar_t *separator = separators[i] != NULL ? separators[i] : m_szDateSeparator;
		m_fPositions[i * 2 + 2] = m_pFont->GetTextWidth(separator) + m_fPositions[i * 2 + 1] + 2.0f;
		m_spinners[i + 1].m_fX = m_fPositions[i * 2 + 2];
	}
	m_fX = m_spinners[0].m_fX - scaleX * 30.0f;
	m_fWidth = scaleX * 60.0f + width;
	float buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	if (!(buttonHeight > m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY))
		buttonHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	m_fHeight = m_spinners[0].GetBoxHeight() + m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY * 2.0f +
	            scaleY * 20.0f + m_fY + buttonHeight;
}

// 0x000C1102
HRESULT CClockEditor::ProcessInput(int *action, SGamepadState **pad)
{
	BOOL changed = FALSE;
	switch (*action)
	{
	case INPUT_A:
	{
		// Sets the clock (the seconds are kept).
		SYSTEMTIME time;
		GetSystemTime(&time);
		time.wDay = (WORD)(m_spinners[m_nDaySpinner].m_nSelected + 1);
		time.wMonth = (WORD)(m_spinners[m_nMonthSpinner].m_nSelected + 1);
		time.wYear = (WORD)(m_spinners[m_nYearSpinner].m_nSelected + 2000);
		int hour = m_spinners[SPINNER_HOUR].m_nSelected;
		if (m_spinners[SPINNER_AMPM].m_nSelected == 1)
			time.wHour = (WORD)(hour < 11 ? hour + 13 : 12);
		else
			time.wHour = (WORD)(hour == 11 ? 0 : hour + 1);
		if (time.wHour == 24)
			time.wHour = 0;
		time.wMinute = (WORD)m_spinners[SPINNER_MINUTE].m_nSelected;
		SetXboxSystemTime(time, TRUE);
		m_bOpen = FALSE;
		PlaySound(m_pSoundAccept);
		*action = INPUT_NONE;
		return S_OK;
	}

	case INPUT_B:
	case INPUT_BACK:
		Show();   // puts the spinners back to the current time
		m_bOpen = FALSE;
		PlaySound(m_pSoundBack);
		*action = INPUT_NONE;
		return S_OK;

	case INPUT_LEFT:
		m_spinners[m_nFocus].m_bFocused = FALSE;
		m_nFocus = m_nFocus == 0 ? 5 : m_nFocus - 1;
		m_spinners[m_nFocus].m_bFocused = TRUE;
		PlaySound(m_pSoundMove);
		break;

	case INPUT_RIGHT:
		m_spinners[m_nFocus].m_bFocused = FALSE;
		m_nFocus = m_nFocus == 5 ? 0 : m_nFocus + 1;
		m_spinners[m_nFocus].m_bFocused = TRUE;
		PlaySound(m_pSoundMove);
		break;

	case INPUT_LTRIGGER:
	case INPUT_RTRIGGER:
	case INPUT_UP:
	case INPUT_DOWN:
	case INPUT_LTHUMB:
		if (m_nFocus == m_nMonthSpinner ||
		    (m_nFocus == m_nYearSpinner && m_spinners[m_nMonthSpinner].m_nSelected == 1))
			s_bDaysValid = FALSE;
		changed = TRUE;
		break;
	}

	if (!s_bDaysValid && m_nFocus == m_nDaySpinner)
	{
		int month = _wtoi(m_spinners[m_nMonthSpinner].GetText());
		int year = _wtoi(m_spinners[m_nYearSpinner].GetText());
		SetDaysInMonth(&m_spinners[m_nDaySpinner], DaysInMonth(month, year));
		s_bDaysValid = TRUE;
	}
	m_spinners[m_nFocus].ProcessInput(action, pad);

	if (changed)
	{
		SYSTEMTIME time;
		GetLocalTime(&time);
		time.wDay = (WORD)(m_spinners[m_nDaySpinner].m_nSelected + 1);
		time.wMonth = (WORD)(m_spinners[m_nMonthSpinner].m_nSelected + 1);
		time.wYear = (WORD)(m_spinners[m_nYearSpinner].m_nSelected + 2000);
		time.wMinute = (WORD)m_spinners[SPINNER_MINUTE].m_nSelected;
		time.wHour = (WORD)(m_spinners[SPINNER_HOUR].m_nSelected + 1);
		swprintf(m_szTime, 30, L"%s %d, %d  %02d:%02d %s", s_monthNames[time.wMonth], time.wDay, time.wYear,
		         time.wHour, time.wMinute, m_spinners[SPINNER_AMPM].GetText());
	}
	return S_OK;
}

// 0x000BFC70
HRESULT CClockEditor::Render()
{
	if (!m_bInitialized)
		return E_FAIL;

	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	float buttonHeight = lineHeight;
	if (!(buttonHeight > m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY))
		buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	DrawWindowFrame(m_fX, m_fY, m_fWidth, m_fHeight, 10.0f, lineHeight, buttonHeight + 10.0f,
	                g_colorScheme.DIALOG_BORDERCOLOR, g_colorScheme.DIALOG_BACKCOLOR,
	                g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 2, 2);
	for (int i = 0; i < 6; i++)
		m_spinners[i].Render();

	float textY = m_spinners[0].GetBoxHeight() * 0.5f + m_spinners[0].m_fY;
	m_pFont->Begin();
	m_pFont->DrawText(m_fX + 10.0f, (lineHeight + 10.0f) * 0.5f + m_fY, g_colorScheme.CAPTION_TEXTCOLOR,
	                  L"Clock Settings", XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_fPositions[1], textY, g_colorScheme.DIALOG_TEXTCOLOR, m_szDateSeparator, XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_fPositions[3], textY, g_colorScheme.DIALOG_TEXTCOLOR, m_szDateSeparator, XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_fPositions[7], textY, g_colorScheme.DIALOG_TEXTCOLOR, L":", XBFONT_CENTER_Y, 0.0f);
	// The time as set so far, under the spinners.
	m_pFont->DrawText(m_spinners[0].m_fX, m_spinners[0].GetBoxHeight() + m_spinners[0].m_fY + 10.0f,
	                  g_colorScheme.DIALOG_TEXTCOLOR, m_szTime, 0, m_fWidth);
	m_pFont->End();

	float glyphWidth = m_pButtonFont->GetTextWidth(L"A") + 5.0f;
	buttonHeight = lineHeight;
	if (!(buttonHeight > m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY))
		buttonHeight = m_pButtonFont->m_fFontHeight * m_pButtonFont->m_fScaleFactorY;
	float buttonY = (m_fY + m_fHeight - 5.0f) - buttonHeight * 0.5f;
	m_pButtonFont->DrawText(m_spinners[0].m_fX, buttonY, 0xFFFFFFFF, L"A", XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_spinners[0].m_fX + glyphWidth, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Accept",
	                  XBFONT_CENTER_Y, 0.0f);
	m_pButtonFont->DrawText(m_fWidth * 0.5f + m_fX, buttonY, 0xFFFFFFFF, L"B", XBFONT_CENTER_Y, 0.0f);
	m_pFont->DrawText(m_fWidth * 0.5f + m_fX + glyphWidth, buttonY, g_colorScheme.DIALOG_TEXTCOLOR, L"Cancel",
	                  XBFONT_CENTER_Y, 0.0f);
	return S_OK;
}
