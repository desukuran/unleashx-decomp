// CFTPQueue: the FTP manager's transfer queue pane. See FtpManager.h.
#include <cmath>
#include <cwchar>

#include "../app/UnleashX.h"
#include "../app/XBApp.h"
#include "../input/Gamepad.h"
#include "../render/Draw.h"
#include "../render/Fonts.h"
#include "../render/Texture.h"
#include "../settings/AppSettings.h"
#include "../skin/SkinColors.h"
#include "../ui/MenuDialogs.h"
#include "FtpManager.h"

std::vector<SFtpQueueItem> g_ftpQueue;   // 0x00277820
BOOL g_bFtpStartQueue = FALSE;           // 0x002761DC
IDirect3DTexture8 *g_pFtpUpArrow = NULL;     // 0x00270FB4
IDirect3DTexture8 *g_pFtpDownArrow = NULL;   // 0x00270FB0

// The queue menu's entries.
enum
{
	QUEUEMENU_TRANSFER = 0,
	QUEUEMENU_REMOVE = 1,
	QUEUEMENU_CLEAR = 2,
};

// 0x0001E4F0 (this in esi)
SFtpQueueItem::SFtpQueueItem(std::string itemSource, std::string itemTarget, BOOL itemIsFolder, DWORD itemSize,
                             BOOL itemUpload)
	: source(itemSource), target(itemTarget), isFolder(itemIsFolder), size(itemSize), upload(itemUpload),
	  unknown44(0)
{
}

// 0x0011F223 (this in eax)
CFTPQueue::CFTPQueue()
{
	m_nSelected = 0;
	m_pUnknown34 = NULL;
	m_pUnknown38 = NULL;
	m_bActive = FALSE;
	m_pFont = NULL;
	m_pMenu = NULL;
	m_bMenuOpen = FALSE;
	m_nTop = 0;
	m_nVisible = 0;
	m_nCount = 0;
	m_fX = 0.0f;
	m_fY = 0.0f;
	m_fWidth = 0.0f;
	m_fHeight = 0.0f;
}

// 0x0011FC6F
CFTPQueue::~CFTPQueue()
{
	g_ftpQueue.clear();
	m_pFont = NULL;
	if (m_pUnknown34 != NULL)
		ReleaseTexture(m_pUnknown34);
	m_pUnknown34 = NULL;
	if (m_pUnknown38 != NULL)
		ReleaseTexture(m_pUnknown38);
	m_pUnknown38 = NULL;
	delete m_pMenu;
	m_pMenu = NULL;
}

// 0x0011FB46 (this in ecx, font in edx)
HRESULT CFTPQueue::Init(CXBFont *font)
{
	if (font == NULL || m_pFont != NULL)
		return E_FAIL;
	m_pFont = font;
	if (g_pFtpUpArrow == NULL)
		g_pFtpUpArrow = g_pResource->GetTexture("FileUp.png");
	if (g_pFtpDownArrow == NULL)
		g_pFtpDownArrow = g_pResource->GetTexture("FileDown.png");
	return S_OK;
}

// 0x00120C15 (this in ebx)
BOOL CFTPQueue::Add(const char *source, const char *target, BOOL isFolder, DWORD size, BOOL upload)
{
	for (std::vector<SFtpQueueItem>::iterator it = g_ftpQueue.begin(); it != g_ftpQueue.end(); ++it)
	{
		if (it->source.compare(source) == 0 && it->target.compare(target) == 0)
			return FALSE;
	}
	g_ftpQueue.push_back(SFtpQueueItem(source, target, isFolder, size, upload));
	m_nVisible = (unsigned)g_ftpQueue.size();
	m_nCount = (unsigned)g_ftpQueue.size() - 1;
	return TRUE;
}

// 0x0011FC20 (this in edi; index in eax). Quirk: m_nCount becomes the
// count here, but the last index in Add().
BOOL CFTPQueue::Remove(unsigned index)
{
	if (index < g_ftpQueue.size())
	{
		g_ftpQueue.erase(g_ftpQueue.begin() + index);
		m_nCount = (unsigned)g_ftpQueue.size();
		if (m_nCount == 0)
			m_nSelected = 0;
		else if (m_nCount <= m_nSelected)
			m_nSelected = m_nCount - 1;
	}
	return TRUE;
}

// 0x0011FC08 (this in edi)
void CFTPQueue::Clear()
{
	g_ftpQueue.clear();
	m_nSelected = 0;
	m_nVisible = 0;
	m_nCount = 0;
}

// 0x0011F1E4 (this in eax)
void CFTPQueue::MoveDown()
{
	if (m_nSelected >= m_nCount)
		return;
	m_nSelected++;
	while (m_nTop + m_nVisible < m_nSelected)
		m_nTop++;
}

// 0x0011F209 (this in eax)
void CFTPQueue::MoveUp()
{
	if ((int)m_nSelected <= 0)
		return;
	m_nSelected--;
	while (m_nSelected < m_nTop)
		m_nTop--;
}

