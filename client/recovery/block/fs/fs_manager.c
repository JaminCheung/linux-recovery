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
#include <block/fs/fs_manager.h>

#define LOG_TAG "filesystem"

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

int fs_init(struct filesystem *this) {
    if (this->params) {
        LOGW("Parameter of filesystem \"%s\" is already  allocated\n", 
            this->name);
        return true;
    }
    this->params = malloc(sizeof(*this->params));
    if (this->params == NULL) {
        LOGE("Cannot get memory space, request size is %d\n", 
            sizeof(*this->params));
        return false;
    }
    return true;
}

int fs_register(struct list_head *head, struct filesystem* this) {
    struct filesystem *m;
    struct list_head *cell;

    if (head || this) {
        LOGE("list head or filesystem to be registered is null\n");
        return false;
    }
    list_for_each(cell, head) {
        m = list_entry(cell, struct filesystem, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGE("Filesystem \''%s\' is already registered", m->name);
            return false;
        }
    }
    list_add_tail(head, &this->list_cell);
    return true;
}

int fs_unregister(struct list_head *head, struct filesystem* this) {
    struct filesystem *m;
    struct list_head *cell;

    if (head || this) {
        LOGE("list head or filesystem to be unregistered is null\n");
        return false;
    }
    list_for_each(cell, head) {
        m = list_entry(cell, struct filesystem, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGI("Filesystem \''%s\' is removed successfully", m->name);
            list_del(cell);
            return true;
        }
    }
    return false;
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

// void fs_set_content_boundary(struct filesystem *this, long long max_mapped_size, 
//                     long long content_start) {
//     this->params->max_mapped_size = max_mapped_size;
//     this->params->content_start = content_start;
// }