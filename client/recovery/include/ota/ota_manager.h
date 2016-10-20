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

struct ota_manager {
    void (*construct)(struct ota_manager* this);
    void (*destruct)(struct ota_manager* this);
    int (*start)(struct ota_manager* this);
    int (*stop)(struct ota_manager* this);
    struct netlink_handler* (*get_hotplug_handler)(struct ota_manager* this);
    struct netlink_handler* nh;
};

void construct_ota_manager(struct ota_manager* this);
void destruct_ota_manager(struct ota_manager* this);

#endif /* OTA_MANAGER_H */
