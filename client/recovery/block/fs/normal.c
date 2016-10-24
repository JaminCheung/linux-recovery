#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/log.h>
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
#include <block/fs/fs_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG "fs_normal"

static int normal_init(struct filesystem *fs) {
    if (!fs_init(fs))
        return false;
    FS_FLAG_SET(fs, PAD);
    FS_FLAG_SET(fs, MARKBAD);
    return true;
};

static long long normal_erase(struct filesystem *fs) {
    return mtd_basic_erase(fs);
}

static long long normal_read(struct filesystem *fs) {
    return mtd_basic_read(fs);
}

static long long normal_write(struct filesystem *fs) {
    return mtd_basic_write(fs);
}

static long long normal_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long normal_get_leb_size(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    return mtd->eb_size;
}
static long long normal_get_max_mapped_size_in_partition(struct filesystem *fs) {
    return mtd_block_scan(fs);
}

struct filesystem fs_normal = {
    .name = BM_FILE_TYPE_NORMAL,
    .init = normal_init,
    .erase = normal_erase,
    .read = normal_read,
    .write = normal_write,
    .get_operate_start_address = normal_get_operate_start_address,
    .get_leb_size = normal_get_leb_size,
    .get_max_mapped_size_in_partition = 
            normal_get_max_mapped_size_in_partition,
};



