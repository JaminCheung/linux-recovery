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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/list.h>
#include <utils/file_ops.h>
#include <mount/mount_manager.h>

#define LOG_TAG "mount_manager"

#define PROC_MOUNTS_FILENAME "/proc/mounts"

const char* supported_filesystem_list[] = {
        "vfat",
        "ntfs",
        "ext4",
        NULL
};

static void dump_mounted_volumes(struct mount_manager* this) {
    struct list_head* pos;

    LOGI("==============================\n");
    LOGI("Dump mounted volumes\n");
    list_for_each(pos, &this->list) {
        struct mounted_volume* volume = list_entry(pos,
                struct mounted_volume, head);

        LOGI("------------------------------\n");
        LOGI("device:      %s\n", volume->device);
        LOGI("mount point: %s\n", volume->mount_point);
        LOGI("file system: %s\n", volume->filesystem);
        LOGI("flags        %s\n", volume->flags);
    }
    LOGI("==============================\n");
}

static void scan_mounted_volumes(struct mount_manager* this) {
    char line[2048] = {0};
    FILE* fp = NULL;

    fp = fopen(PROC_MOUNTS_FILENAME, "r");

    assert_die_if(fp == NULL, "Failed to open %s: %s\n",
            PROC_MOUNTS_FILENAME, strerror(errno));

    /*
     * Clear list first
     */
    if (!list_empty(&this->list)) {
        struct list_head* pos;
        struct list_head* next_pos;

        list_for_each_safe(pos, next_pos, &this->list) {
            struct mounted_volume* volume =
                    list_entry(pos, struct mounted_volume, head);

            list_del(&volume->head);
            free(volume);
        }
    }

    /*
     * Scan /proc/mounts
     */
    while (fgets(line, sizeof(line), fp)) {
        int matches;

        struct mounted_volume* volume = calloc(1,
                sizeof(struct mounted_volume));

        matches = sscanf(line, "%63s %63s %63s %127s",
                volume->device, volume->mount_point,
                volume->filesystem, volume->flags);

        if (matches != 4) {
            LOGW("Failed to parse line %s\n", line);
            free(volume);
            continue;
        }

        list_add_tail(&volume->head, &this->list);
    }

    fclose(fp);
}

static struct mounted_volume*
    find_mounted_volume_by_device(struct mount_manager* this,
            const char* device) {
    assert_die_if(device == NULL, "device is NULL\n");

    struct list_head* pos;
    list_for_each(pos, &this->list) {
        struct mounted_volume* volume = list_entry(pos,
                struct mounted_volume, head);
        if (volume->device != NULL) {
            if (!strcmp(volume->device, device))
                return volume;
        }
    }

    return NULL;
}

static struct mounted_volume*
    find_mounted_volume_by_mount_point(struct mount_manager* this,
            const char* mount_point) {
    assert_die_if(mount_point == NULL, "mount_point is NULL\n");

    struct list_head* pos;
    list_for_each(pos, &this->list) {

        struct mounted_volume* volume = list_entry(pos,
                struct mounted_volume, head);
        if (volume->mount_point != NULL) {
            if (!strcmp(volume->mount_point, mount_point))
                return volume;
        }
    }

    return NULL;
}

static int mount_volume(struct mount_manager* this, const char* device,
        const char* mount_point, const char* filesystem) {
    assert_die_if(device == NULL, "device is NULL\n");
    assert_die_if(mount_point == NULL, "mount_point is NULL\n");
    assert_die_if(filesystem == NULL, "filesystem is NULL\n");

    int error = 0;

    if (!strcmp(filesystem, "ramdisk"))
        return 0;

    const struct mounted_volume* volume =
            this->find_mounted_volume_by_mount_point(this, mount_point);
    if (volume)
        return 0;

    dir_create(mount_point);

    int i;
    for (i = 0; supported_filesystem_list[i]; i++) {
        const char* fs = supported_filesystem_list[i];
        if (!strcmp(fs, filesystem))
            break;
    }

    if (supported_filesystem_list[i] == NULL) {
        LOGE("Unsupport file system: \"%s\" for \"%s\"\n", filesystem ,
                mount_point);
        return -1;
    }

    error = mount(device, mount_point, filesystem,
            MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
    if (error < 0) {
        LOGE("Failed to mount %s: %s\n", mount_point, strerror(errno));
        return -1;
    }
#if 0
    if (!strcmp(filesystem, "ext4")
            || strcmp(filesystem, "vfat")
            || strcmp(filesystem, "ntfs")) {
        error = mount(device, mount_point, filesystem,
                MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if (error < 0) {
            LOGE("Failed to mount %s: %s\n", mount_point, strerror(errno));
            return -1;
        }

    } else {
        LOGE("Unsupport file system: \"%s\" for \"%s\"\n", filesystem ,
                mount_point);
        return -1;
    }
#endif
    return 0;
}

static int umount_volume(struct mount_manager* this,
        struct mounted_volume* volume) {
    assert_die_if(volume == NULL, "volume is NULL\n");

    int error = 0;

    if (!strcmp(volume->filesystem, "ramdisk"))
        return 0;

    error = umount(volume->mount_point);
    if (error < 0) {
        LOGE("Failed to umount %s: %s\n", volume->mount_point, strerror(errno));
        return -1;
    }

    list_del(&volume->head);
    free(volume);

    return 0;
}

void construct_mount_manager(struct mount_manager* this) {
    this->scan_mounted_volumes = scan_mounted_volumes;
    this->find_mounted_volume_by_device = find_mounted_volume_by_device;
    this->find_mounted_volume_by_mount_point = find_mounted_volume_by_mount_point;
    this->mount_volume = mount_volume;
    this->umount_volume = umount_volume;
    this->supported_filesystem_list = supported_filesystem_list;
    this->dump_mounted_volumes = dump_mounted_volumes;

    INIT_LIST_HEAD(&this->list);
}

void destruct_mount_manager(struct mount_manager* this) {
    this->scan_mounted_volumes = NULL;
    this->find_mounted_volume_by_device = NULL;
    this->find_mounted_volume_by_mount_point = NULL;
    this->mount_volume = NULL;
    this->umount_volume = NULL;
    this->dump_mounted_volumes = NULL;
}
