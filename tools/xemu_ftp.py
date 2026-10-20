#!/usr/bin/env python3
"""FTP to UnleashX running in xemu with NAT networking.

xemu's NAT only forwards the ports you ask for, so forward the control port
(guest 21 to host 2121, see BUILDING.md). Data connections use active mode:
the client listens on the host and sends PORT 10,0,2,2,..., the address the
guest sees the host at, so no data ports need forwarding.

  xemu_ftp.py ls  [dir]              list a folder (/E, /D/Folder, ...)
  xemu_ftp.py get <remote> <local>   download
  xemu_ftp.py put <local> <remote>   upload
  xemu_ftp.py putdir <local> <remote>  upload a folder's contents into remote
  xemu_ftp.py cmd <command...>       send a raw command (SITE ..., MKD ...)

--port N picks another host port; the login is xbox/xbox (Config.xml).
"""
import ftplib
import os
import socket
import sys


class XemuFTP(ftplib.FTP):
    def makeport(self):
        listener = socket.create_server(("127.0.0.1", 0))
        port = listener.getsockname()[1]
        self.voidcmd("PORT 10,0,2,2,%d,%d" % (port >> 8, port & 0xFF))
        return listener


def put_tree(ftp, local, remote):
    """Uploads the folder local into remote (which must exist)."""
    for root, dirs, files in os.walk(local):
        rel = os.path.relpath(root, local)
        target = remote if rel == "." else remote + "/" + rel.replace(os.sep, "/")
        try:
            ftp.sendcmd("MKD " + target)   # (says so when it already exists)
        except ftplib.error_perm:
            pass
        for name in sorted(files):
            with open(os.path.join(root, name), "rb") as source:
                ftp.storbinary("STOR %s/%s" % (target, name), source)
            print(target + "/" + name)


def main(argv):
    port = 2121
    if len(argv) > 2 and argv[1] == "--port":
        port = int(argv[2])
        argv = argv[:1] + argv[3:]
    if len(argv) < 2 or argv[1] not in ("ls", "get", "put", "putdir", "cmd"):
        print(__doc__.strip())
        return 2
    ftp = XemuFTP()
    ftp.connect("127.0.0.1", port, timeout=30)
    ftp.login("xbox", "xbox")
    ftp.set_pasv(False)
    op, args = argv[1], argv[2:]
    if op == "ls":
        # UnleashX's LIST ignores a path argument, so change to it first.
        ftp.cwd(args[0] if args else "/")
        ftp.retrlines("LIST")
    elif op == "get":
        with open(args[1], "wb") as out:
            print(ftp.retrbinary("RETR " + args[0], out.write))
    elif op == "put":
        with open(args[0], "rb") as source:
            print(ftp.storbinary("STOR " + args[1], source))
    elif op == "putdir":
        put_tree(ftp, args[0], args[1].rstrip("/"))
    else:
        print(ftp.sendcmd(" ".join(args)))
    ftp.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
