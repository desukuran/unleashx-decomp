# Text editor

`CNotePad` (`src/app/NotePad.cpp`) is the text editor: mode 8 (`MODE_TEXTEDITOR`).
The `TextEditor` action opens it, and so do the file manager's text files.
It also shows the file operation log (`Load`).

## Window

The window is 630x470 at (5, 5), scaled to the screen. From top to bottom it has:

* a caption with the file name ("New Text Document" for a new file);
* the text area;
* a status line: `Ln n  Col n` on the left (both counted from 0), and the size in bytes on the right;
* the embedded on-screen keyboard, when it's shown.

The size is the character count plus 2 per line break, so the CRLF a save writes is counted.

The text area scrolls sideways when the cursor is past column 40. All lines scroll together, by the cursor line's offset.

The cursor is an I-beam. It shows for 0.75 s of every second.

## Controls

| Input | Effect |
|---|---|
| D-pad, triggers | move the cursor (triggers: up/down) |
| Left stick | like the d-pad while the keyboard is hidden; walks the keyboard's keys while it's shown (repeats every 0.15 s) |
| A (keyboard shown) | types the highlighted key |
| B | delete (at the end of a line it joins the next line) |
| X | backspace (at the start of a line it joins the line above) |
| Y | line break |
| BLACK | tab |
| WHITE | show or hide the keyboard |
| Right stick click | gives the keyboard all of the input, or takes it back |
| START | the menu: New Document, Open Document..., Save, Save As..., Exit |
| BACK | exit; asks "save changes?" if the text was modified |

With a debug keyboard you type directly. These keys also work:

* Enter, Backspace, Delete, Tab
* the arrows, Home, End, PgUp, PgDn
* the Windows keys open the menu
* Esc exits

A new document shows the keyboard unless a debug keyboard is plugged in.

## Files

**Open** uses the file browser ("Open Text File"), limited to `.txt`, `.ini` and `.xml`.

**Saving** writes the lines as narrow text joined with CRLF. It creates the folder if it's missing.

**File names** are typed on the global keyboard:

* A file with no name yet suggests the first free `C:\FileNNNN.txt`.
* "Save As" refuses a name that already exists ("'name' Already Exists.").

The "save changes?" box offers Yes, No and Cancel.

* **Yes** saves, asking for a name first if there isn't one, and then continues with New, Open or Exit.
* **No** continues without saving.

## Quirks kept

* **Trailing empty lines grow.** An empty last line still gets a CRLF, so a file that ends in a newline gains an empty line each time it's opened and saved.
* **Missing files open empty.** A file that can't be opened shows as an empty document with no lines. `Open` only fails for an empty path.
* **Multibyte lines get junk.** Lines are converted from UTF-8 with as many characters as they had bytes, so a line with multibyte characters ends in junk.
* **Text width is fixed.** The width of the text area is computed once, the first time the editor is drawn.
* **PgUp can stall.** PgUp can wrap the top line below 0. `MoveUp` then counts it back down one step at a time.
* **Modified flags are off in two places.**
  * Saving an empty document doesn't clear "modified".
  * "Save As" sets it.
