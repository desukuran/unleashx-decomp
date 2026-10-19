// Control Panel: Network Settings. Writes <Network>, <FTP>, <FTPClient> and
// <HTTP> (see docs/SETTINGS.md).
#include "SettingsPages.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/StrUtil.h"
#include "../net/FtpServer.h"
#include "../net/Network.h"
#include "../net/XNet.h"
#include "../render/Draw.h"
#include "../settings/AppSettings.h"
#include "../settings/ConfigLoader.h"
#include "../skin/Skin.h"
#include "../skin/SkinText.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"

// 0x000C1B1A
CNetworkSetting::~CNetworkSetting()
{
	FreePopups();
}

// Converts a narrow setting for a line. Quirk (noted where used): some
// callers pass the length without the '\0', leaving the text unterminated.
static void ToWide(const std::string &text, wchar_t *out, int length, int size)
{
	MultiByteToWideChar(CP_ACP, 0, text.c_str(), length, out, size);
}

// 0x000CF410
HRESULT CNetworkSetting::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;

	LoadSounds();
	SetTitle(L"Network Settings");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();

	SNetworkSettings &net = g_pSettings->m_network;
	SFTPSettings &ftp = g_pSettings->m_ftp;
	wchar_t text[80];

	AddItem(GetString(STR_CURRENT_IP), GetSkinTextValue(TEXT_IP), ITEM_CURRENT_IP, FALSE, TRUE);
	AddItem(GetString(STR_LINK_STATUS), GetSkinTextValue(TEXT_LINKSTATUS), ITEM_LINK_STATUS, FALSE, TRUE);
	AddItem(GetString(STR_MAC_ADDRESS), GetSkinTextValue(TEXT_MAC), ITEM_MAC_ADDRESS, FALSE, TRUE);
	AddItem(GetString(STR_ENABLE), net.m_bEnable ? L"Yes" : L"No", ITEM_ENABLE, FALSE, TRUE);
	AddItem(GetString(STR_TYPE), net.m_bDHCP ? L"DHCP" : L"Static", ITEM_TYPE, FALSE, TRUE);

	// The static address lines: the configured IP, or the one in use.
	BOOL staticAddress = net.m_bEnable && !net.m_bDHCP;
	ToWide(net.m_strIP, text, (int)net.m_strIP.size() + 1, 80);
	if (wcslen(text) == 0)
	{
		std::string ip = GetIPAddressString();
		ToWide(ip, text, (int)GetIPAddressString().size() + 1, 80);
	}
	AddItem(GetString(STR_IP_ADDRESS), text, ITEM_IP, FALSE, staticAddress);
	AddItem(GetString(STR_SUBNET_MASK), GetSkinTextValue(TEXT_SUBNET), ITEM_SUBNET, FALSE, staticAddress);
	AddItem(GetString(STR_GATEWAY), GetSkinTextValue(TEXT_GATEWAY), ITEM_GATEWAY, FALSE, staticAddress);
	AddItem(L"DNS 1", GetSkinTextValue(TEXT_DNS1), ITEM_DNS1, FALSE, TRUE);
	AddItem(L"DNS 2", GetSkinTextValue(TEXT_DNS2), ITEM_DNS2, FALSE, TRUE);

	AddItem(GetString(STR_FTP_SERVER), L"", ITEM_FTP_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ENABLE), ftp.m_bEnable ? L"Yes" : L"No", ITEM_FTP_ENABLE, FALSE, TRUE);
	ToWide(ftp.m_strUser, text, (int)ftp.m_strUser.size() + 1, 80);
	AddItem(GetString(STR_USER), text, ITEM_FTP_USER, FALSE, ftp.m_bEnable);
	// (Shown as plain text, not masked.)
	ToWide(ftp.m_strPassword, text, (int)ftp.m_strPassword.size() + 1, 80);
	AddItem(GetString(STR_PASSWORD), text, ITEM_FTP_PASSWORD, FALSE, ftp.m_bEnable);
	swprintf(text, 80, L"%d", ftp.m_nPort);
	AddItem(GetString(STR_PORT), text, ITEM_FTP_PORT, FALSE, ftp.m_bEnable);
	swprintf(text, 80, L"%d", ftp.m_nMaxUsers);
	AddItem(GetString(STR_MAX_NO_USER), text, ITEM_FTP_MAX_USERS, FALSE, ftp.m_bEnable);
	AddItem(GetString(STR_ALLOW_ANON), ftp.m_bAllowAnon ? L"Yes" : L"No", ITEM_FTP_ALLOW_ANON, FALSE, ftp.m_bEnable);
	swprintf(text, 80, L"%d", ftp.m_nMinPasvPort);
	AddItem(GetString(STR_MIN_PASV_PORT), text, ITEM_FTP_MIN_PASV_PORT, FALSE, ftp.m_bEnable);
	swprintf(text, 80, L"%d", ftp.m_nMaxPasvPort);
	AddItem(GetString(STR_MAX_PASV_PORT), text, ITEM_FTP_MAX_PASV_PORT, FALSE, ftp.m_bEnable);
	// Quirk: converted without the '\0'.
	ToWide(ftp.m_strSyst, text, (int)ftp.m_strSyst.size(), 80);
	AddItem(GetString(STR_SYST_NAME), text, ITEM_FTP_SYST, FALSE, ftp.m_bEnable);
	AddItem(GetString(STR_BOOST_MODE), ftp.m_bBoost ? L"Yes" : L"No", ITEM_FTP_BOOST, FALSE, ftp.m_bEnable);
	// Quirk: enabled with the FTP server here, but with the network in
	// SetItemEnabled.
	AddItem(GetString(STR_AUTO_DETECT), net.m_bAutoDetect ? L"Yes" : L"No", ITEM_AUTO_DETECT, FALSE, ftp.m_bEnable);

	AddItem(GetString(STR_FTP_CLIENT), L"", ITEM_CLIENT_HEADER, TRUE, TRUE);
	swprintf(text, 80, L"%d", ftp.m_nClientMinPort);
	AddItem(GetString(STR_MIN_PORT), text, ITEM_CLIENT_MIN_PORT, FALSE, TRUE);
	swprintf(text, 80, L"%d", ftp.m_nClientMaxPort);
	AddItem(GetString(STR_MAX_PORT), text, ITEM_CLIENT_MAX_PORT, FALSE, TRUE);
	AddItem(GetString(STR_DETECT_PEERS), ftp.m_bClientDetectPeers ? L"Yes" : L"No", ITEM_CLIENT_DETECT_PEERS, FALSE,
	        TRUE);

	AddItem(GetString(STR_HTTP_CONNECTION), L"", ITEM_HTTP_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_ENABLE_PROXY), net.m_bUseProxy ? L"Yes" : L"No", ITEM_PROXY_ENABLE, FALSE, TRUE);
	ToWide(net.m_strProxy, text, (int)net.m_strProxy.size() + 1, 80);
	AddItem(GetString(STR_PROXY), text, ITEM_PROXY, FALSE, net.m_bUseProxy);
	swprintf(text, 80, L"%d", net.m_nProxyPort);
	AddItem(GetString(STR_PORT), text, ITEM_PROXY_PORT, FALSE, net.m_bUseProxy);

	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE_HEADER, TRUE, TRUE);
	AddItem(GetString(STR_SAVE), L"", ITEM_SAVE, FALSE, TRUE);
	AddItem(GetString(STR_RESET), L"(Cancel changes)", ITEM_RESET, FALSE, TRUE);
	AddItem(GetString(STR_EXIT), L"", ITEM_EXIT, FALSE, TRUE);
	return S_OK;
}

