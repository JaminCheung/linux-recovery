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
#include <unistd.h>
#include <sys/reboot.h>
#include <dirent.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/linux.h>
#include <utils/common.h>
#include <utils/compare_string.h>
#include <utils/file_ops.h>
#include <utils/minizip.h>
#include <utils/verifier.h>
#include <netlink/netlink_event.h>
#include <ota/ota_manager.h>

#define LOG_TAG "ota_manager"

static const char* prefix_device_xml = "partition.xml";
static const char* prefix_update_xml = "update.xml";
static const char* prefix_volume_mount_point = "/mnt";
static const char* prefix_volume_device_path = "/dev";
static const char* prefix_update_path = "recovery-update";
static const char* prefix_update_info_pkg = "update000.zip";
static const char* prefix_local_update_path = "/tmp/update";

static void *main_task(void *param);

static inline int ensure_volume_mounted(struct ota_manager* this,
        struct storage_dev* device) {
    int i = 0;
    int error = 0;

    this->mm->scan_mounted_volumes(this->mm);

    for (i = 0; this->mm->supported_filesystem_list[i]; i++) {
        const char* filesystem = this->mm->supported_filesystem_list[i];

        LOGI("Try to mount %s to %s\n", device->dev_name, device->mount_point);

        error = this->mm->mount_volume(this->mm, device->dev_name,
                device->mount_point, filesystem);
        if (!error)
            break;
    }

    if (this->mm->supported_filesystem_list[i] == NULL) {
        LOGE("Faied to mount device \"%s\"\n", device->dev_name);
        return -1;
    }

    this->mm->scan_mounted_volumes(this->mm);

    return 0;
}

static inline int ensure_volume_unmounted(struct ota_manager* this,
        struct storage_dev* device) {
    int error = 0;
    this->mm->scan_mounted_volumes(this->mm);

    struct mounted_volume* volume =
            this->mm->find_mounted_volume_by_device(this->mm, device->dev_name);
    if (volume == NULL) {
        LOGW("Device \"%s\" may be already unmounted\n", device->dev_name);
        return 0;
    }

    LOGI("Try to umount %s from %s\n", device->dev_name, device->mount_point);
    error = this->mm->umount_volume(this->mm, volume);
    if (error < 0) {
        LOGE("Failed to umount device \"%s\": %s", volume->device,
                strerror(errno));
        return -1;
    }

    this->mm->scan_mounted_volumes(this->mm);

    return 0;
}

static inline void add_storage_dev(struct ota_manager* this, const char* name) {

    struct list_head* pos;
    list_for_each(pos, &this->storage_dev_list) {
        struct storage_dev* d = list_entry(pos, struct storage_dev, head);
        if (!strcmp(d->name, name))
            return;
    }

    struct storage_dev* dev = (struct storage_dev* )
            calloc(1, sizeof(struct storage_dev));

    strcpy(dev->name, name);
    sprintf(dev->dev_name, "%s/%s", prefix_volume_device_path, name);
    sprintf(dev->mount_point, "%s/%s", prefix_volume_mount_point, name);

    list_add_tail(&dev->head, &this->storage_dev_list);
}

static inline void del_storage_dev(struct ota_manager* this, const char* name) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &this->storage_dev_list) {
        struct storage_dev* dev =
                list_entry(pos, struct storage_dev, head);

        if (!strcmp(dev->name, name)) {
            list_del(&dev->head);
            free(dev);
        }
    }
}

static void handle_net_event(struct netlink_handler* nh,
        struct netlink_event *event) {

}

