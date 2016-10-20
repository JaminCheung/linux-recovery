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

#ifndef UPDATE_FILE_H
#define UPDATE_FILE_H

#include <limits.h>
#include <utils/list.h>

#define UPDATE_MODE_FULL    0x200
#define UPDATE_MODE_CHUNK   0x201

struct image_info {
    char name[NAME_MAX];
    char fs_type[NAME_MAX];
    uint64_t offset;
    uint64_t size;
    uint32_t update_mode;
    uint32_t chunksize;
    uint32_t chunkcount;
    struct list_head head;
};

struct update_info {
    uint32_t devctl;
    uint32_t image_count;
    struct list_head list;
};

struct part_info {
    char name[NAME_MAX];
    uint64_t offset;
    uint64_t size;
    char block_name[NAME_MAX];
    struct list_head head;
};

struct device_info {
    char type[NAME_MAX];
    uint64_t capacity;
    uint32_t part_count;
    struct list_head list;
};

struct update_file {
    void (*construct)(struct update_file* this);
    void (*destruct)(struct update_file* this);
    int (*parse_device_xml)(struct update_file* this, const char* path);
    int (*parse_update_xml)(struct update_file* this, const char* path);
    void (*dump_update_xml)(struct update_file* this);
    void (*dump_device_xml)(struct update_file* this);
    struct update_info update_info;
    struct device_info device_info;
};

void construct_update_file(struct update_file* this);
void destruct_update_file (struct update_file* this);

#endif /* UPDATE_FILE_H */
