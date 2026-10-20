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
| `bmp` `jpg` `png` `xbx` `xpr` | The picture viewer (below). |
| `zip` `rar` `xip` | Browsed like a folder. |
| `xm` `xbg` | The mesh viewer (below). |

## Archives

Opening a `zip`, `rar` or `xip` file shows its contents in a window
(`src/filemgr/ZipManager.cpp`, `RarManager.cpp`, `XipManager.cpp`). The
title is the archive's path. Each line shows the name (without its folders)
and the size: packed, then `/` and unpacked when the archive records it.
Entries are sorted by name, ignoring case. RAR folders aren't listed; zip
and xip folders are.

A zip's folders show up only when the zip has entries of their own (names
ending in `/`). UnleashX's own zipping writes files only, with their
relative paths, so its zips look flat here even though extracting them
recreates the folders. (Checked against the original: the listing thread
cuts each name at its last `/`, 0x000157A8.)

Password-protected zips can't be extracted: the XUnzip UnleashX linked has
no decryption, so their entries fail. The error box then shows "Error
Code: n" from `GetLastError()` on the drawing thread, not the extraction
thread, so the number (often 3) means nothing. (Same in this build: the
vendored zip_utils could decrypt, but UnleashX never passes a password.)

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

## Picture viewer

`src/filemgr/ImageViewer.cpp`. It opens a picture (`bmp`, `jpg`, `png`), an
`.xbx` (an `.xpr` holding one texture), or an `.xpr`, which shows each of
its named textures. Other extensions are tried as `.xpr`.

| Button | Effect |
|---|---|
| Left / Right | The previous / next texture of an `.xpr` (wraps around). |
| Thumbstick click | Zoom: in the window (the start), its own size (shrunk to fit the screen), stretched over the screen. |
| START | A slideshow of the picture's folder (`bmp`/`jpg`/`png` only; 5 seconds a picture, with transitions). An XboxDings sign before the caption shows it's available. |
| B, BACK | Close. |

In the window, the picture is scaled to the window's width, then shrunk to
fit its height. The caption shows its name and size, and for an `.xpr` the
texture's number (`3 of 7`). Resources that aren't textures say so.

Quirks:

* An `.xbx` that isn't a packed resource is loaded as a picture, but then
  isn't counted, so the window says "(No Image)".
* An `.xpr` has room for 20 textures, but the count isn't checked.
* A slideshow still running when the viewer closes is never freed.
* The image count and the last index shown are globals.

## Mesh viewer

`xbg` and `xm` files open in the mesh viewer (`src/filemgr/XMViewer.cpp`),
over the dimmed screen.

* **`.xbg`:** an XDK sample-framework mesh. Each frame is scaled to a unit
  sphere and centred on the origin.
* **`.xm`:** a raw dump: a 0x18-byte header (primitive type, primitive
  count, FVF, vertex size, vertex count, index count), the vertices, then
  16-bit indices. Only points and lines (primitive types 1 to 4) are
  accepted.

The model is untextured, drawn in the skin's `AMBIENT_CELLCOLOR`, 45 units
in front of the camera. The right stick turns it up to 90 degrees either
way. A, B or BACK closes the viewer.

Quirks:

* An `.xm` draws its vertices twice: once indexed, and once more as a list
  that takes the vertex count as the primitive count.
* No vertex shader is set for an `.xm`.
* A short index read leaves both buffers in place, so the model is drawn
  anyway.
* The error box is drawn under the model.
* The button that dismisses an error also closes the viewer.

## FTP manager

`src/filemgr/Ftp*.cpp`. The left pane is the file manager's own; the right
one lists the FTP sites, and once connected the site's files (through the
FTP client, `FTP.md`). Under the panes, while connected, is the transfer
queue.

### Sites

The site list is `FtpSrv.xml` in the data folder:

```xml
<FTPSite>
  <Site Host="192.168.0.2" Port="21" User="xbox" Pass="eGJveA==" PASV="Yes"
        RemotePath="/E/Games" LocalPath="E:\Games">Living room</Site>
</FTPSite>
```

* `Pass` is Base64.
* A site needs a host and a port.
* `RemotePath` is the folder to open after connecting.
* `LocalPath` is opened in the left pane when connecting, if it exists.

With `<FTPClient DetectPeer="Yes">` (the default) the list also shows the
Xboxes that answer the network scan, named after their reply (or `[ip]`).
Those can't be edited or deleted. `<FTPClient MinPort MaxPort>` set the
active-mode port range.

The site menu (START or WHITE) has these entries:

