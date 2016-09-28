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

#include <signal.h>

#include <utils/log.h>
#include <utils/signal_handler.h>

#define LOG_TAG "signal_handler"

static void init(struct signal_handler* this, int signal,
        signal_handler_t handler) {
    struct sigaction action;

    sigemptyset(&action.sa_mask);
    action.sa_handler = handler;
    action.sa_flags = 0;

    sigaction(signal, &action, NULL);
}

void construct_signal_handler(struct signal_handler* this) {
    this->init = init;
}

void destruct_signal_handler(struct signal_handler* this) {
    this->init = NULL;
}
