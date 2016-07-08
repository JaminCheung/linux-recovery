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

#ifndef NETLINK_LISTENER_H
#define NETLINK_LISTENER_H

#include <netlink/netlink_event.h>
#include <netlink/netlink_handler.h>

#define NETLINK_FORMAT_ASCII 0
#define NETLINK_FORMAT_BINARY 1

struct netlink_listener {
    void (*construct)(struct netlink_listener *this, int socket, int format);
    void (*destruct)(struct netlink_listener *this);
    int (*start_listener)(struct netlink_listener *this);
    int (*stop_listener)(struct netlink_listener *this);
    void (*register_handler)(struct netlink_listener* this,
            struct netlink_handler *handler);
    void (*unregister_handler)(struct netlink_listener* this,
            struct netlink_handler *handler);
    void (*dispatch_event)(struct netlink_listener *this,
            struct netlink_event *event);

    int socket;
    int format;
    char buffer[64 * 1024];
    int pipe[2];
    struct netlink_handler *head;
};

void construct_netlink_listener(struct netlink_listener *this, int socket,
        int format);
void destruct_netlink_listener(struct netlink_listener* this);

#endif /* NETLINK_LISTENER_H */
