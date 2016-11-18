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

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <utils/list.h>
#include <configure/configure_file.h>
#include <configure/update_file.h>
#include <mount/mount_manager.h>
#include <block/block_manager.h>
#include <netlink/netlink_handler.h>
#include <net/net_interface.h>
#include <graphics/gui.h>

enum update_wbuffer_alloc_size_method{
    UPDATE_WBUFFER_ALLOWABLE_MINIMUM_SIZE,
    UPDATE_WBUFFER_FIXED_WITH_CHUCK_SIZE,
};

struct storage_dev {
    char name[64];
    char dev_name[64];
    char mount_point[128];
    struct list_head head;
};

struct ota_manager {
    void (*construct)(struct ota_manager* this);
    void (*destruct)(struct ota_manager* this);
    int (*start)(struct ota_manager* this);
    int (*stop)(struct ota_manager* this);
    void (*load_configure)(struct ota_manager* this, struct configure_file* cf);
    struct netlink_handler* nh;
    struct block_manager* mtd_bm;
    struct mount_manager* mm;
    struct configure_file* cf;
    struct update_file* uf;
    struct net_interface* ni;
    struct gui* gui;
    struct list_head storage_dev_list;
};

void construct_ota_manager(struct ota_manager* this);
void destruct_ota_manager(struct ota_manager* this);

#endif /* OTA_MANAGER_H */
