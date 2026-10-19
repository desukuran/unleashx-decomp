# UnleashX skin format (Skin.xml)

This reference is reconstructed from the decompiled parser (`src/skin/SkinParser.cpp`, original `CSkin::Parse` at `0x000FD644`). It describes what UnleashX 0.39.0528A actually reads, quirks included. Tag and attribute names are case-insensitive.

A skin is a folder (or a `.zip`) under the Skins path containing `Skin.xml` plus its images, fonts and videos. An optional `UXSplash.jpg` (or `.png`) next to it is the loading splash. Eight skins are built in, stored as a zip in the XBE's `INT_SKINS` section: `NoImages_Blue -By Jezz_X`, `NoImages_Green -By Jezz_X`, `NoImages - By Jezz_X`, and `BlueShapes`, `GreenShapes`, `OrangeShapes`, `PurpleShapes` and `RedShapes` (all `- By Kthulu`). Extract them with `tools/xbe.py extract` and unzip `INT_SKINS.bin`.

## File paths

Paths inside the skin are resolved by `CSkin::ResolveFile` (`0x000F9DFC`):

| Path form | Resolves to |
|---|---|
| `X:\...` | used as is |
| `\...` | the dashboard's drive |
| `.\...` | the Skins folder (`<Skin Path=...>` in Config.xml) |
| `file.png`, `..\file.png` | the skin's own folder; each `..\` goes up one level |
| `D:\...` | the dashboard folder (unless running from DVD) |

Everything is lowercased and `"` characters are removed.

## Structure

```xml
<Skin Name="My Skin">
  <Information>
    <Author>...</Author>
    <Version>...</Version>
  </Information>
  <ScreenSize Width="640" Height="480"/>
  <Fonts>
    <Font Name="Verdana8">Verdana8.xpr</Font>
  </Fonts>
  <Main Globe="True" Orb="True" Fog="True">
    <BackGround EnableAudio="False">bground.jpg | intro.wmv | intro.xmv</BackGround>
    <Orb>orb.png</Orb>
    <Cellwall>cellwall.png</Cellwall>
    <GameIcon Enable="True" Opacity="80"><Position Left Top Width Height/></GameIcon>
    <Menu Smooth ShowIcon ShowArrows Rotate Scroll Align Horizontal Fixed FixAt MaxItem>
      <ItemPos Left Top Width Height Align/>   (up to 19)
      <Font Height="20">menu.xpr | menu.ttf</Font>
      <BackColor>0xAARRGGBB</BackColor>
      <BorderColor Width="2">0xAARRGGBB</BorderColor>
      <Position Left Top Width Height Fixed FixAt/>
      <Preview Left Top Width Height EnableAudio EnableMusic/>
    </Menu>
    <InfoFont>Verdana8</InfoFont>
    <Text .../>  <Shape .../>  <Image .../>
  </Main>
  <ColorScheme>...</ColorScheme>
  <FileManager SafeBorder="True" DrawBorder="False"><BackGround>fm.jpg</BackGround></FileManager>
  <LoadingScreen ShowInfo="True"><BackGround>image | video | GamePreview | GameIcon</BackGround></LoadingScreen>
  <IntroMovie>intro.wmv</IntroMovie>
  <MusicPlayer Left Top Width Height Relative="False">
    <BackGround>mp.png</BackGround>
    <ButtonMap>buttons.png</ButtonMap>
    <Text .../>  <Element .../>  <Image .../>
    <PlayList Left Top Width Height OffsetX OffsetY OffsetX2 OffsetY2><BackGround>pl.png</BackGround></PlayList>
    <Dialog OffsetX OffsetY OffsetX2 OffsetY2><BackGround>dlg.png</BackGround></Dialog>
  </MusicPlayer>
</Skin>
```

### `<ScreenSize>`

This picks the render resolution the skin is designed for:

| Width | Height | Result |
|---|---|---|
| < 720 | | 640 wide |
| 720-1279 | | 720 wide |
| 1280-1919 | | 1280×720 |
| ≥ 1920 | | 1920×1080 |
| | < 576 | 480 high |
| | 576-719 | 576 high on PAL, 480 on NTSC |
| | ≥ 1080 | 1080 high |

### `<Main>`

- `Globe`, `Orb` and `Fog` default to true. `Fog` is only read if `Globe` isn't false. A video `<BackGround>` turns all three off.
- `<GameIcon Opacity>` is a percentage (0-100).

### `<Menu>`

| Attribute | Notes |
|---|---|
| `Smooth`, `ShowIcon`, `ShowArrows` | on unless false |
| `Rotate`, `Horizontal` | off unless true. `Horizontal` is forced off when `<ItemPos>` slots are given |
| `Scroll` | `True` or `2` |
| `Align` | `Middle`, `Right`, anything else means left |
| `Fixed` / `FixAt` / `MaxItem` | `FixAt` and `MaxItem` are parsed as **hexadecimal** |

