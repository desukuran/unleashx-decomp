# Settings screens (Control Panel)

The settings screen (`Action="Settings"`) shows ten icons from
`CtlPanel.xpr`; each opens a page. Written from `src/app/SettingsBase.cpp`
and `src/app/Settings*.cpp`.

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

A full-screen editor for the visible screen area (TODO: decompile the
editor, `0x001018DC`).

## HDD, Music, System Settings

TODO.
