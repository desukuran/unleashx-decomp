# The main loop

What `CUnleashX::FrameMove` (`src/app/FrameMove.cpp`) does every frame.
Rendering (`Render`) and startup (`Initialize`) aren't decompiled yet.

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
   ("cancelling..." for copies);
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
* **Boot disc:** when UnleashX itself runs from a disc, its `Release.nfo`
  opens in the text editor once (after a "please wait"), then the launch
  data the disc was started with is handled.
* **Shutdown:** a pending shutdown waits for file operations, at most 30
  seconds; the message box counts down "Xbox will shutdown in... N
  seconds". Answering any message box cancels it.
* **Soundtracks:** once requested, the music manager is created on a
  thread; it moves to the next song every 100 ms check, starts playing when
  soundtrack mode is on, and pauses while a preview video with sound plays
  (unless the skin's preview allows music).
* **File operations:** when one ends, an "operation finished" action is
  sent once (the menu refreshes on it).
* **Globe:** with `<Main Globe>`, the globe sways back and forth along the
  path of 512 heights from the skin, at a speed tied to the frame rate.
