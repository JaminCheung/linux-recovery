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
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <netlink/netlink_listener.h>
#include <netlink/netlink_handler.h>

#define LOG_TAG "recovery--->netlink_listener"

static void *thread_loop(void *param);

static int start_listener(struct netlink_listener *this) {
    int retval = 0;

    if (pipe(this->pipe)) {
        LOGE("Unable to open pipe: %s", strerror(errno));
        return -1;
    }

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    retval = pthread_create(&tid, &attr, thread_loop, (void *) this);
    if (retval) {
        LOGE("pthread_create failed: %s", strerror(errno));
        pthread_attr_destroy(&attr);
        return -1;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

static int stop_listener(struct netlink_listener *this) {
    char c = 0;
    if (!write(this->pipe[1], &c, 1)) {
        LOGE("Unable to write pipe: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void register_handler(struct netlink_listener* this,
        struct netlink_handler* handler) {

    while ((this->head) != NULL) {
        if (handler->get_priority(handler) > this->head->get_priority(handler))
            break;
        this->head = (this->head->next);
    }
    handler->next = this->head;
    this->head = handler;
}

static void unregister_handler(struct netlink_listener* this,
        struct netlink_handler* handler) {
    while (this->head != NULL) {
        if (this->head == handler) {
            this->head = handler->next;
            return;
        }
        this->head = (this->head->next);
    }
}

static void dispatch_event(struct netlink_listener* this,
        struct netlink_event *event) {
    struct netlink_handler *nh, *next_nh;

    nh = this->head;

    while (nh) {
        next_nh = nh->next;

        nh->handler_event(nh, event);

        nh = next_nh;
    }
}

static void *thread_loop(void *param) {
    struct netlink_listener *this = (struct netlink_listener *) param;
    struct pollfd fds[2];

    fds[0].fd = this->socket;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = this->pipe[0];
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    int count;
    int retval;

    for (;;) {

        restart: do {
            count = poll(fds, 2, -1);
        } while (count < 0 && errno == EINTR);

        if (fds[0].revents & POLLIN) {
            count = recv(this->socket, this->buffer, sizeof(this->buffer), 0);
            if (count < 0) {
                LOGE("netlink event recv failed: %s", strerror(errno));
                goto restart;
            }

            struct netlink_event* event = (struct netlink_event *) malloc(
                    sizeof(struct netlink_event));

            event->construct = construct_netlink_event;
            event->destruct = destruct_netlink_event;
            event->construct(event);

            if (event->decode(event, this->buffer, count, this->format) < 0) {
                LOGE("Error decoding netlink_event");
                event->destruct(event);
                free(event);
                goto restart;
            }

            dispatch_event(this, event);

            event->destruct(event);
            free(event);
        }

        if (fds[1].revents & POLLIN) {
            fds[1].revents = 0;
            char c;
            retval = read(fds[1].fd, &c, 1);
            if (!retval) {
                LOGE("Unable to read pipe: %s", strerror(errno));
                continue;
            }

            LOGI("main thread call me break out.");
            break;
        }
    }

    return NULL;
}

void construct_netlink_listener(struct netlink_listener *this, int socket,
        int format) {
    this->start_listener = start_listener;
    this->stop_listener = stop_listener;
    this->register_handler = register_handler;
    this->unregister_handler = unregister_handler;
    this->socket = socket;
    this->format = format;
}

void destruct_netlink_listener(struct netlink_listener *this) {
    if (this->pipe[0] > 0)
        close(this->pipe[0]);

    if (this->pipe[1] > 0)
        close(this->pipe[1]);

    this->start_listener = NULL;
    this->stop_listener = NULL;
    this->register_handler = NULL;
    this->unregister_handler = NULL;
    this->socket = -1;
    this->format = -1;
}
