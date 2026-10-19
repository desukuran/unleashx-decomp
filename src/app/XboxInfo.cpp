// CXboxInfo: "System Info", the list the System page's Xbox Info item
// opens. Settings, versions, devices, drive space, network, FTP statistics
// and UnleashX's own state, one labelled line each.
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../common/XboxKernel.h"
#include "../input/Gamepad.h"
#include "../net/FtpServer.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/Skin.h"
#include "../skin/SkinColors.h"
#include "../skin/SkinText.h"
#include "../system/Drives.h"
#include "../thirdparty/XKUtils.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "SettingsPages.h"

// The lines whose values ProcessInput refreshes (see Init for the order).
enum
{
	LINE_GAMEPAD = 14,
	LINE_INFRARED = 15,
	LINE_KEYBOARD = 16,
	LINE_DRIVE_C = 18,
	LINE_DRIVE_E = 19,
	LINE_DRIVE_F = 20,
	LINE_DRIVE_G = 21,
	LINE_DRIVE_X = 22,
	LINE_DRIVE_Y = 23,
	LINE_DRIVE_Z = 24,
	LINE_DVD = 25,
	LINE_LINK_STATUS = 27,
	LINE_MAC = 28,
	LINE_IP = 29,
	LINE_SUBNET = 30,
	LINE_GATEWAY = 31,
	LINE_DNS1 = 32,
	LINE_DNS2 = 33,
	LINE_FTP_STATUS = 35,
	LINE_FTP_CURRENT = 36,
	LINE_FTP_TOTAL = 37,
	LINE_FTP_RECEIVED = 38,
	LINE_FTP_SENT = 39,
	LINE_MB_TEMP = 41,
	LINE_CPU_TEMP = 42,
	LINE_THREADS = 49,
	LINE_MEMORY = 50,
};

static DWORD s_dwNextRefresh;   // 0x00278D70
static float s_fLabelWidth;     // 0x00278D6C the widest label + 20, found once

// 0x0002DDAE
SInfoLine::SInfoLine(const std::wstring &label_, const std::wstring &value_, int id_, BOOL header_, BOOL visible_)
	: label(label_), value(value_), header(header_), id(id_), visible(visible_)
{
}

// The same code as CSettingItem::SetValue (0x000C00A2).
static void SetLineValue(SInfoLine *line, const wchar_t *text)
{
	if (line == NULL || text == NULL)
		return;
	if (line->value.compare(text) != 0)
		line->value = text;
}

static void AddLine(std::vector<SInfoLine> &lines, const wchar_t *label, const wchar_t *value, BOOL header = FALSE,
                    BOOL visible = TRUE)
{
	lines.push_back(SInfoLine(label, value, (int)lines.size(), header, visible));
}

// " 1 2 4 " style list of the ports with a device in them.
static void FormatPorts(wchar_t *text, const HANDLE *port1, const HANDLE *port2, const HANDLE *port3,
                        const HANDLE *port4)
{
	swprintf(text, 100, L"Port:%s", *port1 != NULL ? L" 1 " : L" ");
	if (*port2 != NULL)
		wcscat(text, L"2 ");
	if (*port3 != NULL)
		wcscat(text, L"3 ");
	if (*port4 != NULL)
		wcscat(text, L"4 ");
}

static double PercentFree(const SDriveInfo *info)
{
	if (info == NULL || info->totalBytes == 0)
		return 0.0;
	return (double)(LONGLONG)info->freeBytes / (double)(LONGLONG)info->totalBytes * 100.0;
}

// C, E, F, G: the skin text sources' values.
static void FormatPartition(wchar_t *text, char drive, int totalSource, int freeSource)
{
	double percent = PercentFree(GetDriveInfo(drive));
	swprintf(text, 100, L"Total: %s  Free: %s  (%.02f%s Free)", GetSkinTextValue(totalSource),
	         GetSkinTextValue(freeSource), percent, L"%");
}

