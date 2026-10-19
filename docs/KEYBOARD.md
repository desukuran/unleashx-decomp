# On-screen keyboard

`CVirtualKeyboard` (`src/ui/VirtualKeyboard.cpp`) handles text entry
everywhere: settings, the startup password, the menu's keyboard actions,
and the text editor. It descends from the XDK's virtual keyboard sample:
the key codes and the English strings ("Sample graphics. Don't use in your
game" is still in the table) come from there. UnleashX restyled it with the
skin's `KB_*` colors and added password, numbers-only and embedded modes.

## Layouts

Three layouts, each four rows (switch with WHITE or the third key of row 3):

| | Row 1 | Row 2 | Row 3 | Row 4 |
|---|---|---|---|---|
| Alphabet | SHIFT A–J 1 2 3 | CAPS LOCK K–T 4 5 6 | SYMBOLS U–Z ‹ › 7 8 9 | DONE Space Backspace 0 . |
| Symbols | SHIFT ! @ # $ % ^ & ( ) _ * + - | CAPS LOCK / \ [ ] £ ¥ \| ; : ' " < > | ACCENTS ? ¿ ¡ = { } ‹ › « » , | DONE Space Backspace 0 . |
| Accents | SHIFT à á â ã ä å æ ç è é © ¢ ® | CAPS LOCK ê ë ì í î ï ñ ò ó ô ¹ ² ³ | ALPHABET õ ö ø ù ú û ‹ › ü ý ÿ | DONE Space Backspace 0 . |

‹ and › move the text cursor. SHIFT applies to the next character, and
CAPS LOCK stays on. With exactly one of them on, letters are upper case.
SHIFT and CAPS LOCK are greyed out on the symbols layout.

## Controls

| Input | Action |
|---|---|
| D-pad | move the highlight (wraps; rows of different widths keep a column) |
| A | press the highlighted key |
| B | delete the character after the cursor |
| X | backspace |
| Y | SHIFT |
| WHITE | next layout (Alphabet ↔ Symbols) |
| BLACK | space |
| Left / right trigger, left stick sideways | move the text cursor |
| Stick click | CAPS LOCK |
| START | done |
| BACK | cancel |
| Remote 0–9 | type the digit |
| Debug keyboard | type directly; Enter = done, Backspace/Delete, the arrows, Caps Lock |

## Modes

* **Password**: the text shows as stars.
* **Numbers only** (settings like IP parts, ports and years): only the
  digits, `.`, the cursor keys and Backspace work. The highlight starts on
  `1`.
* **Embedded** (the text editor): no box, title or text field; the keys
  go to the editor instead.

Text stops being accepted at `m_nMaxLength` characters (64 by default),
or when the text field is full.
