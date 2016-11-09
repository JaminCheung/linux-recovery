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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <linux/kd.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <fb/fb_manager.h>

#define LOG_TAG "fb_manager"

const static char* prefix_fb_dev = "/dev/fb0";
const static char* prefix_fb_bak_dev = "/dev/graphics/fd0";
const static char* prefix_vt_dev = "/dev/tty0";

static int fd;
static int vt_fd;
static uint8_t fb_count;
static uint8_t fb_index;
static uint8_t *fbmem;
static uint32_t screen_size;

static struct fb_fix_screeninfo fb_fixinfo;
static struct fb_var_screeninfo fb_varinfo;

static void dump(struct fb_manager* this) {
    LOGI("==========================\n");
    LOGI("Dump fb info\n");
    LOGI("Screen count:  %u\n", fb_count);
    LOGI("Screen size:   %u\n", screen_size);
    LOGI("Width:         %u\n", fb_varinfo.xres);
    LOGI("Length:        %u\n", fb_varinfo.yres);
    LOGI("BPP:           %u\n", fb_varinfo.bits_per_pixel);
    LOGI("Row bytes:     %u\n", fb_fixinfo.line_length);
    LOGI("Red offset:    %u\n", fb_varinfo.red.offset);
    LOGI("Red length:    %u\n", fb_varinfo.red.length);
    LOGI("Green offset:  %u\n", fb_varinfo.green.offset);
    LOGI("Green length:  %u\n", fb_varinfo.green.length);
    LOGI("Blue offset:   %u\n", fb_varinfo.blue.offset);
    LOGI("Blue length:   %u\n", fb_varinfo.blue.length);
    LOGI("Alpha offset:  %u\n", fb_varinfo.transp.offset);
    LOGI("Alpha length:  %u\n", fb_varinfo.transp.length);
    LOGI("==========================\n");
}

