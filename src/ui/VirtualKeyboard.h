// The on-screen keyboard (CVirtualKeyboard): grew out of the XDK's virtual
// keyboard sample (its key codes and English strings are still here), with
// UnleashX's colors, password masking, a numbers-only mode, an "embedded"
// mode for the text editor (keys go to a CVirtualKeyboardEvent instead of
// the text field), the IR remote and the debug keyboard.
//
// Three layouts of four rows: letters and digits, symbols, accented
// letters. A selects the highlighted key, B deletes, X is backspace, Y
// shift, the stick clicks caps lock, WHITE switches layouts, BLACK is a
// space, the triggers move the text cursor, START is Done, BACK cancels.
#pragma once

#include <string>
#include <vector>

#include "../app/XBApp.h"
#include "../common/Platform.h"

class CXBFont;
class CXBPackedResource;
class CSound;
class CAppSettings;
struct IDirect3DTexture8;
struct SIRRemote;
struct SKeystroke;

// Key codes beyond characters.
#define KEY_BACKSPACE 0x10000
#define KEY_DELETE 0x10001
#define KEY_SHIFT 0x10002
#define KEY_CAPSLOCK 0x10003
#define KEY_ALPHABET 0x10004
#define KEY_SYMBOLS 0x10005
#define KEY_ACCENTS 0x10006
#define KEY_DONE 0x10007
#define KEY_LAYOUT3 0x1000A    // a fourth layout; no key uses it
#define KEY_LEFT 0x22B2        // move the text cursor
#define KEY_RIGHT 0x22B3

enum KeyboardLayout
{
	KEYBOARD_ALPHABET = 0,
	KEYBOARD_SYMBOLS = 1,
	KEYBOARD_ACCENTS = 2,
};

// A key. 0x24 bytes.
struct SKeyboardKey
{
	int code;               // +0x00 character or KEY_*
	int width;              // +0x04 pixels (before scaling)
	std::wstring label;     // +0x08 for the special keys; empty = the character
};
typedef std::vector<SKeyboardKey> KeyboardRow;
typedef std::vector<KeyboardRow> KeyboardLayoutRows;

// A language's strings (the sample's table; only English). 0xC bytes.
struct SKeyboardLanguage
{
	DWORD languageId;       // +0x00
	DWORD unknown04;        // +0x04
	const wchar_t *const *strings;   // +0x08 21 strings, see GetKeyboardString
};

// Receives the keys when the keyboard drives something else (the text
// editor). (RTTI CVirtualKeyboardEvent)
class CVirtualKeyboardEvent
{
public:
	virtual ~CVirtualKeyboardEvent() {}           // slot 0, 0x00024813
	virtual void OnChar(wchar_t ch) = 0;           // slot 1
	virtual void OnDelete() = 0;                   // slot 2
	virtual void OnBackspace() = 0;                // slot 3
	// The debug keyboard's virtual key (Backspace, Enter, the arrows).
	virtual void OnKey(int virtualKey) = 0;        // slot 4
};

// 0x14C bytes.
class CVirtualKeyboard
{
public:
	CVirtualKeyboard();                                   // 0x000DE5B4 (this in edi)

	// Sets it up: the font, the key texture (else "Keyboard" from resource,
	// if given), the position (0 = centered/bottom) and the event sink.
	// font2 isn't used.
	virtual HRESULT Create(CXBFont *font, CXBFont *font2, IDirect3DTexture8 *keys, CXBPackedResource *resource,
	                       float x, float y, CVirtualKeyboardEvent *events);   // slot 0, 0x000DFD78
	virtual HRESULT ProcessInput(int *action);            // slot 1, 0x000DE58D
	virtual HRESULT Render();                             // slot 2, 0x000DDA5A
	// Starts a new input: scales and places the keyboard, clears the text
	// and the done/cancelled state.
	virtual void Open();                                  // slot 3, 0x000DC9D7

	// The typed text as narrow text; "" if cancelled. (this in eax)
	std::string GetResult();                              // 0x00014242
	void SetTitle(const wchar_t *title);                  // 0x0001433E (this in esi)
	// Replaces the text and puts the cursor at its end. (this in ebx)
	void SetText(const wchar_t *text);                    // 0x00014302
	// The typed text, or "" if cancelled (or empty). (this in eax)
	const wchar_t *GetText();                             // 0x0001421F
	BOOL IsDone() const { return m_bDone || m_bCancelled; }   // 0x00014325 (this in eax)

