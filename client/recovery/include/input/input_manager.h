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

#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <utils/list.h>
#include <linux/input.h>

struct input_manager;

typedef void (*input_event_listener_t)(struct input_manager* this,
        struct input_event *event);

struct input_manager {
    void (*construct)(struct input_manager* this);
    void (*destruct)(struct input_manager* this);

    int (*init)(struct input_manager* this);
    int (*deinit)(struct input_manager* this);
    int (*start)(struct input_manager* this);
    int (*stop)(struct input_manager* this);
    int (*get_device_count)(struct input_manager* this);
    void (*register_event_listener)(struct input_manager* this,
            input_event_listener_t listener);
    void (*unregister_event_listener)(struct input_manager* this,
            input_event_listener_t listener);
    void (*dump_event)(struct input_manager* this, struct input_event* event);
    const char* (*type2str)(struct input_manager* this, uint32_t event_type);
    const char* (*code2str)(struct input_manager* this, uint32_t event_type,
            uint32_t event_code);

    struct list_head input_dev_list;
    struct list_head listener_list;
    int max_fd;
    int local_pipe[2];
    pthread_mutex_t device_list_lock;
    pthread_mutex_t listener_lock;
};

void construct_input_manager(struct input_manager* this);
void destruct_input_manager(struct input_manager* this);

#endif /* INPUT_MANAGER_H */