* **Connect** (also A on a site).
* **Quick Connect...:** the site editor without saving. The values are kept
  for the next quick connect.
* **New Site..., Edit Site..., Delete Site.** A name already in use gets
  `(1)`, `(2)` and so on.
* **Refresh:** scans the network again.
* **View Log...**
* **Switch to Local Browser:** back to the file manager.

The site editor ("FTP Site Manager") has these lines:

| Line | Notes |
|---|---|
| Site name | 30 characters; only for new sites. |
| Address | 100 characters. |
| Port | 1-65536. |
| User Name | |
| Password | Shown as stars. |
| PASV | A toggles it. |
| Remote Path | A `/` is put in front. |
| Local Path | Chosen with the folder browser; not for a quick connect. |

X clears the name, address, user and password. New sites start as port 21,
user and password `xbox`, passive mode, and open the keyboard for the name.

### Connected

| Button | Effect |
|---|---|
| A | Opens a folder (files don't open). |
| B | Up a folder (not while a transfer runs). |
| X | Mark (the first line can't be marked). |
| LT / RT | The left / right pane. |
| Left / Right (and the remote's) | Cycle left pane, right pane, queue. |
| Right stick click | The queue takes the input, or gives it back. |
| Left stick click | The FTP log. |
| START, WHITE | The menu. |
| BACK | "Disconnect and exit?" |

The menu has these entries:

* **Transfer:** runs the queue. If the queue is empty, the marked items (or
  the highlighted one) are queued first.
* **Add to Queue:** the active pane's marked items (or the highlighted one)
  go to the other side, as uploads or downloads.
* **Delete..., Rename..., Create Folder...:** in the active pane. Deleting
  locally lists the files and asks with the totals. Deleting on the FTP
  side asks directly, then `DELE`s files and removes folder trees.
* **Site Raw Command...:** sends a line of up to 80 characters and shows the
  reply.
* **Reboot, Shutdown, Power Cycle, Close Tray, Eject Tray, Drive Status:**
  `SITE REBOOT`, `SITE SHUTDOWN`, `SITE POWERCYCLE`, `SITE TRAYCLOSE`,
  `SITE EJECT` and `SITE DRIVESTAT`. These are UnleashX's own server
  commands; the reply is shown.
* **View Log..., Refresh, Disconnect.**

The queue lists source and target with an arrow (up or down). Its menu has
**Transfer**, **Remove** and **Clear Queue**. A transfer runs the items in
order. A failed item stays in the queue and the rest go on. The result
tells the files, bytes, time and KB/s for each direction.

### FTP log

The log keeps the last 1000 lines: commands, replies (green), errors (red)
and the client's own messages. The d-pad scrolls. The menu has **Close**,
**Clear** and **Save...** (a text file, by default the first free
`C:\LogNNNN.txt`).

### FTP quirks

* A multi-line message is logged as several copies of its start, cut to
  each line's length.
* Scrolling the log stops 12 lines before the end.
* A failed new folder is reported with the rename message.
* The download time in the result divides by 6000, so its minutes are ten
  times too high.
* The transfer uses the file-copy LED, not the FTP ones.
* The queue's highlight loses part of its alpha when the queue is active.
* A target path on the FTP side that doesn't start with `/` is replaced by
  `/`.

### USB drives

On a USB drive (P: to S:) copying works both ways and files open in the
viewers that read through the file layer (pictures, music), but Delete and
New folder don't: they call the kernel's `DeleteFileA`/`RemoveDirectoryA`
(0x0012A006) and `CreateDirectoryTree` (0x00127E8E), which can't see those
drives, and report an error. The text editor and the video player have the
same limit (see TEXT_EDITOR.md, VIDEO.md); FTP transfers too (FTP.md). As in
the original.

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
    `.bak` (`PatchXbe`, `src/system/XbePatch.cpp`).
    * **Allowed media:** the certificate's allowed media becomes
      0x400001FF. The certificate is assumed to start at the image header's
      size.
    * **Code patches:** the first match of each of four byte patterns is
      replaced. Two turn a `jge` after the media check into `jmp`, one
      clears a `FATX....D:\` string, one turns `test al, 1 / jnz` into nops.
    * **Result:** "patched" means at least one pattern was found.
  * **Rename XBE title:** changes the title stored in the XBE. Quirk: 41
    characters are written into the 40-character field, which clears the low
    word of the first alternate title ID.
* **Text:** **New text document** or **Open with text editor**.
* **Format drive:** only in Root, for drives other than C: and D: that have
  no size information or are H: and up. It asks first.
* **Switch to FTP:** the FTP manager (below). Needs the network.

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