	int m_unknown04;                       // +0x004
	CVirtualKeyboardEvent *m_pEvents;      // +0x008
	BOOL m_bEmbedded;                      // +0x00C no frame, title or text field
	int m_unknown10;                       // +0x010
	CXBFont *m_pFont;                      // +0x014
	BOOL m_bCapsLock;                      // +0x018
	BOOL m_bShift;                         // +0x01C for the next character
	BOOL m_bHidden;                        // +0x020 not drawn, ignores the gamepad
	int m_nLanguage;                       // +0x024
	DWORD m_dwLanguageId;                  // +0x028
	std::wstring m_strText;                // +0x02C
	UINT m_nCursor;                        // +0x048
	std::vector<KeyboardLayoutRows> m_layouts;   // +0x04C
	int m_nLayout;                         // +0x05C KeyboardLayout
	UINT m_nRow;                           // +0x060 the highlighted key
	UINT m_nCol;                           // +0x064
	UINT m_nLastCol;                       // +0x068 for moving up and down
	int m_unknown6C;                       // +0x06C
	CXBStopWatch m_repeatTimer;            // +0x070 (only used by unused code)
	float m_fRepeatDelay;                  // +0x090 0.333, then 0.085
	int m_unknown94;                       // +0x094
	CXBStopWatch m_cursorTimer;            // +0x098 the cursor's blink
	IDirect3DTexture8 *m_pKeyTexture;      // +0x0B8
	CSound *m_pClickSound;                 // +0x0BC "Keyboard Stroke"
	BOOL m_bClickSound;                    // +0x0C0
	const SKeyboardLanguage *m_pLanguage;  // +0x0C4
	float m_fKeyHeight;                    // +0x0C8
	float m_fKeyWidth;                     // +0x0CC the "W" key
	UINT m_nRows;                          // +0x0D0 4
	float m_fScaleX;                       // +0x0D4 the screen's scale (at least 1)
	float m_fScaleY;                       // +0x0D8
	int m_unknownDC[5];                    // +0x0DC
	std::wstring m_strTitle;               // +0x0F0
	SIRRemote *m_pIR;                      // +0x10C its key codes type characters too
	CAppSettings *m_pSettings;             // +0x110
	SKeystroke *m_pKeystroke;              // +0x114 the debug keyboard
	float m_fX;                            // +0x118 the box
	float m_fY;                            // +0x11C (below the title)
	float m_fWidth;                        // +0x120
	float m_fHeight;                       // +0x124
	float m_fTextHeight;                   // +0x128 the text field
	float m_fTextWidth;                    // +0x12C
	float m_fKeysX;                        // +0x130 the keys' margin in the box
	float m_fTitleHeight;                  // +0x134
	BOOL m_bCancelled;                     // +0x138 BACK
	BOOL m_bDone;                          // +0x13C START or Done
	BOOL m_bNumeric;                       // +0x140 digits (and '.') only
	BOOL m_bPassword;                      // +0x144 shown as stars
	UINT m_nMaxLength;                     // +0x148 64

private:
	void SetLanguage(int language);                       // 0x000DBE8C (language in eax, this in ecx)
	wchar_t ApplyCase(int ch);                            // 0x000DBB39 (this in eax)
	void PlayClick();                                     // 0x000DBEB2 (this in eax)
	void BuildLayouts();                                  // 0x000DE720 (this on the stack)
	void DrawFrame();                                     // 0x000DC0DF (this in eax)
	void DrawTextBox();                                   // 0x000DC27D (this in eax)
	// (this in ebx)
	void DrawKey(float x, float y, const SKeyboardKey &key, DWORD color, DWORD textColor);   // 0x000DC3CB
	void DrawText(float x, float y);                      // 0x000DCD8B (this on the stack)
	void RenderKeys();                                    // 0x000DD35E (this in eax)
	void PressKey(int code);                              // 0x000DD605 (this in ebx)
	BOOL IsKeyDisabled();                                 // 0x000DDB0B (this in eax) the highlighted one
	void RememberColumn();                                // 0x000DDBDD (this in esi)
	void MoveRight();                                     // 0x000DDC52 (this in eax)
	void MoveLeft();                                      // (inlined in HandleAction)
	void MoveDown();                                      // 0x000DDCC9 (this in esi)
	void MoveUp();                                        // 0x000DDDF7 (this in esi)
	void PressSelected();                                 // 0x000DDF85 (this in esi)
	void HandleAction(int *action);                       // 0x000DDFCD (this in ecx, action in eax)
	int HandleKeystroke();                                // 0x000DE1EA (this in eax)
	SKeyboardKey &GetSelectedKey() { return m_layouts[m_nLayout][m_nRow][m_nCol]; }
};

extern CVirtualKeyboard g_virtualKeyboard;               // 0x00278328
extern BOOL g_bKeyboardAccepted;                         // 0x0027A5CC TODO: verify (also read for the passcode pad)

// An argument that is "KBResult" becomes the keyboard's text (trimmed);
// "$KBResult$" inside it is replaced by that text.
void ApplyKeyboardResult(std::string &arg);
