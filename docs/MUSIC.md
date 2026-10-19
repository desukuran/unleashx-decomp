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
