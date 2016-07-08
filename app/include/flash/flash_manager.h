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

#ifndef FLASH_MANAGER_H
#define FLASH_MANAGER_H

#include <lib/libmtd.h>


#define ACTION_ERASE    0
#define ACTION_WRITE    1
#define ACTION_READ     2

struct flash_event {
    int type;
    int progress;
    char *name;
};

typedef void (*flash_event_listener_t)(void* param, struct flash_event* event);

struct flash_manager {
    void (*construct)(struct flash_manager* this,
            flash_event_listener_t listener, void* param);
    void (*destruct)(struct flash_manager* this);
    int (*init_libmtd)(struct flash_manager* this);
    int (*close_libmtd)(struct flash_manager* this);
    int (*partition_write)(struct flash_manager* this,
            const char* part_name, const char* image_name);
    int (*partition_read)(struct flash_manager* this, const char* part_name,
            const char* image_name);
    int (*partition_erase)(struct flash_manager* this, const char* part_name);

    int (*random_write)(struct flash_manager* this, char* buf,
            unsigned int offset, unsigned int count);
    int (*random_read)(struct flash_manager* this, char* buf,
            unsigned int offset, unsigned int count);
    int (*random_erase)(struct flash_manager* this, unsigned int start_blk,
            int count);
    struct mtd_dev_info* (*get_mtd_dev_info_by_name)(struct flash_manager* this,
            const char* name);

    libmtd_t mtd_desc;
    struct mtd_info mtd_info;
    struct mtd_dev_info* mtd_dev_info;

    flash_event_listener_t listener;
    void* param;

};

void construct_flash_manager(struct flash_manager* this,
        flash_event_listener_t listener, void* param);
void destruct_flash_manager(struct flash_manager* this);

#endif /* FLASH_MANSGER_H */
