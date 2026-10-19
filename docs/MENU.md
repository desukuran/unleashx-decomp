# The menu

How the `<Menu>` in Config.xml becomes the list on screen. This was written
from `CMenu` (`src/ui/Menu.cpp`) and the folder scan
(`src/ui/MenuScan.cpp`). What each `Action` does is in
[MENU_ACTIONS.md](MENU_ACTIONS.md).

```xml
<Menu>
  <Item Action="E:\Apps\XBMC\default.xbe" Icon="E:\Apps\XBMC\icon.xbx">XBMC</Item>
  <List Text="Games" Sort="on" DelayLoad="false">
    <Path>E:\Games</Path>
    <Path>F:\Games</Path>
  </List>
  <List Text="Tools">
    <Item Action="Settings">Settings</Item>
    <Item Action="filemanager" Arg1="E:\">File Manager</Item>
  </List>
</Menu>
```

## Items and lists

`<Item>` attributes:

| Attribute | |
|---|---|
| `Action` | What to run; see MENU_ACTIONS.md. An item without an Action is a label (shown but does nothing). |
| `Arg1`, `Arg2`, `Arg` | Arguments of the action. |
| `Text` | The item's text if the element has none. |
| `Icon`, `AltIcon` | Icon images (png, jpg, bmp, dds, `.xbx`/`.xpr` textures, or an `.xbe` for its title image). `AltIcon` wins if its file exists. Relative paths are relative to UnleashX's folder. |
| `Preview` | Preview video, for skins that show one. |
| `Region` | Game region (1 = NTSC-M, 2 = NTSC-J, 4 = PAL, 0 = homebrew, 7 = all). |
| `Password` | Asks for this text on the keyboard before running the item. |
| `Passcode` | Asks for this button code on the passcode pad. |

Unlocked items stay unlocked until the dashboard restarts. Locked items are
never added to the recent list.

`<List>` attributes and children:

| | |
|---|---|
| `Text` | The text shown for the sub-menu. |
| `Sort` | `off`, `no` or `false` keeps the items in file order. Otherwise items are sorted by text, ignoring case. Quirk: in a sorted list, an item with the same text as an earlier one is dropped. A `<Sort>off</Sort>` child changes it for the items after it. The root `<Menu>` is never sorted. |
| `DelayLoad` | `true`, `on` or `1`: the items' XBEs aren't scanned for icons the first time the list is opened. The attribute is then set to `False`. For `<Path>` lists it also selects the quick folder scan (below). |
| `Batch` | `true`: selecting the list runs all its items one after the other instead of opening it. |
| `<Path>` | A folder to scan for games (below). |

## Automatic icons and regions

Unless the item already has an `Icon`/`AltIcon`, the first time an XBE
item is shown UnleashX looks it up and stores the results on the item
(`Scanned="1"`):

* `Region` from the XBE certificate.
* `Icon`: `E:\UDATA\<title id>\TitleImage.xbx`. If missing, the title image is
  extracted from the XBE, and a `TitleMeta.xbx` with `TitleName=<name>` is
  written next to it. When running from DVD the icon goes to
  `Z:\Media\<title id>.xbx` instead.
* `AltIcon`: the first of `icon.png`, `icon.jpg`, `avalaunch_icon.png`,
  `mxm_thumb.jpg`, `default.png`, `default.jpg` in the XBE's folder.
* `Preview` (if the skin shows previews): the first of `preview.wmv`,
  `preview.xmv`, `mxm_ss.wmv`, `default.wmv` in the XBE's folder.

## Game folders

