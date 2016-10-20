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

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/linux.h>
#include <netlink/netlink_handler.h>
#include <netlink/netlink_event.h>
#include <ota/ota_manager.h>

#define LOG_TAG "ota_manager"

static void handle_net_event(struct netlink_handler* nh,
        struct netlink_event *event) {

}

static void handle_block_event(struct netlink_handler* nh,
        struct netlink_event* event) {

}

static void handle_event(struct netlink_handler* nh,
        struct netlink_event* event) {
    event->dump(event);

    if (!strcmp(event->get_subsystem(event), "block")) {
        handle_block_event(nh, event);

    } else if (!strcmp(event->get_subsystem(event), "net")) {
        handle_net_event(nh, event);
    }
}

static struct netlink_handler* get_hotplug_handler(
        struct ota_manager* this) {
    return this->nh;
}

static int start(struct ota_manager* this) {
    LOGI("Start ota manager.\n");
    return 0;
}

static int stop(struct ota_manager* this) {
    LOGI("Stop ota manager.\n");
    return 0;
}

void construct_ota_manager(struct ota_manager* this) {
    this->start = start;
    this->stop = stop;

    /*
     * Instance netlink handler
     */
    this->nh = (struct netlink_handler *) calloc(1,
            sizeof(struct netlink_handler));
    this->nh->construct = construct_netlink_handler;
    this->nh->deconstruct = destruct_netlink_handler;
    this->nh->construct(this->nh, "all sub-system", 0, handle_event, this);
    this->get_hotplug_handler = get_hotplug_handler;
}

void destruct_ota_manager(struct ota_manager* this) {
    this->start = NULL;
    this->stop = NULL;

    /*
     * Destruct netlink_handler
     */
    this->nh->deconstruct(this->nh);
    this->nh = NULL;
}
