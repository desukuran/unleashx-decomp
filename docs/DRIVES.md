# Drives

UnleashX maps its own drive letters with kernel symbolic links (`src/system/Drives.cpp`, `Storage.cpp`, `Dvd.cpp`, `DvdPoll.cpp`).

| Letters | What | Device |
|---|---|---|
| C, E, F, G, X, Y, Z | Hard disk partitions 2, 1, 6, 7, 3, 4, 5 | `\Device\Harddisk0\PartitionN` |
| D | The DVD drive | `\Device\Cdrom0` |
| H..O | Memory units, `port * 2 + slot` | XAPI's memory unit mount |
| P..S | FAT32 partitions of a USB mass storage device | The bundled FAT32 driver |
| T, U | `E:\TDATA`, and `E:\UDATA\<title id>` for the selected XBE | |
| V | A second CD-ROM device | `\Device\CdRom1` |

* **F: and G:** follow `EnableDriveF` / `EnableDriveG`. G: also needs F: to be over 1 GB.
* **X, Y, Z:** an empty one is formatted when it's mounted.
* **Z: fallback:** if Z: can't be mounted, the title's utility drive is used instead. Quirk: the lookup of its device gets the letter `'Z'` where a pointer to the drive's name belongs, so that path would fault.

## Memory units and USB storage

* **Polling:** memory units are polled once a second (`RefreshMemoryUnits`), and every drive's space every 30 seconds or after an FTP upload (`RefreshDrives`). Inserted units are mounted; removed ones are forgotten.
* **Mounting:** a slot is first mounted as a memory unit (`CXBMemUnit`, from the XDK samples). Its name is "Xbox MU 1A (name)" and its space is read through the letter.
* **USB fallback:** if that fails, the slot's device is opened raw and handed to the FAT32 driver, and each partition found goes to P..S. The memory unit letter then stays unmounted, so `MountDrive` returns FALSE even though the USB letters work.
* **Removal:** removing such a device shuts the FAT32 driver down and clears P..S from its first partition on.

## Formatting

`FormatDrive` handles three cases:
* **Memory units H..O:** formatted as FAT (`XapiFormatFATVolume`).
* **Hard disk partitions:** Z: is linked to the partition and the utility drive is formatted, then Z: is remounted. Quirk: that works for any partition, C: and E: included.
* **USB letters P..S:** unmounted, and then the format fails.

## The DVD drive

`PollDvdDrive` runs from the monitor thread.

1. Every 333 ms it reads the SMC's tray state: 0x10 open, 0x40 closed without media, 0x60 closed with media, anything else moving.
2. With `DVDMediaCheckExtended`, "closed without media" is double-checked by asking the drive (`IOCTL_CDROM_CHECK_VERIFY`).
3. When a disc arrives, D: is remounted (the file system is dismounted in between). 800 ms later the disc is identified, first match wins:

| Test | Type | Title (`$DVDTitle$`) |
|---|---|---|
| `D:\default.xbe` | Game | `D:\DiskTitle.txt`, else the XBE's title, else "Unknown Xbox Title" |
| `D:\VIDEO_TS\VIDEO_TS.IFO` | Video | |
| Any file under `D:\` | Data | |
| A readable CD table of contents | Audio CD | |
| Raw sector 16 readable | Data, through XBMC's ISO 9660 reader | The volume name, or "ISO9660 Disc" |
| A raw CD-DA sector readable | Audio CD | |

Otherwise the disc is Unknown.

When the tray opens, the title and the ISO reader are dropped and D: is unmounted.
* **Quirk:** only the first half of the title buffer is cleared.
* **Quirk:** the reader's device handle is closed twice.
