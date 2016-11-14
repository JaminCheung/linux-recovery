#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <types.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/ubi/ubi.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/block_manager.h>
#include <version.h>
#include <utils/assert.h>
#include <utils/common.h>
#include "header.h"

extern int test_read(void);
extern int test_sysinfo(void);
extern int test_format(void);
extern int test_update(void);

int main(int argc, char **argv) {
#if defined TEST_READ
    test_read();
#elif defined TEST_SYSINFO
    test_sysinfo();
#elif defined TEST_FORMAT
    test_format();
#elif defined TEST_UPDATE
    test_update();
#endif

    return 0;
}