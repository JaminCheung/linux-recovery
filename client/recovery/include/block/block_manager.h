/*
 *  Copyright (C) 2016, Zhang YanMing <jamincheung@126.com>
 *
 *  Linux recovery updater
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef  BLOCK_MANAGER_H
#define BLOCK_MANAGER_H

#include <inttypes.h>
#include <lib/libmtd.h>
#include <types.h>

#define BM_BLOCK_TYPE_MTD   "mtd"
#define BM_BLOCK_TYPE_MMC  "mmc"

#define BM_FILE_TYPE_NORMAL  "normal"
#define BM_FILE_TYPE_CRAMFS   "cramfs"
#define BM_FILE_TYPE_JFFS2       "jffs2"
#define BM_FILE_TYPE_UBIFS      "ubifs"
#define BM_FILE_TYPE_YAFFS2    "yaffs2"

#define BM_FILE_TYPE_INIT(name)      \
    char *name[] = {\
        BM_FILE_TYPE_NORMAL,  \
        BM_FILE_TYPE_CRAMFS,\
        BM_FILE_TYPE_JFFS2, \
        BM_FILE_TYPE_UBIFS,\
        BM_FILE_TYPE_YAFFS2\
    }

enum block_operation {
    BLOCK_OPERATION_ERASE,
    BLOCK_OPERATION_WRITE,
    BLOCK_OPERATION_READ,
};

enum block_operation_method {
    BLOCK_OPERATION_METHOD_PARTITION,
    BLOCK_OPERATION_METHOD_RANDOM,
};

struct bm_event {
    char *mtdchar;
    int operation;
    int progress;
};

typedef void (*bm_event_listener_t)(void* param, struct bm_event* event);

struct bm_write_prepare_info {
    unsigned long write_offset;
};

struct bm_operation_option {
    int method;     /* one in block_operation_method*/
    int filetype;     /* one in BM_FILE_TYPE_INIT*/
};

struct block_manager {
    void (*construct)(struct block_manager* this, char *blockname,
                             bm_event_listener_t listener, void* param);
    void (*destruct)(struct block_manager* this);

    void (*get_supported)(struct block_manager* this, char *buf);
    void (*get_supported_filetype)(struct block_manager* this, char *buf);

    int (*erase)(struct block_manager* this, unsigned long offset, unsigned long length,
            struct bm_operation_option *option);

    int (*write)(struct block_manager* this, unsigned long offset, char* buf, unsigned long length,
        struct bm_operation_option *option);

    int (*read)(struct block_manager* this, unsigned long offset, char* buf,
                unsigned long length, struct bm_operation_option *option);

    unsigned long (*get_max_allowable_filesize_in_partition)(struct block_manager* this,  unsigned long offset,
                char *imgtype);
    int (*write_prepare)(struct block_manager* this, unsigned long offset,  char *imgtype,
                struct bm_write_prepare_info *info);
    int (*write_finish)(struct block_manager* this);

    struct mtd_dev_info* (*get_mtd_dev_info_by_name)(struct block_manager* this,
            const char* name);
    // struct mtd_info* (*get_mtd_info)(struct block_manager* this);
    // struct mtd_dev_info* (*get_mtd_dev_info_by_name)(struct block_manager* this, const char* mtdchar);
    struct bm_write_prepare_info *prepared;

    libmtd_t mtd_desc;
    struct mtd_info mtd_info;
    struct mtd_dev_info* mtd_dev_info;

    bm_event_listener_t event_listener;

    char *name;
    struct list_head  list_cell;
    struct list_head  list_fs_head;
    void* param;
};

extern int mtd_manager_init(void);
extern int mtd_manager_destroy(void);
extern int mmc_manager_init(void);
void construct_block_manager(struct block_manager* this, char *blockname,
                             bm_event_listener_t listener, void* param);
void destruct_block_manager(struct block_manager* this);

int register_block_manager(struct block_manager *this);
int unregister_block_manager(struct block_manager* this);

#endif /* block_manager_H */
