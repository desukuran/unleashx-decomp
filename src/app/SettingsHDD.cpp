// Control Panel: HDD Settings. Writes <Preference><EnableDriveF>,
// <EnableDriveG> and <DVDMediaCheck>; the rest is information.
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/StrUtil.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinText.h"
#include "../system/Drives.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"

// 0x000C1B6D
CHDDSetting::CHDDSetting()
{
	m_pLockDialog = NULL;
	m_bLockDialogOpen = FALSE;
}

// 0x000C1B3A
CHDDSetting::~CHDDSetting()
{
	FreePopups();
	if (m_pLockDialog != NULL)
	{
		delete m_pLockDialog;
		m_pLockDialog = NULL;
	}
}

// "<total>/<free>" in MB of a drive the skin texts don't cover.
static void FormatDriveSpace(wchar_t *text, const SDriveInfo *info)
{
	double total = 0.0;
	double free = 0.0;
	if (info != NULL)
	{
		total = (double)(LONGLONG)info->totalBytes * (1.0 / 1048576.0);
		free = (double)(LONGLONG)info->freeBytes * (1.0 / 1048576.0);
	}
	swprintf(text, 100, L"%.02f MB/%.02f MB", total, free);
}

static BOOL IsDriveMounted(char drive)
{
	SDriveInfo *info = GetDriveInfo(drive);
	return info != NULL && info->mounted;
}

// SMART attribute IDs and the line each is shown on.
struct SSmartLine
{
	DWORD id;
	int type;
	const wchar_t *label;
};

static const SSmartLine s_smartLines[] = {
	{ 0x01, CHDDSetting::ITEM_SMART_RAW_READ_ERROR, L"Raw Read Error" },
	{ 0x03, CHDDSetting::ITEM_SMART_SPIN_UP_TIME, L"Spin Up Time" },
	{ 0x04, CHDDSetting::ITEM_SMART_START_STOP, L"Start/Stop Count" },
	{ 0x05, CHDDSetting::ITEM_SMART_REALLOCATED, L"Reallocated Sector" },
	{ 0x07, CHDDSetting::ITEM_SMART_SEEK_ERROR, L"Seek Error Rate" },
	{ 0x09, CHDDSetting::ITEM_SMART_POWER_ON, L"Power On Time" },
	{ 0x0A, CHDDSetting::ITEM_SMART_SPIN_RETRY, L"Spin Retry" },
	{ 0x0C, CHDDSetting::ITEM_SMART_POWER_CYCLE, L"Power Cycle" },
	{ 0xC7, CHDDSetting::ITEM_SMART_CRC_ERROR, L"U-ATA CRC Error" },
	{ 0xC8, CHDDSetting::ITEM_SMART_WRITE_ERROR, L"Write Error" },
};

static const SSmartLine *FindSmartLine(DWORD id)
{
	for (size_t i = 0; i < sizeof(s_smartLines) / sizeof(s_smartLines[0]); i++)
	{
		if (s_smartLines[i].id == id)
			return &s_smartLines[i];
	}
	return NULL;
}

#define SMART_TEMPERATURE 0xC2

