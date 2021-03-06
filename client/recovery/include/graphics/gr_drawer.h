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

#ifndef GR_DRAWER_H
#define GR_DRAWER_H

#include <types.h>

struct gr_surface {
    uint32_t width;
    uint32_t height;
    uint32_t row_bytes;
    uint32_t pixel_bytes;
    uint8_t *raw_data;
};

struct gr_drawer {
    void (*construct)(struct gr_drawer* this);
    void (*destruct)(struct gr_drawer* this);

    int (*init)(struct gr_drawer* this);
    int (*deinit)(struct gr_drawer* this);

    uint32_t (*get_fb_width)(struct gr_drawer* this);
    uint32_t (*get_fb_height)(struct gr_drawer* this);

    void (*get_font_size)(uint32_t *width, uint32_t* height);

    void (*set_pen_color)(struct gr_drawer* this, uint8_t red, uint8_t green,
            uint8_t blue);

    int (*draw_png)(struct gr_drawer* this, struct gr_surface* surface,
            uint32_t pos_x, uint32_t pos_y);
    int (*draw_text)(struct gr_drawer* this, uint32_t pos_x, uint32_t pos_y,
            const char* text, uint8_t bold);

    void (*display)(struct gr_drawer* this);

    int (*blank)(struct gr_drawer* this, uint8_t blank);
    void (*fill_screen)(struct gr_drawer* this);
    int (*fill_rect)(struct gr_drawer* this, uint32_t x1, uint32_t y1,
            uint32_t x2, uint32_t y2);
};

void construct_gr_drawer(struct gr_drawer* this);
void destruct_gr_drawer(struct gr_drawer* this);

#endif /* GR_DRAWER_H */