// 0x00122FFD (this in eax)
HRESULT CFTPQueue::ProcessInput(int *action)
{
	if (*action == INPUT_NONE)
		return S_OK;
	if (m_bMenuOpen)
	{
		m_pMenu->ProcessInput(action);
		m_bMenuOpen = m_pMenu->IsOpen();
		if (!m_bMenuOpen && !m_pMenu->m_bCancelled && m_pMenu->m_nSelection != -1)
		{
			switch (m_pMenu->m_nSelection)
			{
			case QUEUEMENU_TRANSFER:
				g_bFtpStartQueue = TRUE;
				m_bActive = FALSE;
				break;
			case QUEUEMENU_REMOVE:
				Remove(m_nSelected);
				break;
			case QUEUEMENU_CLEAR:
				Clear();
				break;
			}
		}
		*action = INPUT_NONE;
		return S_OK;
	}
	switch (*action)
	{
	case INPUT_RTRIGGER:
	case INPUT_DOWN:
		MoveDown();
		break;
	case INPUT_LTRIGGER:
	case INPUT_UP:
		MoveUp();
		break;
	case INPUT_START:
	case INPUT_WHITE:
		if (m_pMenu == NULL)
		{
			m_pMenu = new CDropDown;
			if (m_pMenu != NULL)
			{
				m_pMenu->AddItem(L"Transfer");
				m_pMenu->AddItem(L"Remove");
				m_pMenu->AddItem(L"Clear Queue");
				m_pMenu->Create(m_pFont, m_fX + 5.0f, m_fY + 5.0f);
			}
		}
		if (m_pMenu != NULL)
		{
			m_pMenu->Show(m_fX + 40.0f, m_fY + 15.0f);
			m_bMenuOpen = TRUE;
		}
		break;
	case INPUT_BACK:
	case INPUT_BLACK:
		// Back to the panes.
		m_bActive = FALSE;
		break;
	}
	*action = INPUT_NONE;
	return S_OK;
}

// 0x0011F79D (this in esi)
HRESULT CFTPQueue::Render()
{
	float scale = g_pFtpSettings->m_fFontScale;
	m_pFont->SetScaleFactors(scale, scale);
	float bottom = m_fY + m_fHeight;
	float lineHeight = (float)floor(m_pFont->m_fScaleFactorY * m_pFont->m_fCellHeight);
	float iconSize = 16.0f;
	float y = m_fY + 2.0f;
	float x = m_fX + 2.0f;
	float right = (m_fWidth + m_fX) - 4.0f;
	if (lineHeight < 14.0f)
	{
		lineHeight = 15.0f;
		m_pFont->SetTextHeight(15);
	}
	else
	{
		iconSize = lineHeight;
	}
	wchar_t text[0x104];
	text[0] = 0;
	unsigned index = m_nTop;
	SetScissorRect(TRUE, m_fX + 1.0f, m_fY + 1.0f, m_fWidth - 1.0f, m_fHeight - 1.0f);
	float textX = x + iconSize + 2.0f;
	float halfLine = lineHeight * 0.5f;
	for (std::vector<SFtpQueueItem>::iterator it = g_ftpQueue.begin() + m_nTop; it != g_ftpQueue.end(); ++it)
	{
		DWORD color = g_colorScheme.OPTION_TEXTCOLOR;
		if (index == m_nSelected)
		{
			// Quirk: the active pane's "fade" is the whole color, so its
			// alpha loses the color's blue byte; the inactive one's fades
			// to a third.
			DWORD fade = m_bActive ? g_colorScheme.OPTION_ACTIVEBACKCOLOR
			                       : ((g_colorScheme.OPTION_ACTIVEBACKCOLOR >> 24) * 2) / 3;
			DrawGradientBox(x - 1.0f, y - 1.0f, right + 1.0f, y + lineHeight + 1.0f,
			                g_colorScheme.DIALOG_BORDERCOLOR - (fade << 24),
			                g_colorScheme.OPTION_ACTIVEBACKCOLOR - (fade << 24));
			color = m_bActive ? g_colorScheme.OPTION_ACTIVETEXTCOLOR : g_colorScheme.OPTION_TEXTCOLOR;
		}
		DrawTexture(it->upload ? g_pFtpUpArrow : g_pFtpDownArrow, x, y + 1.0f, iconSize, iconSize, 1.0f, NULL, 0.0f,
		            NULL);
		swprintf(text, 0x104, L"%hs", it->source.c_str());
		m_pFont->DrawText(textX, halfLine + y, color, text, XBFONT_CENTER_Y, m_fWidth * 0.5f);
		swprintf(text, 0x104, L"%hs", it->target.c_str());
		m_pFont->DrawText(m_fWidth * 0.5f + textX, halfLine + y, color, text, XBFONT_CENTER_Y,
		                  m_fWidth * 0.5f - iconSize);
		y += lineHeight;
		if (bottom < y)
		{
			m_nVisible = (index - m_nTop) - 1;
			if (index < m_nSelected)
				m_nSelected = index;
			break;
		}
		index++;
	}
	SetScissorRect(FALSE, 0.0f, 0.0f, 0.0f, 0.0f);
	if (m_bMenuOpen)
		m_pMenu->Render();
	return S_OK;
}
