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

#ifndef NETLINK_MANAGER_H
#define NETLINK_MANAGER_H

#include <netlink/netlink_handler.h>

struct netlink_manager {
    void (*construct)(struct netlink_manager *this);
    void (*destruct)(struct netlink_manager *this);
    int socket;
    struct netlink_listener *listener;
    int (*start)(struct netlink_manager *this);
    int (*stop)(struct netlink_manager *this);
    void (*register_handler)(struct netlink_manager* this,
            struct netlink_handler* handler);
    void (*unregister_handler)(struct netlink_manager* this,
            struct netlink_handler* handler);
};

void construct_netlink_manager(struct netlink_manager *this);
void destruct_netlink_manager(struct netlink_manager* this);

#endif /* NETLINK_MANAGER_H */