// X, Y, Z (the cache partitions).
static void FormatCachePartition(wchar_t *text, char drive)
{
	const SDriveInfo *info = GetDriveInfo(drive);
	if (info == NULL)
	{
		// (Doubles for the %s: garbage, if it doesn't crash.)
		swprintf(text, 100, L"Total: %s  Free: %s  (%.02f%s Free)", 0.0, 0.0, 0.0, L"%");
	}
	else if (info->totalBytes == 0)
	{
		wcscpy(text, L"Unformatted");
	}
	else
	{
		double total = (double)(LONGLONG)info->totalBytes;
		double free = (double)(LONGLONG)info->freeBytes;
		swprintf(text, 100, L"Total: %.02f MB Free: %.02f MB  (%.02f%s Free)", total * (1.0 / 1048576.0),
		         free * (1.0 / 1048576.0), free / total * 100.0, L"%");
	}
}

// 0x000C00CC
CXboxInfo::CXboxInfo()
{
	m_nSelected = 0;
	m_pFont = NULL;
	m_pSound = NULL;
	m_nLastMove = 0;
	m_nFirstVisible = 0;
	m_nLastVisible = 0;
	m_nVisibleCount = 0;
	m_bInitialized = FALSE;
	m_fY = 0.0f;
	m_fX = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
	m_bClosed = FALSE;
	m_pBackground = NULL;
}

// 0x000BF855
CXboxInfo::~CXboxInfo()
{
	m_lines.clear();
	m_nSelected = 0;
	m_pFont = NULL;
	m_pSound = NULL;
}