- `<ItemPos>` gives fixed slots for up to 19 items, each with its own `Align`. With a `Height` and a vertically centered alignment, the original adds the half-height offset twice.
- `<Font>` is a font name from `<Fonts>`, a standard name, a `.xpr` file, or a `.ttf` file sized by `Height`/`Size` (default 20).
- `<Position>` sets the menu area. A `Width` under 50 becomes 270.
- `<Preview>` is the game-preview video area. Its `EnableAudio` defaults to on and `EnableMusic` to off.

## Elements

### `<Text>`

```xml
<Text Source="$C_PFree$ Free" Left="540" Top="305" Width="78" Height="16"
      Align="Middle" Color="0xFF404040" Font="Verdana8" Scroll="True" zIndex="2"
      ScaleX="100" ScaleY="100"/>
```

`Source` is one of:
- a **keyword** from the table below, e.g. `Source="time"`;
- text containing **one** `$Keyword$`, which is replaced by the value. Any `%` is removed first, and if several tokens appear only the first in the table order is used;
- **plain text**, shown as is.

The element's own content is ignored, and a `<Text>` with no `Source` shows nothing.

Attribute rules:

| Attribute | Notes |
|---|---|
| `Left`, `Top`, `Width`, `Height` | Width is clipped to the screen edge. Width and height are capped at 640 and 480 |
| `Align` | see the alignment table below. A `Width` adds truncation |
| `Color` | hex ARGB; the default is the font's color |
| `Scroll` | `True` (1) or a number; `2` is the newsfeed ticker. Scrolling needs a `Width` |
| `Font` | font name |
| `zIndex` | draw order within the screen |
| `ScaleX`, `ScaleY` | percentages, default 100 |

Quirks:
- `ScaleX`/`ScaleY` are only recognized when the value is **25 or more**.
- `Align`, `Color`, `Scroll`, `Font` and `zIndex` are only recognized when the value reads as a number **under 25**. Words and hex colors read as 0, so they're fine.
- With `<Newsfeed Continuous="Yes">`, a `$Newsfeed$` text always becomes a ticker.

Alignment values:

| `Align` | Meaning |
|---|---|
| `TopLeft` (default) | left, top |
| `TopCenter`, `Middle` | horizontally centered |
| `TopRight`, `Right` | right-aligned |
| `MiddleLeft` | vertically centered |
| `MiddleCenter` | both centered |
| `MiddleRight` | right, vertically centered |

### Text sources

Keywords are case-insensitive; the `$Token$` forms are written exactly like this, case included.

| Keyword | `$Token$` | Shows |
|---|---|---|
| `time`, `date`, `datetime` | `$Time$`, `$Date$`, `$DateTime$` | clock |
| `nickname` | `$NickName$` | Xbox nickname |
| `c_free` `e_free` `f_free` `g_free` | `$C_Free$`... | free space |
| `c_total` ... `g_total` | `$C_Total$`... | total space |
| `c_freemb` ... `g_freemb` | `$C_FreeMB$`... | free space in MB |
| `c_totalmb` ... `g_totalmb` | `$C_TotalMB$`... | total space in MB |
| `c_pfree` ... `g_pfree` | `$C_PFree$`... | percent free |
| `cputemp`, `mbtemp`, `hdtemp` | `$CPUTemp$`, `$MBTemp$`, `$HDTemp$` | temperature (presumably in the `UseFahrenheit` unit) |
| `cputempc`/`f`, `mbtempc`/`f`, `hdtempc`/`f` | `$CPUTempC$`... | temperature in °C / °F |
| `fanspeed` | `$FanSpeed$` | |
| `freememory`, `totalmemory` | `$FreeMemory$`, `$TotalMemory$` | RAM |
| `ip`, `subnet`, `gateway`, `dns1`, `dns2`, `mac`, `linkstatus` | `$IP$`... | network |
| `numclient` | `$NumClient$` | FTP clients |
| `dvdtitle`, `dvdtype`, `dvdmodel`, `tray` | `$DVDTitle$`... | DVD drive |
| `hddmodel`, `hddserial`, `hddstatus`, `hddkey` | `$HDDModel$`... | hard disk |
| `biosversion`, `biosmd5`, `serialnum`, `chipid`, `videomode`, `version` | | system info |
| `appversion` | `$AppVersion$` | UnleashX version |
| `skinauthor`, `skinversion` | | from `<Information>` |
| `launchmode` | `$LaunchMode$` | |
| `thread` | `$Thread$` | |
| `newsfeed`, `newssource` | `$Newsfeed$`, `$NewsSource$` | RSS ticker |
| `songname`, `songtime`, `songlength`, `songbitrate`, `songfrequency`, `songchannel`, `songtrack`, `soundtrack` | `$SongName$`... | music player |

