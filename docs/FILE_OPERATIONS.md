# File operations

The file operations behind the `copy`, `move`, `delete`, `zip`, `unzip` and `CopyDVD` actions, the menu's delete and the FTP title-image zip. They are written from `src/system/FileOps.cpp`, which sits on the virtual file layer in `src/system/FileIO.h`.

## Running an operation

Each operation runs on a thread of its own, one at a time:

1. It waits for the previous operation to finish.
2. It lists everything under the source (`CollectFiles`): first each folder's files, then each subfolder, then that subfolder's contents.
3. It checks that the destination has room for the total size: error 112 if not, or 3 if the drive isn't mounted.
4. It works through the list.

While it runs, the progress box shows the current file and target, the file and overall percentages, the elapsed and remaining time, and the rate. Remaining time is `(left / (done / elapsed + 1)) + 1` seconds; the rate is in KB/s. The operation can be cancelled.

The box itself is `DrawProgressBox` (`src/render/ProgressBox.cpp`).
* **Contents:** a beveled box with the title (its dots count up every half second: "Copying...", "Copying.", "Copying.."), up to two lines, then a blue bar per percentage with the number to its right, then "Elapse: mm:ss  Remaining: mm:ss Rate: n KB/s".
* **Size:** a zero height fits the parts, and a zero width is the screen's less 70.
* **The FTP manager's version** (`DrawTransferBox`) has one bar, with "Current Transfer *n* Bytes *n* Bytes" above it.
* **Quirks:**
  * The byte counts print only their low 32 bits.
  * A box that would run off the bottom is moved up only until it ends 5 pixels below the screen's edge.

## Settings

| `<Preference>` setting | Effect |
|---|---|
| `AutoRename` | Renames names that FATX can't hold (see `MakeFatxPath`) and logs each rename. |
| `AbortOnError` | Stops at the first failure. Otherwise the operation goes on and only reports the failure at the end. |
| `AutoPatch` | After copying `D:\`, patches the copied XBEs to run from the hard disk. Each original is kept as `.bak`. |
| `DefaultFolder` | Where `CopyDVD` puts a disc, if that drive has over 1 GB free. |
| LED patterns | One LED pattern while files are listed or worked on, another while copying. |

## The log

Each operation starts a log with `BEGIN ...` and adds one line per event: a failure, a rename, a skipped file, a patched XBE. The log is kept, with an `END ...` line, only if something was logged. The menu then offers to show it.

## Copy

* **A single file** goes into the destination folder, or to the destination name if that has a dot after its last backslash.
* **A folder:**
  * Files already at the destination with the same size are skipped.
  * Each file is tried three times.
  * `dashupdate.xbe`, `update.xbe` and `downloader.xbe` on a game disc are left out.

## Move

* **Same drive:** files are renamed with `MoveFileWithProgress`. Folders aren't created first; see the quirks.
* **Another drive:** the folders are created and the files are moved. Then the emptied folders are removed, deepest first, and finally the source folder.

## Delete

All the files go first, then the folders deepest first, then the folder itself.

## Zip, unzip and unrar

* **Zip** writes `<name>.zip` next to (or at) the destination, with paths relative to the source. The FTP server's title-image zip keeps full paths.
* **Unzip** turns `:` into `_` in names, and recreates the folders as it goes.
* **Unrar** strips the archive's top folder when the destination already ends with that folder's name.

## Copy DVD

The disc goes into a folder named after its contents:

| Disc | Folder |
|---|---|
| Game disc | `Games\<title>`. The title is cut at `:` and has no symbols, no spaces, and at most 20 characters; if it ends up empty, the title ID is used. |
| Video disc | `Videos\Video` |
| Data disc | `Data\<disc label>` |

The drive is picked in this order:

1. The `DefaultFolder` drive, if it has over 1 GB free.
2. Otherwise the roomier of F: and G:.
3. E:, if it has more room than whichever drive was picked.

An XBE given as the argument copies just that game's folder.

## Quirks kept

* A single file zips with the zip's own path as the entry name.
* A move on the same drive doesn't create subfolders first, so files in subfolders can fail to move (TODO: verify on hardware).
* A delete of a path that doesn't exist leaves the "operation running" flag set.
