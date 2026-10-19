// D. J. Bernstein's ftpparse (LIST line parser), statically linked into
// UnleashX for the FTP client (0x000BB3F0-0x000BBE70). Third-party library:
// only the interface is declared here.
#pragma once

#include <ctime>

#define FTPPARSE_SIZE_UNKNOWN 0
#define FTPPARSE_SIZE_BINARY 1
#define FTPPARSE_SIZE_ASCII 2

#define FTPPARSE_MTIME_UNKNOWN 0
#define FTPPARSE_MTIME_LOCAL 1
#define FTPPARSE_MTIME_REMOTEMINUTE 2
#define FTPPARSE_MTIME_REMOTEDAY 3

#define FTPPARSE_ID_UNKNOWN 0
#define FTPPARSE_ID_FULL 1

struct ftpparse
{
	char *name;         // +0x00 not NUL-terminated
	int namelen;        // +0x04
	int flagtrycwd;     // +0x08 a directory
	int flagtryretr;    // +0x0C a file
	int sizetype;       // +0x10
	long size;          // +0x14
	int mtimetype;      // +0x18
	time_t mtime;       // +0x1C (32-bit on the Xbox)
	int idtype;         // +0x20
	char *id;           // +0x24
	int idlen;          // +0x28
};

// 1 if buf (len bytes, one line without its end) is a listing line. (fp in
// edi)
int ftpparse(struct ftpparse *fp, char *buf, int len);   // 0x000BB675