static void handle_block_event(struct netlink_handler* nh,
        struct netlink_event* event) {
    struct ota_manager* this =
            (struct ota_manager *) nh->get_private_data(nh);

    int nparts = -1;
    const char* type = event->find_param(event, "DEVTYPE");
    const char* name = event->find_param(event, "DEVNAME");
    const char* nparts_str = event->find_param(event, "NPARTS");
    const int action = event->get_action(event);

    if (nparts_str)
        nparts = atoi(nparts_str);

    if (!is_prefixed_with(name, "sd") && !is_prefixed_with(name, "mmcblk"))
        return;

    if (action == NLACTION_ADD) {
        if ((!strcmp(type, "disk") && !nparts) || (!strcmp(type, "partition")))
            add_storage_dev(this, name);

    } else if (action == NLACTION_REMOVE) {
        if ((!strcmp(type, "disk") && !nparts) || (!strcmp(type, "partition")))
            del_storage_dev(this, name);
    }
}

static void handle_event(struct netlink_handler* nh,
        struct netlink_event* event) {
    const char* subsystem = event->get_subsystem(event);

    if (!strcmp(subsystem, "block"))
        event->dump(event);

    if (!strcmp(subsystem, "block")) {
        handle_block_event(nh, event);

    } else if (!strcmp(subsystem, "net")) {
        handle_net_event(nh, event);
    }
}

static int start(struct ota_manager* this) {
    int error = 0;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    error = pthread_create(&tid, &attr, main_task, (void *) this);
    if (error) {
        LOGE("pthread_create failed: %s", strerror(errno));
        pthread_attr_destroy(&attr);
        return -1;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

static int stop(struct ota_manager* this) {
    LOGI("Stop ota manager.\n");

    return 0;
}

static void mount_all(struct ota_manager* this) {
    struct list_head* pos;

    dir_delete(prefix_volume_mount_point);

    list_for_each(pos, &this->storage_dev_list) {
        struct storage_dev* device = list_entry(pos, struct storage_dev, head);

        msleep(100);

        ensure_volume_mounted(this, device);
    }
}

static void umount_all(struct ota_manager* this) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &this->storage_dev_list) {
        struct storage_dev* device = list_entry(pos, struct storage_dev, head);

        msleep(100);

        ensure_volume_unmounted(this, device);

        list_del(&device->head);
        free(device);
    }
}

static void recovery_finish(int error) {
    LOGI("Recovery finish %s!\n", !error ? "success" : "failure");

    sync();

    if (error < 0) {
#ifdef LOCAL_DEBUG
        reboot(RB_POWER_OFF);
#else
        exit(-1);
#endif
    } else {
#ifdef LOCAL_DEBUG
        reboot(RB_AUTOBOOT);
#else
        exit(0);
#endif
    }

    for (;;) {
        LOGE("Should not come here...\n");
        sleep(1);
    }
}

static int check_update_pkg(struct ota_manager* this, const char* path) {
    int nkeys = 0;

    RSAPublicKey* keys = load_keys(public_key_path, &nkeys);
    if (keys == NULL) {
        LOGE("Failed to load public keys from: %s\n", public_key_path);
        return -1;
    }

    if (verify_file(path, keys, nkeys) < 0) {
        LOGE("Failed to verify file: %s\n", path);
        return -1;
    }

    return 0;
}

static struct mounted_volume* find_valid_update_volume(struct ota_manager* this) {
    char path[PATH_MAX] = {0};
    struct list_head* pos;