// 0x000BED3F
void CNetworkSetting::SetItemEnabled(int index, BOOL enabled)
{
	switch (index)
	{
	case 0:
		SetItemEnabled(ITEM_ENABLE, g_pSettings->m_network.m_bEnable);
		SetItemEnabled(ITEM_FTP_ENABLE, g_pSettings->m_ftp.m_bEnable);
		SetItemEnabled(ITEM_PROXY_ENABLE, g_pSettings->m_network.m_bUseProxy);
		break;
	case ITEM_ENABLE:
	{
		m_items[ITEM_TYPE].m_bEnabled = enabled;
		m_items[ITEM_DNS1].m_bEnabled = enabled;
		m_items[ITEM_DNS2].m_bEnabled = enabled;
		m_items[ITEM_AUTO_DETECT].m_bEnabled = enabled;
		BOOL isStatic = enabled && _wcsicmp(m_items[ITEM_TYPE].m_strValue.c_str(), L"STATIC") == 0;
		m_items[ITEM_IP].m_bEnabled = isStatic;
		m_items[ITEM_SUBNET].m_bEnabled = isStatic;
		m_items[ITEM_GATEWAY].m_bEnabled = isStatic;
		break;
	}
	case ITEM_TYPE:
		m_items[ITEM_IP].m_bEnabled = enabled;
		m_items[ITEM_SUBNET].m_bEnabled = enabled;
		m_items[ITEM_GATEWAY].m_bEnabled = enabled;
		break;
	case ITEM_FTP_ENABLE:
		for (int i = ITEM_FTP_USER; i <= ITEM_FTP_BOOST; i++)
			m_items[i].m_bEnabled = enabled;
		break;
	case ITEM_PROXY_ENABLE:
		m_items[ITEM_PROXY].m_bEnabled = enabled;
		m_items[ITEM_PROXY_PORT].m_bEnabled = enabled;
		break;
	}
}

