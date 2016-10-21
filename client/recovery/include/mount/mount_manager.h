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

#ifndef MOUNT_MANAGER_H
#define MOUNT_MANAGER_H

#include <utils/list.h>

struct mounted_volume {
    char device[64];
    char mount_point[64];
    char filesystem[64];
    char flags[128];
    struct list_head head;
};

struct mount_manager {
    void (*construct)(struct mount_manager* this);
    void (*destruct)(struct mount_manager* this);
    void (*scan_mounted_volumes)(struct mount_manager* this);
    void (*dump_mounted_volumes)(struct mount_manager* this);
    const struct mounted_volume*
        (*find_mounted_volume_by_device)(struct mount_manager* this, const char* device);
    const struct mounted_volume*
        (*find_mounted_volume_by_mount_point)(struct mount_manager* this, const char* mount_point);
    int (*umount_volume)(struct mount_manager* this,
            struct mounted_volume* volume);
    int (*mount_volume)(struct mount_manager* this, const char* device,
            const char* mount_point, const char* filesystem);
    struct list_head list;
};

void construct_mount_manager(struct mount_manager* this);
void destruct_mount_manager(struct mount_manager* this);

#endif /* MOUNT_MANAGE_H */
