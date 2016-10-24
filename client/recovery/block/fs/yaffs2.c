#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <utils/log.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <types.h>
#include <linux/jffs2.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/fs/fs_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG  "fs_yaffs2"

static int yaffs2_init(struct filesystem *fs) {
    if (!fs_init(fs))
        return false;
    fs->tagsize = YAFFS2_TAG_SIZE;
    FS_FLAG_SET(fs, AUTOPLACE);
    FS_FLAG_SET(fs, WRITEOOB);
    FS_FLAG_SET(fs, MARKBAD);
    return true;
};

static long long yaffs2_erase(struct filesystem *fs) {
    return mtd_basic_erase(fs);
}

static long long yaffs2_read(struct filesystem *fs) {
    return mtd_basic_read(fs);
}

static long long yaffs2_write(struct filesystem *fs) {
    return mtd_basic_write(fs);
}

static long long yaffs2_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long yaffs2_get_leb_size(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    return mtd->eb_size + YAFFS2_TAG_SIZE;
}
static long long yaffs2_get_max_mapped_size_in_partition(struct filesystem *fs) {
    return mtd_block_scan(fs);
}

struct filesystem fs_yaffs2 = {
    .name = BM_FILE_TYPE_YAFFS2,
    .init = yaffs2_init,
    .erase = yaffs2_erase,
    .read = yaffs2_read,
    .write = yaffs2_write,
    .get_operate_start_address = yaffs2_get_operate_start_address,
    .get_leb_size = yaffs2_get_leb_size,
    .get_max_mapped_size_in_partition =
            yaffs2_get_max_mapped_size_in_partition,
};