// The XNet address of one of the five lines, in the block in use.
static DWORD GetConfigAddress(int index)
{
	const DWORD *v2 = &g_netConfig.v2Ip;
	const DWORD *v1 = &g_netConfig.v1Ip;
	return g_netConfig.v2Tag == XNET_CONFIG_V2_TAG ? v2[index] : v1[index];
}

// 0x000C0107
void CNetworkSetting::ShowAddresses(BOOL current)
{
	wchar_t text[100];
	if (!current)
	{
		// The settings, unless the line was edited. Quirk: converted
		// without the '\0'.
		SNetworkSettings &net = g_pSettings->m_network;
		const std::string *values[] = { &net.m_strIP, &net.m_strSubnet, &net.m_strGateway, &net.m_strDNS1,
		                                &net.m_strDNS2 };
		for (int i = 0; i < 5; i++)
		{
			ToWide(*values[i], text, (int)values[i]->size(), 100);
			if (!m_items[ITEM_IP + i].m_bChanged)
				SetItemValue(&m_items[ITEM_IP + i], text, FALSE);
		}
	}
	else
	{
		for (int i = 0; i < 5; i++)
		{
			swprintf(text, 100, L"%hs", FormatIPAddress(GetConfigAddress(i)).c_str());
			SetItemValue(&m_items[ITEM_IP + i], text, FALSE);
		}
	}
}

// 0x000BEF1A
BOOL ValidateSettingValue(int index, wchar_t *text, char *message)
{
	// The range and the value put back on failure for each number line.
	unsigned minimum, maximum, fallback;
	const char *emptyMessage;
	const char *rangeMessage;
	switch (index)
	{
	case CNetworkSetting::ITEM_FTP_PORT:
		minimum = 1;
		maximum = 0xFFFF;
		fallback = 21;
		emptyMessage = "FTP Port can not be empty.";
		rangeMessage = "%d is an invalid port for FTP Server.";
		break;
	case CNetworkSetting::ITEM_FTP_MAX_USERS:
		minimum = 1;
		maximum = 10;
		fallback = 2;
		emptyMessage = "Maximum number of FTP user can not be empty.";
		rangeMessage = "Maxumum number of users allowed is %d.";
		break;
	case CNetworkSetting::ITEM_FTP_MIN_PASV_PORT:
	case CNetworkSetting::ITEM_FTP_MAX_PASV_PORT:
		minimum = 1024;
		maximum = 0xFFFF;
		fallback = index == CNetworkSetting::ITEM_FTP_MIN_PASV_PORT ? 1024 : 0xFFFF;
		emptyMessage = "PASV Port can not be empty.";
		rangeMessage = "%d is an invalid port for PASV port.";
		break;
	case CNetworkSetting::ITEM_CLIENT_MIN_PORT:
	case CNetworkSetting::ITEM_CLIENT_MAX_PORT:
		minimum = 0;
		maximum = 0xFFFF;
		// Quirk: compares with the PASV line (17), so it's always 65535.
		fallback = index != CNetworkSetting::ITEM_FTP_MIN_PASV_PORT ? 0xFFFF : 0;
		emptyMessage = "Port can not be empty.";
		rangeMessage = "%d is an invalid port for port.";
		break;
	case CNetworkSetting::ITEM_PROXY_PORT:
		minimum = 1;
		maximum = 0xFFFF;
		fallback = 1081;
		emptyMessage = "HTTP Proxy Port can not be empty.";
		rangeMessage = "%d is an invalid port for HTTP Proxy address.";
		break;
	default:
		return TRUE;
	}

	if (text == NULL || wcslen(text) == 0)
	{
		sprintf(message, "%s", emptyMessage);
		swprintf(text, 100, L"%d", fallback);
		return FALSE;
	}
	unsigned value = (unsigned)_wtol(text);
	if (value >= minimum && value <= maximum)
		return TRUE;
	swprintf(text, 100, L"%d", fallback);
	sprintf(message, rangeMessage, value);
	return FALSE;
}