// 0x000D0AD7
HRESULT CHDDSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Harddisk Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	SPreferences &prefs = g_pSettings->m_prefs;
	wchar_t text[100];
	AddItem(GetString(STR_ENABLE_DRIVE_F), prefs.m_bEnableDriveF ? L"Yes" : L"No", ITEM_DRIVE_F, FALSE, TRUE);
	AddItem(GetString(STR_ENABLE_DRIVE_G), prefs.m_bEnableDriveG ? L"Yes" : L"No", ITEM_DRIVE_G, FALSE, TRUE);

	AddItem(GetString(STR_HARD_DRIVE_INFO), L"", ITEM_HDD_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_MODEL), GetSkinTextValue(TEXT_HDDMODEL), ITEM_MODEL, FALSE, FALSE);
	AddItem(GetString(STR_SERIAL_NO), GetSkinTextValue(TEXT_HDDSERIAL), ITEM_SERIAL, FALSE, FALSE);
	AddItem(GetString(STR_STATUS), GetSkinTextValue(TEXT_HDDSTATUS), ITEM_STATUS, FALSE,
	        (GetHddSecurityStatus() & HDD_SECURITY_SUPPORTED) != 0);
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_C_TOTALMB), GetSkinTextValue(TEXT_C_FREEMB));
	AddItem(L"Drive C", text, ITEM_SPACE_C, FALSE, FALSE);
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_E_TOTALMB), GetSkinTextValue(TEXT_E_FREEMB));
	AddItem(L"Drive E", text, ITEM_SPACE_E, FALSE, FALSE);
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_F_TOTALMB), GetSkinTextValue(TEXT_F_FREEMB));
	AddItem(L"Drive F", text, ITEM_SPACE_F, FALSE, FALSE, IsDriveMounted('F'));
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_G_TOTALMB), GetSkinTextValue(TEXT_G_FREEMB));
	AddItem(L"Drive G", text, ITEM_SPACE_G, FALSE, FALSE, IsDriveMounted('G'));
	FormatDriveSpace(text, GetDriveInfo('X'));
	AddItem(L"Drive X", text, ITEM_SPACE_X, FALSE, FALSE, IsDriveMounted('X'));
	FormatDriveSpace(text, GetDriveInfo('Y'));
	AddItem(L"Drive Y", text, ITEM_SPACE_Y, FALSE, FALSE, IsDriveMounted('Y'));
	FormatDriveSpace(text, GetDriveInfo('Z'));
	AddItem(L"Drive Z", text, ITEM_SPACE_Z, FALSE, FALSE, IsDriveMounted('Z'));

	AddItem(L"S.M.A.R.T", L"", ITEM_SMART_HEADER, TRUE, TRUE);
	char info[100];
	GetHddInfoText(1, info);
	StrUtf8ToWide(info, text, 100);
	AddItem(L"Info", text, ITEM_INFO, FALSE, FALSE);
	GetHddInfoText(2, info);
	StrUtf8ToWide(info, text, 100);
	AddItem(L"Transfer Mode", text, ITEM_TRANSFER_MODE, FALSE, FALSE);
	AddItem(L"Temp.", GetSkinTextValue(TEXT_HDTEMP), ITEM_TEMPERATURE, FALSE, FALSE);
	// The SMART lines in the order the disk reports the attributes. Quirk:
	// HandleInput updates them by type, which only matches this order when
	// the disk lists them in ID order without gaps.
	for (unsigned i = 0; i < 11; i++)
	{
		DWORD id, value, worst;
		GetSmartAttribute(i, &id, &value, &worst);
		if (id == SMART_TEMPERATURE)
			continue;
		const SSmartLine *line = FindSmartLine(id);
		if (line == NULL)
			continue;
		swprintf(text, 100, L"Current: %d\tMax.: %d", value, worst);
		AddItem(line->label, text, line->type, FALSE, FALSE);
	}

	AddItem(GetString(STR_DVD_DRIVE_INFO), L"", ITEM_DVD_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_DVD_DRIVE), GetSkinTextValue(TEXT_DVDMODEL), ITEM_DVD_MODEL, FALSE, FALSE);
	AddItem(GetString(STR_SERIAL_NO), GetSkinTextValue(TEXT_DVDSERIAL), ITEM_DVD_SERIAL, FALSE, FALSE);
	AddItem(GetString(STR_MEDIA_CHECK), prefs.m_bDVDMediaCheckExtended ? L"Extended" : L"Normal", ITEM_MEDIA_CHECK,
	        FALSE, TRUE);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// Every 5 seconds: drive space and SMART values.