The numeric IDs are in `enum SkinTextSource` (`src/skin/Skin.h`).

### `<Shape>` (in `<Main>`) / `<Element>` (in `<MusicPlayer>`)

```xml
<Shape Type="roundedbox" Left="10" Top="10" Width="100" Height="20"
       BackColor="0x80000000" BorderColor="0xFFFFFFFF" zIndex="1"/>
```

| `Type` | |
|---|---|
| *(default)* | box |
| `roundedbox` | |
| `triangle` | `Direction` defaults to up; also `Down`, `Left`, `Right` |
| `meter` | |
| `stopstatus`, `playstatus`, `pausestatus`, `songstatus` | |
| `volume`, `progress` | `Direction` defaults to right; also `Down`, `Left`, `Up` |
| `prev`, `play`, `pause`, `stop`, `next`, `prevtrack`, `nexttrack`, `random`, `mute` | music-player buttons |

### `<Image>`

```xml
<Image Left="20" Top="20" Width="64" Height="64" Opacity="80" zIndex="3">logo.png</Image>
<Image Source="GameIcon" .../>
```

- `Source` can be `GameIcon`, `GamePreview` (needs a `<Menu><Preview>` area) or `AlbumArt`. Otherwise the element text, or failing that `Source`, is the file.
- `Opacity` is a percentage. Values 0 and 1 both mean fully opaque.
- A missing `Width`/`Height` is taken from the image file.

## `<ColorScheme>`

```xml
<ColorScheme Name="Blue">
  <MENU_TEXTCOLOR>0x99FFFFFF</MENU_TEXTCOLOR>
  ...
</ColorScheme>
```

Colors for the dashboard's own UI (`SColorScheme`, `src/skin/SkinColors.h`). Each child sets one color in hex ARGB, and a value of 0 is ignored. `<ColorScheme>` can appear anywhere in Skin.xml. The defaults are listed in `SColorScheme::SetDefaults`.

`AMBIENT_FOGCOLOR` `AMBIENT_SKYCOLOR` `AMBIENT_CELLCOLOR` `CAPTION_TEXTCOLOR` `CAPTION_BACKCOLOR` `DIALOG_BORDERCOLOR` `DIALOG_BACKCOLOR` `DIALOG_INNER_BORDERCOLOR` `DIALOG_INNER_BACKCOLOR` `DIALOG_TEXTCOLOR` `DIALOG_DISABLEDTEXT` `OPTION_TEXTCOLOR` `OPTION_ACTIVETEXTCOLOR` `OPTION_ACTIVEBACKCOLOR` `BUTTON_BORDERCOLOR` `BUTTON_TEXTCOLOR` `BUTTON_BACKCOLOR` `BUTTON_ACTIVETEXTCOLOR` `BUTTON_ACTIVEBACKCOLOR` `KB_KEYBG_HIGHLIGHT` `KB_KEYBG_DISABLED` `KB_KEY_PRESSED` `KB_KEY_NORMAL` `KB_KEY_DISABLED` `KB_KEY_ACTIVEBORDER` `KB_KEY_TEXTCOLOR` `KB_KEY_TEXTBACKCOLOR` `KB_KEY_TEXTBORDERCOLOR` `MENU_TEXTCOLOR` `MENU_ACTIVETEXTCOLOR` `MENU_BORDERCOLOR` `MENU_BACKCOLOR` `MENU_ACTIVEBACKCOLOR` `MENU_ACTIVEBORDERCOLOR` `WindowBackCOLOR` `WindowForeCOLOR` `MM_BORDERCOLOR` `MMLightBackColor` `MMDarkBackColor` `MMLightBarColor` `MMBarBackColor` `MMDarkBarColor` `MMForeColor` `MMOffForeColor` `MMActiveForeColor`

`MENU_BORDERCOLOR` and `MENU_BACKCOLOR` are the defaults for `<Menu><BorderColor>`/`<BackColor>`. The `MM*` colors are used by the built-in music player.

Two adjustments keep colors distinguishable: if `KB_KEYBG_DISABLED` equals `KB_KEYBG_HIGHLIGHT` it is changed by 1, and if `OPTION_ACTIVETEXTCOLOR` equals `DIALOG_TEXTCOLOR` it is darkened slightly.

## Built-in layouts

- **No `<Main>`** (and the `[Default]` skin): a column on the left with date/time, free MB per drive (F/G only if mapped) and CPU/board temperatures; IP, FTP clients, tray state and nickname on the right; song name and time.
- **No `<MusicPlayer>`**: a player drawn with shapes in the `MM*` colors (`CSkin::SetDefaultMusicPlayerLayout`).
