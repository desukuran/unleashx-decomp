// The settings screen ("Control Panel"): CSettingsMenu shows ten icons, each
// opening a page derived from CSettingsBase. A page is a scrolling list of
// CSettingItem lines (label and value); the page subclasses fill the list
// from CAppSettings, change values on input and write them to Config.xml.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"
#include "../input/Gamepad.h"

class CClockEditor;
class CXmlElement;
class CFileBrowser;
class CIPAddressEditor;
class CMessageBox;
class CSound;
class CXBFont;
struct IDirect3DTexture8;

// One line of a page. 0x74 bytes.
class CSettingItem
{
public:
	CSettingItem(std::wstring label, std::wstring value, int type, BOOL header, BOOL enabled, BOOL visible,
	             int value2, BOOL password);                 // 0x000203D8
	// The copy constructor (0x000204E7) and destructor (0x0002053C) are the
	// compiler's.

	// Sets the shown value without marking the line changed. The original
	// takes this in eax.
	void SetValue(const wchar_t *text);                       // 0x000C00A2

	std::wstring m_strLabel;        // +0x00
	std::wstring m_strValue;        // +0x1C the text shown on the right
	BOOL m_bHeader;                 // +0x38 a section caption: centered, not selectable
	BOOL m_bEnabled;                // +0x3C
	int m_nType;                    // +0x40 how the value is edited (page-specific)
	BOOL m_bVisible;                // +0x44 hidden lines take no space and can't be selected
	BOOL m_bChanged;                // +0x48
	int m_nValue;                   // +0x4C e.g. the index of the chosen option
	std::wstring m_strSavedValue;   // +0x50 m_strValue when the page was opened
	int m_nSavedValue;              // +0x6C
	BOOL m_bPassword;               // +0x70 the value is shown as '*'s
};

// vtable 0x002568CC. 0xB8 bytes.
class CSettingsBase
{
public:
	CSettingsBase();                                          // 0x000C05DD
	virtual ~CSettingsBase();                                 // 0x000C187F
	virtual HRESULT Init(CXBFont *font);                      // 0x000BEBB2
	// action is an InputAction; it is set to INPUT_NONE when used.
	virtual HRESULT HandleInput(int *action, SGamepadState **pad);   // 0x000C5BB0
	virtual HRESULT Render();                                 // 0x000C551A
	// Lays the page out again and forgets unsaved changes.
	virtual void Reset();                                     // 0x000C16C8
	virtual BOOL IsOpen();                                    // 0x000204BF
	virtual void SetItemEnabled(int index, BOOL enabled);     // 0x000204C8

	void SetTitle(const wchar_t *title);                      // 0x00020499
	// Appends a line (the pages construct and push_back a CSettingItem).
	void AddItem(const wchar_t *label, const wchar_t *value, int type, BOOL header, BOOL enabled,
	             BOOL visible = TRUE, int value2 = 0, BOOL password = FALSE)
	{
		m_items.push_back(CSettingItem(label, value, type, header, enabled, visible, value2, password));
	}
	void LoadSounds();                                        // 0x000C17F0
	BOOL IsSelectedSelectable();                              // 0x0002DDFC
	// The selected line can't be changed (disabled, a header or hidden).
	BOOL IsSelectedDisabled();                                // 0x00020455
	// Sets a line's value (if it differs); changed marks the line and the page
	// as modified, else the line's changed mark is cleared. The original
	// takes item in edi and text in ebx.
	void SetItemValue(CSettingItem *item, const wchar_t *text, BOOL changed);   // 0x000C004C
	void RevertChanges();                                     // 0x000BF231
	void ClearChanged();                                      // 0x000BEEFA
	void FreePopups();                                        // 0x000C1816
	void DrawBackground();                                    // 0x000BF2B5
	// Render's body (see SettingsBase.cpp).
	void RenderPage(float &labelWidth, BOOL musicPage);
	// B/BACK: asks "Save changes?" if anything changed, else closes.
	void AskToSave();                                         // 0x000C5499
	// Creates the message box on first use. The original passes this in edi.
	void ShowMessage(const char *text, const wchar_t *title, int buttons);   // 0x000C5358
	// Opens the IP address pop-up / the clock pop-up / the on-screen
	// keyboard (owner tells the keyboard whether it's a new input; the
	// original takes maxLength in eax, 0 = 64, and owner in edi).
	void ShowIPEditor(const wchar_t *address, const wchar_t *title);   // 0x000CB39E
	void ShowClockEditor();                                   // 0x000CB434
	// filters is "ext" or "ext1|ext2|...". The original takes this in edi.
	void ShowFileBrowser(const wchar_t *title, int unknown124, int option11C, int unknown120,
	                     const char *startPath, const char *filters);   // 0x000D2057
	void OpenKeyboard(const wchar_t *title, const wchar_t *text, BOOL password, int maxLength,
	                  int owner);                             // 0x000BFFE0
	// The end of each page's Save (repeated in each in the original):
	// reports error, or writes Config.xml (when changed) and says whether
	// a restart is needed. Returns ok.
	BOOL FinishSave(BOOL ok, const char *error, BOOL changed, BOOL needRestart);
	void ShowSavedMessage();                                  // 0x000C54E0
	void ShowRestartMessage();                                // 0x000C54FD

	float m_fX;                     // +0x04
	float m_fY;                     // +0x08
	float m_fWidth;                 // +0x0C
	float m_fHeight;                // +0x10
	CXBFont *m_pFont;               // +0x14
	CSound *m_pSoundMove;           // +0x18 "Scroll"
	CSound *m_pSoundSelect;         // +0x1C "Select"
	BOOL m_bClosed;                 // +0x20
	int m_nSelected;                // +0x24
	int m_nFirstVisible;            // +0x28
	int m_nVisibleCount;            // +0x2C
	int m_nLastVisible;             // +0x30
	wchar_t m_szTitle[30];          // +0x34
	BOOL m_bKeyboardOpen;           // +0x70 the virtual keyboard edits a value
	BOOL m_bAskSave;                // +0x74 the message box is the save question
	BOOL m_bModified;               // +0x78
	BOOL m_bMessageBoxOpen;         // +0x7C
	CMessageBox *m_pMessageBox;     // +0x80
	BOOL m_bIPEditorOpen;           // +0x84
	CIPAddressEditor *m_pIPEditor;  // +0x88
	BOOL m_bClockEditorOpen;        // +0x8C
	CClockEditor *m_pClockEditor;   // +0x90
	BOOL m_bFileBrowserOpen;        // +0x94
	CFileBrowser *m_pFileBrowser;   // +0x98 "Browse XBE"
	BOOL m_bSelectionMoved;         // +0x9C set by HandleInput
	IDirect3DTexture8 *m_pBackground;   // +0xA0 the page's icon, set by CSettingsMenu
	std::vector<CSettingItem> m_items;  // +0xA4
	float m_fLabelWidth;            // +0xB4 widest label + 20, worked out on the first Render
};

// "Yes", "True" or "1" / "No", "False" or "0" (any case). NULL is neither.
// The originals take text in esi.
BOOL IsYesW(const wchar_t *text);                             // 0x0002DD70
BOOL IsNoW(const wchar_t *text);                              // 0x0002039A
// *element, or (if NULL) a new child of parent called name. FALSE if parent
// is NULL or the child couldn't be made. The original takes element in esi.
BOOL GetOrAddChildElement(const char *name, CXmlElement *parent, CXmlElement **element);   // 0x000C5E51