// 0x000D2D87
HRESULT CXboxInfo::Init(CXBFont *font)
{
	if (m_pFont != NULL)
		return S_OK;
	m_bClosed = FALSE;
	if (font != NULL)
		m_pFont = font;
	else
		g_fonts.Find("SmallFont", &m_pFont);
	m_fY = 10.0f;
	m_fX = 20.0f;
	m_fWidth = 600.0f;
	m_fHeight = 460.0f;

	wchar_t text[100];
	AddLine(m_lines, L"Xbox Info", L"", TRUE);

	memset(text, 0, sizeof(text));
	const wchar_t *name = NULL;
	switch (XGetLanguage())
	{
	case XC_LANGUAGE_ENGLISH: name = L"English"; break;
	case XC_LANGUAGE_JAPANESE: name = L"Japanese"; break;
	case XC_LANGUAGE_GERMAN: name = L"German"; break;
	case XC_LANGUAGE_FRENCH: name = L"French"; break;
	case XC_LANGUAGE_SPANISH: name = L"Spanish"; break;
	case XC_LANGUAGE_ITALIAN: name = L"Italian"; break;
	case XC_LANGUAGE_KOREAN: name = L"Korean"; break;
	case XC_LANGUAGE_TCHINESE: name = L"Chinese (Traditional)"; break;
	case XC_LANGUAGE_PORTUGUESE: name = L"Portuguese"; break;
	}
	if (name != NULL)
		swprintf(text, 100, L"%s", name);
	AddLine(m_lines, L"Language", text);

	memset(text, 0, sizeof(text));
	name = NULL;
	switch (XGetGameRegion())
	{
	case 1: name = L"Region 1 (North America)"; break;
	case 2: name = L"Region 2 (Japan)"; break;
	case 4: name = L"Region 4 (Rest of the world)"; break;
	}
	if (name != NULL)
		swprintf(text, 100, L"%s", name);
	AddLine(m_lines, L"Game Region", text);

	memset(text, 0, sizeof(text));
	DWORD standard = XGetVideoStandard();
	if (standard == XC_VIDEO_STANDARD_NTSC_M)
		name = L"NTSC M";
	else if (standard == XC_VIDEO_STANDARD_NTSC_J)
		name = L"NTSC J";
	else if (standard == XC_VIDEO_STANDARD_PAL_I)
		name = L"PAL";
	else
		name = L"";
	swprintf(text, 100, L"%s", name);
	DWORD flags = XGetVideoFlags();
	if (flags & XC_VIDEO_FLAGS_WIDESCREEN)
		wcscat(text, L" Widescreen");
	else if (flags & XC_VIDEO_FLAGS_LETTERBOX)
		wcscat(text, L" Letter Box");
	if (flags & XC_VIDEO_FLAGS_HDTV_720p)
		wcscat(text, L" 720P");
	else if (flags & XC_VIDEO_FLAGS_HDTV_1080i)
		wcscat(text, L" 1080i");
	else if (flags & XC_VIDEO_FLAGS_HDTV_480p)
		wcscat(text, L" 480p");
	if (XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I && (flags & XC_VIDEO_FLAGS_PAL_60Hz))
		wcscat(text, L" 60Hz");
	AddLine(m_lines, L"Video Standard", text);

	memset(text, 0, sizeof(text));
	DWORD audio = XGetAudioFlags();
	name = NULL;
	switch (audio & 0xFFFF)
	{
	case 0: name = L"Stereo"; break;
	case 1: name = L"Mono"; break;
	case 2: name = L"Dolby Surround Sound"; break;
	}
	if (name != NULL)
		swprintf(text, 100, L"%s", name);
	if (audio & 0x10000)
		wcscat(text, L" Dolby Digital");
	if (audio & 0x20000)
		wcscat(text, L" DTS");
	AddLine(m_lines, L"Audio", text);

	memset(text, 0, sizeof(text));
	name = NULL;
	switch (XGetAVPack())
	{
	case XC_AV_PACK_SCART: name = L"Advanced SCART AV Pack"; break;
	case XC_AV_PACK_HDTV: name = L"High Definition AV Pack"; break;
	case XC_AV_PACK_RFU: name = L"RF Adapter"; break;
	case XC_AV_PACK_SVIDEO: name = L"Advanced AV Pack"; break;
	case XC_AV_PACK_STANDARD: name = L"Standard AV Pack"; break;
	}
	if (name != NULL)
		swprintf(text, 100, L"%s", name);
	AddLine(m_lines, L"AV Cable", text);

	memset(text, 0, sizeof(text));
	name = NULL;
	switch (XGetParentalControlSetting())
	{
	case 0: name = L"All games"; break;
	case 1: name = L"Adult"; break;
	case 2: name = L"Mature"; break;
	case 3: name = L"Teen"; break;
	case 5: name = L"Kids to Adults"; break;
	case 6: name = L"Early Childhood"; break;
	}
	if (name != NULL)
		swprintf(text, 100, L"%s", name);
	AddLine(m_lines, L"Parental Control", text);

	swprintf(text, 100, L"Region %d", g_eeprom.GetDVDRegion());
	AddLine(m_lines, L"DVD Region", text);
	AddLine(m_lines, L"Version", GetSkinTextValue(TEXT_VERSION));
	AddLine(m_lines, L"Serial Number", GetSkinTextValue(TEXT_SERIALNUM));
	AddLine(m_lines, L"HDD Key", GetSkinTextValue(TEXT_HDDKEY));
	AddLine(m_lines, L"Kernel Version", GetSkinTextValue(TEXT_KERNELVERSION));
	swprintf(text, 100, L"Flags: %d\tGPU Rev.: %d\tMCP Rev.: %d", XboxHardwareInfo->Flags,
	         XboxHardwareInfo->GpuRevision, XboxHardwareInfo->McpRevision);
	AddLine(m_lines, L"Hardware Info", text);

	// The values below are filled in by ProcessInput.
	AddLine(m_lines, L"Devices", L"", TRUE);
	AddLine(m_lines, L"Gamepad", L"");
	AddLine(m_lines, L"InfraRed", L"");
	AddLine(m_lines, L"Keyboard", L"");
	AddLine(m_lines, L"Storage", L"", TRUE);
	AddLine(m_lines, L"Drive C", L"");
	AddLine(m_lines, L"Drive E", L"");
	AddLine(m_lines, L"Drive F", L"", FALSE, g_pSettings->m_prefs.m_bEnableDriveF);
	AddLine(m_lines, L"Drive G", L"", FALSE, g_pSettings->m_prefs.m_bEnableDriveG);
	AddLine(m_lines, L"Drive X", L"");
	AddLine(m_lines, L"Drive Y", L"");
	AddLine(m_lines, L"Drive Z", L"");
	AddLine(m_lines, L"DVD Drive", L"");

	AddLine(m_lines, GetString(STR_NETWORK), L"", TRUE);
	AddLine(m_lines, GetString(STR_LINK_STATUS), GetSkinTextValue(TEXT_LINKSTATUS));
	AddLine(m_lines, GetString(STR_MAC_ADDRESS), GetSkinTextValue(TEXT_MAC));
	AddLine(m_lines, GetString(STR_IP_ADDRESS), GetSkinTextValue(TEXT_IP));
	AddLine(m_lines, GetString(STR_SUBNET_MASK), GetSkinTextValue(TEXT_SUBNET));
	AddLine(m_lines, GetString(STR_GATEWAY), GetSkinTextValue(TEXT_GATEWAY));
	AddLine(m_lines, L"DNS 1", GetSkinTextValue(TEXT_DNS1));
	AddLine(m_lines, L"DNS 2", GetSkinTextValue(TEXT_DNS2));

	AddLine(m_lines, L"FTP Statistics", L"", TRUE);
	AddLine(m_lines, GetString(STR_STATUS), g_ftpServer.IsRunning() ? L"Active" : L"Inactive");
	swprintf(text, 100, L"%d", g_ftpServer.m_nCurrentUsers);
	AddLine(m_lines, L"Current", text);
	swprintf(text, 100, L"%d", g_ftpServer.m_dwTotalConnections);
	AddLine(m_lines, L"Total", text);
	swprintf(text, 100, L"%d Bytes %d Files %d Failed", g_ftpServer.m_dwBytesReceived,
	         g_ftpServer.m_dwFilesReceived, g_ftpServer.m_dwFailedReceives);
	AddLine(m_lines, L"Received", text);
	// (Formatted, but the line gets "".)
	swprintf(text, 100, L"%d Bytes %d Files %d Failed", g_ftpServer.m_dwBytesSent, g_ftpServer.m_dwFilesSent,
	         g_ftpServer.m_dwFailedSends);
	AddLine(m_lines, L"Sent", L"");

	AddLine(m_lines, GetString(STR_MISC), L"", TRUE);
	AddLine(m_lines, L"M/B Temp", GetSkinTextValue(TEXT_MBTEMP));
	AddLine(m_lines, L"CPU Temp", GetSkinTextValue(TEXT_CPUTEMP));
	AddLine(m_lines, L"Chip ID", GetSkinTextValue(TEXT_CHIPID));

	AddLine(m_lines, L"UnleashX", L"", TRUE);
	AddLine(m_lines, L"Version", GetSkinTextValue(TEXT_APPVERSIONBUILD));
	AddLine(m_lines, L"Mode", GetSkinTextValue(TEXT_LAUNCHMODE));
	AddLine(m_lines, L"Path", GetSkinTextValue(TEXT_HOMEPATH));
	AddLine(m_lines, L"Phy. Location", GetSkinTextValue(TEXT_LAUNCHPATH));
	AddLine(m_lines, L"Threads", GetSkinTextValue(TEXT_THREAD));
	swprintf(text, 100, L"Total: %s  Free: %s", GetSkinTextValue(TEXT_TOTALMEMORY),
	         GetSkinTextValue(TEXT_FREEMEMORY));
	AddLine(m_lines, L"Memory", text);
	float width;
	float height;
	GetDisplaySize(&width, &height, FALSE);
	swprintf(text, 100, L"W%d  H%d", (int)width, (int)height);
	AddLine(m_lines, L"Video Buffer", text);

	m_pSound = g_soundEffects.Find("Scroll");
	m_bInitialized = TRUE;
	m_nLastVisible = (int)m_lines.size() - 1;
	Show();
	return S_OK;
}

