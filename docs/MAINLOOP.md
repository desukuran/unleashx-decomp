# The main loop

What `CUnleashX::FrameMove` (`src/app/FrameMove.cpp`) and `Render`
(`src/app/AppRender.cpp`) do every frame. Startup (`Initialize`) isn't
decompiled yet.

## Input

One action per frame comes from `CGamepadInput::GetAction`
(`src/input/Gamepad.cpp`): the first gamepad port with something pressed
(remembered), else the IR remote. Held buttons repeat after 0.333 s, then
every 0.085 s (every sixth repeat waits the long delay again); the triggers
repeat faster the harder they're pressed. The face buttons, START and BACK
only report again after three repeats. The remote's arrows, SELECT and BACK
also act on the gamepad state (so screens that read the stick see them).
Without gamepad or remote input, the debug keyboard is read (see MENU.md).

The action goes to the first of these that is open:

1. a message box (it takes all input);
2. a file operation in progress outside the file manager: BACK cancels it
   (a copy says "Copy operation has been Canceled.");
3. the startup password (keyboard), then the startup passcode (buttons):
   after `<Password MaxTries>` failures the box says so and the Xbox turns
   off when it's closed. Quirk: a configured password of `MicrosoftXBOX`
   accepts anything;
4. the clock prompt after a cold boot (invalid clock);
5. the current screen: menu, file manager, game manager, settings, skins,
   System Info, text editor, music player. A screen that closes returns to
   the menu.

The screen saver then gets the input while it runs. BACK (without the
keyboard up) and the remote's MENU go back to the menu.

## Background chores

* **Disc autostart:** when enabled, a game, video, audio or data disc is
  started once it's inserted (an audio CD plays in the music player if
  that's open). Holding either trigger skips it.
* **FTP:** a pending `SITE EXEC` launch is started.
* **Live Update, second half:** the installer (`UXUpdate.xbe`) starts the
  new UnleashX with launch data naming the old dashboard XBE. After 5
  seconds it says "Update is validated. Keep this running a few more
  minutes. Press OK to proceed with the upgrade."; then the new version's
  `Release.nfo` opens in the text editor, and once that's closed the old
  XBE is renamed to `.bak` and the new one moved in its place.
* **Shutdown:** a pending shutdown waits for file operations, at most 30
  seconds; the message box counts down "Xbox will shutdown in... N
  seconds". Answering any message box cancels it.
* **Soundtracks:** once requested, the music manager is created on a
  thread. While it plays it's checked every frame (moving on to the next
  song); when it stops, it's started again if soundtrack mode is on, and
  paused while a preview video with sound plays (unless the skin's preview
  allows music).
* **File operations:** when one ends, an "operation finished" action is
  sent once (the menu refreshes on it).
* **Globe:** with `<Main Globe>`, the globe sways back and forth along the
  path of 512 heights from the skin, at a speed tied to the frame rate.

## Drawing

Each frame clears to the skin's sky color (black without a globe), then:

1. a video or slideshow screen saver alone, full screen; otherwise
2. the background (globe/orb), except for the file manager, settings,
   System Info and the text editor on a 1080-line screen;
3. the skin's own elements (menu, game manager, settings, skins, System
   Info and the music player screens);
4. the focused thing: a message box, the password keyboard, the passcode
   pad, or the current screen. Over the menu, a running file operation shows
   its progress box ("Copying/Moving/Deleting/Compressing/Extracting.
   Please Wait"; copies and moves with From/To and an overall bar), and a
   failed one its error ("An error occured while copying '...'. Error Code:
   N", offering to show the log file when there is one);
5. a text screen saver on top, or the clock prompt;
6. the pointer cursor, for 5 seconds after it moved, fading out by 10.

Screenshots: holding the left trigger (not the right) and pressing BLACK,
or holding BLACK and clicking the left stick, saves the next frame from the
back buffer as `screenshotN.bmp` on the first of Z:, Y:, X: with 1MB free.

