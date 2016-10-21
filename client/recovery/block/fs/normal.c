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

#define GET_TYPE(type, member)      (type *)(member->priv)   

static int normal_init(struct filesystem *fs) {
    return true;
};

static int normal_erase(struct filesystem *fs) {


    return true;
}

static int normal_read(struct filesystem *fs) {
    return true;
}

static int normal_write(struct filesystem *fs) {
    return true;
}

static long long normal_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long normal_get_leb_size(struct filesystem *fs) {
    return fs->params->blksize;
}
static long long normal_get_max_mapped_size_in_partition(struct filesystem *fs) {
    long long retval = 0;
    return retval;
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



