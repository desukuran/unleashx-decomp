// CFTPSite: the FTP manager's site editor ("FTP Site Manager"). See
// FtpManager.h.
#include <cstdio>
#include <cstring>
#include <cwchar>

#include "../audio/Sound.h"
#include "../common/FileUtil.h"
#include "../common/StrUtil.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../ui/FileBrowser.h"
#include "../ui/VirtualKeyboard.h"
#include "FtpManager.h"

// 0x0013DDDB (this in esi)
CFTPSite::CFTPSite()
{
	m_bSaved = FALSE;
	m_bQuickConnect = FALSE;
	m_bNewSite = TRUE;
	m_strName.clear();
	m_strAddress.clear();
	m_nPort = 21;
	m_strUser.clear();
	m_strPassword.clear();
	m_bPassive = TRUE;
	m_strRemotePath.clear();
	m_strLocalPath.clear();
}

// 0x0013DD6C
CFTPSite::~CFTPSite()
{
	FreePopups();
}

// 0x0013D990
HRESULT CFTPSite::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	if (m_pFont != NULL)
		return S_OK;
	LoadSounds();
	SetTitle(L"FTP Site Manager");
	m_pFont = font;
	GetScreenSize(&m_fWidth, &m_fHeight);
	m_fY = 30.0f;
	m_fX = 40.0f;
	m_fWidth = 560.0f;
	m_fHeight = 420.0f;
	m_items.clear();
	AddItem(L"Site name", L"", ITEM_NAME, FALSE, TRUE, TRUE, 0, FALSE);
	AddItem(L"Address", L"", ITEM_ADDRESS, FALSE, TRUE, TRUE, 1, FALSE);
	AddItem(L"Port", L"", ITEM_PORT, FALSE, TRUE, TRUE, 1, FALSE);
	AddItem(L"User Name", L"", ITEM_USER, FALSE, TRUE, TRUE, 0, FALSE);
	AddItem(L"Password", L"", ITEM_PASSWORD, FALSE, TRUE, TRUE, 1, TRUE);
	AddItem(L"PASV", m_bPassive ? L"Yes" : L"No", ITEM_PASV, FALSE, TRUE, TRUE, 1, FALSE);
	AddItem(L"Remote Path", L"", ITEM_REMOTE_PATH, FALSE, TRUE, TRUE, 1, FALSE);
	AddItem(L"Local Path", L"", ITEM_LOCAL_PATH, FALSE, TRUE, TRUE, 1, FALSE);
	AddItem(L"Save", L"", ITEM_SAVE_HEADER, TRUE, TRUE, TRUE, 0, FALSE);
	AddItem(L"Save", L"", ITEM_SAVE, FALSE, TRUE, TRUE, 0, FALSE);
	AddItem(L"Reset", L"", ITEM_RESET, FALSE, TRUE, TRUE, 0, FALSE);
	AddItem(L"Cancel", L"", ITEM_CANCEL, FALSE, TRUE, TRUE, 0, FALSE);
	return S_OK;
}

