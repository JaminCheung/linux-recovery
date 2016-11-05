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
#include <utils/assert.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/ubi/ubi.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/fs/fs_manager.h>

#define LOG_TAG "fs_manager"

int target_endian = __BYTE_ORDER;

extern struct filesystem fs_normal;
extern struct filesystem fs_jffs2;
extern struct filesystem fs_ubifs;
extern struct filesystem fs_yaffs2;
extern struct filesystem fs_cramfs;
static struct filesystem* fs_supported_list[] = {
    &fs_normal,
    &fs_jffs2,
    &fs_ubifs,
    &fs_yaffs2,
    &fs_cramfs,
};

void fs_write_flags_get(struct filesystem *fs,
                                int *noecc, int *autoplace, int *writeoob,
                                int *oobsize, int *pad, int *markbad) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    *noecc = FS_FLAG_IS_SET(fs, NOECC);
    *autoplace = FS_FLAG_IS_SET(fs, AUTOPLACE);
    *writeoob = FS_FLAG_IS_SET(fs, WRITEOOB);
    *oobsize = fs->tagsize ? fs->tagsize : mtd->oob_size;
    *pad = FS_FLAG_IS_SET(fs, PAD);
    *markbad = FS_FLAG_IS_SET(fs, MARKBAD);
#ifdef FS_OPEN_DEBUG
    LOGI("noecc = %d\n", *noecc);
    LOGI("autoplace = %d\n", *autoplace);
    LOGI("writeoob = %d\n", *writeoob);
    LOGI("oobsize = %d\n", *oobsize);
    LOGI("pad = %d\n", *pad);
    LOGI("markbad = %d\n", *markbad);
#endif
}

int fs_alloc_params(struct filesystem *this) {
    if (this->params)
        return 0;
    this->params = calloc(1, sizeof(*this->params));
    if (this->params == NULL) {
        LOGE("Cannot get memory space, request size is %d\n",
            sizeof(*this->params));
        return -1;
    }
    return 0;
}

int fs_free_params(struct filesystem *this) {
    if (this->params) {
        free(this->params);
        this->params = NULL;
    }
    return 0;
}

int fs_register(struct list_head *head, struct filesystem* this) {
    struct filesystem *m;
    struct list_head *cell;

    if (head == NULL || this == NULL) {
        LOGE("list head or filesystem to be registered is null\n");
        return -1;
    }

    if (this->init(this) < 0) {
        LOGE("Filesystem \'%s\' init failed\n", this->name);
        return -1;
    }

    list_for_each(cell, head) {
        m = list_entry(cell, struct filesystem, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGW("Filesystem \'%s\' is already registered", m->name);
            return 0;
        }
    }
    if (this->alloc_params(this) < 0) {
        LOGE("Filesystem \'%s\' alloc parameter failed\n", this->name);
        return -1;
    }
    list_add_tail(&this->list_cell, head);
    return 0;
}

int fs_unregister(struct list_head *head, struct filesystem* this) {
    struct filesystem *m;
    struct list_head *cell;
    struct list_head* next;
    if (head == NULL || this == NULL) {
        LOGE("list head or filesystem to be unregistered is null\n");
        return -1;
    }
    list_for_each_safe(cell, next, head) {
        m = list_entry(cell, struct filesystem, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGI("Filesystem \''%s\' is removed successfully", m->name);
            this->free_params(this);
            list_del(cell);
            return 0;
        }
    }
    return -1;
}

struct filesystem* fs_get_registered_by_name(struct list_head *head, char *filetype) {
    struct filesystem *m;
    struct list_head *cell;
    list_for_each(cell, head) {
        m = list_entry(cell, struct filesystem, list_cell);
        if (!strcmp(m->name,  filetype)) {
            return m;
        }
    }
    return NULL;
}

struct filesystem* fs_get_suppoted_by_name(char *filetype) {
    int i;
    for (i = 0; i < sizeof(fs_supported_list) / sizeof(fs_supported_list[0]); i++) {
        if (!strcmp(fs_supported_list[i]->name,  filetype)) {
            return fs_supported_list[i];
        }
    }
    return NULL;
}
