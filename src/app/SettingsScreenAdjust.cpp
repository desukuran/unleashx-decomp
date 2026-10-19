// Control Panel: Screen Adjust. The page only forwards to CScreenAdjuster.
#include "SettingsPages.h"

// 0x000C1BB8
CScreenAdjust::~CScreenAdjust()
{
	FreePopups();
	// (m_adjuster's destructor runs here.)
}

// 0x000C1B87
HRESULT CScreenAdjust::Init(CXBFont *font)
{
	if (font == NULL || m_pFont != NULL)
		return E_FAIL;
	LoadSounds();
	m_pFont = font;
	return m_adjuster.Init(font);
}

// 0x000C39E6
HRESULT CScreenAdjust::HandleInput(int *action, SGamepadState **pad)
{
	m_adjuster.HandleInput(action, pad);
	// Quirk: m_bClosed gets the editor's *open* flag. Nothing else on this
	// page reads it (Render and IsOpen are the editor's).
	m_bClosed = m_adjuster.m_bOpen;
	// Quirk: returns whatever was left in eax (not a meaningful value).
	return S_OK;
}

// 0x000C05CF
HRESULT CScreenAdjust::Render()
{
	m_adjuster.Render();
	return S_OK;   // (eax is left from the call)
}

// 0x0002DE35
void CScreenAdjust::Reset()
{
	m_adjuster.Reset();
	CSettingsBase::Reset();
}

// 0x0002DE49
BOOL CScreenAdjust::IsOpen()
{
	return m_adjuster.m_bOpen;
}
