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
#include <stdarg.h>
#include <string.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/file_ops.h>
#include <utils/common.h>
#include <utils/png_decode.h>
#include <graphics/gui.h>

#define LOG_TAG "gui"

#define kMaxCols   96
#define kMaxRows   96

static uint32_t char_width;
static uint32_t char_height;
static int text_rows;
static int text_cols;
static int text_top;
static int text_row;
static int text_col;
static char text[kMaxRows][kMaxCols];

static struct gr_drawer* gr_drawer;

static int show_log(struct gui* this, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';

        int row = (text_top+text_rows-1) % text_rows;
        for (int ty = gr_drawer->get_fb_height(gr_drawer) - char_height, count = 0;
             ty > 2 && count < text_rows;
             ty -= char_height, ++count) {

            gr_drawer->draw_text(gr_drawer, 4, ty, text[row], 0);

            --row;

            if (row < 0)
                row = text_rows-1;
        }

        gr_drawer->display(gr_drawer);
    }

    return 0;
}

static int show_progress(struct gui* this, uint8_t progress) {
    return -1;
}

static int show_image(struct gui* this, const char* path) {
    assert_die_if(path == NULL, "path is NULL\n");

    struct gr_surface* surface = NULL;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t pos_x;
    uint32_t pos_y;

    if (file_exist(path) < 0) {
        LOGE("File not exist: %s\n", strerror(errno));
        return -1;
    }

    if (png_decode_image(path, &surface) < 0) {
        LOGE("Failed to decode image: %s\n", path);
        return -1;
    }

    image_width = surface->width;
    image_height = surface->height;
    pos_x = (gr_drawer->get_fb_width(gr_drawer) - image_width) / 2;
    pos_y = (gr_drawer->get_fb_height(gr_drawer) - image_height) / 2;

    return gr_drawer->draw_png(gr_drawer, surface, pos_x, pos_y);
}

static int init(struct gui* this) {
    gr_drawer = _new(struct gr_drawer, gr_drawer);
    gr_drawer->init(gr_drawer);

    gr_drawer->get_font_size(&char_width, &char_height);

    text_col = text_row = 0;
    text_rows = gr_drawer->get_fb_height(gr_drawer) / char_height;
    if (text_rows > kMaxRows)
        text_rows = kMaxRows;

    text_top = 1;

    text_cols = gr_drawer->get_fb_width(gr_drawer) / char_width;
    if (text_cols > kMaxCols - 1)
        text_cols = kMaxCols - 1;

    return 0;
}

static int deinit(struct gui* this) {
    gr_drawer->deinit(gr_drawer);
    _delete(gr_drawer);

    gr_drawer = NULL;

    return 0;
}

void construct_gui(struct gui* this) {
    this->init = init;
    this->deinit = deinit;
    this->show_image = show_image;
    this->show_progress = show_progress;
    this->show_log = show_log;
}

void destruct_gui(struct gui* this) {
    this->init = NULL;
    this->deinit = NULL;
    this->show_image = NULL;
    this->show_log = NULL;
    this->show_progress = NULL;
}
