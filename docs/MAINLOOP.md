# The main loop

What `CUnleashX::Initialize` (`src/app/Initialize.cpp`) does at startup,
and what `FrameMove` (`src/app/FrameMove.cpp`) and `Render`
(`src/app/AppRender.cpp`) do every frame.

## Startup

1. **Drives.** The drive UnleashX runs from is mounted (running from F: or
   G: turns those drives on), then C:. Run from DVD, E: and the Z: cache
   are mounted too; if neither C:\ nor E:\ exists, there's no hard disk.
2. **Loading.** The skin resources (`LoadResources`) and `Config.xml`
   load on threads, then a startup thread runs. Run from DVD, a Z: cache
   with under 5 MB free is formatted first.
3. **Splash.** Meanwhile, whatever the previous program left on screen is
   shown, fading out over 80 frames. If there's nothing,
   `E:\UDATA\9e115330\0064122817A8\Splash.jpg` (or `UXSplash.jpg` there)
   is shown instead. Holding BACK skips the splash.
4. **Recovery screen.** If required resources are missing, UnleashX sets
   up the utility drive again and retries. If that fails too, it turns on
   FTP at a fixed address, 192.168.0.100 (subnet 255.255.255.0), and shows
   "Not all required resources can be loaded. This is usually caused by
   lack of HD space on E:\ or X:\." for 5 minutes. During that time an
   FTP `SITE EXEC` still launches. After that it starts
   `C:\XboxBoxDash.xbe`.
5. **Boot shortcuts.** A button held at boot can run its configured
   shortcut (`RunBootShortcut`, not decompiled yet). When it returns TRUE,
   the Xbox reboots to the dashboard.
6. **Video mode.** The mode comes from the video settings:
   * PAL is 640x576, or 720 wide when widescreen is on. PAL60 gives 480
     lines at 60 Hz.
   * NTSC can be widescreen. With the HD AV pack it can be 480p, 1080i
     (which wins over 720p), or 720p.
   * 16-bit uses X1R5G5B5. Without Fast Refresh it waits for vsync.

   Buttons held at boot change this:

   | Held at boot | Effect |
   |---|---|
   | BACK | Default settings for this run, in 640x480 interlaced. Screen calibration and display filters are skipped. A safe mode. |
   | BACK + right trigger | Switches the Xbox to NTSC-M (if it isn't already) and reboots. |
   | BACK + left trigger | Switches the Xbox to PAL-I (if it isn't already) and reboots. |
   | Left stick click | No vsync for this run. |
7. **Sound.** 5.1 is forced on when the Xbox is set to stereo and 5.1 is
   enabled. DirectSound gets the light HRTF and the effects image
   `dsstdfx.bin`, from the data folder or its parent. Without DirectSound,
   there's no music and no intro movie.
8. **Skin check.** `skincheck.dat` exists while a skin loads. If it's
   still there at the next start (the skin hung or crashed the Xbox), the
   default layout is used and a warning is shown. The skin name can be:
   * a full path (with `:\`),
   * a built-in skin,
   * a folder under the skins path, using its `skin.xml` or else the first
     `.zip` in it.
9. **Prompts and threads.** After a cold boot (invalid clock), the clock
   prompt appears. The monitor thread starts, and the network thread does
   too if the network is on. If the system is locked, the startup password
   ("Enter System Password" on the keyboard) or the button passcode is
   shown.
10. **Intro.** Once the skin has loaded, its intro movie plays (if it has
    one). A or B skips it. Then come the warnings for a bad `Config.xml` or
    skin, and the Live Update check if the update installer launched this
    build.
11. **Finish.** The splash finishes fading. The soft-display and flicker
    filters are applied when they aren't the defaults. If the mode changed,
    the device is reset. Finally the screen calibration from `videoset.dat`
    is applied when it fits the display: the top-left corner within 10%,
    and the bottom-right past 90%.

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

