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

#ifndef FB_MANAGER_H
#define FB_MANAGER_H

#include <types.h>
#include <linux/fb.h>

struct fb_manager {
    void (*construct)(struct fb_manager* this);
    void (*destruct)(struct fb_manager* this);

    int (*init)(struct fb_manager* this);
    int (*deinit)(struct fb_manager* this);

    void (*dump)(struct fb_manager* this);

    void (*display)(struct fb_manager* this);

    uint32_t (*get_screen_size)(struct fb_manager* this);
    uint32_t (*get_screen_width)(struct fb_manager* this);
    uint32_t (*get_screen_height)(struct fb_manager* this);

    uint8_t* fbmem;
};

void construct_fb_manager(struct fb_manager* this);
void destruct_fb_manager(struct fb_manager* this);

#endif /* FB_MANAGER_H */
