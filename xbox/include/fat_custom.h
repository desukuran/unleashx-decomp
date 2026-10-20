// UnleashX build options for fat_io_lib (thirdparty/fat_io_lib/fat_opts.h,
// with FATFS_USE_CUSTOM_OPTS_FILE). The file manager keeps several files
// open at once (copies, the music player), so more than the default two.
#pragma once
#define FATFS_MAX_OPEN_FILES 32
// The FAT is read 16 KB at a time (counting the free space reads all of
// it; one sector at a time that took 40 s on a 64 MB stick in xemu).
#define FAT_BUFFERS 4
#define FAT_BUFFER_SECTORS 32
#define FATFS_INC_TIME_DATE_SUPPORT 1
#define FAT_PRINTF(a)