// 0x000CEC23
HRESULT CNetworkSetting::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (g_virtualKeyboard.m_bCancelled == 0)
		{
			wchar_t text[260];
			char message[260];
			wcscpy(text, g_virtualKeyboard.GetText());
			if (ValidateSettingValue(m_nSelected, text, message))
			{
				// Numbers as checked; the text lines as typed.
				switch (m_nSelected)
				{
				case ITEM_FTP_PORT:
				case ITEM_FTP_MAX_USERS:
				case ITEM_FTP_MIN_PASV_PORT:
				case ITEM_FTP_MAX_PASV_PORT:
				case ITEM_CLIENT_MIN_PORT:
				case ITEM_CLIENT_MAX_PORT:
				case ITEM_PROXY_PORT:
					SetItemValue(&m_items[m_nSelected], text, TRUE);
					break;
				default:
					SetItemValue(&m_items[m_nSelected], g_virtualKeyboard.GetText(), TRUE);
					break;
				}
			}
			else
			{
				ShowMessage(message, L"Invalid Value", MSGBOX_BUTTONS_OK);
			}
		}
		if (g_virtualKeyboard.m_bNumeric != 0)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = 0;
		return S_OK;
	}
	if (m_bIPEditorOpen)
	{
		m_pIPEditor->ProcessInput(action, pad);
		m_bIPEditorOpen = m_pIPEditor->m_bOpen;
		if (!m_bIPEditorOpen && !m_pIPEditor->m_bCancelled)
		{
			wchar_t address[24];
			m_pIPEditor->GetAddress(address);
			SetItemValue(&m_items[m_nSelected], address, TRUE);
		}
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
	static DWORD s_dwNextRefresh = GetTickCount() + 3000;   // 0x0027A42C

	switch (*action)
	{
	case INPUT_NONE:
	{
		// Every few seconds: the current address and link.
		if (s_dwNextRefresh >= GetTickCount())
			return S_OK;
		s_dwNextRefresh = GetTickCount() + 3000;
		if (!IsEthernetLinkUp())
		{
			SetItemValue(&m_items[ITEM_CURRENT_IP], L"No Link", FALSE);
			SetItemValue(&m_items[ITEM_LINK_STATUS], L"", FALSE);
			return S_OK;
		}
		SNetworkSettings &net = g_pSettings->m_network;
		if (net.m_bEnable && net.m_strIP.size() == 0)
		{
			// No address configured: take the one in use (if any yet).
			DWORD ip = GetConfigAddress(0);
			if (ip == (DWORD)-1 || ip == 0)
			{
				SetItemValue(&m_items[ITEM_CURRENT_IP], L"No Link", FALSE);
				return S_OK;
			}
			net.m_strIP = FormatIPAddress(GetConfigAddress(0));
			net.m_strSubnet = FormatIPAddress(GetConfigAddress(1));
			net.m_strGateway = FormatIPAddress(GetConfigAddress(2));
			net.m_strDNS1 = FormatIPAddress(GetConfigAddress(3));
			net.m_strDNS2 = FormatIPAddress(GetConfigAddress(4));
			return S_OK;
		}
		SetItemValue(&m_items[ITEM_CURRENT_IP], GetSkinTextValue(TEXT_IP), FALSE);
		SetItemValue(&m_items[ITEM_LINK_STATUS], GetSkinTextValue(TEXT_LINKSTATUS), FALSE);
		if (net.m_bDHCP)
		{
			ShowAddresses(TRUE);
			s_dwNextRefresh += 2000;
		}
		else if (!m_bModified)
		{
			ShowAddresses(FALSE);
			s_dwNextRefresh += 5000;
		}
		return S_OK;
	}

	case INPUT_A:
	case INPUT_START:
		break;

	case INPUT_X:
		// Clears the user, password or SYST.
		if (IsSelectedDisabled())
			return S_OK;
		if (item.m_nType == ITEM_FTP_USER || item.m_nType == ITEM_FTP_PASSWORD || item.m_nType == ITEM_FTP_SYST)
			SetItemValue(&m_items[m_nSelected], L"", TRUE);
		return S_OK;

	default:
		// (B/BACK is handled here too, the same as in the base class, which
		// has always used it.)
		return S_OK;
	}

	if (IsSelectedDisabled())
		return S_OK;
	CSettingItem *selected = &m_items[m_nSelected];
	const wchar_t *label = item.m_strLabel.c_str();
	const wchar_t *value = item.m_strValue.c_str();
	switch (item.m_nType)
	{
	case ITEM_TYPE:
	{
		BOOL isStatic;
		if (_wcsicmp(value, L"Static") == 0)
		{
			SetItemValue(selected, L"DHCP", TRUE);
			isStatic = FALSE;
		}
		else
		{
			SetItemValue(selected, L"Static", TRUE);
			isStatic = TRUE;
		}
		SetItemEnabled(ITEM_TYPE, isStatic);
		ShowAddresses(!isStatic);
		break;
	}

	case ITEM_IP:
	case ITEM_SUBNET:
	case ITEM_GATEWAY:
	case ITEM_DNS1:
	case ITEM_DNS2:
	case ITEM_PROXY:
		ShowIPEditor(value, label);
		break;

	case ITEM_ENABLE:
	case ITEM_FTP_ENABLE:
	case ITEM_PROXY_ENABLE:
		SetItemEnabled(item.m_nType, item.m_strValue == L"No");
		// fall through
	case ITEM_FTP_ALLOW_ANON:
	case ITEM_FTP_BOOST:
	case ITEM_AUTO_DETECT:
	case ITEM_CLIENT_DETECT_PEERS:
		SetItemValue(selected, item.m_strValue != L"Yes" ? L"Yes" : L"No", TRUE);
		break;

	case ITEM_FTP_USER:
	case ITEM_FTP_PASSWORD:
		OpenKeyboard(label, value, FALSE, 15, 0);
		break;
	case ITEM_FTP_SYST:
		OpenKeyboard(label, value, FALSE, 40, 0);
		break;
	case ITEM_FTP_MAX_USERS:
		OpenKeyboard(label, value, FALSE, 2, 1);
		break;
	case ITEM_FTP_PORT:
	case ITEM_FTP_MIN_PASV_PORT:
	case ITEM_FTP_MAX_PASV_PORT:
	case ITEM_CLIENT_MIN_PORT:
	case ITEM_CLIENT_MAX_PORT:
	case ITEM_PROXY_PORT:
		OpenKeyboard(label, value, FALSE, 5, 1);
		break;

	case ITEM_SAVE:
		if (m_bModified)
			Save();
		else
			m_bClosed = TRUE;
		break;
	case ITEM_RESET:
		RevertChanges();
		SetItemEnabled(0, TRUE);
		break;
	case ITEM_EXIT:
		AskToSave();
		break;

	default:
		return S_OK;
	}
	PlaySound(m_pSoundSelect);
	return S_OK;
}