static void RefreshInfo(CHDDSetting *page)
{
	BOOL modified = page->m_bModified;
	wchar_t text[100];
	std::vector<CSettingItem> &items = page->m_items;

	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_C_TOTALMB), GetSkinTextValue(TEXT_C_FREEMB));
	page->SetItemValue(&items[CHDDSetting::ITEM_SPACE_C], text, FALSE);
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_E_TOTALMB), GetSkinTextValue(TEXT_E_FREEMB));
	page->SetItemValue(&items[CHDDSetting::ITEM_SPACE_E], text, FALSE);
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_F_TOTALMB), GetSkinTextValue(TEXT_F_FREEMB));
	page->SetItemValue(&items[CHDDSetting::ITEM_SPACE_F], text, FALSE);
	items[CHDDSetting::ITEM_SPACE_F].m_bVisible = IsDriveMounted('F');
	swprintf(text, 100, L"%s/%s", GetSkinTextValue(TEXT_G_TOTALMB), GetSkinTextValue(TEXT_G_FREEMB));
	page->SetItemValue(&items[CHDDSetting::ITEM_SPACE_G], text, FALSE);
	items[CHDDSetting::ITEM_SPACE_G].m_bVisible = IsDriveMounted('G');
	static const char drives[] = { 'X', 'Y', 'Z' };
	for (int i = 0; i < 3; i++)
	{
		SDriveInfo *info = GetDriveInfo(drives[i]);
		FormatDriveSpace(text, info);
		page->SetItemValue(&items[CHDDSetting::ITEM_SPACE_X + i], text, FALSE);
		items[CHDDSetting::ITEM_SPACE_X + i].m_bVisible = info != NULL && info->mounted;
	}

	// Quirk: this temperature text is overwritten below without being used.
	swprintf(text, 100, L"%s", GetSkinTextValue(TEXT_HDTEMP));
	if (wcslen(text) == 0)
		wcscpy(text, L"(Not Supported)");

	for (unsigned i = 0; i < 11; i++)
	{
		DWORD id, value, worst;
		GetSmartAttribute(i, &id, &value, &worst);
		int type;
		if (id == SMART_TEMPERATURE)
		{
			// Shown in the chosen unit. Quirk: 32.5 instead of 32, and the
			// conversions only happen above 90 (to Celsius) or below 60 (to
			// Fahrenheit), guessing which unit the drive reports.
			type = CHDDSetting::ITEM_TEMPERATURE;
			if (!g_pSettings->m_prefs.m_bUseFahrenheit)
			{
				if (value > 90)
				{
					value = (DWORD)(((float)value - 32.5f) * (5.0f / 9.0f));
					worst = (DWORD)(((float)worst - 32.5f) * (5.0f / 9.0f));
				}
			}
			else if (value < 60)
			{
				value = (DWORD)((float)value * 1.8f + 32.5f);
				worst = (DWORD)((float)worst * 1.8f + 32.5f);
			}
		}
		else
		{
			const SSmartLine *line = FindSmartLine(id);
			if (line == NULL)
				continue;
			type = line->type;
		}
		swprintf(text, 100, L"Current: %d\tMax.: %d", value, worst);
		page->SetItemValue(&items[type], text, FALSE);
	}
	page->m_bModified = modified;
}

