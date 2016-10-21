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
#define BM_BLOCK_TYPE_MTD_NAND  BM_BLOCK_TYPE_MTD"_nand"
#define BM_BLOCK_TYPE_MTD_NOR  BM_BLOCK_TYPE_MTD"_nor"
#define BM_BLOCK_TYPE_MMC  "mmc"

#define BM_BLOCK_TYPE_INIT(name)      \
    char *name[] = {\
        BM_BLOCK_TYPE_MTD,  \
        BM_BLOCK_TYPE_MMC,\
    }

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
        BM_FILE_TYPE_YAFFS2,\
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

#define BM_OPERATE_METHOD_INIT(name)      \
    char *name[] = {\
        BLOCK_OPERATION_METHOD_PARTITION,  \
        BLOCK_OPERATION_METHOD_RANDOM,\
    }

struct bm_operate_prepare_info {
    long long write_start;
    unsigned long physical_unit_size;
    unsigned long logical_unit_size;
    long long max_size_mapped_in_partition;
};

struct bm_operation_option {
    int method;     /* one in block_operation_method*/
    char* filetype;     /* one in BM_FILE_TYPE_INIT*/
};

struct bm_event {
    char *mtdchar;
    int operation;
    int progress;
};
typedef void (*bm_event_listener_t)(void* param, struct bm_event* event);

union bm_dev_info {
    struct mtd_dev_info mtd_dev_info;
};
struct bm_part_info {
    union bm_dev_info part;
    long long start;
    int fd;
    char *path;
};

struct block_manager {
    void (*construct)(struct block_manager* this, char *blockname,
                      bm_event_listener_t listener, void* param);
    void (*destruct)(struct block_manager* this);
    void (*get_supported)(struct block_manager* this, char *buf);
    void (*get_supported_filetype)(struct block_manager* this, char *buf);
    struct bm_operation_option* (*set_operation_option) (struct block_manager* this,
            int method, char *filetype);
    int (*erase)(struct block_manager* this, long long offset, long long length,
                 struct bm_operation_option *option);
    int (*write)(struct block_manager* this, long long offset, char* buf, long long length,
                 struct bm_operation_option *option);
    int (*read)(struct block_manager* this, long long offset, char* buf,
                long long length, struct bm_operation_option *option);
    struct bm_operate_prepare_info* (*prepare)(
                struct block_manager* this,
                long long offset,
                long long length,
                struct bm_operation_option *option);
    unsigned long (*get_prepare_leb_size)(struct block_manager* this);
    unsigned long (*get_max_size_mapped_in)(struct block_manager* this);
    int (*finish)(struct block_manager* this);
    long long (*get_partition_size_by_offset)(struct block_manager* this,
            long long offset);
    long long (*get_partition_size_by_node)(struct block_manager* this,
                                            char *mtdchar);
    long long (*get_capacity)(struct block_manager* this);
    int (*get_blocksize)(struct block_manager* this, long long offset);

    struct mtd_dev_info* (*get_mtd_dev_info_by_name)(struct block_manager* this,
            const char* name);
    // struct mtd_info* (*get_mtd_info)(struct block_manager* this);
    // struct mtd_dev_info* (*get_mtd_dev_info_by_name)(struct block_manager* this, const char* mtdchar);
    struct bm_operation_option operate_option;
    struct bm_operate_prepare_info *prepared;

    libmtd_t mtd_desc;
    struct mtd_info mtd_info;
    // struct mtd_dev_info* mtd_dev_info;
    struct bm_part_info *part_info;

    bm_event_listener_t event_listener;

    char *name;
    struct list_head  list_cell;
    struct list_head  list_head;
    struct list_head  list_fs_head;
    void* param;
};

void construct_block_manager(struct block_manager* this, char *blockname,
                             bm_event_listener_t listener, void* param);
void destruct_block_manager(struct block_manager* this);

int register_block_manager(struct block_manager *this);
int unregister_block_manager(struct block_manager* this);

#endif /* block_manager_H */
