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
  Manage Soundtracks (`CAudioTrackOption`, not decompiled yet), Open File
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
