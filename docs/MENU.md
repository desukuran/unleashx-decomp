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
