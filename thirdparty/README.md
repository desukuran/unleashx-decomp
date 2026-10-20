# Third-party code

UnleashX linked these libraries statically. They aren't part of the decomp
(their code isn't reconstructed from the binary); the Xbox build compiles
these public sources instead, through small adapters in `../xbox/` that
present the interfaces UnleashX's code expects (declared in
`../src/thirdparty/`).

| Folder | What | Version | License | Changes |
|---|---|---|---|---|
| `libmad/` | Underbit's MPEG audio decoder (MP3 playback) | 0.15.1b | GPL v2 or later (`COPYING`) | none; the `msvc++` `config.h` and `mad.h` are used |
| `ftpparse/` | D. J. Bernstein's FTP `LIST` parser (the FTP client) | 20001223 | "Commercial use is fine, if you let me know what programs you're using this in." (see the header) | none |
| `zip_utils/` | Lucian Wischik's zip/unzip, repackaged Info-ZIP/zlib code (zip and skin archives, the built-in media) | 2005 | zlib/Info-ZIP style, free use (see the files' headers) | a stray line removed from the top of `zip.cpp` |
| `unrar/` | XBMC's UnrarXLib: Eugene Roshal's unRAR 3.51 with the `urarlib_*` interface (RAR archives) | unRAR 3.51 | unRAR license (`license.txt`): free to use for extracting; **may not be used to develop a RAR (WinRAR) compatible archiver** | `shim/` replaces the few XBMC classes it used (`XFILE::CFile`, `CUtil`, `URIUtils`, `CEvent`) with Win32 ones; `urarlib_list` reports folders as `FILE_ATTRIBUTE_DIRECTORY`, as UnleashX's copy did |
| `fat_io_lib/` | Rob Riglar's FAT File IO Library (USB drives P: to S:); UnleashX's driver was an early version of it | v2.6.11, commit in `VERSION` | GPL v3 (`LICENSE`) | marked "UnleashX build": several volumes (`fl_select_fs`, files remember their volume), `.`/`..` entries in new folders (UnleashX's copy had them), all FAT copies written, the FSInfo free count marked unknown, `"w"` truncates an existing file, empty files keep no cluster (and get one when written) |
| `extract-xiso/` | XboxDev's extract-xiso (makes the disc image; a host tool, not linked in) | 2.7.1, commit in `VERSION` | its own permissive license (`LICENSE.TXT`) | none |

Not from here:

* **The FAT32 driver's interface** (`Fat32*`, four volumes from the
  partition table, Win32-style searches) is our adapter onto fat_io_lib,
  `../xbox/Fat32.cpp`, tested by `tests/fat32_test.cpp` on mkfs.fat images.
* **XBMC's iso9660 reader** (DVD and CD data discs) is replaced by our own
  reader, `../xbox/Iso9660.cpp`, tested by `tests/iso9660_test.cpp`.
* **XKUtils** (Team Assembly): the few EEPROM functions UnleashX calls and
  HMAC-SHA1 are written in `../xbox/XKUtils.cpp`.
* **XBMC's CIoSupport** (tray, raw sectors) is written from the binary in
  `../xbox/IoSupport.cpp`.
* **The XDK's sample framework** (CXBFont, CXBApplicationEx, XBInput, ...)
  is UnleashX's own modified copy, reconstructed in `../src/xdk/`.
