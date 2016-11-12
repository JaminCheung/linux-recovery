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
#include <stdarg.h>

#include <utils/log.h>
#include <utils/common.h>
#include <utils/assert.h>
#include <graphics/gr_drawer.h>
#include <graphics/font_10x18.h>
#include <fb/fb_manager.h>

#define LOG_TAG "gr_drawer"

struct gr_font {
    uint32_t cwidth;
    uint32_t cheight;
    struct gr_surface* texture;
};

#define kMaxCols   96
#define kMaxRows   96

static struct gr_font* gr_font;
static struct fb_manager* fb_manager;
static uint32_t fb_width;
static uint32_t fb_height;
static uint32_t fb_bits_per_pixel;
static uint32_t fb_bytes_per_pixel;
static uint32_t fb_row_bytes;

static uint8_t gr_current_r = 255;
static uint8_t gr_current_g = 255;
static uint8_t gr_current_b = 255;
static uint8_t gr_current_a = 255;

static int text_rows;
static int text_cols;
static int text_top;
static int text_row;
static int text_col;
static char text[kMaxRows][kMaxCols];

static int outside(uint32_t pos_x, uint32_t pos_y) {
    if ((pos_y >= fb_height) || (pos_x >= fb_width))
        return 1;

    return 0;
}

static uint32_t  make_pixel(uint8_t red, uint8_t green, uint8_t blue,
        uint8_t alpha) {
    uint32_t redbit_len = fb_manager->get_redbit_length(fb_manager);
    uint32_t redbit_off = fb_manager->get_redbit_offset(fb_manager);

    uint32_t greenbit_len = fb_manager->get_greenbit_length(fb_manager);
    uint32_t greenbit_off = fb_manager->get_greenbit_offset(fb_manager);

    uint32_t bluebit_len = fb_manager->get_bluebit_length(fb_manager);
    uint32_t bluebit_off = fb_manager->get_bluebit_offset(fb_manager);

    uint32_t alphabit_len = fb_manager->get_alphabit_length(fb_manager);
    uint32_t alphabit_off = fb_manager->get_alphabit_offset(fb_manager);

    uint32_t pixel = (uint32_t)(((red >> (8 - redbit_len)) << redbit_off)
            | ((green >> (8 - greenbit_len)) << greenbit_off)
            | ((blue >> (8 - bluebit_len)) << bluebit_off)
            | ((alpha >> (8 - alphabit_len)) << alphabit_off));

    return pixel;
}

static int draw_png(struct gr_drawer* this, struct gr_surface* surface,
            uint32_t pos_x, uint32_t pos_y) {
    if (outside(pos_x, pos_y)) {
        LOGE("Position out bound of screen\n");
        return -1;
    }

    uint8_t *buf = (uint8_t *) fb_manager->fbmem;

    for (int i = 0; i < surface->height; i++) {
        if ((i >= fb_height) || (i + pos_y > fb_height))
            break;

        for (int j = 0; j < surface->width; j++) {
            if ((j >= fb_width) || (j + pos_x > fb_width))
                break;

            uint8_t red = surface->raw_data[4 * (j + surface->width * i)];
            uint8_t green = surface->raw_data[4 * (j + surface->width * i) + 1];
            uint8_t blue = surface->raw_data[4 * (j + surface->width * i) + 2];
            uint8_t alpha = surface->raw_data[4 * (j + surface->width * i) + 3];

            uint32_t pos = (i + pos_y ) * fb_width + j + pos_x;
            uint32_t pixel = make_pixel(red, green, blue, alpha);

            for (int x = 0; x < fb_bytes_per_pixel; x++)
                buf[fb_bytes_per_pixel * pos + x] = pixel >> (fb_bits_per_pixel -
                        (fb_bytes_per_pixel -x) * 8);

        }
    }

    fb_manager->display(fb_manager);

    return 0;
}

static int blank(struct gr_drawer* this, uint8_t blank) {
    return fb_manager->blank(fb_manager, blank);
}

static void set_pen_color(struct gr_drawer* this, uint8_t red,
        uint8_t green, uint8_t blue, uint8_t alpha) {

    gr_current_r = red;
    gr_current_g = green;
    gr_current_b = blue;
    gr_current_a = alpha;
}

static void fill_screen(struct gr_drawer* this) {
    uint8_t *buf = (uint8_t *) fb_manager->fbmem;

    uint32_t pixel = make_pixel(gr_current_r, gr_current_g, gr_current_b, 0);

    for (int i = 0; i < fb_height; i++) {
        for (int j = 0; j < fb_width; j++) {
            uint32_t pos = i * fb_width + j;

            for (int x = 0; x < fb_bytes_per_pixel; x++)
                buf[fb_bytes_per_pixel * pos + x] = pixel >> (fb_bits_per_pixel -
                        (fb_bytes_per_pixel -x) * 8);
        }
    }

    fb_manager->display(fb_manager);
}

