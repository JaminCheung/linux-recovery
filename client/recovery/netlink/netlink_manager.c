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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/netlink.h>

#include <utils/log.h>
#include <netlink/netlink_handler.h>
#include <netlink/netlink_manager.h>
#include <netlink/netlink_listener.h>

#define LOG_TAG "netlink_manager"

static int start(struct netlink_manager *this) {
    int retval = 0;

    retval = this->listener->start_listener(this->listener);
    if (retval) {
        LOGE("Unable to start netlink_listener: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static int stop(struct netlink_manager *this) {
    if (!this->listener->stop_listener(this->listener)) {
        LOGE("Unable to stop netlink_listener: %s", strerror(errno));
        return -1;
    }

    destruct_netlink_listener(this->listener);
    this->listener = NULL;

    close(this->socket);
    this->socket = -1;

    return 0;
}

static void register_handler(struct netlink_manager* this,
        struct netlink_handler *handler) {
    this->listener->register_handler(this->listener, handler);
}

static void unregister_handler(struct netlink_manager* this,
        struct netlink_handler *handler) {
    this->listener->unregister_handler(this->listener, handler);
}

void construct_netlink_manager(struct netlink_manager *this) {
    this->start = start;
    this->stop = stop;
    this->register_handler = register_handler;
    this->unregister_handler = unregister_handler;

    struct sockaddr_nl nladdr;
    int size = 64 * 1024;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = (pthread_self() << 16) | getpid();
    nladdr.nl_groups = 0xffffffff;

    this->socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (this->socket < 0) {
        LOGE("Unable to create uevent socket: %s", strerror(errno));
        return;
    }

    if (setsockopt(this->socket, SOL_SOCKET, SO_RCVBUFFORCE, &size,
            sizeof(size)) < 0) {
        LOGE("Unable to set uevent socket options: %s", strerror(errno));
        return;
    }

    if (bind(this->socket, (struct sockaddr *) &nladdr, sizeof(nladdr)) < 0) {
        LOGE("Unable to bind uevent socket: %s", strerror(errno));
        return;
    }

    this->listener = (struct netlink_listener *) malloc(
            sizeof(struct netlink_listener));
    this->listener->construct = construct_netlink_listener;
    this->listener->destruct = destruct_netlink_listener;

    this->listener->construct(this->listener, this->socket,
            NETLINK_FORMAT_ASCII);
}

void destruct_netlink_manager(struct netlink_manager* this) {
    this->stop(this);

    this->start = NULL;
    this->stop = NULL;
    this->register_handler = NULL;
    this->unregister_handler = NULL;
}
