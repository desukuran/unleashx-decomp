# The FTP server

UnleashX runs its own FTP server so files can be copied to and from the Xbox
over the network. This page was written from the decompiled server
(`src/net/FtpServer.cpp`, `FtpConnection.cpp`, `FtpTransfer.cpp`) and the
dashboard's SITE command handler (`src/app/FtpEvents.cpp`). A host test
(`tests/ftp_test.cpp`) runs the same code on a PC and drives a session with
a small client.

## Settings

```xml
<Settings>
  <FTP Enable="Yes">
    <User>xbox</User>
    <Password>xbox</Password>
    <Port>21</Port>
    <MaxUsers>2</MaxUsers>
    <AllowAnon>No</AllowAnon>
    <Greeting>Welcome to XBOX FTP Server</Greeting>
    <Boost>Yes</Boost>
    <MinPASVPort>1024</MinPASVPort>
    <MaxPASVPort>65536</MaxPASVPort>
    <SYST>UNIX Type: L8</SYST>
  </FTP>
</Settings>
```

| Setting | |
|---|---|
| `User`, `Password` | The login. The user name is compared without case, the password with case. Empty values keep the defaults `xbox`/`xbox`. |
| `Port` | 21. |
| `MaxUsers` | 1 to 15 simultaneous logins (other values keep 2). |
| `AllowAnon` | Lets `anonymous` log in with any password. Anonymous users can only read: no uploads, deletes, renames, folders, EXEC or SITE. |
| `Greeting` | The text of the 230 reply after a successful login. |
| `Boost` | Allows the Avalaunch boost mode (below). With it on, `SYST` claims to be Avalaunch, which is how boost-capable clients notice. |
| `MinPASVPort`, `MaxPASVPort` | The range passive data ports are picked from at random. Values outside 1024..65536 are clamped. |
| `SYST` | The `SYST` reply when boost mode is off. `UNIX / UX ` is put in front unless it starts with `UNIX `. |

`<Network><AutoDetect>` also matters: while it is on and nobody is
connecting, the server answers the LAN discovery broadcast (below).

The server starts once the network is up. The menu actions `FTPStart`,
`FTPStop` and `FTPReset` control it (see MENU_ACTIONS.md). While a file is
sent or received the LED shows `<LED><FTPSend>` or `<LED><FTPReceive>`, during
a boost transfer `<FTPBoost>`.

## Paths