static void text_blend(uint8_t* src_p, int src_row_bytes, uint8_t* dst_p,
        int dst_row_bytes, int width, int height) {

    uint32_t pixel = make_pixel(gr_current_r, gr_current_g, gr_current_b, 0);

    for (int i = 0; i < height; i++) {
        uint8_t* sx = src_p;
        uint8_t* px = dst_p;

        for (int j = 0; j < width; j++) {
            uint8_t alpha = *sx++;

            if (alpha == 255)
                for (int x = 0; x < fb_bytes_per_pixel; x++)
                    px[x] = pixel >> (fb_bits_per_pixel
                            - (fb_bytes_per_pixel -x) * 8);

                px += fb_bytes_per_pixel;
        }

        src_p += src_row_bytes;
        dst_p += dst_row_bytes;
    }
}

static int draw_text(struct gr_drawer* this, uint32_t pos_x, uint32_t pos_y,
        const char* text, uint8_t bold) {
    assert_die_if(text == NULL, "text is NULL\n");

    uint8_t *buf = (uint8_t *) fb_manager->fbmem;

    struct gr_font *font = gr_font;
    uint32_t off;

    bold = bold && (font->texture->height != font->cheight);
    while((off = *text++)) {
        off -= 32;
        if (outside(pos_x, pos_y) ||
                outside(pos_x + font->cwidth - 1, pos_y + font->cheight - 1))
            return -1;

        if (off < 96) {

            uint8_t* src_p = font->texture->raw_data + (off * font->cwidth) +
                (bold ? font->cheight * font->texture->row_bytes : 0);

            uint8_t* dst_p = buf + pos_y * fb_row_bytes +
                    pos_x * fb_bytes_per_pixel;

            text_blend(src_p, font->texture->row_bytes, dst_p, fb_row_bytes,
                    font->cwidth, font->cheight);

        }

        pos_x += font->cwidth;
    }

    return 0;
}

static int print_text(struct gr_drawer* this, const char* fmt, ...) {
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
        for (int ty = fb_height - gr_font->cheight, count = 0;
             ty > 2 && count < text_rows;
             ty -= gr_font->cheight, ++count) {

            draw_text(this, 4, ty, text[row], 0);

            --row;

            if (row < 0)
                row = text_rows-1;
        }

        fb_manager->display(fb_manager);
    }

    return 0;
}

static void display(struct gr_drawer* this) {
    return fb_manager->display(fb_manager);
}

static int init(struct gr_drawer* this) {
    fb_manager = _new(struct fb_manager, fb_manager);
    fb_manager->init(fb_manager);

    fb_width = fb_manager->get_screen_width(fb_manager);
    fb_height = fb_manager->get_screen_height(fb_manager);
    fb_bits_per_pixel = fb_manager->get_bits_per_pixel(fb_manager);
    fb_row_bytes = fb_manager->get_row_bytes(fb_manager);
    fb_bytes_per_pixel = fb_bits_per_pixel / 8;

    gr_font = calloc(1, sizeof(struct gr_font));

    gr_font->texture = calloc(1, sizeof(*gr_font->texture));
    gr_font->texture->width = font.width;
    gr_font->texture->height = font.height;
    gr_font->texture->row_bytes = font.width;
    gr_font->texture->pixel_bytes = 1;

    uint8_t* bits = malloc(font.width * font.height);
    gr_font->texture->raw_data = (void *) bits;

    uint8_t data;
    uint8_t* in = font.rundata;
    while ((data = *in++)) {
        memset(bits, (data & 0x80) ? 0xff : 0, data & 0x7f);
        bits += (data & 0x7f);
    }

    gr_font->cwidth = font.cwidth;
    gr_font->cheight = font.cheight;

    text_col = text_row = 0;
    text_rows = fb_height / gr_font->cheight;
    if (text_rows > kMaxRows)
        text_rows = kMaxRows;

    text_top = 1;

    text_cols = fb_width / gr_font->cwidth;
    if (text_cols > kMaxCols - 1)
        text_cols = kMaxCols - 1;

    return 0;
}

static int deinit(struct gr_drawer* this) {
    fb_manager->deinit(fb_manager);
    _delete(fb_manager);

    free(gr_font->texture);
    free(gr_font);

    fb_manager = NULL;
    gr_font = NULL;

    return 0;
}

void construct_gr_drawer(struct gr_drawer* this) {
    this->draw_png = draw_png;
    this->draw_text = draw_text;
    this->print_text = print_text;
    this->blank = blank;
    this->fill_screen = fill_screen;
    this->init = init;
    this->deinit = deinit;
    this->set_pen_color = set_pen_color;
    this->display = display;
}

void destruct_gr_drawer(struct gr_drawer* this) {
    this->draw_png = NULL;
    this->draw_png = NULL;
    this->print_text = NULL;
    this->blank = NULL;
    this->fill_screen = NULL;
    this->init = NULL;
    this->deinit = NULL;
    this->set_pen_color = NULL;
    this->display = NULL;
}
