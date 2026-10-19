# Menu actions

What an `<Item>` in Config.xml's `<Menu>` does when it is selected. This was
written from `CUnleashX::CMenuEvent::OnItemSelected`
(`src/app/MenuActions.cpp`, `0x000EE514`). Command names are
case-insensitive.

```xml
<Item Action="E:\Games\Halo\default.xbe" Region="4" Arg1="">Halo</Item>
<Item Action="filemanager" Arg1="E:\">File Manager</Item>
<Item Action="copy" Arg1="E:\a.txt" Arg2="F:\a.txt">Copy a file</Item>
```

## Launching an XBE

If the `Action` ends in `.xbe`, it is the path of the program to start:

* `/` is turned into `\` and doubled backslashes are collapsed.
* A missing file shows "Unable to launch "...". File does not exists."
* Items without `Password`/`Passcode` are added to the recent list
  (`recent.dat`, 10 entries, launch counts). Items on `D:` are never added.
* `Region` (a number) is the game's region. Leave it out, or use -1, to read
  it from the XBE. When `<Preference AutoVideoMode>` is on, UnleashX switches
  the video standard to match: 1 is NTSC-M, 2 is NTSC-J, 4 is PAL (60 Hz with
  `AutoVideoPAL60`), and other values use `MultiRegion`.
* `Arg1` is passed to the XBE as its launch argument.
* `Preview` is the video shown by a loading screen whose `<BackGround>` is
  `GamePreview`. With `GameIcon` the item's icon is shown.

The loading screen, the `D:` mapping and the launch data are described in
`docs/LAUNCHING.md`.

## Commands

| Action | What it does |
|---|---|
| `LaunchDVD` | Starts the disc in the drive (`docs/LAUNCHING.md`). |
| `filemanager` | Opens the file manager, at `Arg1` if given. |
| `SavesManager`, `GameManager` | Opens the game/saves manager (`docs/GAME_SAVES.md`). Refused when running from DVD. |
| `Settings` | Opens the settings menu. |
| `Skins`, `Skin` | Opens the skin chooser. |
| `reboot`, `restart` | Reboots: an SMC reset when UnleashX is the dashboard, otherwise back to the default dashboard. |
| `reset` | Relaunches the default dashboard (`XLaunchNewImage(NULL)`). |
| `powercycle` | SMC power cycle. |
| `shutdown` | Powers off (through the main loop). |
| `ftpstart`, `ftpstop`, `ftpreset` | Starts, stops or restarts the FTP server. |
| `netreset` | Stops FTP and the network, then starts the network again from the settings. |
| `trayopen`, `trayclose` | Ejects or loads the DVD tray. |
| `PrepareHD` | Sets up a new hard disk from the UnleashX CD: formats C, E, X, Y, Z (and F/G if confirmed) and copies `D:\CDrive`, `D:\EDrive`, `D:\FDrive`, `D:\GDrive` onto them. |
| `SetClock` | Opens the date/time screen. |
| `TextEditor` | Opens the text editor, on the file in `Arg1` or on a new file. |
| `Xenium-OS`, `XeniumOS` | Writes 1 to I/O port 0xEF and resets, which boots a Xenium modchip's own menu. |
| `X2IGR_OFF`, `X2IGR_ON` | Creates or deletes `E:\x2_IGR_disabled` (the X2 modchip's in-game-reset switch), then resets. |

## File commands

These take paths in `Arg1` (or `Arg`) and `Arg2`. Paths go through the same
rules as everywhere else (see `ResolvePath` in `src/common/FileUtil.cpp`):
`/` becomes `\`, relative paths are relative to UnleashX's folder, and so on.

| Action | Arguments | |
|---|---|---|
| `format` | `Arg1` = drive letter | Formats the partition. |
| `CopyDVD` | `Arg1` = destination (optional) | Copies the disc to the hard disk. |
| `copy` | `Arg1` source, `Arg2` destination | Copies a file or folder. |
| `Move` | `Arg1`, `Arg2` | Moves a file or folder. |
| `Rename` | `Arg1`, `Arg2` | `MoveFile`. |
| `Delete` | `Arg1` | Deletes a folder (recursively) or a file. |
| `zip` | `Arg1` source, `Arg2` zip file | |
| `unzip`, `UnRar` | `Arg1` archive, `Arg2` destination | Either command handles both formats: a `.rar` extension means RAR. |
| `VideoMode` | `Arg1` = `NTSC M`/`NTSC`, `NTSC J`, `PAL50`, `PAL60` or `PAL`; with `PAL`, `Arg2="60"` selects 60 Hz | Changes the video standard. Nothing happens if the box is already PAL, whatever the refresh rate. |

Source and destination must both be given and must differ.

### The on-screen keyboard

An argument that is exactly `KBResult` is replaced by the text last typed on
the on-screen keyboard, with surrounding spaces trimmed. Otherwise any
`$KBResult$` inside the argument is replaced by that text.

## `<Command>`

`<Command Action="filemanager" Arg1="...">` behaves like the `<Item>` with
the same attributes. Other `<Command>` actions are ignored here; the menu
code handles them.

## Errors

An unknown action, or a file command with bad arguments, shows "Unable to
execute command. Item: ... Action: ..." under the title "Invalid Command".
