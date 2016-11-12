#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
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
#include <utils/assert.h>
#include <block/fs/fs_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG  "fs_cramfs"

static int cramfs_init(struct filesystem *fs) {
    FS_FLAG_SET(fs, PAD);
    FS_FLAG_SET(fs, MARKBAD);
    return 0;
};

static int64_t cramfs_erase(struct filesystem *fs) {
    return mtd_basic_erase(fs);
}

static int64_t cramfs_read(struct filesystem *fs) {
    assert_die_if(1, "%s is not served temporarily\n", __func__);
    return 0;
}

static int64_t cramfs_write(struct filesystem *fs) {
    return mtd_basic_write(fs);
}

static int64_t cramfs_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long cramfs_get_leb_size(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    return mtd->eb_size;
}
static int64_t cramfs_get_max_mapped_size_in_partition(struct filesystem *fs) {
    return mtd_block_scan(fs);
}

struct filesystem fs_cramfs = {
    .name = BM_FILE_TYPE_CRAMFS,
    .init = cramfs_init,
    .alloc_params = fs_alloc_params,
    .free_params = fs_free_params,
    .set_params = fs_set_params,
    .erase = cramfs_erase,
    .read = cramfs_read,
    .write = cramfs_write,
    .get_operate_start_address = cramfs_get_operate_start_address,
    .get_leb_size = cramfs_get_leb_size,
    .get_max_mapped_size_in_partition =
    cramfs_get_max_mapped_size_in_partition,
};
