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
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/block_manager.h>

#define LOG_TAG BM_BLOCK_TYPE_MTD

static int open_libmtd(struct block_manager* this) {
    int retval = 0;
    int i;

    this->mtd_desc = libmtd_open();
    if (!this->mtd_desc) {
        LOGE("Failed to open libmtd");
        return false;
    }

    retval = mtd_get_info(this->mtd_desc, &this->mtd_info);
    if (retval < 0) {
        LOGE("Failed to get mtd info.");
        return false;
    }

    this->mtd_dev_info = (struct mtd_dev_info*) malloc(
                             sizeof(struct mtd_dev_info) * this->mtd_info.mtd_dev_cnt);
    memset(this->mtd_dev_info, 0,
           sizeof(struct mtd_dev_info) * this->mtd_info.mtd_dev_cnt);
    for (i = 0; i < this->mtd_info.mtd_dev_cnt; i++) {
        retval = mtd_get_dev_info1(this->mtd_desc, i, &this->mtd_dev_info[i]);
        if (retval < 0){
            LOGE("Can\'t get dev info at \"mtd%d\"\n", i);
            return false;
        }
    }

    return true;
}

static int close_libmtd(struct block_manager* this) {
    if (this->mtd_desc)
        libmtd_close(this->mtd_desc);

    if (this->mtd_dev_info)
        free(this->mtd_dev_info);

    return true;
}

static int mtd_block_erase(struct block_manager* this, unsigned long offset, unsigned long length,
            struct bm_operation_option *option) {
    return true;
}

static int mtd_block_write(struct block_manager* this, unsigned long offset, char* buf,
            unsigned long length,  struct bm_operation_option *option) {
    return true;
}

static int mtd_block_read(struct block_manager* this, unsigned long offset, char* buf,
                unsigned long length, struct bm_operation_option *option) {
    return true;
}

// preperation  for writing, exactly caculate the starting offset of nandflash
static int mtd_write_prepare(struct block_manager* this, unsigned long offset,  char *imgtype,
                struct bm_write_prepare_info *info) {
    return true;
}


static int mtd_write_finish(struct block_manager* this) {
    return true;
}

static unsigned long mtd_get_max_allowable_filesize_in_partition(struct block_manager* this,  unsigned long offset,
                char *imgtype) {
    unsigned long retval  = 0;

    return retval;
}

// static int get_mtd_info(struct block_manager* this) {
//     return true;
// }

// static struct mtd_dev_info* get_mtd_dev_info_by_name(struct block_manager* this, const char* mtdchar) {
//     return true;
// }

static struct block_manager mtd_manager = {
    .name = BM_BLOCK_TYPE_MTD,
    .erase = mtd_block_erase,
    .read = mtd_block_read,
    .write = mtd_block_write,
    .get_max_allowable_filesize_in_partition = mtd_get_max_allowable_filesize_in_partition,
    .write_prepare = mtd_write_prepare,
    .write_finish = mtd_write_finish,

    // .get_mtd_info = get_mtd_info,
    // .get_mtd_dev_info_by_name = get_mtd_dev_info_by_name,
};

int mtd_manager_init(void) {
    if (register_block_manager(&mtd_manager)
            && open_libmtd(&mtd_manager)) {
        return true;
    }
    return false;
}

int mtd_manager_destroy(void) {
    if (close_libmtd(&mtd_manager)
            && unregister_block_manager(&mtd_manager)) {
        return true;
    }
    return false;
}
