# Launching programs

How UnleashX starts another XBE, written from `LaunchXbe`
(`src/app/MenuActions.cpp`), `ShowLaunchScreen` and `CUnleashX::LaunchDVD`
(`src/app/LaunchScreen.cpp`) and the launchers in `src/system/XbeLaunch.cpp`.

## The steps

`LaunchXbe(path, name, region, args)` is what menu items, the recent list,
the file manager, FTP `SITE EXEC` and the disc auto-launch all call:

1. A missing file fails right away.
2. With `<Preference AutoVideoMode>`, the XBE's region (and a missing name)
   is read from its certificate.
3. The background threads are told to stop, and the loading screen plays.
4. The dashboard is cleaned up, and the video standard is switched to the
   game's region when needed (see `docs/MENU_ACTIONS.md`).
5. `LaunchXbeImage` starts the program. If control comes back, the launch
   failed: after half a second UnleashX reboots (back to the default
   dashboard when it runs from `D:`, otherwise with an SMC reset).

## The loading screen

Set by the skin's `<LoadingScreen>` (`docs/SKIN_FORMAT.md`). The music stops
and the flicker filter is softened. Then:

* If the loading preview names an existing video file, it plays once, full
  screen. With `<BackGround>GamePreview`, the selected item's `Preview`
  attribute replaces it. A or START skips the video.
* Otherwise the loading picture fades in over black. That's the skin's
  loading background, or the item's icon with `GameIcon`. The fade runs in
  80 steps of 20 ms (1.6 s); A or START cuts it short.

With `ShowInfo`, the dialog font shows "Loading..." (string 0x6D) centered
at a third of the screen's height. The title is centered below it, 1.5 times
as large: the item's name, else the XBE's certificate title.

On SD screens (640 wide or less) the last frame is kept on screen while the
next program starts (`PersistDisplay`).

## Where D: points

A program expects its files on `D:`, so before any launch `\??\D:` is
re-linked to the XBE's folder and the kernel is asked to run
`D:\<file name>` (lowercased):

| Drive | Device |
|---|---|
| `C:` | `\Device\Harddisk0\Partition2` |
| `D:` | `\Device\Cdrom0` |
| `E:` | `\Device\Harddisk0\Partition1` |
| `F:` | `\Device\Harddisk0\Partition6` |
| `G:` | `\Device\Harddisk0\Partition7` |
| `h:`..`z:` (lowercase) | `\Device\Harddisk0\Partition8`.. (letter − `a` + 1) |
| no drive | `\Device\Cdrom0\<path>` |

Quirk: only lowercase letters work beyond `G:`. `H:\x.xbe` gives an empty
device path and the link fails. When the link can't be made, the launcher
returns the status with `0x4000000` set.

## Launch data

`XLaunchNewImage` passes no data. To hand a program something, the launchers
write the title info for the next boot instead (`XWriteTitleInfoAndReboot`,
internal XAPI) and quick-reboot into it:

* **Arguments** (an item's `Arg1`): 0xC00 bytes laid out as follows.

  | Offset | Content |
  |---|---|
  | +0 | magic `0xEE456777` |
  | +4 | the argument string |
  | +0x2F4 | the byte 1 |

  The launch type is `LDT_TITLE`.
* **Live Update** starts `UXUpdate.xbe` with the following data, as
  `LDT_FROM_UPDATE`.

  | Offset | Content |
  |---|---|
  | +0 | UnleashX's title ID |
  | +4 | the target path |
  | +0x130 | the file name |
  | +0x2F4 | the byte 4 |

  This launcher also leaves `Z:\LaunchXbe.txt` (path and type) behind, plus
  `Z:\NotExist.txt` when the path is missing: debugging leftovers.
* **The MS dashboard's music screen** (audio CDs, see below) gets an
  `LD_LAUNCH_DASHBOARD` with reason `XLD_LAUNCH_DASHBOARD_MUSIC`.
  Wrappers for the settings and the memory screen exist but are never
  called.

## Discs (`LaunchDVD`)

Run by the `LaunchDVD` menu action ("manual") and when a disc is inserted.
An inserted disc only starts when its kind's `AutoLaunch` is on (the
`<Games>`, `<DVD>`, `<AudioCD>` and `<Data>` preferences, whose text is the
program to use);
holding a trigger while it's inserted skips it (`docs/MAINLOOP.md`). Nothing
happens while an update is being installed.

| Disc | What starts |
|---|---|
| Game | `D:\default.xbe`, named by the disc title. Inserted, a disc whose title is "UnleashX Xbox Launcher" or the one UnleashX itself runs from is ignored. |
| Video DVD | The `<DVD>` program, when set. |
| Audio CD | If the `<AudioCD>` program is the `<MSDashBoard>` path (as in the release Config.xml), the MS dashboard opens on its music screen. If it's empty, UnleashX's music player plays the CD. Otherwise that XBE starts. |
| Data | The `<Data>` program when set, else the file manager opens at `D:\`. |

Quirk: with both `<AudioCD>` and `<MSDashBoard>` empty, the two compare
equal. An audio CD then starts the default dashboard instead of playing in
UnleashX.

## Title images

`ExtractXbeTitleImage` saves an XBE's `$$XTIMAGE` section for the game scan's
cache. The section is found from the section headers in the first 256 KB of
the XBE; files under 10 KB are skipped. The image is handled by format:

* An XPR texture is loaded and written back out as an `.xpr` file.
* A DDS file is copied as it is, only when the target doesn't exist yet. It
  is deleted again if D3DX can't load it.

`LoadXbeIcon` loads the same section straight into a texture (menu icons,
FTP `SITE TITLES`). Unlike the extractor, it keeps looking at later sections
when one isn't usable.