static int init(struct fb_manager* this) {
    fd = open(prefix_fb_dev, O_RDWR | O_SYNC);
    if (fd < 0) {
        LOGW("Failed to open %s, try %s\n", prefix_fb_dev, prefix_fb_bak_dev);

        fd = open(prefix_fb_bak_dev, O_RDWR | O_SYNC);
        if (fd < 0) {
            LOGE("Failed to open %s: %s\n", prefix_fb_bak_dev, strerror(errno));
            return -1;
        }
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_varinfo) < 0) {
        LOGE("Failed to get screen var info: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    if(ioctl(fd, FBIOGET_FSCREENINFO, &fb_fixinfo) < 0) {
        LOGE("Failed to get screen fix info: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    fbmem = (uint8_t*) mmap(0, fb_fixinfo.smem_len, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (fbmem == MAP_FAILED) {
        LOGE("Failed to mmap frame buffer: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    memset(fbmem, 0, fb_fixinfo.smem_len);

    fb_count = fb_fixinfo.smem_len / (fb_varinfo.yres
            * fb_fixinfo.line_length);

    screen_size = fb_fixinfo.line_length * fb_varinfo.yres;

    this->fbmem = (uint8_t *) calloc(1, fb_fixinfo.line_length
            * fb_varinfo.yres);

    vt_fd = open(prefix_vt_dev, O_RDWR | O_SYNC);
    if (vt_fd < 0) {
        LOGW("Failed to open %s: %s\n", prefix_vt_dev, strerror(errno));
    } else if (ioctl(vt_fd, KDSETMODE, (void*)KD_GRAPHICS) < 0) {
        LOGE("Failed to set KD_GRAPHICS on %s: %s\n", prefix_vt_dev,
                strerror(errno));

        close(fd);
        free(this->fbmem);
        ioctl(vt_fd, KDSETMODE, (void*) KD_TEXT);
        close(vt_fd);

        return -1;
    }

    return 0;
}

static int deinit(struct fb_manager* this) {
    if (munmap(fbmem, fb_fixinfo.smem_len) < 0) {
        LOGE("Failed to mumap frame buffer: %s\n", strerror(errno));
        return -1;
    }

    if (this->fbmem)
        free(this->fbmem);

    close(fd);
    fd = -1;

    ioctl(vt_fd, KDSETMODE, (void*) KD_TEXT);
    close(vt_fd);
    vt_fd = -1;

    return 0;
}

static int set_displayed_fb(struct fb_manager* this, uint8_t index) {
    assert_die_if(index > fb_count, "Invalid frame buffer index\n");

    fb_varinfo.yoffset = index * fb_varinfo.yres;
    if (ioctl(fd, FBIOPAN_DISPLAY, &fb_varinfo) < 0) {
        LOGE("Failed to set displayed frame buffer: %s\n", strerror(errno));
        return -1;
    }

    fb_index = index;

    return 0;
}

static void display(struct fb_manager* this) {
    if (fb_count > 1) {
        if (fb_index >= fb_count)
            fb_index = 0;

        memcpy(fbmem + fb_index * screen_size, this->fbmem, screen_size);

        set_displayed_fb(this, fb_index);

        fb_index++;

    } else {
        memcpy(fbmem, this->fbmem, screen_size);
    }
}

static uint32_t get_screen_size(struct fb_manager* this) {
    return screen_size;
}

static uint32_t get_screen_height(struct fb_manager* this) {
    return fb_varinfo.yres;
}

static uint32_t get_screen_width(struct fb_manager* this) {
    return fb_varinfo.xres;
}

static uint32_t get_redbit_offset(struct fb_manager* this) {
    return fb_varinfo.red.offset;
}

static uint32_t get_redbit_length(struct fb_manager* this) {
    return fb_varinfo.red.length;
}

static uint32_t get_greenbit_offset(struct fb_manager* this) {
    return fb_varinfo.green.offset;
}

static uint32_t get_greenbit_length(struct fb_manager* this) {
    return fb_varinfo.green.length;
}

static uint32_t get_bluebit_offset(struct fb_manager* this) {
    return fb_varinfo.blue.offset;
}

static uint32_t get_bluebit_length(struct fb_manager* this) {
    return fb_varinfo.blue.length;
}

static uint32_t get_alphabit_offset(struct fb_manager* this) {
    return fb_varinfo.transp.offset;
}

static uint32_t get_alphabit_length(struct fb_manager* this) {
    return fb_varinfo.transp.length;
}

static uint32_t get_bits_per_pixel(struct fb_manager* this) {
    return fb_varinfo.bits_per_pixel;
}

static uint32_t get_row_bytes(struct fb_manager* this) {
    return fb_fixinfo.line_length;
}

void construct_fb_manager(struct fb_manager* this) {
    this->init = init;
    this->deinit = deinit;

    this->dump = dump;

    this->display = display;

    this->get_screen_size = get_screen_size;
    this->get_screen_height = get_screen_height;
    this->get_screen_width = get_screen_width;

    this->get_redbit_offset = get_redbit_offset;
    this->get_redbit_length = get_redbit_length;

    this->get_greenbit_offset = get_greenbit_offset;
    this->get_greenbit_length = get_greenbit_length;

    this->get_bluebit_offset = get_bluebit_offset;
    this->get_bluebit_length = get_bluebit_length;

    this->get_alphabit_offset = get_alphabit_offset;
    this->get_alphabit_length = get_alphabit_length;

    this->get_bits_per_pixel = get_bits_per_pixel;
    this->get_row_bytes = get_row_bytes;

    fd = -1;
    vt_fd = -1;

    this->fbmem = NULL;
}

void destruct_fb_manager(struct fb_manager* this) {
    this->init = NULL;
    this->deinit = NULL;

    this->dump = NULL;

    this->display = NULL;

    this->get_screen_size = NULL;
    this->get_screen_height = NULL;
    this->get_screen_width = NULL;

    this->get_redbit_offset = NULL;
    this->get_redbit_length = NULL;

    this->get_greenbit_offset = NULL;
    this->get_greenbit_length = NULL;

    this->get_bluebit_offset = NULL;
    this->get_bluebit_length = NULL;

    this->get_alphabit_offset = NULL;
    this->get_alphabit_length = NULL;

    this->get_bits_per_pixel = NULL;
    this->get_row_bytes = NULL;

    fd = -1;
    vt_fd = -1;

    this->fbmem = NULL;
}
