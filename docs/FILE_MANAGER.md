# The file manager

This page describes the file manager (`FileManager` action, or `LaunchDVD`
with a data disc). It was written from the decompiled code in
`src/filemgr/`. The copy, move, delete and zip work itself is described in
`FILE_OPERATIONS.md`; this page covers the screen around it.

## Screen

The screen has two panes, A (left) and B (right).

* **Header:** the active pane's folder and its file and folder counts, with
  the total size.
* **Footer:** the highlighted item's name, size and date.
* **Panes:** in "Root" a pane lists the drives. The size column shows free
  space (used space for the DVD) and the date column shows the drive's total
  size. Unformatted memory units and USB drives appear as
  `X [Not formatted]`.
* **Pane order:** each folder lists `..`, then its folders, then its files.
  Folders show the skin's `Folder.png` and files `File.png`.

The panes are either side by side or one at a time; when one shows, it gets
the whole width and also the dates. The skin's `<FileManager>` settings
choose the frame (`SafeBorder`: a wider margin) and whether frames are drawn
over a background picture (`DrawBorder`).

## Buttons

| Button | Effect |
|---|---|
| Up/Down, left stick | Move. The stick only counts when pushed all the way. |
| A | Enter a folder (`..` goes up) or open a file (below). `D` only opens with a readable, non-audio disc. |
| X | Mark the item and move down. |
| BLACK | Invert the marks. |
| B | Up one folder. |
| LT / RT, remote Left/Right | Make the left / right pane active. |
| Right stick click, remote DISPLAY | Side by side, or one pane. |
| START, WHITE | The context menu. |
| BACK | Leave. |

You can also use the pointer:

* A click highlights an item, and a second click marks it.
* A double click opens it.
* Clicking the other pane switches to it.

## Opening files

| Extension | Opens in |
|---|---|
| `xbe` | Launched. |
| `wav` `wma` `mp3` | The music player. |
| `wmv` `xmv` | The video player, full screen; the music pauses. |
| `txt` `xml` `ini` `nfo` `cfg` `lst` | The text editor. |
| `bmp` `jpg` `png` `xbx` `xpr` | The picture viewer. |
| `zip` `rar` `xip` | Browsed like a folder. |
| `xm` `xbg` | The module player. |

## Archives

Opening a `zip`, `rar` or `xip` file shows its contents in a window
(`src/filemgr/ZipManager.cpp`, `RarManager.cpp`, `XipManager.cpp`). The
title is the archive's path. Each line shows the name (without its folders)
and the size: packed, then `/` and unpacked when the archive records it.
Entries are sorted by name, ignoring case. RAR folders aren't listed; zip
and xip folders are.

| Button | Effect |
|---|---|
| Up/Down, LT/RT, left stick | Move. |
| X | Mark the entry and move down. Marked entries are drawn highlighted. |
| A, START, WHITE | The menu ("Zip Explorer", or "Rar Explorer" for RAR). |
| B, BACK | Close. |

The menu has four entries:

* **Extract selected File(s):** extracts into a folder next to the archive,
  named after it (`E:\a\b.zip` goes to `E:\a\b\`).
* **Extract selected File(s) to...:** asks for the folder first.
* **Extract all Files** and **Extract all Files to...:** the same, for all
  entries.

"Selected" means the marked entries, or the highlighted one when nothing is
marked. A progress box shows the current file and the count of files done.
The result box reports how many files were "unzipped", "unRarred" or
"extracted".

Zip entries keep their folders, with `:` turned into `_`. RAR entries keep
theirs too, except that when the archive starts with a folder that folder is
the base, which is dropped. An "extract all" of a RAR is one unrarlib call.

### Archive quirks

* The xip browser's menu is also titled "Zip Explorer".
* The zip and RAR browsers have code to cancel a running extract with B,
  but it's never reached: buttons are ignored while an extract runs. Only
  the global cancel flag stops a RAR extract.
* The xip browser doesn't create the archive's subfolders, so files in them
  fail to extract.
* The zip browser has a hidden "add to zip" operation (menu index 6) that no
  entry reaches. The xip browser checks for it too.
* RAR: the base folder is also replaced in the *target* path. Extracting
  only the highlighted entry ignores the base folder. If its old file can't
  be deleted, the browser creates a folder named after the file in the
  current directory. Marked files get their attributes reset on their
  folder, not the file.
* RAR: the listing never frees unrarlib's list, and each entry's path leaks
  a `_strdup` copy.
* Unless a message is showing, an archive with no entries closes as soon as
  it's listed.

## Context menu

Entries that don't apply are greyed out.

* **Folders and marks:**
  * **Root folder:** back to the drive list.
  * **Sort:** by name, size, type or date. Choosing the same order again
    reverses it.
  * **Select all / Clear selection / Inverse selection:** the marks.
* **File operations:** each works on the marked items of the active pane,
  or on the highlighted one when nothing is marked.
  * **Copy / Copy (resume) / Move:** into the other pane's folder.
    * The folders must differ and the target drive must be there.
    * Before starting, the manager lists the files and checks the free space.
      It then asks for confirmation with the file and folder counts and the
      total size.
    * "Copy (resume)" skips files already there with the same size.
  * **Delete:** asks first, like a copy.
  * **Rename / New folder:** the keyboard asks for the name.
  * **Calculate used space:** the files, folders and bytes under the
    selection.
* **Disc:**
  * **Copy disc:** asks for the target, starting from the active folder or
    `E:\Games\`.
  * **Quick copy disc:** copies to the default games folder. See
    `FILE_OPERATIONS.md` (Copy DVD).
* **Zip:**
  * **Add to zip file:** suggests a name from the highlighted item, or else
    the folder. A relative name goes into the current folder, a folder gets
    `new.zip`, and `.zip` is added when missing.
* **XBE:**
  * **Patch XBE file:** patches a hard disk XBE and keeps the original as
    `.bak`.
  * **Rename XBE title:** changes the title stored in the XBE.
* **Text:** **New text document** or **Open with text editor**.
* **Format drive:** only in Root, for drives other than C: and D: that have
  no size information or are H: and up. It asks first.
* **Switch to FTP:** the FTP manager. Needs the network.

While an operation runs, a box shows its progress. The current file and
target are shown, with the file's and the overall percentages. When it
finishes, the result says what was done. If anything was logged, it offers
to show the log in the text editor.

## Quirks

* While an operation is running, **X** cancels it.
* Copies may reuse the last file list when nothing has changed since.
  Moves and deletes always list again.
* The zip name stops at the first `.` in the whole path, not just in the
  file name.
* In Root, the second drive's line shows the DVD tray state, whichever drive
  that is.
* After a Copy DVD, the result box is shown again on every frame until it
  closes. Its error message is the game saves' copy error.
* Both format results are titled "Success".
* "Copy (resume)" closes the destination's search handle with `CloseHandle`
  instead of `FindClose`.
* Name sorting compares names without case, and a second file with the same
  name in a listing is dropped. Sorting by date compares only the low 32 bits
  of the time.
