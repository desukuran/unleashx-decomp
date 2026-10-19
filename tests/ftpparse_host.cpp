// Host stand-in for D. J. Bernstein's ftpparse (linked into the XBE, not
// part of this repository): just the UNIX "ls -l" lines the UnleashX FTP
// server sends.
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "../src/thirdparty/FtpParse.h"

int ftpparse(struct ftpparse *fp, char *buf, int len)
{
	memset(fp, 0, sizeof(*fp));
	if (len < 10 || (buf[0] != '-' && buf[0] != 'd'))
		return 0;
	// perms, links, owner, group, size, month, day, time or year, name
	int field = 0;
	int i = 0;
	while (i < len && field < 8)
	{
		while (i < len && buf[i] != ' ')
			i++;
		while (i < len && buf[i] == ' ')
			i++;
		field++;
		if (field == 4)
			fp->size = atol(buf + i);
	}
	if (i >= len)
		return 0;
	fp->name = buf + i;
	fp->namelen = len - i;
	if (buf[0] == 'd')
		fp->flagtrycwd = 1;
	else
		fp->flagtryretr = 1;
	fp->sizetype = FTPPARSE_SIZE_BINARY;
	return 1;
}
