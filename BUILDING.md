# Building UnleashX

The result is a new `Default.xbe` built from `src/`: it behaves like
UnleashX 0.39.0528A but is not a byte-for-byte copy of the original (see
the [README](README.md)).

## What you need

* **The Xbox XDK**, build 5933 (the one UnleashX was built with; nearby 5xxx
  builds should work too: the XDK 5788 headers and libraries also build
  cleanly). It isn't included. Either an installed one
  (`C:\Program Files\Microsoft Xbox SDK`, with `xbox\bin`, `xbox\include`,
  `xbox\lib`) or a copy of that folder anywhere, or a flat copy of `bin`,
  `include` and `lib`. Only its compiler (`bin\vc71`), `imagebld`, headers
  and libraries are used.
* **Python 3**, for the last step (rescrambling the XBE for retail
  kernels, `tools/xbe.py retail`).
* On **Linux or macOS**: GNU make, Wine (to run the XDK's tools) and a C
  compiler (for extract-xiso).
* On **Windows**: Visual Studio .NET 2003 with the XDK installed, or GNU
  make from MSYS2 or Git Bash.

Everything else is in the repository.

## Linux and macOS

```sh
make -f Makefile.xbox XDK=/path/to/xdk info     # check what it found
make -f Makefile.xbox XDK=/path/to/xdk -j4 xiso
```

(`make xiso XDK=...` and `make xbox XDK=...` from the main Makefile do the
same.)

Instead of `XDK=` you can set the `XDK` environment variable, or put the
XDK (or a symlink to it) at `./xdk`; do that if its path has spaces,
which make can't handle.

Outputs, in `build/xbox/`:

| File | What |
|---|---|
| `default.xbe` | UnleashX, scrambled for retail kernels (consoles, xemu) |
| `debug.xbe` | The same as imagebld made it, for debug kits |
| `UnleashX.iso` | A disc image: `default.xbe`, the XML files and the sample skin (`assets/disc/`) |
| `default.map` | The linker map, for addresses in crash reports |
| `obj/**/*.log` | The compiler's full output for each file |

A full build takes a minute or two (`-j4`). The first time, Wine may set up
its prefix, which takes a little longer.

## Windows

**Visual Studio .NET 2003**: open `vs2003/UnleashX.sln` and build the
Release configuration. The XDK's Visual Studio integration provides the Xbox
platform, compiler and libraries; the project's post-build step runs
imagebld with UnleashX's sections and then `tools\xbe.py retail` (if Python
is on the path). The output is `build\vs2003\default.xbe` (the
`UnleashX.xbe` the IDE's own image step makes next to it lacks UnleashX's
resources). The project is generated
from the same file lists as the Makefile by `python tools/gen_vcproj.py`;
rerun that after adding or removing files, or changing the zips in
`assets/`.

**GNU make**: from an MSYS2 or Git Bash shell, the same command as above
runs the XDK's tools directly (no Wine). Make can't handle spaces in paths,
so link the XDK into the repository first (a junction, from `cmd`):

```bat
mklink /J xdk "C:\Program Files\Microsoft Xbox SDK"
```

and then just `make -f Makefile.xbox -j4 xiso`.

## Running it

**xemu**: set the disc image as the DVD (Machine → Load Disc) and boot. xemu
needs an MCPX boot ROM and a flash BIOS (your own dumps). The XBE isn't
signed, so the BIOS has to be one that runs unsigned code (a modded BIOS;
the 4627 retail BIOS in xemu has worked in our tests). A blank hard disk
image is at <https://github.com/xemu-project/xemu-hdd-image>.

FTP into xemu: turn networking on (NAT) and forward the guest's port 21 to
a host port. Either add this to `xemu.toml` (`~/.local/share/xemu/xemu/` on
Linux) before starting xemu:

```toml
[net]
enable = true
backend = 'nat'

[net.nat]
forward_ports = [ { host = 2121, guest = 21, protocol = 'tcp' } ]
```

or, on a running xemu started with a monitor (`-qmp unix:qmp.sock,server,nowait`),
send the HMP command `hostfwd_add xemu-netdev tcp:127.0.0.1:2121-:21`. Then
`python3 tools/xemu_ftp.py ls /E` (login `xbox`/`xbox`). The script uses
active mode with the address the guest sees the host at (10.0.2.2), so the
data ports need no forwarding; a normal client in passive mode would need the
whole `MinPASVPort`..`MaxPASVPort` range forwarded.

**A modded console**: run `default.xbe` from the hard disk (FTP it to, say,
`E:\Apps\UnleashX\` with the XML files and `Skins` from `assets/disc/`), or
burn the disc image. As a dashboard, it replaces your current one the usual
way; try it as an app first.

On first start UnleashX unpacks its media into a save game on `E:`
(`E:\UDATA\9e115330\`).

## Changing things

* **The media** (`assets/media.zip`: fonts, the globe and orb models and
  textures, the splash image, sounds): unzip, change, zip again. The size
  of the zip is compiled in and recorded on the Xbox, so a changed zip is
  unpacked again on the next start. The **built-in skins**
  (`assets/internal_skins.zip`, the `Skin.xml` files) work the same way.
* **The default font** and **title images** are `.xpr` files
  (`assets/default_font.xpr`, `title_image.xpr`, `save_image.xpr`); the
  XDK's bundler makes those.
* **Files on the disc**: anything in `assets/disc/` goes onto the disc
  image (`Config.xml`, the languages, `Skins/<name>/`).
* **The code**: `src/` by subsystem, see the [docs](docs/). `make test`
  builds the host tests with any modern g++.

## Troubleshooting

* `No XDK found`: `make -f Makefile.xbox XDK=... info` lists what was found;
  the compiler is looked for in `bin/vc71`, imagebld in `bin`.
* A compile error: the full output is in `build/xbox/obj/<file>.obj.log`.
* Black screen or a reboot loop on the Xbox or in xemu: UnleashX's crash
  handler writes the exception to `Z:\debug.txt` and reboots. The address in
  it can be looked up in `default.map` (the map's addresses are 0x3EF480
  above the XBE's for `.text`: the PE was linked at 0x400000 with `.text`
  at 0x480, imagebld moves it to 0x11000).