FTP paths start with the drive letter: `/C/xboxdash.xbe` is `C:\xboxdash.xbe`
and `/` lists the drives (every mounted drive except T: and U:, all dated
like `C:\xboxdash.xbe`). `\` works as well as `/`; `:` is removed. Only `..`
and `CDUP` go up a level: a path like `a/../b` is kept as it is.

## Logging in

After connecting the client gets a banner with its address, the Xbox's local
time, the server statistics (users, bytes and files received and sent,
failures) and the used/free/total space of every drive, then
`220 UnleashX FTP Server ready.`

Login is `USER` then `PASS`. Three wrong passwords end the connection. A
connection is closed after 5 idle minutes (not while a transfer runs), and
when more than `MaxUsers` are logged in a new one gets `421 Too many users
are connected, please try again later.`

## Commands

| Command | |
|---|---|
| `USER`, `PASS` | Only before login. |
| `QUIT`, `BYE` | `221 Bye`. |
| `PWD`, `XPWD` | The current directory. |
| `CWD`, `XCWD` | Change directory. `CWD /` and `CWD /..` go to the root. |
| `CDUP`, `XCUP` | Up one level. |
| `LIST [path]` | `ls -l` style listing. Options (anything starting with `-`) are ignored. |
| `NLST` | Names only, always of the current directory. |
| `RETR file` | Download. `REST` resumes. |
| `STOR file` | Upload. `REST` resumes. Not allowed straight into `/`. |
| `APPE file` | Upload, appended to an existing file. |
| `DELE file`, `RMD dir`, `XRMD dir` | Delete. Not for anonymous users. |
| `MKD dir`, `XMKD dir` | Create a folder. |
| `RNFR old`, `RNTO new` | Rename or move. |
| `SIZE file` | The size in bytes. |
| `REST offset` | Restart point for the next transfer. |
| `TYPE A`, `TYPE I` | Accepted; all transfers are binary. |
| `PASV`, `PASW` | Passive mode: the server listens on a random port and waits for the client. |
| `PORT a,b,c,d,p1,p2` | Active mode: the server connects to the client. `0,0,0,0` means the address of the control connection. |
| `ABOR` | Stops the running transfer. |
| `NOOP` | |
| `SYST`, `RSYST` | The system type. |
| `HELP [command]` | The command list, or one command's help. |
| `EXEC xbe` | Meant to launch an XBE, but broken (see quirks); `SITE EXEC` works. |
| `SITE ...` | See below. |
| `SPRT BOOST` | Answers `226 Boost mode OK` after a successful `SITE BOOST`. |

When the setting `<Preference><AutoRename>` is on, uploaded file names are
made FATX-legal (allowed characters, at most 42) and the reply says
`Transfer Complete. File renamed to '...'`. With it off, longer names are
refused with `450 Filename too long`.

## SITE commands

Handled by the server:

| Command | |
|---|---|
| `SITE BOOST SUPPORTED` | `226 Boost mode OK` if boost mode is allowed. |
| `SITE BOOST START UPLOAD n` | Starts a boost upload (below). |
| `SITE FREESPACE x` | Free bytes on drive x (`0` if not mounted), as a 220 reply. |
| `SITE MAKE_FILE_TREE dir` | Writes the tree under dir to `content.txt` on the first of Z:, X:, Y: with 1 MB free: `D,<folder>` and `F,<name>,<size>,<attributes>,<created>` lines. |
| `SITE TITLES dir` | One `150` line per game in dir: `<folder>\default.xbe`, title, bytes, bytes on disk, title ID, folders, files, allowed media, region (tab-separated). The title images are saved as `<title id>.bmp` in `<drive>:\titleimages` and zipped to `<drive>:\titleimage.zip`. |
| `SITE TITLEIMAGE` | `200 OK` if the last `SITE TITLES` could save title images. |

Handled by the dashboard:

| Command | |
|---|---|
| `SITE REBOOT`, `SITE RESTART` | Back to the default dashboard. |
| `SITE SHUTDOWN` | Turns the Xbox off. |
| `SITE POWERCYCLE` | Power cycle. |
| `SITE FTPRESET`, `SITE FTPSTOP` | Restart or stop the FTP server. |
| `SITE NETRESET` | Restart the network. |
| `SITE TRAYCLOSE`, `SITE TRAYOPEN`, `SITE EJECT` | The DVD tray. |
| `SITE DRIVESTAT` | Used/free/total MB of every drive. |
| `SITE EXEC path`, `SITE LAUNCH path` | Launch an XBE, e.g. `SITE EXEC /E/Apps/XBMC/default.xbe`. |
| `SITE MD5 path` | The MD5 of a file. |
| `SITE XBERENAME "path" new title` | Changes the title name in an XBE's certificate. |

Anything else gets `502 Command not implemented.`

## Discovery

Other dashboards find FTP servers by broadcasting `ping` to UDP port 4905.
The server answers (to the broadcast address and to the sender) with
`<nickname>;<user>;<password>;<port>;<boost>`, where boost is 1 if boost mode
is allowed. UnleashX's own network scan (`ScanNetworkClients`) sends the same
ping and turns the answers into FTP sites for the file manager.

## Boost mode

Avalaunch's fast upload of a whole folder tree:

1. The client sends `SITE BOOST START UPLOAD <n>`; the server answers
   `226 Initiating Boost Mode. Start sending list.`
2. The client sends n lines on the control connection: `D,<folder>` for a
   folder (an FTP path, or an Xbox path with `:`), `F,<name>,<size>` for a
   file in the last folder. An empty line also ends the list.
3. The server answers `226 - <entries> <port>` and waits up to 15 seconds for
   a data connection to that port.
4. The client sends the contents of all files back to back. The server
   creates the folders and writes the files, reading into two 1 MB buffers
   while a second thread writes. Everything else on the server waits in the
   meantime.
5. `226 Ending Boost Mode. Transfer Complete.` or `226 Socket error during
   transfer. Boost Mode terminated.`

## Quirks

* Files on USB drives (P: to S:) can't be downloaded (`RETR`) or uploaded
  by the client: both read with `ReadNext(0)`, which means "a whole buffer"
  for the other drives but reads 0 bytes from the FAT32 driver, so the
  transfer stops at once with 426. Listings and file manager copies work.
  (0x000B70E3 and 0x000815E4; kept.)
Behaviour the original has, kept in the decompiled code:

* The discovery answer contains the FTP password in plain text.
* `EXEC` can't work: the server removes `:` from the argument and the
  dashboard rebuilds `C\dir\x.xbe` as `C:dir\x.xbe` without the backslash.
* `SITE` arguments are also stripped of `:` and get `\` turned into `/`
  (a check meant to skip SITE tests the wrong command).
* After `USER`, any command other than `PASS` (even another `USER`) gets
  `503 Login with USER first.`
* Reply texts are run through `printf`, so a path with `%` in it in a
  `PWD`/`CDUP`/`DELE` reply can garble the reply.
* `HELP` sends the command list as one 214 reply with embedded line breaks.
* `NLST` ignores its argument; the root listing ignores `NLST`'s names-only
  mode; listings show the creation date.
* `STOR` reports `226 Transfer Complete.` when the connection breaks or
  `ABOR` stops it, and also after `450 Cannot access file.`
* `APPE` statistics skip every seventh block.
* `RNTO` doesn't forget the source, so a second `RNTO` renames again.
* The store-in-root check only catches relative names (`STOR /x` in `/` is
  let through).
* The passive address is worked out once and never refreshed.
* Without a `PORT` command, active mode connects to a garbled port (the
  control connection's port byte divided by 255).
* `SITE BOOST START DOWNLOAD` is recognized and then ignored, with no reply.
* If a boost client disconnects early the server keeps waiting for data
  forever; the fallback without the big buffers loops endlessly when it
  resumes a partly uploaded file.
* `SITE MAKE_FILE_TREE` only converts FTP paths whose first `/` is the
  second character, and uses an unmounted X: or Y: as is.

# The FTP client

The file manager can also browse other FTP servers. Each site pane talks to
its server through `CDirFTP` (`src/net/FtpClient.cpp`), the FTP version of
the virtual file layer's `CDirectory`, and logs every command and reply to
the FTP log window. `tests/ftpclient_test.cpp` runs it against the server
above.

## Session

* **Connecting:** the client connects with 30/60 second receive/send timeouts
  (15/20 once connected) and waits for `220`. It logs in with `USER` and
  `PASS`.
  * An empty user name logs in as `Anonymous` with the password
    `UnleashX@TheBestDash.com`.
  * A `230-` banner is read up to the final `230 `.
* **After login:**
  * `SYST` is sent. A reply containing `UnleashX` or `/ UX ` marks an
    UnleashX server.
  * `REST 100` is sent, and on `350` reset with `REST 0`. This only records
    that resuming works; nothing resumes.
* **Directories:** `CWD`, then `PWD` for the new current directory, which
  always ends in `/`. A name without `/` is taken relative to the current
  directory, and `CDUP` goes up.
* **Data connections:** passive (`PASV`) first. The first time a server
  refuses `PASV`, the client switches to active mode (`PORT`) for the rest
  of the session. Active mode listens on the next port of
  `<FTPClient><MinPort>`..`<MaxPort>` and waits 15 seconds for the server.
* **Listings:** `LIST` in ASCII mode, once per directory, parsed with
  ftpparse. `.` and `..` are skipped.
* **Transfers:** in binary mode, with the same progress box as the local
  file operations.
  * **Downloads:** `SIZE` gives the total. Names FATX can't hold are renamed,
    and both names are logged. An empty file is created without `RETR`.
  * **Uploads:** `STOR`.
* **Disconnecting:** `QUIT`.

## Folders

* **Download:** a source ending in `/` downloads the whole tree: the files of
  each folder first, then its subfolders.
* **Upload:** a folder upload lists the local tree first. On an UnleashX
  server it then asks `SITE FREESPACE` and stops with "Not enough free space
  on target host." when the tree doesn't fit.
  * UnleashX servers create folders as files arrive.
  * For other servers, a folder gets `MKD` unless its parent's listing
    already has it.
  * A file that fails is logged and the upload goes on.
* **Delete:** deleting a folder `DELE`s every file in the tree, then `RMD`s
  the folders deepest first. It reports "Deleted n Folders and n Files in
  mm:ss."
* **Rename:** `RNFR` and `RNTO`.

The client also has a boost-mode upload, the client side of the server's
`SITE BOOST START UPLOAD`, for uploading folders. After logging in, the FTP
manager sends `SITE BOOST SUPPORTED` when the `SYST` reply doesn't contain
`UNIX Type:` (any case), and uses boost mode with servers that answer `226`.
If boost mode fails before any file was sent, the client logs "Boost mode
failed. Transfering in Normal mode." and falls back to a normal upload.

## Client quirks

* A refused `CWD` (`550`) still counts as success, staying in the directory
  `PWD` reports.
* A reply is read a byte at a time. If the server closes the connection
  mid-reply, the client waits forever.
* An upload whose local file can't be opened waits for the reply to a
  `STOR` it never sent (20 seconds).
* An empty downloaded file is "closed" with the wrong call, so its handle
  stays open.
* Cancelling the deletion of a folder tree logs "Upload Operation Canceled."
* The boost upload connects to the address of the last `PASV` reply, and
  skips empty files.
