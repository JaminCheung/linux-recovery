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

#ifndef NET_INTERFACE_H
#define NET_INTERFACE_H

#include <netdb.h>

typedef enum {
    CABLE_STATE_UNPLUGIN = 0,
    CABLE_STATE_PLUGIN,
    CABLE_STATE_ERR,
    CABLE_STATE_INIT
} cable_state_t;

typedef void (*detect_listener_t)(void* param, bool state);

struct net_interface {
    void (*construct)(struct net_interface* this, const char* if_name);
    void (*destruct)(struct net_interface* this);
    int (*icmp_echo)(struct net_interface* this, const char* server_addr, int timeout);
    int (*get_hwaddr)(struct net_interface* this, unsigned char *hwaddr);
    int (*set_hwaddr)(struct net_interface* this, const unsigned char* hwaddr);
    int (*get_addr)(struct net_interface* this, in_addr_t* addr);
    int (*set_addr)(struct net_interface* this, in_addr_t addr);
    int (*init_socket)(struct net_interface* this);
    void (*close_socket)(struct net_interface* this);
    int (*up)(struct net_interface* this);
    int (*down)(struct net_interface* this);
    cable_state_t (*get_cable_state)(struct net_interface* this);
    int (*start_cable_detector)(struct net_interface* this, detect_listener_t listener, void* param);
    void (*stop_cable_detector)(struct net_interface* this);

    int socket;
    int icmp_socket;
    char* if_name;
    detect_listener_t detect_listener;
    void* detect_param;

    cable_state_t cable_status;
    int detect_pid;
    bool detect_stopped;
    bool detect_exit;
    pthread_mutex_t detect_lock;
    pthread_cond_t detect_cond;
};


void construct_net_interface(struct net_interface* this, const char* if_name);
void destruct_net_interface(struct net_interface* this);

#endif /* NET_INTERFACE_H */