// 0x000D0159
HRESULT CHDDSetting::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (m_bLockDialogOpen)
	{
		m_pLockDialog->ProcessInput(action, pad);
		if (m_pLockDialog->IsOpen())
			return S_OK;
		if (m_pLockDialog->WasChanged())
		{
			InvalidateSkinText(TEXT_HDDSTATUS);
			SetItemValue(&m_items[m_nSelected], GetSkinTextValue(TEXT_HDDSTATUS), FALSE);
		}
		m_bLockDialogOpen = FALSE;
		return S_OK;
	}

	if (FAILED(CSettingsBase::HandleInput(action, pad)))
		return S_OK;
	if (m_bModified && m_bAskSave && Save())
	{
		m_bClosed = TRUE;
		m_bAskSave = FALSE;
		return S_OK;
	}

	CSettingItem item = m_items[m_nSelected];
	static DWORD s_dwNextRefresh = GetTickCount() + 1000;   // 0x0027A434

	switch (*action)
	{
	case INPUT_NONE:
		if (s_dwNextRefresh < GetTickCount())
		{
			s_dwNextRefresh = GetTickCount() + 5000;
			RefreshInfo(this);
		}
		return S_OK;

	case INPUT_A:
	case INPUT_START:
		break;

	default:
		// (B/BACK and up/down are handled here too, the same as in the base
		// class, which has always used them.)
		if (s_dwNextRefresh < GetTickCount())
			s_dwNextRefresh = GetTickCount() + 2000;
		return S_OK;
	}

	if (IsSelectedDisabled())
		return S_OK;
	CSettingItem *selected = &m_items[m_nSelected];
	switch (item.m_nType)
	{
	case ITEM_DRIVE_F:
	case ITEM_DRIVE_G:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;
	case ITEM_STATUS:
		if (m_pLockDialog == NULL)
		{
			m_pLockDialog = new CHddLockDialog;
			if (m_pLockDialog != NULL)
				m_pLockDialog->Init(m_pFont);
		}
		if (m_pLockDialog != NULL)
		{
			m_pLockDialog->Show();
			m_bLockDialogOpen = TRUE;
		}
		else
		{
			m_bLockDialogOpen = FALSE;
		}
		break;
	case ITEM_MEDIA_CHECK:
		SetItemValue(selected, item.m_strValue != L"Extended" ? L"Extended" : L"Normal", TRUE);
		break;
	case ITEM_SAVE:
		if (m_bModified)
			Save();
		else
			m_bClosed = TRUE;
		break;
	case ITEM_RESET:
		RevertChanges();
		break;
	case ITEM_EXIT:
		AskToSave();
		break;
	}
	PlaySound(m_pSoundSelect);
	return S_OK;
}

// 0x000C8367
HRESULT CHDDSetting::Render()
{
	CSettingsBase::Render();
	if (m_bLockDialogOpen)
		m_pLockDialog->Render();
	return S_OK;
}

// 0x000C7EB2
BOOL CHDDSetting::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	BOOL needRestart = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	SPreferences &prefs = g_pSettings->m_prefs;
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		const char *name;
		if (it->m_nType == ITEM_DRIVE_F)
			name = "EnableDriveF";
		else if (it->m_nType == ITEM_DRIVE_G)
			name = "EnableDriveG";
		else if (it->m_nType == ITEM_MEDIA_CHECK)
			name = "DVDMediaCheck";
		else
			continue;

		CXmlElement *preference = g_config.FindElement(settings, "Preference");
		CXmlElement *element = NULL;
		if (GetOrAddChildElement("Preference", settings, &preference))
		{
			element = preference->FindFirstChild(name);
			if (!GetOrAddChildElement(name, preference, &element))
				element = NULL;
		}
		if (element == NULL)
		{
			// Quirk: skipped silently for the media check; the drive G line
			// reports drive F.
			if (it->m_nType != ITEM_MEDIA_CHECK)
			{
				sprintf(message, "Unable to set Drive F enable. Error Code: %d", GetLastError());
				ok = FALSE;
			}
			continue;
		}

		if (it->m_nType == ITEM_MEDIA_CHECK)
		{
			prefs.m_bDVDMediaCheckExtended = wcscmp(L"Extended", it->m_strValue.c_str()) == 0;
			element->SetValue(prefs.m_bDVDMediaCheckExtended ? "Extended" : "Normal");
			changed = TRUE;
			continue;
		}

		// Mapped or unmapped right away.
		char drive = it->m_nType == ITEM_DRIVE_F ? 'F' : 'G';
		BOOL enable = !IsNoW(it->m_strValue.c_str());
		if (drive == 'F')
		{
			prefs.m_bEnableDriveF = enable;
			SetDriveFMapped(enable);
		}
		else
		{
			SetDriveGMapped(enable);
		}
		if (enable)
			MountDrive(drive);
		else
			UnmountDrive(drive);
		if (drive == 'G')
			prefs.m_bEnableDriveG = enable;
		element->SetValue(enable ? "Yes" : "No");
		changed = TRUE;
		needRestart = TRUE;
	}
	return FinishSave(ok, message, changed, needRestart);
}