// 0x000BEC78
void CXboxInfo::Show()
{
	m_fY = 10.0f;
	m_fX = 20.0f;
	m_fWidth = 600.0f;
	m_fHeight = 460.0f;
	ScaleToScreen(&m_fX, &m_fWidth, &m_fY, &m_fHeight, TRUE);
	m_bClosed = FALSE;
	m_nSelected = 0;
}

// 0x000D2200
HRESULT CXboxInfo::ProcessInput(SGamepadState **pad, int *action)
{
	if (*action == INPUT_LTHUMB && pad != NULL)
	{
		if (*pad != NULL)
		{
			if ((*pad)->fY1 == 1.0f)
				*action = INPUT_UP;
			else if ((*pad)->fY1 == -1.0f)
				*action = INPUT_DOWN;
		}
		if (*pad != NULL)
		{
			if ((*pad)->fX1 == 1.0f)
				*action = INPUT_RIGHT;
			else if ((*pad)->fX1 == -1.0f)
				*action = INPUT_LEFT;
		}
	}
	// (A copy of the selected line is made here and not used. With no lines
	// it copies from past the end.)

	switch (*action)
	{
	case INPUT_NONE:
	{
		// Idle: refresh the live values every 1.5s.
		if (GetTickCount() <= s_dwNextRefresh)
			return S_OK;
		s_dwNextRefresh = GetTickCount() + 1500;
		wchar_t text[100];
		SInfoLine *lines = &m_lines[0];
		FormatPorts(text, &g_Gamepads[0].hDevice, &g_Gamepads[1].hDevice, &g_Gamepads[2].hDevice,
		            &g_Gamepads[3].hDevice);
		SetLineValue(&lines[LINE_GAMEPAD], text);
		FormatPorts(text, &g_irRemotes[0].hDevice, &g_irRemotes[1].hDevice, &g_irRemotes[2].hDevice,
		            &g_irRemotes[3].hDevice);
		SetLineValue(&lines[LINE_INFRARED], text);
		FormatPorts(text, &g_keyboards[0].hDevice, &g_keyboards[1].hDevice, &g_keyboards[2].hDevice,
		            &g_keyboards[3].hDevice);
		SetLineValue(&lines[LINE_KEYBOARD], text);

		FormatPartition(text, 'C', TEXT_C_TOTALMB, TEXT_C_FREEMB);
		SetLineValue(&lines[LINE_DRIVE_C], text);
		FormatPartition(text, 'E', TEXT_E_TOTALMB, TEXT_E_FREEMB);
		SetLineValue(&lines[LINE_DRIVE_E], text);
		FormatPartition(text, 'F', TEXT_F_TOTALMB, TEXT_F_FREEMB);
		SetLineValue(&lines[LINE_DRIVE_F], text);
		FormatPartition(text, 'G', TEXT_G_TOTALMB, TEXT_G_FREEMB);
		SetLineValue(&lines[LINE_DRIVE_G], text);
		FormatCachePartition(text, 'X');
		SetLineValue(&lines[LINE_DRIVE_X], text);
		FormatCachePartition(text, 'Y');
		SetLineValue(&lines[LINE_DRIVE_Y], text);
		FormatCachePartition(text, 'Z');
		SetLineValue(&lines[LINE_DRIVE_Z], text);
		// (No check for a missing D: entry.)
		const SDriveInfo *dvd = GetDriveInfo('D');
		swprintf(text, 100, L"Total: %.02f MB  %s", (double)(LONGLONG)dvd->totalBytes * (1.0 / 1048576.0),
		         GetSkinTextValue(TEXT_DVDTITLE));
		SetLineValue(&lines[LINE_DVD], text);

		SetLineValue(&lines[LINE_IP], GetSkinTextValue(TEXT_IP));
		SetLineValue(&lines[LINE_LINK_STATUS], GetSkinTextValue(TEXT_LINKSTATUS));
		SetLineValue(&lines[LINE_MAC], GetSkinTextValue(TEXT_MAC));
		SetLineValue(&lines[LINE_IP], GetSkinTextValue(TEXT_IP));   // (twice)
		SetLineValue(&lines[LINE_SUBNET], GetSkinTextValue(TEXT_SUBNET));
		SetLineValue(&lines[LINE_GATEWAY], GetSkinTextValue(TEXT_GATEWAY));
		SetLineValue(&lines[LINE_DNS1], GetSkinTextValue(TEXT_DNS1));
		SetLineValue(&lines[LINE_DNS2], GetSkinTextValue(TEXT_DNS2));

		SetLineValue(&lines[LINE_FTP_STATUS], g_ftpServer.IsRunning() ? L"Active" : L"Inactive");
		swprintf(text, 100, L"%d User(s)", g_ftpServer.m_nCurrentUsers);
		SetLineValue(&lines[LINE_FTP_CURRENT], text);
		swprintf(text, 100, L"%d Connections", g_ftpServer.m_dwTotalConnections);
		SetLineValue(&lines[LINE_FTP_TOTAL], text);
		swprintf(text, 100, L"%u Bytes %d Files %d Failed", g_ftpServer.m_dwBytesReceived,
		         g_ftpServer.m_dwFilesReceived, g_ftpServer.m_dwFailedReceives);
		SetLineValue(&lines[LINE_FTP_RECEIVED], text);
		swprintf(text, 100, L"%u Bytes %d Files %d Failed", g_ftpServer.m_dwBytesSent, g_ftpServer.m_dwFilesSent,
		         g_ftpServer.m_dwFailedSends);
		SetLineValue(&lines[LINE_FTP_SENT], text);

		SetLineValue(&lines[LINE_MB_TEMP], GetSkinTextValue(TEXT_MBTEMP));
		SetLineValue(&lines[LINE_CPU_TEMP], GetSkinTextValue(TEXT_CPUTEMP));
		SetLineValue(&lines[LINE_THREADS], GetSkinTextValue(TEXT_THREAD));
		swprintf(text, 100, L"Total: %s  Free: %s", GetSkinTextValue(TEXT_TOTALMEMORY),
		         GetSkinTextValue(TEXT_FREEMEMORY));
		SetLineValue(&lines[LINE_MEMORY], text);
		return S_OK;
	}

	case INPUT_B:
	case INPUT_BACK:
	case INPUT_BLACK:
		*action = INPUT_NONE;
		m_bClosed = TRUE;
		return S_OK;

	case INPUT_LTRIGGER:
	case INPUT_UP:
	{
		if (m_lines.size() == 0 || m_nSelected == 0)
			return S_OK;
		// The previous visible line.
		int tries = 0;
		for (;;)
		{
			m_nSelected = m_nSelected == 0 ? 0 : m_nSelected - 1;
			if (tries > (int)m_lines.size())
				break;
			tries++;
			if (m_lines[m_nSelected].visible)
				break;
		}
		PlaySound(m_pSound);
		m_nLastMove = -1;
		break;
	}

	case INPUT_RTRIGGER:
	case INPUT_DOWN:
	{
		if (m_lines.size() == 0 || (unsigned)m_nSelected >= m_lines.size() - 1)
			return S_OK;
		int tries = 0;
		for (;;)
		{
			m_nSelected = m_nSelected == (int)m_lines.size() - 1 ? (int)m_lines.size() - 1 : m_nSelected + 1;
			if (tries > (int)m_lines.size())
				break;
			tries++;
			if (m_lines[m_nSelected].visible)
				break;
		}
		PlaySound(m_pSound);
		m_nLastMove = 1;
		break;
	}

	default:
		return S_OK;
	}

	// Moved: refresh on the next idle frame, and scroll to the selection.
	s_dwNextRefresh = 0;
	int selected = m_nSelected;
	if (selected < m_nFirstVisible)
	{
		m_nFirstVisible = selected;
		m_nLastVisible = m_nVisibleCount + selected - 1;
		if (m_nLastVisible > (int)m_lines.size() - 1)
		{
			m_nLastVisible = (int)m_lines.size() - 1;
			m_nFirstVisible = m_nLastVisible - m_nVisibleCount + 1;
		}
	}
	if (selected > m_nLastVisible)
	{
		m_nLastVisible = selected;
		m_nFirstVisible = selected - m_nVisibleCount + 1;
		if (m_nFirstVisible < 0)
		{
			m_nFirstVisible = 0;
			m_nLastVisible = m_nVisibleCount - 1;
		}
	}
	return S_OK;
}

