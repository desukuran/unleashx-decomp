# Music

How the background music works (`CMusicManager`, `src/audio/MusicManager.cpp`).

## Setup

The music manager is created when the menu starts (`MusicManagerThread` in
`src/app/Startup.cpp`). `<Audio SoundTrack>` decides what it finds:

| SoundTrack | Xbox soundtracks | Music folder (`<Audio MusicPath>`) |
|---|---|---|
| 0 | no (the menu's ambient sound plays instead) | no |
| 1 | yes | no |
| 2 | no | yes |
| 3 | yes | yes |

`<Audio Random>` turns on random play. `<Audio Global>` (random across
soundtracks) only takes effect together with Random. With 5.1 on, the
players get the speaker configuration.

## The soundtrack list

1. **Audio CD** (index 0): one song per track, named "Track N". (Quirk:
   the track lengths are read off by one, so each track shows the next
   one's length.)
2. **Media Player** (index 1): a file or folder played from the file
   manager. A folder only plays with random play on. The soundtrack is
   named after the song's folder.
3. **Soundtracks**: the Xbox soundtracks ripped by the MS dashboard (WMA),
   then every folder under the music folder that holds `.mp3` or `.wma`
   files. Folders are named after themselves; the top folder is "Custom
   Soundtrack".

At startup a random user soundtrack is loaded.

## Info.xml

The music folder gets an `Info.xml` that remembers titles and lengths:

```xml
<Music>
  <d41d8cd98f00b204e9800998ecf8427e Title="My Album">
    <01-Song.mp3 Length="215" Title="Real Song Title"/>
  </d41d8cd98f00b204e9800998ecf8427e>
</Music>
```

Each folder's element is named by the MD5 of its path. Inside it, each
song's element is named by its file name with the spaces removed. The
`Title` attributes override the folder and file names, and you can edit
them by hand. While a song plays, its length is stored, and so is the
title from its tags, unless that title is just part of the file name. The
music player screen can rename a folder too.

## Album art

The music player shows `folder.jpg` (or `folder.png`) from the song's
folder.

## Playing order

* Songs play in order or, with Random, in a shuffled order made when the
  soundtrack loads.
* At the end of a soundtrack, the next soundtrack starts. An audio CD
  stops at the end instead.
* With Global, after each song there's a 50% chance of moving on to the
  next soundtrack. Soundtracks are visited in a shuffled order, and a new
  order is drawn after the last one.
* When the CD is removed, playback moves to the next soundtrack.

## Controls (while music plays)

These only work when the music volume is 5 or more.

| Input | Action |
|---|---|
| Right stick click | pause / resume |
| WHITE + left / right | previous / next song |
| WHITE + up / down | next / previous soundtrack |
| Remote ⏪ / ⏩ | previous / next song |
| Remote ⏮ / ⏭ | previous / next soundtrack |
| Remote PLAY / STOP / PAUSE | play / stop / pause |

## Screen saver

The screen saver pauses the music, unless `<ScreenSaver BGM>` is on. A
video screen saver only plays its own sound when BGM is on and no music
was playing (see `src/app/ScreenSaver.cpp`).

## The music player screen

`src/musicplayer/` (`CAudioGUI`, mode 9). The skin's `<MusicPlayer>`
section draws it (see `SKIN_FORMAT.md`). Its button shapes, state icons,
level meter and bars are drawn by the player:

* Button shapes `Prev`..`Mute` use one texture map: the skin's
  `<ButtonMap>`, else `MPlayer.png` from the media folder. It's an 8x8
  grid of cells, each 1/8 of the texture:
  * rows 0-1: the buttons previous song, play, pause, stop, next song,
    previous soundtrack and next soundtrack, in columns 0-6. Column 7
    holds Random (row 0) over Mute (row 1).
  * rows 2-3: the same, lit. These are drawn over the play, pause or stop
    button that matches the music's state, and in place of Random/Mute
    when they're on.
  * rows 4-5: the same, focused.
  * row 6: the song progress bar, with the empty track in the left half
    and the filled part in the right half.
  * row 7: the volume bar, laid out the same way.
* Disabled buttons are drawn at half opacity. Previous/next song needs two
  or more songs. Previous/next soundtrack needs two or more of your own
  soundtracks.
* The bars fill in the shape's `Direction`. The default is left to right;
  `Down` fills from the top, `Left` from the right, and the fourth from the
  bottom. The bar picture is rotated to match.
* The level meter shows 12 bars of DirectSound's output levels, from -60
  to 0 dB. It's only drawn while music plays.
* Texts only scroll while music plays.

### Keys

| Input | Action |
|---|---|
| Left / right | move the focus between the skin's buttons (disabled ones are skipped) |
| A (or a pointer click on a button) | press the focused button |
| Up / down on the volume button, remote up / down, right stick | volume |
| Up / down, triggers | previous / next soundtrack; with the song list shown, move in the list |
| Right stick click | show / hide the song list |
| START | the options menu |
| B, BACK, BLACK | close |
| WHITE + d-pad, remote transport keys, other keys | the music manager's keys (above) |

* On Play with the song list shown, the selected song plays.
* The options menu has Play Disc (enabled with an audio CD in the drive),
  Manage Soundtracks (see below), Open File
  (WMA/MP3/WAV) and Browse Folder (all songs under a folder). The last two
  play through the Media Player soundtrack. The menu opens by itself when
  a disc is inserted while the player is open.
* The song list (`CSongListControl`) shows the current soundtrack's songs
  with their lengths and marks the playing one with `»`. It uses the
  skin's `<PlayList>` box and picture. Without a picture it draws a
  built-in panel.

Quirks:

* The player saves the music's state before Open File, so that B could
  restore it, but the flag that would allow the restore is never cleared.
* The menu also disables a sixth option that doesn't exist.
* There's an unused built-in player window (`0x00135089`), drawn from the
  layout `Show` computes.

## The players

The music manager has one player per kind of song. It opens a song with
the matching player, calls Play, and then calls Process every frame; when
Process fails, the song is over and the next one starts.

| Player | Songs | How |
|---|---|---|
| `CWMAPlayer` | WMA files, Xbox soundtrack songs | XACT: each song gets a new looping WMA playlist on the data folder's sound bank cue `Cue1`, holding just the file, the soundtrack or the soundtrack song. Playback timing starts with XACT's "stream data available" notification. |
| `CMP3Player` | MP3 | `CAudioDecoder`: libmad frames, dithered to 16 bits (madplay's linear dither) and written into a looping DirectSound buffer about a second ahead. |
| `CWAVPlayer` | WAV | Up to 64 KB of data in one buffer (`CAudioBuf`); longer files are streamed from disk (`CFilePump`, 4 packets of 8 KB). |
| `CDDAPlayer` | audio CD tracks | `XCDPlayer` reads the raw sectors (16 at a time) into a 4-second stream. |

The streaming classes (`CAudioBuf`, `CAudioPump`, `CFilePump`,
`CWMAPump`, `XCDPlayer`) come from the MS dashboard
(`src/audio/AudioPump.cpp`, `CdAudio.cpp`). A pump's buffer is cut into
packets, each with a notification event. A thread refills a packet with
the next data as soon as the play cursor passes it. When the data runs
out, the rest of the packet is filled with silence and the pump stops
after the last full packet has played. `CWMAPump` is never used.

With 5.1 enabled, the players are given the speaker mix bins. The WAV,
CD and MP3 buffers then switch to a 6-channel format; the WMA player
routes its cue through an XACT sound source instead.

The song's title comes from the WMA stream, from the ID3v1 tag for MP3
(with "album - artist" as a second line), from "Track N" for the CD, and
otherwise from the file's name.

The time played is counted with the performance counter while the song
plays.

### Quirks

* The time played is shown in whole seconds.
* The WAV and CD players report the song length in seconds, where the
  others give milliseconds (plus one for MP3 and WAV).
* A WAV file's title drops the last letter of the name.
* A WAV file without a `data` chunk hangs the chunk search.
* A streamed WAV plays any chunks after the data as noise.
* The WMA and MP3 players keep a `_strdup` copy of every path they open.
* After the MP3 decoder runs dry, the song still counts as playing for
  2.5 s.
* The CD's table of contents is meant to be re-read once when the drive
  isn't ready, but the retry loop gives up after the first try.
* A WMA file that isn't on a hard disk drive is copied to `Z:\tmp.wma`
  first.

## Managing soundtracks

"Manage Tracks" in the player's START menu opens the soundtrack tools
(`src/musicplayer/TrackOptions.cpp`, `TrackEditor.cpp`, `TrackCopy.cpp`).
The Xbox soundtracks are edited directly in the MS dashboard's database
(below); the music folders can only be played and renamed.

"Select Audio Track" lists Audio CD (only enabled with an audio CD in the
drive), every Xbox soundtrack, then every music folder. Choosing one opens
its menu, titled with its name:

| Source | Menu |
|---|---|
| Audio CD | Play, Copy (always greyed out) |
| Xbox soundtrack | Play, Copy, Edit, Rename, Delete |
| Music folder | Play, Rename (Copy, Edit and Delete greyed out) |

* **Play** starts the soundtrack (an Xbox one by its ID, a folder by its
  place in the music manager's list).
* **Rename** opens the keyboard (32 characters). An Xbox soundtrack is
  renamed in ST.DB; a folder gets the title in its `Info.xml`.
* **Delete** asks "Are you sure that you want to delete '...'?" (No
  selected). It removes the soundtrack, its song groups and its folder of
  WMA files.
* **Edit** shows the songs above four buttons: Move Up, Move Down, Rename
  (31 characters) and Delete. Left/right pick a button, A uses it on the
  selected song, B closes. Moving swaps the song with its neighbour.
* **Copy** shows the songs above Select (tick or untick the selected
  song), Select All and Copy. Copy (with something ticked) asks for the
  target: New Soundtrack... (the keyboard suggests the first free
  "Soundtrack N") or one of the other Xbox soundtracks. A thread copies
  each ticked song's WMA file, with a progress box, and adds it to the
  target in ST.DB. Song names starting with "Track" are renumbered as
  "Track N" in the target. If nothing could be copied, a new target
  soundtrack is deleted again.

After an edit the list is filled again. XAPI keeps the soundtrack it read
last in memory, so the list first reads another soundtrack's first song to
make XAPI read ST.DB again.

### CD ripping (disabled)

The copy screen also has a CD path, used when the source soundtrack is 0.
It reads the track's raw audio into `x:\tempcda.cda`, after checking the
free space on X:, and records the loudest sample. A WMA encoder would then
make `x:\tempwma.wma`, which is moved to the soundtrack's folder.
`EncodeTrackToWma` is a stub: it builds a gain table and deletes the input
without encoding anything. The path can't be reached anyway, because the CD
menu's Copy is greyed out. If it ran, every track would be added to ST.DB
without a file, since the move of the missing WMA fails and isn't treated
as an error.

### Quirks

* An Xbox soundtrack with ID 0, the first one made in a fresh ST.DB, has
  the same menu value as Audio CD. It opens the CD's menu, and its song
  list shows the CD.
* Pressing B on the copy target menu starts a copy into soundtrack -1,
  which then fails with "An error occurred while copying file(s)".
* The copy's error is only cleared once per copy, so after one song fails
  every later song fails too.
* Right on the copy screen can move past Copy to a fourth button that
  isn't there.
* Unselected button labels use the dialog text color, not the button text
  color.
* The rename error message has `%s to %s` but gets the names as wide
  strings, so only their first letters show. The soundtrack version also
  gets no error code.
* The copy screen keeps the editor's Yes/No delete handling, which nothing
  there triggers.

## ST.DB

`E:\TDATA\fffe0000\music\ST.DB` is the MS dashboard's soundtrack
database (`src/audio/SoundtrackDB.cpp`). It's made of 0x200-byte sectors:

| Sector | Contents |
|---|---|
| 0 | header: version (1), soundtrack count, next soundtrack ID, the 100 soundtrack IDs in list order, next song serial |
| 1-100 | one soundtrack per used slot: magic `0x21371`, ID, song count, up to 84 song group numbers, total time (ms), name (32 UTF-16) |
| 101 on | song groups: magic `0x31073`, soundtrack ID, place in the soundtrack, in-use flag, 6 song IDs, 6 times (ms), 6 names |

A song's ID is `(soundtrack ID << 16) | serial`, and its file is
`music\<soundtrack %04x>\<song %08x>.wma`. A soundtrack holds up to 500
songs and the database up to 100 soundtracks. A new database is the header
plus 100 empty soundtrack sectors. A new song group reuses the first free
one, or is added at the end of the file. `tests/soundtrackdb_test.cpp`
checks the format.

Quirks:

* Deleting the only song of a soundtrack's last group leaves that group
  marked in use, so it's never reused.
* A sector in the song group area that isn't a song group makes the
  search for a free group append groups forever.