// One line of the Network page as it's saved: <parent> (under <Settings>),
// optionally its child <child>, and the message when that can't be made.
struct SNetworkSaveTarget
{
	int type;
	const char *parent;
	const char *child;          // NULL: an attribute of parent
	const char *error;
};

static const SNetworkSaveTarget s_saveTargets[] = {
	{ CNetworkSetting::ITEM_ENABLE, "Network", NULL, "Unable to set Enable value for Networking. Error Code: %d" },
	{ CNetworkSetting::ITEM_TYPE, "Network", NULL, "Unable to set Networking Type. Error Code: %d" },
	{ CNetworkSetting::ITEM_IP, "Network", "IP", "Unable to set IP Address: %d" },
	{ CNetworkSetting::ITEM_SUBNET, "Network", "Subnet", "Unable to set Subnet mask: %d" },
	{ CNetworkSetting::ITEM_GATEWAY, "Network", "Gateway", "Unable to set Default Gateway: %d" },
	{ CNetworkSetting::ITEM_DNS1, "Network", "DNS1", "Unable to set Primary DNS Server: %d" },
	{ CNetworkSetting::ITEM_DNS2, "Network", "DNS2", "Unable to set Secondary DNS Server: %d" },
	{ CNetworkSetting::ITEM_AUTO_DETECT, "Network", "AutoDetect", "Unable to set Auto-Detect settings: %d" },
	{ CNetworkSetting::ITEM_CLIENT_MIN_PORT, "FTPClient", NULL, "Unable to set Port: %d" },
	{ CNetworkSetting::ITEM_CLIENT_MAX_PORT, "FTPClient", NULL, "Unable to set Port: %d" },
	// Quirk: the FTP server's message.
	{ CNetworkSetting::ITEM_CLIENT_DETECT_PEERS, "FTPClient", NULL,
	  "Unable to set Enable value for FTP Server. Error Code: %d" },
	{ CNetworkSetting::ITEM_FTP_ENABLE, "FTP", NULL, "Unable to set Enable value for FTP Server. Error Code: %d" },
	{ CNetworkSetting::ITEM_FTP_USER, "FTP", "User", "Unable to set FTP User: %d" },
	{ CNetworkSetting::ITEM_FTP_PASSWORD, "FTP", "Password", "Unable to set FTP Password: %d" },
	{ CNetworkSetting::ITEM_FTP_PORT, "FTP", "Port", "Unable to set FTP Port: %d" },
	{ CNetworkSetting::ITEM_FTP_MAX_USERS, "FTP", "MaxUsers", "Unable to set FTP Maximum Number of User: %d" },
	{ CNetworkSetting::ITEM_FTP_ALLOW_ANON, "FTP", "AllowAnon", "Unable to set FTP Maximum Number of User: %d" },
	{ CNetworkSetting::ITEM_FTP_BOOST, "FTP", "Boost", "Unable to set Boost settings: %d" },
	{ CNetworkSetting::ITEM_FTP_MIN_PASV_PORT, "FTP", "MinPASVPort", "Unable to set FTP PASV Port: %d" },
	{ CNetworkSetting::ITEM_FTP_MAX_PASV_PORT, "FTP", "MaxPASVPort", "Unable to set FTP PASV Port: %d" },
	// Quirk: the password's message.
	{ CNetworkSetting::ITEM_FTP_SYST, "FTP", "SYST", "Unable to set FTP Password: %d" },
	{ CNetworkSetting::ITEM_PROXY_ENABLE, "HTTP", NULL, "Unable to set Enable value for HTTP Proxy. Error Code: %d" },
	{ CNetworkSetting::ITEM_PROXY, "HTTP", NULL, "Unable to set HTTP Proxy address: %d" },
	{ CNetworkSetting::ITEM_PROXY_PORT, "HTTP", NULL, "Unable to set HTTP Proxy Port: %d" },
};