// 0x000BF326
HRESULT CXboxInfo::Render()
{
	if (!m_bInitialized || m_bClosed)
		return S_OK;

	m_pFont->SetScaleFactors(g_pSettings->m_fFontScale, g_pSettings->m_fFontScale);
	DimScreen(0.3f);
	if (m_pBackground != NULL)
		DrawTexture(m_pBackground, m_fX, m_fY, m_fWidth, m_fHeight, 0.9f, NULL, 0.0f, NULL);
	else
		DrawBevelBox(m_fX, m_fY, m_fWidth, m_fHeight, g_colorScheme.DIALOG_BORDERCOLOR,
		             g_colorScheme.DIALOG_BACKCOLOR, 1.0f, 1);
	float lineHeight = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	DrawRect(m_fX + 10.0f, lineHeight + m_fY + 10.0f, m_fWidth - 20.0f, m_fHeight - 20.0f - lineHeight,
	         g_colorScheme.DIALOG_INNER_BORDERCOLOR, g_colorScheme.DIALOG_INNER_BACKCOLOR, 1.0f, 1);

	// The caption.
	float captionHeight = lineHeight + 10.0f;
	DrawGradientRect(m_fX + 10.0f, m_fY + 2.0f, m_fWidth - 20.0f, captionHeight, g_colorScheme.CAPTION_BACKCOLOR,
	                 g_colorScheme.DIALOG_BACKCOLOR, FALSE, 0);
	m_pFont->DrawText(m_fX + 12.0f, captionHeight * 0.5f + m_fY + 2.0f, g_colorScheme.CAPTION_TEXTCOLOR,
	                  L"System Info", XBFONT_CENTER_Y | XBFONT_TRUNCATED, m_fWidth - 30.0f);

	if (s_fLabelWidth == 0.0f)
	{
		for (size_t i = 0; i < m_lines.size(); i++)
		{
			float width = m_pFont->GetTextWidth(m_lines[i].label.c_str());
			if (width > s_fLabelWidth)
				s_fLabelWidth = width;
		}
		s_fLabelWidth = s_fLabelWidth + 20.0f;
	}
	float x = m_fX + 12.0f;
	float y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY * 0.5f + captionHeight + m_fY + 4.0f;
	float width = m_fWidth - 24.0f;
	if (s_fLabelWidth > width * 0.5f)
		s_fLabelWidth = width * 0.5f - 2.0f;
	float valueWidth = width - s_fLabelWidth;

	m_pFont->Begin();
	float bottom = m_fHeight + m_fY - 10.0f - m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
	for (int i = 0; i < (int)m_lines.size(); i++)
	{
		const SInfoLine &line = m_lines[i];
		if (i < m_nFirstVisible || !line.visible)
			continue;
		if (i == m_nSelected)
		{
			m_pFont->End();
			float height = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY;
			DrawGradientRect(x - 1.0f, y - height * 0.5f, width, height, g_colorScheme.OPTION_ACTIVEBACKCOLOR,
			                 g_colorScheme.DIALOG_INNER_BACKCOLOR, FALSE, g_colorScheme.OPTION_ACTIVEBACKCOLOR);
			m_pFont->Begin();
		}
		if (line.header)
		{
			m_pFont->DrawText(m_fWidth * 0.5f + m_fX, y, g_colorScheme.CAPTION_TEXTCOLOR | 0xFF000000,
			                  line.label.c_str(), XBFONT_CENTER_X | XBFONT_CENTER_Y, width);
		}
		else if (i == m_nSelected)
		{
			m_pFont->DrawText(x, y, g_colorScheme.OPTION_ACTIVETEXTCOLOR, line.label.c_str(),
			                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, s_fLabelWidth);
			m_pFont->DrawText(s_fLabelWidth + x, y, g_colorScheme.OPTION_ACTIVETEXTCOLOR, line.value.c_str(),
			                  XBFONT_CENTER_Y, valueWidth);
		}
		else
		{
			m_pFont->DrawText(x, y, g_colorScheme.OPTION_TEXTCOLOR, line.label.c_str(),
			                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, s_fLabelWidth);
			m_pFont->DrawText(s_fLabelWidth + x, y, g_colorScheme.OPTION_TEXTCOLOR, line.value.c_str(),
			                  XBFONT_CENTER_Y | XBFONT_TRUNCATED, valueWidth);
		}
		y = m_pFont->m_fFontHeight * m_pFont->m_fScaleFactorY + y + 2.0f;
		if (y > bottom)
		{
			m_nLastVisible = i;
			m_nVisibleCount = i - m_nFirstVisible + 1;
			break;
		}
	}
	m_pFont->End();
	return S_OK;
}
