// The "lock/unlock your hard disk" question of the HDD settings page.
#include <cstring>

#include "../audio/Sound.h"
#include "../audio/SoundEffects.h"
#include "../system/SystemInfo.h"
#include "../ui/Language.h"
#include "../ui/StringIds.h"
#include "Screens.h"
#include "SettingsPages.h"

// 0x001196A6
CHddLockDialog::CHddLockDialog()
{
	m_pMessageBox = NULL;
	m_pFont = NULL;
	m_pSoundSelect = NULL;
	m_bDone = FALSE;
	m_bChanged = FALSE;
	m_bOpen = TRUE;
	m_bUnlock = TRUE;
	m_bMessageBoxOpen = FALSE;
}

// 0x001196D6
CHddLockDialog::~CHddLockDialog()
{
	if (m_pMessageBox != NULL)
	{
		delete m_pMessageBox;
		m_pMessageBox = NULL;
	}
	m_pMessageBox = NULL;
}

// 0x001196EB
HRESULT CHddLockDialog::Init(CXBFont *font)
{
	if (font == NULL)
		return E_FAIL;
	m_pFont = font;
	if (m_pSoundSelect == NULL)
		m_pSoundSelect = g_soundEffects.Find("Select");
	return S_OK;
}

// 0x00119712
void CHddLockDialog::ShowMessage(const char *text, const wchar_t *title, int buttons)
{
	if (m_pMessageBox == NULL)
	{
		m_pMessageBox = new CMessageBox;
		if (m_pMessageBox != NULL && FAILED(m_pMessageBox->Create(m_pFont, NULL, 0.0f, 0.0f, 0.0f, 0.0f, 10)))
		{
			delete m_pMessageBox;
			m_pMessageBox = NULL;
			return;
		}
		m_pMessageBox->SetButtonText(MSGBOX_OK, GetString(STR_OK));
		m_pMessageBox->SetButtonText(MSGBOX_CANCEL, GetString(STR_CANCEL));
		m_pMessageBox->SetButtonText(MSGBOX_ABORT, GetString(STR_ABORT));
		m_pMessageBox->SetButtonText(MSGBOX_RETRY, GetString(STR_RETRY));
		m_pMessageBox->SetButtonText(MSGBOX_IGNORE, GetString(STR_IGNORE));
		m_pMessageBox->SetButtonText(MSGBOX_YES, GetString(STR_YES));
		m_pMessageBox->SetButtonText(MSGBOX_NO, GetString(STR_NO));
	}
	if (m_pMessageBox != NULL)
	{
		m_pMessageBox->Show(text, title, buttons);
		m_bMessageBoxOpen = TRUE;
	}
}

// 0x00119824
void CHddLockDialog::Show()
{
	m_bDone = FALSE;
	m_bChanged = FALSE;
	m_bOpen = TRUE;
	WORD status = GetHddSecurityStatus();
	if (!(status & HDD_SECURITY_SUPPORTED))
	{
		ShowMessage(GetStringA(STR_MSG_HDUNLOCKABLE), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
	}
	else if (status & HDD_SECURITY_ENABLED)
	{
		m_bUnlock = TRUE;
		ShowMessage(GetStringA(STR_MSG_HDUNLOCK_ASK), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
		if (m_pMessageBox != NULL)
			m_pMessageBox->SetSelection(1);   // "No"
	}
	else
	{
		m_bUnlock = FALSE;
		ShowMessage(GetStringA(STR_MSG_HDLOCK_ASK), GetString(STR_CONFIRM), MSGBOX_BUTTONS_YESNO);
	}
}

// 0x001198A4
HRESULT CHddLockDialog::ProcessInput(int *action, SGamepadState **pad)
{
	if (m_bDone)
		return S_FALSE;

	// The left stick, pushed all the way, works like the d-pad.
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
	if (!m_bMessageBoxOpen)
		return S_OK;

	m_pMessageBox->ProcessInput(action);
	m_bMessageBoxOpen = m_pMessageBox->IsOpen();
	*action = INPUT_NONE;
	if (m_bMessageBoxOpen)
		return E_FAIL;

	int result = m_pMessageBox->GetResult();
	if (result == MSGBOX_OK)
	{
		// The result message was closed.
		m_bOpen = FALSE;
		m_bDone = TRUE;
		PlaySound(m_pSoundSelect);
		return S_OK;
	}
	if (m_pMessageBox->GetResult() != MSGBOX_YES)
	{
		m_bChanged = FALSE;
		m_bDone = TRUE;
		m_bOpen = FALSE;
		return E_FAIL;
	}

	if (m_bUnlock)
	{
		if (UnlockHdd())
		{
			m_bChanged = TRUE;
			m_bUnlock = FALSE;
			ShowMessage(GetStringA(STR_MSG_HDUNLOCK_SUCCESS), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
			return S_OK;
		}
	}
	else if (LockHdd())
	{
		m_bChanged = TRUE;
		m_bUnlock = TRUE;
		ShowMessage(GetStringA(STR_MSG_HDLOCK_SUCCESS), GetString(STR_SUCCESS), MSGBOX_BUTTONS_OK);
		return S_OK;
	}
	ShowMessage(GetStringA(STR_MSG_HDUNLOCK_FAILED), GetString(STR_ERROR), MSGBOX_BUTTONS_OK);
	return S_OK;
}

// 0x001196C3
HRESULT CHddLockDialog::Render()
{
	if (m_pMessageBox == NULL)
		return E_FAIL;
	m_pMessageBox->Render();
	return S_OK;
}
