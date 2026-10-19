# Game saves manager

The `SavesManager` / `GameManager` menu action (application mode 3,
refused when UnleashX runs from DVD). It lists the games that have saved
data, in the style of the MS dashboard's memory screen, and lets you copy
or delete saves. The code is `CGameManager` in `src/app/GameManager.cpp`
(scan, copy, delete, input) and `src/app/GameManagerRender.cpp` (drawing).

## Scan

Opening the screen, and the Black button, run a scan on a thread. "Searching
for game saves" (`MSG_GAMESAVES_SEARCH`) shows while it runs.

1. **Hard disk.** Each folder of `E:\UDATA` that has a `TitleMeta.xbx` is a
   game. Its name is the `TitleName=` value in that file, which can be
   UTF-16 with a BOM or plain text (`ReadMetaFile`). The image is
   `TitleImage.xbx`, at 96x96.
   - Saves: the game's folder is mapped as `M:` and listed with
     `XFindFirstSaveGame`. Each save's image is its `saveimage.xbx`, at
     48x48, or the one in its parent folder.
   - Downloaded content: the same folder under `E:\TDATA`, mapped as `T:`
     and listed with `XFindFirstContent`.
   - The total size comes from `XGetDisplayBlocks` (16 KB blocks).
   - The hard disk games are sorted by name, case-insensitively.
2. **Memory units.** `H:` to `O:` are checked the same way for
   `TitleMeta.xbx`.

The first six games' images load right after the scan; the rest load when
they're drawn. Images go through `CImageManager`
(`src/render/ImageManager.cpp`), which is a reference-counted texture cache
keyed by path.

### Quirks

- Games on memory units never get save items. The scan reads each save's
  `SaveMeta.xbx` name and then drops it, so these games show "0 Saves".
- Content items are always empty: no title, path or size. Their constructor
  builds a temporary item and throws it away.
- A reload clears the list without freeing the old games.

## Screen

- **Header (top left):** "Xbox Game Saves" and the free space on `E:` (in
  KB under 10 MB).
- **Game list:** each game has a name plate, a picture and a grid of save
  pictures, four to a row and up to three rows. The list starts at the
  selected game. Each game below it is fainter than the one above, until
  the screen is full.
- **Grid arrows:** on the selected game's grid, arrows show when there are
  more rows above or below.
- **Info panel (left):** for a game, the name, "n Saves, n Contents", the
  size in KB and blocks, and the folder. For a selected save, its name,
  size, blocks and folder. The last line is "Hard disk" or "Mem Unit".
- **Button hint (bottom):** "J Reload Game Saves", where J is a
  button glyph in the XboxDings font (for Black). It sits after an empty space as wide
  as "Download Game Saves", a feature that was removed. Its code is still
  in the binary but nothing turns it on.
- **Selection frame:** the selected picture gets a pulsing frame.
- **Copy progress:** while a copy runs, a box shows "Copying. Please
  Wait..." and a progress bar (by bytes, or by files when sizes are
  unknown).

Some panel sizes use the wrong axis' scale factor (x for y and the other
way round). The code keeps them, so they only show at non-4:3 resolutions.

## Controls

| Button | Action |
|---|---|
| Up / Down, left stick | Previous / next game, or a row up / down in the grid |
| L / R triggers | Previous / next game, leaving the grid |
| Right | Into the grid (first save), then the next save in the row |
| Left | Previous save; from the first column, back to the game |
| A / Start | The "Select" menu: Copy (only on a save), Delete |
| Black | Rescan |
| B / Back | Close |

Sounds: "Game Save Select" when the game changes, "Memory Slot" when the
save changes, "Action Complete" on the first input after a copy finishes.
As a quirk, "Action Complete" also plays on the first input after a scan.

## Copy

"Copy To" lists the possible targets:
- the Xbox hard disk, which is only offered for a game on a memory unit;
- memory units 1A to 4B (`H:` to `O:`) that are inserted, except the one
  the game is on.

With no targets you get `MSG_GAMESAVES_INSERT_MU`. Content can't be copied
(`MSG_GAMESAVES_CONTENT`).

1. A memory unit needs more than 1 MB free (`MSG_GAMESAVES_NO_DEST`).
2. Then the save's size is checked against the target's free space
   (`MSG_GAMESSAVES_BAD_DEST`). Quirk: the size in KB is compared with the
   free bytes.
3. The game folder's own files (TitleMeta, TitleImage...) are copied to
   `<target>:\<title id>\` on a memory unit, or to `E:\UDATA\<title id>\`.
4. The save folder is copied with the file operations thread.

If the copy fails, `MSG_GAMESAVES_COPY_ERROR` shows the file and error code.

## Delete

Delete asks first. For a save, `MSG_GAMESAVES_DELETE_ITEM` shows its name
and blocks. For a whole game, `MSG_GAMESAVES_DELETE` shows the name,
blocks and the save and content counts. Quirk: the counts are never filled
in, so they show 0. "No" is selected by default.

With the folder mapped as `M:`, each save is first removed with
`XDeleteSaveGame` and then its folder is deleted. Quirk: `XDeleteSaveGame`
is given the save's name in quotes, so that call fails and the folder
delete does the work.

The result is `MSG_GAMESAVES_DELETE_SUCCESS` or `MSG_GAMESAVES_DELETE_ERROR`.
On success, the save or game is removed from the list. A deleted game is
never freed.
