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

#include <utils/log.h>
#include <utils/assert.h>
#include <fb/fb_manager.h>

#define LOG_TAG "fb_manager"

const static char* prefix_fb_dev = "/dev/fb0";

static int fd;
static uint8_t fb_count;
static uint8_t fb_index;
static uint8_t *fbmem;
static size_t screen_size;

struct fb_fix_screeninfo fb_fixinfo;

static void dump(struct fb_manager* this) {
    LOGI("==========================\n");
    LOGI("Dump fb info\n");
    LOGI("Frame count: %u\n", fb_count);
    LOGI("Width:       %u\n", this->fb_varinfo.xres);
    LOGI("Length:      %u\n", this->fb_varinfo.yres);
    LOGI("Bpp:         %u\n", this->fb_varinfo.bits_per_pixel);
    LOGI("==========================\n");
}

static int init(struct fb_manager* this) {
    fd = open(prefix_fb_dev, O_RDWR | O_SYNC);
    if (fd < 0) {
        LOGE("Failed to open %s: %s\n", prefix_fb_dev, strerror(errno));
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &this->fb_varinfo) < 0) {
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

    fb_count = fb_fixinfo.smem_len / (this->fb_varinfo.yres
            * fb_fixinfo.line_length);

    screen_size = fb_fixinfo.line_length * this->fb_varinfo.yres;

    this->fbmem = (uint8_t *) malloc(sizeof(fb_fixinfo.line_length
            * this->fb_varinfo.yres));

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

    return 0;
}

static int set_displayed_fb(struct fb_manager* this, uint8_t index) {
    assert_die_if(index > fb_count, "Invalid frame buffer index\n");

    this->fb_varinfo.yoffset = index * this->fb_varinfo.yres;
    if (ioctl(fd, FBIOPAN_DISPLAY, &this->fb_varinfo) < 0) {
        LOGE("Failed to set displayed frame buffer: %s\n", strerror(errno));
        return -1;
    }

    fb_index = index;

    return 0;
}

static void fb_flip_display(struct fb_manager* this) {
    if (fb_count > 1) {
        uint8_t index = 0;

        index = fb_index + 1;

        if (index > fb_count)
            index = 0;

        memcpy(fbmem + index * screen_size, this->fbmem, screen_size);

        set_displayed_fb(this, index);

    } else {
        memcpy(fbmem, this->fbmem, screen_size);
    }
}

void construct_fb_manager(struct fb_manager* this) {
    this->init = init;
    this->deinit = deinit;

    this->dump = dump;

    this->fb_flip_display = fb_flip_display;

    fd = -1;
    this->fbmem = NULL;
}

void destruct_fb_manager(struct fb_manager* this) {
    this->init = NULL;
    this->deinit = NULL;

    this->dump = NULL;
    this->fb_flip_display = NULL;

    fd = -1;
    this->fbmem = NULL;
}