A `<Path>` inside a list is scanned for games, if it has a drive letter
(`/` works as well as `\`). The scan covers the folder's own `default.xbe` and the `default.xbe` of each
sub-folder (one level deep). Each XBE becomes an item named after the game's
title (or, without one, its folder name).

Scan results are kept in a cache, `items.xml` in UnleashX's folder:

```xml
<Cache>
  <Folder Path="E:\Games\" DelayLoad="False">
    <Item TStamp="42feb880" Action="E:\Games\Halo\default.xbe" Region="4"
          ID="4d530004" Icon="E:\UDATA\4d530004\TitleImage.xbx">Halo</Item>
  </Folder>
</Cache>
```

* An XBE is read again only when its icon is gone, or when its time stamp
  (`TStamp`) changed, in which case only the name and region are refreshed.
* Cached items whose XBE was deleted are removed, and so are menu items
  whose `.xbe` doesn't exist.
* A quick scan (the list has `DelayLoad="true"`, or UnleashX runs from DVD)
  doesn't re-read cached XBEs. It only looks for `icon.png`/`icon.jpg` and
  `preview.wmv`/`preview.xmv`. A full scan also tries `attractmode.xmv`
  first for the preview.
* Cached items are then merged into the menu list. An item already in the
  list (same `Action`) keeps its own text and attributes and only gets
  missing ones (name, `Icon`, `AltIcon`, `Preview`) plus the `ID`. New items
  are appended, or inserted right after the `<Path>` in the root `<Menu>`.

## Recent list

Launched XBEs are remembered in `recent.dat` (in the data folder): 10
entries of name, path, launch count and region, most recent first. Items on
`D:` aren't added.

## Controls

| Button | |
|---|---|
| A | Run the item / open the list. |
| B, BACK, remote LEFT | Back to the parent list. |
| D-pad up/down, triggers, left stick (fully up/down) | Move the selection; labels are skipped. With wrapping on, the ends wrap around. |
| D-pad left/right | Move the selection in horizontal menus. |
| START | The item's context menu (below). |
| X | The recent list (if `AllowRecentList`). |
| Remote 0-9 | Shortcuts: `<Settings><Shortcut><IR_1>...</IR_1>` in Config.xml, an XBE path or an action without spaces. |
| BACK during a file operation | Cancels it. |
| Y, remote INFO | The System Info screen (SETTINGS.md, "Xbox Info"). |
| BLACK | Pauses/resumes the preview video. |
| BLACK held + D-pad right | The next RSS news item. |
| WHITE | The music player, while soundtrack music plays. |
| Right stick click, remote PLAY | Starts the soundtrack player when it isn't running yet; with a preview playing, toggles it full screen. |
| Remote MENU (any screen), BACK (other screens) | Back to the menu. |

The debug keyboard works too: Enter and the Windows keys are START, Esc is
B, the arrows move, the Menu key is WHITE, Alt+F4 is BACK, and other keys
type.

With previews on, the selected item's `Preview` starts after the selection
has rested for half a second.

## Context menu

START opens a menu for the selected item:

| Entry | Available when |
|---|---|
| Select/Play | always |
| Launch in NTSC mode | an XBE with region 0, 4 or 7 (launches with region 1 and AutoVideoMode) |
| Launch in PAL mode | an XBE with region 0, 1-3 or 7 (launches with region 4) |
| Information... | always: type, command or the XBE's title, ID, region, folder size; icon, preview, protection |
| Backup to Harddisk... | running from DVD, on a game |
| Delete Game... | `AllowUninstall`, not from DVD, on a game: deletes the XBE's folder after a Yes |
| Refresh List... | always: rescans the game folders |
| Recent Titles... | `AllowRecentList`, not from DVD |
| Edit Item... | `AllowItemEdit`, not from DVD, not on a label |
| Open Containing Folder | `LinkToGame`, on an XBE: the file manager at the XBE's folder |
| Open Saves Folder | `LinkToSave`, on an XBE: the file manager at `E:\UDATA\<title id>` |

Quirk: the NTSC/PAL entries change the item's `Region` for the rest of the
session.

## Item editor

"Edit Item..." opens `CMenuItemEditor` (`src/app/MenuItemEditor.cpp`). It's a
Control Panel style page with these lines:

| Line | A | START / X |
|---|---|---|
| Title | keyboard (40 characters, must not be empty) | |
| Icon | file pop-up (`png`, `jpg`, `bmp`, `xbx`), starting in the current file's folder | Clear Entry, Download Online... |
| Preview | file pop-up (`xmv`, `wmv`) | Clear Entry, Download Online... |
| Password | keyboard (15 characters, masked) | Clear Entry |
| Passcode | the passcode pad | Clear Entry |
| Save / Reset / Exit | write, revert, close | |

Saving writes the changed values to two places: the menu's own copy of the
entry, and the file's copy.

* Games found by the game-folder scan are looked up in `D:\items.xml` by
  `Action`.
* Everything else is looked up in `Config.xml` under `<Menu>`. An `<Item>`
  is matched by `Action` and a `<List>` by `Text`.

What each line writes:

* The title is an `<Item>`'s text, or a `<List>`'s `Text` attribute. For a
  game, it's also written into the XBE (`SetXbeTitleName`).
* The icon is written as `AltIcon`, the others as `Preview`, `Password`
  and `Passcode`. An empty value removes the attribute.
* A changed password or passcode asks for a restart.

### Download Online...

This is for XBEs. It uses the title ID from `<Item ID>`, or reads it from
the XBE.

1. It fetches `http://www.xbox-skins.net/games/xml/<id>.xml`. If that fails,
   it sends `request.php?t=<title>&h=<id>&f=thumb|wmv` to ask for the game
   and says so.
2. The `<XBG>` element says what's available:
   * `Thumb="1"`: the icon `thumbs/<id>.png` is fetched.
   * `Video` and `VidID`: the preview `sendvid.php?sid=<VidID>` is fetched.
     `Video="2"` means an `.xmv`, else `.wmv`.
   * `VC` says how the preview is packed: 0 plain, 1 rar, else zip.
3. A thread installs the file next to the XBE:
   * the icon as `icon.png`, backing up the old one to `icon.bak`;
   * the preview as `preview.xmv`/`.wmv`, backing up to `preview.bak`.
     Packed previews are unpacked first into `ziptmp\` next to the download
     cache.
   * The new path goes into the line, and the new preview starts playing.

B cancels a download. `xboxskins.jpg` from the media folder is shown in the
progress box.

Quirks:

* Exit with unsaved changes warns "your changes will be lost", but Yes
  saves them. That's the Control Panel's Yes answer.
* When an item sits two or more lists deep in `Config.xml`, the save looks
  in the wrong list (only the nearest list's name is matched under
  `<Menu>`), so it fails.
* From an unpacked preview archive, the first file is installed whatever
  its type. The `.xmv`/`.wmv` test is inverted.
* The new title is copied over the menu entry's text without a length check.

## Passwords

`Password` items open the keyboard (masked, up to 50 characters);
`Passcode` items open the button pad. A wrong entry shows "The password
(passcode) you entered is incorrect!" and asks again. When the dashboard
asks for its own master password, `MicrosoftXBOX` is accepted.

## Menu-level actions

These actions are handled by the menu itself, not by the dispatcher in
MENU_ACTIONS.md:

| Action | |
|---|---|
| `AskUser` | A Yes/No box: caption from `Arg1` or `Caption`, text from the element, `Text` or `Arg2` (`\n` makes a new line). In a batch, anything but Yes/OK skips the rest of the current sub-list. |
| `MessageBox` | An OK box, the same way (without `Arg2`). |
| `InputBox` | The keyboard: title from `Arg1`/`Caption`, starting text from the element, `Text` or `Arg2`. The typed text is `$KBResult$` for the next items. |

After a `Format` or `rename` outside a batch the menu reports "Finished
formatting Drive ..."/"Failed to format Drive ..." or "... successfully
renamed to ..."/"Failed to rename ... to ...".

## Batches

`<List Batch="true">` runs its items in order, one per frame, waiting for
each file operation to finish. Nested lists run their items in turn. A
locked item asks for its password first. If an item fails, the batch stops
with "Batch process failed. The last item was: ...". BACK cancels the
running operation and the batch.