// 0x0013D699
void CFTPSite::Reset()
{
	CSettingsBase::Reset();
	if (!m_bNewSite)
	{
		m_nSelected = 1;
	}
	else
	{
		// A new site: the defaults, and the keyboard for its name.
		m_strName.clear();
		m_strAddress.clear();
		m_nPort = 21;
		m_strUser = "xbox";
		m_strPassword = "xbox";
		m_bPassive = TRUE;
		m_strLocalPath.clear();
		m_strRemotePath.clear();
		m_nSelected = 0;
		CSettingItem item = m_items[ITEM_NAME];
		OpenKeyboard(item.m_strLabel.c_str(), L"", FALSE, 20, 0);
		m_bKeyboardOpen = TRUE;
	}

	wchar_t text[0x104];
	StrUtf8ToWide(m_strName.c_str(), text, 100);
	SetItemValue(&m_items[ITEM_NAME], text, FALSE);
	StrUtf8ToWide(m_strAddress.c_str(), text, 100);
	SetItemValue(&m_items[ITEM_ADDRESS], text, FALSE);
	if (m_nPort == 0 || (unsigned)m_nPort > 0x10000)
		m_nPort = 21;
	swprintf(text, 0x104, L"%d", m_nPort);
	SetItemValue(&m_items[ITEM_PORT], text, FALSE);
	StrUtf8ToWide(m_strUser.c_str(), text, 100);
	SetItemValue(&m_items[ITEM_USER], text, FALSE);
	StrUtf8ToWide(m_strPassword.c_str(), text, 100);
	SetItemValue(&m_items[ITEM_PASSWORD], text, FALSE);
	SetItemValue(&m_items[ITEM_PASV], m_bPassive ? L"Yes" : L"No", FALSE);
	StrUtf8ToWide(m_strRemotePath.c_str(), text, 100);
	SetItemValue(&m_items[ITEM_REMOTE_PATH], text, FALSE);
	StrUtf8ToWide(m_strLocalPath.c_str(), text, 0x104);
	SetItemValue(&m_items[ITEM_LOCAL_PATH], text, FALSE);
	// The name only for a new site; a quick connect has no local path.
	SetItemEnabled(ITEM_NAME, m_bNewSite);
	SetItemEnabled(ITEM_LOCAL_PATH, m_bNewSite || !m_bQuickConnect);
	m_bModified = FALSE;
	m_bSaved = FALSE;
}

// 0x0013DE98 (this in ebx)
BOOL CFTPSite::Save()
{
	BOOL closed = TRUE;
	BOOL ok = TRUE;
	char message[100];
	if (m_bModified)
	{
		m_bSaved = TRUE;
		for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end() && ok; ++it)
		{
			if (it->m_bHeader)
				continue;
			const wchar_t *value = it->m_strValue.c_str();
			switch (it->m_nType)
			{
			case ITEM_NAME:
				m_strName = StrFromWide(value);
				if (m_strName.size() == 0)
				{
					strcpy(message, "Site Name can not be empty.");
					ok = FALSE;
				}
				break;
			case ITEM_ADDRESS:
				m_strAddress = StrFromWide(value);
				if (m_strAddress.size() == 0)
				{
					strcpy(message, "Address Name can not be empty.");
					ok = FALSE;
				}
				break;
			case ITEM_PORT:
			{
				unsigned port = (unsigned)_wtoi(value);
				if (port == 0 || port > 0x10000)
				{
					sprintf(message, "The Port Number %d is an invalid value.", port);
					ok = FALSE;
				}
				else
				{
					m_nPort = (int)port;
				}
				break;
			}
			case ITEM_USER:
				m_strUser = StrFromWide(value);
				break;
			case ITEM_PASSWORD:
				m_strPassword = StrFromWide(value);
				break;
			case ITEM_PASV:
				m_bPassive = !IsNoW(value);
				break;
			case ITEM_REMOTE_PATH:
				m_strRemotePath = StrFromWide(value);
				break;
			case ITEM_LOCAL_PATH:
				m_strLocalPath = StrFromWide(value);
				break;
			}
		}
		if (!ok)
		{
			closed = FALSE;
			ShowMessage(message, L"Save Error", MSGBOX_BUTTONS_OK);
			m_bSaved = FALSE;
		}
		else if (m_bModified)
		{
			m_bModified = FALSE;
			m_bSaved = TRUE;
			for (std::vector<CSettingItem>::iterator it = m_items.begin(); it != m_items.end(); ++it)
				it->m_bChanged = FALSE;
		}
	}
	m_bClosed = closed;
	return ok;
}

