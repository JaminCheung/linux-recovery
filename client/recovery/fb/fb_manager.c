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

#include <utils/log.h>
#include <utils/assert.h>
#include <fb/fb_manager.h>

#define LOG_TAG "fb_manager"

const static char* prefix_fb_dev = "/dev/fb0";

static void dump(struct fb_manager* this) {
    LOGI("==========================\n");
    LOGI("Dump fb info\n");
    LOGI("Width:  %u\n", this->fb_varinfo.xres);
    LOGI("Length: %u\n", this->fb_varinfo.yres);
    LOGI("Bpp:    %u\n", this->fb_varinfo.bits_per_pixel);
    LOGI("==========================\n");
}

static int init(struct fb_manager* this) {
    this->fd = open(prefix_fb_dev, O_RDWR | O_SYNC);
    if (this->fd < 0) {
        LOGE("Failed to open %s: %s\n", prefix_fb_dev, strerror(errno));
        return -1;
    }

    if (ioctl(this->fd, FBIOGET_VSCREENINFO, &this->fb_varinfo) < 0) {
        LOGE("Failed to get screen var info: %s\n", strerror(errno));
        return -1;
    }

    if(ioctl(this->fd, FBIOGET_FSCREENINFO, &this->fb_fixinfo) < 0) {
        LOGE("Failed to get screen fix info: %s\n", strerror(errno));
        return -1;
    }

    this->screen_size = this->fb_varinfo.xres * this->fb_varinfo.yres
            * this->fb_varinfo.bits_per_pixel / 8;

    this->fbmem = (uint8_t*) mmap(0, this->screen_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, this->fd, 0);
    if (this->fbmem == MAP_FAILED) {
        LOGE("Failed to mmap frame buffer: %s\n", strerror(errno));
        return -1;
    }

    return this->fd;
}

static int deinit(struct fb_manager* this) {
    if (munmap(this->fbmem, this->screen_size) < 0) {
        LOGE("Failed to mumap frame buffer: %s\n", strerror(errno));
        return -1;
    }

    close(this->fd);

    return 0;
}

void construct_fb_manager(struct fb_manager* this) {
    this->init = init;
    this->deinit = deinit;

    this->dump = dump;

    this->fd = -1;
    this->fbmem = NULL;
}

void destruct_fb_manager(struct fb_manager* this) {
    this->init = NULL;
    this->deinit = NULL;

    this->dump = NULL;

    this->fd = -1;
    this->fbmem = NULL;
}