// 0x000C66FD
BOOL CNetworkSetting::Save()
{
	BOOL ok = TRUE;
	BOOL changed = FALSE;
	BOOL needRestart = FALSE;
	char message[100];
	if (!m_bModified)
		return TRUE;

	std::string text;
	g_config.m_pCurrent = &g_config.m_root;
	CXmlElement *settings = g_config.FindElement(&g_config.m_root, "Settings");
	if (settings == NULL)
	{
		sprintf(message, GetStringA(STR_MSG_SETTING_NOCONFIG), STR_MSG_SETTING_NOCONFIG);
		ShowMessage(message, GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
		return FALSE;
	}

	SNetworkSettings &net = g_pSettings->m_network;
	SFTPSettings &ftp = g_pSettings->m_ftp;
	for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
	{
		if (!it->m_bChanged || !it->m_bVisible || !it->m_bEnabled || it->m_bHeader)
			continue;
		const SNetworkSaveTarget *target = NULL;
		for (size_t i = 0; i < sizeof(s_saveTargets) / sizeof(s_saveTargets[0]); i++)
		{
			if (s_saveTargets[i].type == it->m_nType)
				target = &s_saveTargets[i];
		}
		if (target == NULL)
			continue;

		CXmlElement *parent = g_config.FindElement(settings, target->parent);
		CXmlElement *element = NULL;
		if (GetOrAddChildElement(target->parent, settings, &parent))
		{
			element = parent;
			if (target->child != NULL)
			{
				element = parent->FindFirstChild(target->child);
				if (!GetOrAddChildElement(target->child, parent, &element))
					element = NULL;
			}
		}
		if (element == NULL)
		{
			sprintf(message, target->error, GetLastError());
			ok = FALSE;
			continue;
		}

		const wchar_t *value = it->m_strValue.c_str();
		switch (it->m_nType)
		{
		case ITEM_ENABLE:
			net.m_bEnable = !IsNoW(value);
			element->SetAttribute("Enable", net.m_bEnable ? "Yes" : "No");
			changed = needRestart = TRUE;
			break;
		case ITEM_TYPE:
			net.m_bDHCP = _wcsicmp(value, L"dhcp") == 0;
			element->SetAttribute("Type", net.m_bDHCP ? "DHCP" : "Static");
			changed = needRestart = TRUE;
			break;
		case ITEM_IP:
		case ITEM_SUBNET:
		case ITEM_GATEWAY:
		case ITEM_DNS1:
		case ITEM_DNS2:
		{
			std::string *strings[] = { &net.m_strIP, &net.m_strSubnet, &net.m_strGateway, &net.m_strDNS1,
			                           &net.m_strDNS2 };
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = needRestart = TRUE;
			*strings[it->m_nType - ITEM_IP] = text;
			break;
		}
		case ITEM_AUTO_DETECT:
			net.m_bAutoDetect = !IsNoW(value);
			element->SetValue(net.m_bAutoDetect ? "Yes" : "No");
			changed = TRUE;
			break;
		case ITEM_CLIENT_MIN_PORT:
		case ITEM_CLIENT_MAX_PORT:
			text = StrFromWide(value);
			element->SetAttribute(it->m_nType == ITEM_CLIENT_MIN_PORT ? "MinPort" : "MaxPort", text.c_str());
			changed = TRUE;
			if (it->m_nType == ITEM_CLIENT_MIN_PORT)
				ftp.m_nClientMinPort = _wtol(value);
			else
				ftp.m_nClientMaxPort = _wtol(value);
			break;
		case ITEM_CLIENT_DETECT_PEERS:
			ftp.m_bClientDetectPeers = !IsNoW(value);
			element->SetAttribute("DetectPeer", ftp.m_bClientDetectPeers ? "Yes" : "No");
			changed = TRUE;
			break;
		case ITEM_FTP_ENABLE:
			ftp.m_bEnable = !IsNoW(value);
			element->SetAttribute("Enable", ftp.m_bEnable ? "Yes" : "No");
			changed = needRestart = TRUE;
			break;
		case ITEM_FTP_USER:
		case ITEM_FTP_PASSWORD:
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = needRestart = TRUE;
			if (it->m_nType == ITEM_FTP_USER)
				ftp.m_strUser = text;
			else
				ftp.m_strPassword = text;
			break;
		case ITEM_FTP_PORT:
		case ITEM_FTP_MAX_USERS:
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = needRestart = TRUE;
			if (it->m_nType == ITEM_FTP_PORT)
				ftp.m_nPort = _wtol(value);
			else
				ftp.m_nMaxUsers = _wtol(value);
			break;
		case ITEM_FTP_ALLOW_ANON:
			ftp.m_bAllowAnon = !IsNoW(value);
			element->SetValue(ftp.m_bAllowAnon ? "Yes" : "No");
			changed = needRestart = TRUE;
			break;
		case ITEM_FTP_BOOST:
			ftp.m_bBoost = !IsNoW(value);
			element->SetValue(ftp.m_bBoost ? "Yes" : "No");
			changed = TRUE;
			g_ftpServer.m_bBoost = ftp.m_bBoost;   // applies right away
			break;
		case ITEM_FTP_MIN_PASV_PORT:
		case ITEM_FTP_MAX_PASV_PORT:
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = TRUE;
			if (it->m_nType == ITEM_FTP_MIN_PASV_PORT)
				ftp.m_nMinPasvPort = _wtol(value);
			else
				ftp.m_nMaxPasvPort = _wtol(value);
			break;
		case ITEM_FTP_SYST:
			text = StrFromWide(value);
			element->SetValue(text.c_str());
			changed = TRUE;
			ftp.m_strSyst = text;
			g_ftpServer.SetSyst(ftp.m_strSyst.c_str());   // applies right away
			break;
		case ITEM_PROXY_ENABLE:
			net.m_bUseProxy = !IsNoW(value);
			element->SetAttribute("UseProxy", net.m_bUseProxy ? "Yes" : "No");
			changed = needRestart = TRUE;
			break;
		case ITEM_PROXY:
			text = StrFromWide(value);
			element->SetAttribute("Proxy", text.c_str());
			changed = TRUE;
			net.m_strProxy = text;
			break;
		case ITEM_PROXY_PORT:
			text = StrFromWide(value);
			element->SetAttribute("Port", text.c_str());
			changed = needRestart = TRUE;
			net.m_nProxyPort = _wtol(value);
			break;
		}
	}
	return FinishSave(ok, message, changed, needRestart);
}