    /*
     * For each mounted volumes
     */
    list_for_each(pos, &this->mm->list) {
        struct mounted_volume *volume = list_entry(pos, struct mounted_volume,
                head);
        sprintf(path, "%s/%s", volume->mount_point, prefix_update_path);

        /*
         * Check update dir on mounted vloume
         */
        if (dir_exist(path) < 0)
            continue;

        /*
         * Verifier update000.zip
         */
        sprintf(path, "%s/%s", path, prefix_update_info_pkg);
        if (file_exist(path) < 0 || check_update_pkg(this, path) < 0)
            continue;

        /*
         * Create unzip dir
         */
        if (dir_create(prefix_local_update_path) < 0)
            continue;

        /*
         * Un-zip update pkg
         */
        if (unzip(path, prefix_local_update_path, NULL, 1) < 0)
            continue;

        /*
         * Parse & check device info
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s", prefix_local_update_path, prefix_device_xml);
        if (this->uf->parse_device_xml(this->uf, path) < 0)
            continue;

        this->uf->dump_device_xml(this->uf);

        //TODO: check device info

        /*
         * Parse update info
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s", prefix_local_update_path, prefix_update_xml);
        if (this->uf->parse_update_xml(this->uf, path) < 0)
            continue;
        this->uf->dump_update_xml(this->uf);

        uint32_t chunk_count = 0;
        struct list_head* sub_pos;
        list_for_each(sub_pos, &this->uf->update_info.list) {
            struct image_info * info = list_entry(sub_pos, struct image_info,
                    head);
            if (info->chunkcount == 0)
                chunk_count += 1;
            else
                chunk_count += info->chunkcount;
        }

        DIR * dir = opendir(prefix_local_update_path);
        if (dir == NULL)
            continue;

        uint32_t pkg_count = 0;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (!strcmp(de->d_name, "..") || !strcmp(de->d_name, "."))
                continue;

            if (is_prefixed_with(de->d_name, "update")
                    && is_suffixed_with(de->d_name, ".zip"))
                pkg_count++;
        }

        if (chunk_count != (pkg_count - 1))
            continue;
    }

    return NULL;
}

static int update_from_storage(struct ota_manager* this) {
    struct mounted_volume *volume= find_valid_update_volume(this);

    if (volume == NULL) {
        LOGE("Failed to found valid volume contain update package\n");
        return -1;
    }


    return 0;
}

static int update_from_network(struct ota_manager* this) {
    int error = 0;

    error = this->ni->icmp_echo(this->ni, this->cf->server_ip, 2000);
    if (error < 0) {
        LOGE("Server \"%s\" is unreachable\n", this->cf->server_ip);
        return -1;
    }

    return 0;
}

static void *main_task(void* param) {
    struct ota_manager* this = (struct ota_manager*) param;

    int error = 0;

    cold_boot("/sys/block");
    cold_boot("/sys/class/net");

    mount_all(this);

    error = update_from_storage(this);
    if (error < 0)
        error = update_from_network(this);

    umount_all(this);

    recovery_finish(error);

    return NULL;
}

static void load_configure(struct ota_manager* this,
        struct configure_file* cf) {
    assert_die_if(cf == NULL, "cf is NULL\n");

    this->cf = cf;
}

void construct_ota_manager(struct ota_manager* this) {
    INIT_LIST_HEAD(&this->storage_dev_list);

    this->start = start;
    this->stop = stop;
    this->load_configure = load_configure;

    /*
     * Instance netlink handler
     */
    this->nh = (struct netlink_handler *) calloc(1,
            sizeof(struct netlink_handler));
    this->nh->construct = construct_netlink_handler;
    this->nh->deconstruct = destruct_netlink_handler;
    this->nh->construct(this->nh, "all sub-system", 0, handle_event, this);

    /*
     * Instance mount manager
     */
    this->mm = _new(struct mount_manager, mount_manager);

    /*
     * Instance update file
     */
    this->uf = _new(struct update_file, update_file);

    /*
     * Instance net interface
     */
    this->ni = (struct net_interface*) calloc(1, sizeof(struct net_interface));
    this->ni->construct = construct_net_interface;
    this->ni->destruct = destruct_net_interface;
    this->ni->construct(this->ni, NULL);
    this->ni->init_socket(this->ni);
}

void destruct_ota_manager(struct ota_manager* this) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &this->storage_dev_list) {
        struct storage_dev* device = list_entry(pos, struct storage_dev, head);

        list_del(&device->head);
        free(device);
    }

    _delete(this->cf);
    _delete(this->uf);
    _delete(this->mm);

    this->cf = NULL;
    this->uf = NULL;
    this->start = NULL;
    this->stop = NULL;
    this->load_configure = NULL;

    /*
     * Destruct netlink_handler
     */
    this->nh->deconstruct(this->nh);
    this->nh = NULL;

    /*
     * Destruct net_interface
     */
    this->ni->destruct(this->ni);
    this->ni = NULL;
}
