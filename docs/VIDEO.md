# Video

How videos play (`CWMVideoPlayer`, `src/media/WMVideoPlayer.cpp`).

## Uses

| Where | Mode |
|---|---|
| Skin `<Background>` video | full screen, looping, drawn as a texture |
| Skin preview video (`<Preview>`) | in the preview's rectangle (or full screen), looping |
| Intro movie (`<IntroMovie>`) | full screen, plays once; A or START skips it |
| Screen saver video | full screen, looping; sound only with ScreenSaverBGM and no music playing |
| File manager (a `.wmv` or `.xmv` file) | modal: the hardware overlay, full screen at first |

Only `.wmv` and `.xmv` files are accepted, and the whole path must be at
least 8 characters long. WMV files go through the XDK WMV sample's player
(`CWmvSamplePlayer`, `src/xdk/WmvPlayer.cpp`, on WMVDEC; the build needs an XDK
4xxx's decoder for it, see BUILDING.md), decoded into two textures that take turns. XMV files go through
the XDK samples' `CXMVPlayer` (xmvhelper, as in the XMVCompare sample).
Both are library code and aren't reconstructed. UnleashX changed
`CXMVPlayer` a little: it can switch the movie's sound off, keeps the first
audio stream's DirectSound stream (with 5.1 on, a mono or stereo one is
set to the music volume with 0x4B0 headroom; the 5.1 speaker mix bins are
looked up for this but never passed to the decoder),
loops by resetting the decoder at the end, and has no device argument. The frames are YUY2 and drawn with YUV enabled.

## The file manager's viewer

The modal player shows the video on the overlay. Out of full screen it's
fitted into a window (580x440 at 640x480) that shows the file's name and
the video's size (and bit rate, for WMV).

| Input | Action |
|---|---|
| A, B, BLACK, BACK | stop |
| Right stick click, remote DISPLAY | full screen on/off |

Other input is swallowed. Open errors ("File does not exists.", "Failed to
open '...'. File may be corrupt or damaged." for WMV, "... Unsupported file
format." for XMV) show in a message box.

## Quirks

* A rejected name (short, or another extension) leaves the player marked
  as still opening, so the next Open waits forever.
* Fitting the video into the window checks the height against three text
  lines but cuts it to six.
* The skin passes its "loading" flag where the input action goes, so a
  non-looping preview would stop while a skin loads (previews loop, so it
  doesn't show).
* The window's caption height is computed once and never used.