// 0x0013E31D
HRESULT CFTPSite::HandleInput(int *action, SGamepadState **pad)
{
	if (m_bClosed)
		return S_FALSE;

	if (m_bKeyboardOpen)
	{
		g_virtualKeyboard.ProcessInput(action);
		m_bKeyboardOpen = !g_virtualKeyboard.IsDone();
		if (m_bKeyboardOpen)
			return S_OK;
		if (!g_virtualKeyboard.m_bCancelled)
		{
			wchar_t text[260];
			wcscpy(text, g_virtualKeyboard.GetText());
			if (m_nSelected == ITEM_PORT)
			{
				unsigned port = (unsigned)_wtoi(g_virtualKeyboard.GetText());
				if (port == 0 || port > 0x10000)
				{
					// (The keyboard stays numeric.)
					ShowMessage("Invalid Port Number specified", L"Invalid Value", MSGBOX_BUTTONS_OK);
					return S_OK;
				}
			}
			else if (m_nSelected == ITEM_REMOTE_PATH && text[0] != L'/')
			{
				wcscpy(text, L"/");
				wcscat(text, g_virtualKeyboard.GetText());
			}
			SetItemValue(&m_items[m_nSelected], text, TRUE);
		}
		if (g_virtualKeyboard.m_bNumeric)
			g_virtualKeyboard.Open();
		g_virtualKeyboard.m_bNumeric = FALSE;
		return S_OK;
	}

	if (m_bFileBrowserOpen)
	{
		m_pFileBrowser->ProcessInput(action, pad);
		if (m_pFileBrowser->IsOpen())
			return S_OK;
		m_bFileBrowserOpen = FALSE;
		if (m_pFileBrowser->m_bCancelled)
			return S_OK;
		std::string path = m_pFileBrowser->GetSelectedPath();
		if (path.size() != 0)
		{
			wchar_t text[260];
			MultiByteToWideChar(CP_ACP, 0, path.c_str(), (int)path.size() + 1, text, 0x104);
			SetItemValue(&m_items[m_nSelected], text, TRUE);
		}
		return S_OK;
	}

	if (FAILED(CSettingsBase::HandleInput(action, pad)))
		return S_OK;
	if (m_bModified && m_bAskSave && Save())
	{
		m_bAskSave = FALSE;
		m_bClosed = TRUE;
		return S_OK;
	}

	CSettingItem item = m_items[m_nSelected];
	std::string path;
	static DWORD s_dwTimer = GetTickCount() + 1000;   // 0x0027A45C, not used
	(void)s_dwTimer;

	switch (*action)
	{
	case INPUT_A:
	case INPUT_START:
		*action = INPUT_NONE;
		if (IsSelectedDisabled())
			return S_OK;
		switch (item.m_nType)
		{
		case ITEM_NAME:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 30, 0);
			break;
		case ITEM_ADDRESS:
		case ITEM_REMOTE_PATH:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 100, 0);
			break;
		case ITEM_PORT:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 5, 1);
			break;
		case ITEM_USER:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), FALSE, 0, 0);
			break;
		case ITEM_PASSWORD:
			OpenKeyboard(item.m_strLabel.c_str(), item.m_strValue.c_str(), TRUE, 0, 0);
			break;
		case ITEM_PASV:
			SetItemValue(&m_items[m_nSelected], item.m_strValue.compare(L"Yes") == 0 ? L"No" : L"Yes", TRUE);
			break;
		case ITEM_LOCAL_PATH:
			path = StrFromWide(item.m_strValue.c_str());
			if (path.size() != 0)
				ResolvePath(path.c_str(), path);
			ShowFileBrowser(item.m_strLabel.c_str(), 1, FALSE, TRUE, path.c_str(), NULL);
			break;
		case ITEM_SAVE:
			// (Saves even when nothing changed.)
			m_bModified = TRUE;
			Save();
			break;
		case ITEM_RESET:
			RevertChanges();
			break;
		case ITEM_CANCEL:
			AskToSave();
			break;
		}
		PlaySound(m_pSoundSelect);
		break;

	case INPUT_B:
	case INPUT_BACK:
		AskToSave();
		*action = INPUT_NONE;
		break;

	case INPUT_X:
		// Clears the name, address, user or password.
		*action = INPUT_NONE;
		if (!IsSelectedDisabled() && (item.m_nType < 2 || item.m_nType == ITEM_USER || item.m_nType == ITEM_PASSWORD))
			SetItemValue(&m_items[m_nSelected], L"", TRUE);
		break;
	}
	return S_OK;
}
