// A spinner: a box showing one of a list of strings, changed with up/down
// (with arrows that pulse while it has the focus). The IP address editor
// and the clock editor are built from these. 0x54 bytes.
#pragma once

#include <string>
#include <vector>

#include "../common/Platform.h"

class CAppSettings;
class CSound;
class CXBFont;
struct IDirect3DTexture8;
struct SGamepadState;

class CUpDown
{
public:
	CUpDown();                                            // 0x000D84A1
	virtual ~CUpDown();                                   // 0x000D80B4
	// width/height 0 = fit the widest item / the font. Font NULL = MainFont.
	// (this in esi, font in eax)
	HRESULT Create(CXBFont *font, float x, float y, float width, float height);   // 0x000D81C2
	HRESULT AddItem(const wchar_t *text);                 // 0x000D8398
	// Unused. Adds wcslen(text) items, the i-th being text + i.
	HRESULT AddItems(const wchar_t *text);                // 0x000D8469
	// (this in edi, index in eax)
	HRESULT SetItemText(int index, const wchar_t *text);  // 0x000D8083
	// The selected item, or L"". (this in edi)
	const wchar_t *GetText();                             // 0x000D787C
	// (this in ebx)
	HRESULT Render();                                     // 0x000D78B8
	// Up/down (and the triggers) change the selection, skipping empty items.
	// (action in ecx, this in edx, pad in eax)
	HRESULT ProcessInput(int *action, SGamepadState **pad);   // 0x000D7EF8

	IDirect3DTexture8 *m_pTexture;    // +0x04 "Keyboard" (the key background)
	CSound *m_pSound;                 // +0x08 "Scroll"
	CXBFont *m_pFont;                 // +0x0C
	BOOL m_bCreated;                  // +0x10 Create does nothing when set (nothing sets it)
	BOOL m_bFocused;                  // +0x14
	std::vector<std::wstring> m_items;   // +0x18
	int m_nSelected;                  // +0x28
	int m_nLastMove;                  // +0x2C 1 up, -1 down, 0 after 2s idle (lights an arrow)
	float m_fPadX;                    // +0x30 8
	float m_fPadY;                    // +0x34 19
	CAppSettings *m_pSettings;        // +0x38
	float m_fY;                       // +0x3C
	float m_fX;                       // +0x40
	float m_fWidth;                   // +0x44 of the text, unscaled
	float m_fHeight;                  // +0x48
	float m_fArrowWidth;              // +0x4C
	float m_fArrowHeight;             // +0x50

	// The box's size: the text area scaled by the font scale plus the padding.
	float GetBoxWidth() const;        // inlined
	float GetBoxHeight() const;       // inlined

private:
	// The selection is an empty string. (this in edi)
	BOOL IsSelectedEmpty();                               // 0x000D7837
};
