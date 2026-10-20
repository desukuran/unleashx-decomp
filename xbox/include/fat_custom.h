// UnleashX build options for fat_io_lib (thirdparty/fat_io_lib/fat_opts.h,
// with FATFS_USE_CUSTOM_OPTS_FILE). The file manager keeps several files
// open at once (copies, the music player), so more than the default two.
#pragma once
#define FATFS_MAX_OPEN_FILES 32
#define FAT_BUFFERS 8
#define FATFS_INC_TIME_DATE_SUPPORT 1
#define FAT_PRINTF(a)
