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

#ifndef GUI_H
#define GUI_H

#include <types.h>
#include <graphics/gr_drawer.h>

struct gui {
    void (*construct)(struct gui* this);
    void (*destruct)(struct gui* this);

    int (*init)(struct gui* this);
    int (*deinit)(struct gui* this);

    int (*show_log)(struct gui* this, const char* fmt, ...);
    int (*show_progress)(struct gui* this, uint8_t progress);
    int (*show_image)(struct gui* this, const char* path);
};

void construct_gui(struct gui* this);
void destruct_gui(struct gui* this);

#endif /* GUI_H */
