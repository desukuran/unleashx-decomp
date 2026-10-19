# Settings screens (Control Panel)

The settings screen (`Action="Settings"`) shows ten icons from
`CtlPanel.xpr`; each opens a page. Written from `src/app/SettingsBase.cpp`,
`src/app/Settings*.cpp` and the pop-ups (`IPAddressEditor.cpp`,
`ClockEditor.cpp`, `XboxInfo.cpp`, `src/ui/UpDown.cpp`).

## The icon screen

"Control Panel" lists, in order: AutoLaunch, Date/Time, AutoRun
(shortcuts), Live Update, Network, Screen Calibration, Music (sounds),
Storage (Harddisk), System and Display (video). The selected page's icon
is drawn large and faint behind the list, and becomes the background of the
page it opens. Up/Down (and the triggers, and the left stick) move, A opens
the page (it's created the first time), B or BACK closes the screen. A page
keeps all input until it closes. After Screen Adjust closes, the Control
Panel lays itself out again, since the visible area may have moved.

## Pages in general

A page is a dialog with a caption and a list of lines (label and value).
Section captions are centered and can't be selected. Lines that can't be
changed right now are grayed (they can still be selected).

* **Up/Down, the triggers, the left stick:** move the selection (skipping
  captions and hidden lines) and scroll the list.
* **A or START:** change the selected line: toggle Yes/No, step through a
  list of values, or open the on-screen keyboard, the IP address editor, the
  clock editor or the XBE browser.
* **X:** clear a path or text line (where the page allows it).
* **B or BACK:** close the page. If anything changed it asks "Your settings
  has changed... Do you want to save the changes?" (Yes / No / Cancel).
* The last section of every page has **Save**, **Reset** (undo the changes
  made since the page opened) and **Exit**.

Saving writes only the changed lines to `Config.xml` (creating missing
elements), updates the running settings, and then says either "Settings was
saved successfully." or "You need to restart your xbox for the settings to
take effect." When UnleashX runs from DVD, nothing is written ("Some
settings have been applied but you need to run this application from the
hard drive to persist the changes.").

## Video Settings

| Line | Values | Saved as |
| --- | --- | --- |
| Video Standard | NTSC M / NTSC J / PAL | the Xbox EEPROM (restart) |
| Format | Normal / Widescreen | `<Preference><VideoSettings Widescreen>` and the EEPROM |
| Enable 480p, 720p, 1080i | Yes / No (with the HDTV or SCART AV pack) | `Use480P`, `Use720P`, `Use1080I` and the EEPROM |
| Enable PAL-60 | Yes / No (PAL Xboxes only) | `UsePAL60` and the EEPROM |
| Faster Refresh Rate | Yes / No | `FastRefresh` (applied on save) |
| Compress Texture | Yes / No | `EnableTextureCompression` |
| Test | tries the chosen mode for 10 seconds | |
| Color | 16 bit / 32 bit | `Use16bit="True/False"` |
| Soften | Yes / No (applied at once) | `Soften` |
| Filter Level | 0-5 (applied at once) | `FilterLevel` |
| Auto Switch | Yes / No | `<Preference><AutoVideoMode>Yes</AutoVideoMode>` |
| Multi-Region Mode | Off / NTSC M / NTSC J / PAL | `<AutoVideoMode MultiRegion>` |
| Force PAL60 | Yes / No | `<AutoVideoMode UsePAL60>` |

The EEPROM lines (standard, format and the video modes) need a restart.
Quirks: leaving the page without saving keeps a Soften or Filter Level that
was tried out; a 16/32-bit change alone isn't applied until a restart.

## Date/Time Settings

The Clock Settings line opens the clock editor. 24H Format
(`<Preference><Use24HFormat>`), Autoset Clock (`<AutoSetTime>`),
and the SNTP time server: `<SNTP Synchronize="Yes" Port="123"
HTTP="Yes">address</SNTP>`. The port must be 1-65535 (an invalid entry is
replaced by 13). Quirk: `HTTP` ("Allow HTTP") is saved but never read back.

## AutoLaunch Settings

What to start when a disc is inserted: games (`<Preference><Games
AutoLaunch>`), DVD movies (`<DVD AutoLaunch>`, with the player XBE as the
element's value, default `C:\xboxdash.xbe`), audio CDs (`<AudioCD>`) and data
discs (`<Data>`, the "File Manager"). The players are chosen with the XBE
browser; X clears one. A player that doesn't exist isn't saved ("Unable to
set '...' as the default DVD Player.").

## AutoRun Settings (quick launch)

The page is titled "AutoRun Settings". An XBE for each gamepad button (A, B, X, Y, Start, White, Black) and remote
control digit (0-9): `<Settings><Shortcut><GP_A>path</GP_A>...<IR_9>`. An
empty line removes its element. Needs a restart.

## Live Update

Auto-Check (`<Preference><LiveUpdate AutoCheck>`), the server (the element's
value), and Check Update / Check WIP Update (the WIP line only with
`EnableWIP="Yes"`, and not when running from DVD). Checking fetches
`<server>Version.xml` (see [NETWORK.md](NETWORK.md)); if a newer build is
listed it offers to download `UXUpdate2.bin` (`UXUWIP2.dat` for WIP),
trying the `<URL>` mirrors in Version.xml after a 404. The download is a RAR
archive with its signature blanked out: UnleashX restores it, unpacks the
files next to itself (`default.xbe` becomes `UXUpdate.xbe`) with a built-in
password, and after "Update successfully downloaded" launches
`UXUpdate.xbe` to finish. B cancels a check or download.

## Network Settings

| Section | Lines | Saved as |
| --- | --- | --- |
| | Current IP, Link Status, MAC Address | (read-only, refreshed every 2-5 s) |
| | Enable, Type (DHCP/Static), IP Address, Subnet Mask, Gateway, DNS 1, DNS 2 | `<Network Enable Type>` and `<IP>`, `<Subnet>`, `<Gateway>`, `<DNS1>`, `<DNS2>` |
| FTP Server | Enable, User, Password, Port, Max. No. of User, Allow Anonymous, Min./Max. PASV Port, SYST Name, Boost Mode, Auto-Detect | `<FTP Enable>` with `<User>`, `<Password>`, `<Port>`, `<MaxUsers>`, `<AllowAnon>`, `<MinPASVPort>`, `<MaxPASVPort>`, `<SYST>`, `<Boost>`; `<Network><AutoDetect>` |
| FTP Client | Min. Port, Max. Port, Detect Peers | `<FTPClient MinPort MaxPort DetectPeer>` |
| HTTP Connection | Enable Proxy, Proxy, Port | `<HTTP UseProxy Proxy Port>` |

Addresses use the IP address editor, the rest the keyboard. Ranges: FTP port
1-65535, users 1-10, PASV ports 1024-65535, proxy port 1-65535. Boost mode
and the SYST name apply at once; most other lines need a restart. X clears
the user, password or SYST name. Quirks: the FTP password is shown in plain
text; a failure to save the SYST name reports "Unable to set FTP Password".

## Screen Adjust

A full-screen editor for the visible screen area (`CAppSettings::m_viewport`,
the area everything is drawn in). The current area is outlined, with its
corner coordinates, its size and help text.

* **Left trigger:** the d-pad and left stick move the top-left corner (up
  to 10% of the display in); the bottom-right corner stays put, so the size
  changes with it. This is the starting mode.
* **Right trigger:** the d-pad and left stick move the bottom-right corner
  (between 90% and 100% of the display).
* **Right stick:** always moves the bottom-right corner.
* **A or START:** save and close; **B or BACK:** put the area back and
  close.

Saving copies the area into the current video mode's entry and writes all
six entries (0x18 bytes each) to `<data>\videoset.dat`; a failure shows
"An error occured while saving calibration data." Quirk: the whole viewport
is copied over the entry, so the mode's font scale (the entry's last field)
gets the viewport's MaxZ.

## Pop-ups

**Spinners** (`CUpDown`): a key-shaped box showing one value; Up/Down (and
the triggers, and the left stick) step through the values, wrapping and
skipping blank ones. The focused one is highlighted with pulsing arrows; the
arrow last used stays lit (until other input, at most every 2 seconds).

**IP address editor:** four spinners (0-255) with dots between them. Left
and Right move between them (wrapping), A accepts, B or BACK cancels.
Quirks: an address of 16 characters or more isn't terminated when converted;
all three dots are placed using the first spinner's width.

**Clock Settings** (also `Action="SetClock"` and the cold boot clock
prompt): six spinners, the date in the Xbox language's order (m/d/y for
English and others, y/m/d for Japanese, Korean and Chinese, d.m.y for
German and Italian, d/m/y for French, Spanish and Portuguese; years
2000-2099), then hour (1-12), minute and AM/PM. The day spinner is fitted
to the month (and leap years) when it gets the focus after the month or
year changed. Under the spinners the chosen time is spelled out ("January
1, 2004  12:00 AM"). A sets the clock (keeping the current seconds), B or
BACK cancels. Quirks: opening it before noon leaves AM/PM as it was; the
spelled-out time uses the 24-hour hour when it opens.

## Xbox Info (System Info)

A scrolling list of labelled lines, opened from System Settings:

* **Xbox Info:** language, game region, video standard and modes, audio,
  AV cable, parental control, DVD region, version, serial number, HDD key,
  kernel version, hardware flags/GPU/MCP revisions.
* **Devices:** the ports with a gamepad, IR receiver or keyboard.
* **Storage:** C, E, F, G (total, free and percent free; F and G only when
  enabled), X, Y, Z (in MB, or "Unformatted"), the DVD drive.
* **Network:** link status, MAC, IP, subnet mask, gateway, DNS 1 and 2.
* **FTP Statistics:** status, users, connections, bytes/files/failures
  received and sent.
* **Misc:** board and CPU temperature, flash chip ID.
* **UnleashX:** version, launch mode, path, physical location, threads,
  memory, video buffer size.

Devices, drives, network, FTP and the temperatures refresh every 1.5
seconds while idle. Up/Down scroll; B, BACK or BLACK close it. Quirks:
"Sent" is empty until the first refresh; a missing X/Y/Z drive formats
doubles as strings; there's no check for a missing DVD drive entry.

## Harddisk Settings

Enable Drive F / Enable Drive G (`<Preference><EnableDriveF>`,
`<EnableDriveG>`; mapped or unmapped at once, restart), the hard disk's
model, serial and lock status (A on the status line offers to lock or
unlock the disk), the space on C, E, F, G, X, Y and Z (total/free), the
SMART values, the DVD drive's model and serial, and the DVD Media Check
(Normal / Extended, `<DVDMediaCheck>`). Drive space and SMART values refresh
every 5 seconds. Quirk: the temperature SMART value is converted with 32.5
instead of 32, and only when it looks like the other unit (above 90 or
below 60).

The geometry and transfer mode lines print IDENTIFY's mode *bit masks*,
not mode numbers: a drive in Ultra DMA mode 5 shows "Ultra DMA Mode 32".

**Locking** (`LockHdd`, `src/system/Hardware.cpp`) sends ATA SECURITY SET
PASSWORD twice:
1. The master password "TEAMASSEMBLY".
2. The Xbox's user password: HMAC-SHA1, under the console's HDD key, of
   the drive's model and serial number (as the kernel reports them, without
   trailing spaces).

It succeeds once the drive reports security enabled.

**Unlocking** sends SECURITY DISABLE PASSWORD with the user password, and
falls back to the master password.

Quirks:
* The HMAC key's length is the key's `strlen`. A console whose HDD key has
  a zero byte gets a password made from a shortened key, which differs from
  the one its kernel expects.
* A password whose first byte is 0 is treated as no password, so nothing
  happens.

## Sounds and Audio Settings

| Line | Values | Saved as |
| --- | --- | --- |
| Play Music | Off / Xbox / Custom / Both | `<Audio><EnableSoundTrack>` |
| Random Music, Global | Yes / No | `<EnableSoundTrack Random Global>` |
| Music Path | a folder (pop-up; X clears it) | `<EnableSoundTrack MusicPath>` |
| Surround (5.1) | Yes / No; also the Xbox's Dolby Digital setting | `<EnableSoundTrack Enable_5_1>` |
| Music Volume, Sound Volume | 0-100 with LEFT/RIGHT, drawn as bars | `<MusicVolume>`, `<SoundVolume>` |
| Enable (sound effects) | Yes / No | `<EnableSoundEffect>` |
| Scroll, Select, Action Complete, Ambient, Keyboard Stroke, Back, Menu Up, Menu Down, Game Save Select, Memory Slot | a .wav (pop-up; X clears it) | `<Audio><Action_Complete>file</...>` (`[None]` when empty) |

Volumes, random/global and 5.1 apply to the playing music at once; sound
files are reloaded at once.

## System Settings

* **Xbox Info:** dashboard version, the Xbox Info screen (below), and the
  Xbox's nickname (saved to the Xbox, at most 32 characters).
* **File Operation:** Auto Rename, Auto Patch XBE, Abort On Error, default
  backup folder (`<Preference><AutoRename>` etc., `<DefaultFolder>`).
* **Context Menu:** `<ContextMenu AllowUninstall AllowItemEdit
  AllowRecentList LinkToGame LinkToSave>`.
* **Font:** Font Scale 1.0x-3.0x (A steps 0.1, LEFT/RIGHT too); it's kept
  per video mode in `<data>\videoset.dat`, not Config.xml. Scroll Speed
  (`<ScrollSpeed>`, saved as e.g. `20f`).
* **Temperature:** board and CPU temperature, Use Fahrenheit, the
  temperature threshold (`<TempThreshold>`), and Fan Speed 1.0x-5.0x
  (`<FanSpeed>`, applied at once; 1.0x gives the fan back to the SMC).
* **Misc:** language file (`<LanguageFile>`, loaded at once), Auto Turn Off
  minutes.
* **LED Color:** the color for Default, Screen Saver, Copying, FTP Send,
  FTP Receive, FTP Boost and Busy (`<Settings><LED><Default>Red</...>`), and
  Test LED to try a color.
* **Screen Saver:** wait, type (Text / Video / Slideshow), text, video,
  path, background music (`<Preference><ScreenSaver Wait Type EnableBGM>`
  with `<Text>`, `<Video>`, `<Path>` children).
* **Skin:** skins path (`<Skin Path>`), current skin.
* **RSS Newsfeed:** `<Settings><Newsfeed Enable Interval Continuous
  Separator>`; enabling it without any `<URL>` adds xbox-scene.com and
  xbins.org.

X sets the selected line back to its default. Quirks: typed scroll speeds
and temperature thresholds aren't range-checked; the screen saver path is
stored in the text setting until restart.

## The file browser

`CFileBrowser` (`src/ui/FileBrowser.cpp`) is the pop-up behind every "Browse..." item. The text editor's Open uses it too.

### What it lists

It starts with the mounted drives. Inside a drive it lists `..` first, then the folders as `<name>`, then the files. With "[New Folder]" enabled, that entry comes last.

Two modes:

* **Folder mode** (`SetSelectFiles(FALSE)`): only folders are listed, and A always enters.
* **File mode** (`SetSelectFiles(TRUE)`): files are listed too, filtered by the extensions given (`"xbe"`, `"wmv|xmv"`). A on a file picks it.

Filters match the end of the name, ignoring case. So `txt` also matches `footxt`.

### Controls

| Input | Effect |
|---|---|
| A | enter the folder, or pick the file |
| A on `..` | go up |
| B | go up |
| START | pick the highlighted entry. A folder only counts when the caller allows folder results. |
| BACK or BLACK | cancel |
| "[New Folder]" | asks for a name (at most 64 characters) and creates the folder |

The left stick only moves the selection when it's pushed all the way.

### Position memory

Each folder depth remembers its selection and scroll position. Going up returns to where you were. Going down into a folder also restores whatever position was last used at that depth.

### Quirks kept

* A drive picked from the drive list with START comes back as `\C`.
* The `_strdup` copy of each filter is never freed.
