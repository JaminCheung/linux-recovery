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

static const char* prefix_global_xml = "global.xml";
static const char* prefix_device_xml = "device.xml";
static const char* prefix_update_xml = "update.xml";
static const char* prefix_volume_mount_point = "/mnt";
static const char* prefix_volume_device_path = "/dev";
static const char* prefix_storage_update_path = "recovery-update";
static const char* prefix_update_pkg = "update";
static const char* prefix_local_update_path = "/tmp/update";
static const char* prefix_image_logo_path = "/res/image/logo.png";
static const char* prefix_image_progress_path = "/res/image/progress_";

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
        LOGE("Failed to umount device \"%s\": %s\n", volume->device,
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

static void bm_event_listener(struct block_manager* bm,
        struct bm_event* event, void* param) {
    struct ota_manager* this = (struct ota_manager *)param;

    (void)this;
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

    if (error)
        exit(-1);
     else
        exit(0);

    for (;;) {
        LOGE("Should not come here...\n");
        sleep(1);
    }
}

static int verify_update_pkg(struct ota_manager* this, const char* path) {
    int nkeys = 0;

    RSAPublicKey* keys = load_keys(g_data.public_key_path, &nkeys);
    if (keys == NULL) {
        LOGE("Failed to load public keys from: %s\n", g_data.public_key_path);
        return -1;
    }

    if (verify_file(path, keys, nkeys) == VERIFY_FAILURE) {
        LOGE("Failed to verify file: %s\n", path);
        return -1;
    }

    return 0;
}

static int check_device_info(struct ota_manager* this,
        struct device_info* device_info) {
    return 0;
}

static int write_update_pkg(struct ota_manager* this, const char* path,
        struct image_info* info) {
    return 0;
}

static struct mounted_volume* find_valid_update_volume(struct ota_manager* this) {
    char path[PATH_MAX] = {0};
    struct list_head* pos;

    /*
     * Create unzip directory
     */
    dir_delete(prefix_local_update_path);
    if (dir_create(prefix_local_update_path) < 0) {
        LOGE("Failed to create %s\n", prefix_local_update_path);
        return NULL;
    }

    /*
     * For each mounted volumes
     */
    list_for_each(pos, &this->mm->list) {
        struct mounted_volume *volume = list_entry(pos, struct mounted_volume,
                head);
        sprintf(path, "%s/%s", volume->mount_point, prefix_storage_update_path);

        LOGI("Lookup mount point: %s\n", volume->mount_point);

        /*
         * Check update dir on mounted vloume
         */
        if (dir_exist(path) < 0)
            continue;

        /*
         * Parse global.xml
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s/%s", volume->mount_point, prefix_storage_update_path,
                prefix_global_xml);
        if (file_exist(path) < 0)
            continue;

        if (this->uf->parse_global_xml(this->uf, path) < 0) {
            LOGE("Failed to parse %s\n", prefix_global_xml);
            continue;
        }
        this->uf->dump_device_type_list(this->uf);

        /*
         * For each device type
         */
        int i;
        const char** device_type_list = this->uf->get_device_type_list(this->uf);
        for (i = 0; device_type_list[i]; i++) {
            const char* devtype = device_type_list[i];

            LOGI("Lookup for device: %s\n", devtype);

            struct device_info* device_info =
                    this->uf->get_device_info_by_devtype(this->uf, devtype);
            struct update_info* update_info =
                    this->uf->get_update_info_by_devtype(this->uf, devtype);
            if (device_info == NULL || update_info == NULL) {
                LOGE("Failed to find device info or update_info for %s\n", devtype);
                break;
            }

            memset(path, 0, sizeof(path));
            sprintf(path, "%s/%s/%s", volume->mount_point, prefix_storage_update_path,
                    devtype);

            if (dir_exist(path) < 0) {
                LOGE("Failed to found %s\n", path);
                break;
            }

            /*
             * Verifier update000.zip
             */
            memset(path, 0, sizeof(path));
            sprintf(path, "%s/%s/%s/%s%s", volume->mount_point,
                    prefix_storage_update_path, devtype, prefix_update_pkg,
                    "000.zip");

            LOGI("Verifying %s\n", path);

            if (file_exist(path) < 0 || verify_update_pkg(this, path) < 0)
                break;

            /*
             * Un-zip update pkg
             */
            LOGI("Unziping %s\n", path);
            if (unzip(path, prefix_local_update_path, NULL, 1) < 0) {
                LOGE("Failed to unzip %s to %s\n", path, prefix_local_update_path);
                break;
            }

            /*
             * Parse & check device info
             */
            memset(path, 0, sizeof(path));
            sprintf(path, "%s/%s", prefix_local_update_path, prefix_device_xml);

            LOGI("Parsing %s\n", path);

            if (this->uf->parse_device_xml(this->uf, path, device_info) < 0) {
                LOGE("Failed to parse %s\n", path);
                break;
            }
            this->uf->dump_device_info(this->uf, device_info);

            /*
             * Check device info
             */
            LOGI("Checking device info: %s\n", devtype);
            if (check_device_info(this, device_info) < 0) {
                LOGE("Failed to check device info for %s\n", devtype);
                break;
            }

            /*
             * Parse update info
             */
            memset(path, 0, sizeof(path));
            sprintf(path, "%s/%s", prefix_local_update_path, prefix_update_xml);

            LOGI("Parsing %s\n", path);

            if (this->uf->parse_update_xml(this->uf, path, update_info) < 0) {
                LOGE("Failed to parse %s\n", path);
                break;
            }
            this->uf->dump_update_info(this->uf, update_info);

            uint32_t chunk_count = 0;
            struct list_head* sub_pos;
            list_for_each(sub_pos, &update_info->list) {
                struct image_info * info = list_entry(sub_pos, struct image_info,
                        head);
                chunk_count += info->chunkcount;
            }

            memset(path, 0, sizeof(path));
            sprintf(path, "%s/%s/%s", volume->mount_point, prefix_storage_update_path,
                    devtype);
            if (dir_exist(path) < 0) {
                LOGE("Failed to access %s: %s\n", path, strerror(errno));
                break;
            }

            DIR * dir = opendir(path);
            uint32_t pkg_count = 0;
            struct dirent *de;
            while ((de = readdir(dir)) != NULL) {
                if (!strcmp(de->d_name, "..") || !strcmp(de->d_name, "."))
                    continue;

                if (is_prefixed_with(de->d_name, "update")
                        && is_suffixed_with(de->d_name, ".zip"))
                    pkg_count++;
            }

            if (chunk_count != (pkg_count - 1)) {
                LOGE("update pakage count error, pkg count %u, actual count %u\n",
                        chunk_count, pkg_count);
                closedir(dir);
                break;
            }

            closedir(dir);
        }

        if (device_type_list[i] != NULL)
            continue;

        dir_delete(prefix_local_update_path);

        return volume;
    }

    dir_delete(prefix_local_update_path);

    return NULL;
}

static int update_from_storage(struct ota_manager* this) {
    char path[PATH_MAX] = {0};
    struct mounted_volume *volume= find_valid_update_volume(this);

    if (volume == NULL) {
        LOGE("Failed to found valid volume contain update package\n");
        return -1;
    }

    int i;
    const char** device_type_list = this->uf->get_device_type_list(this->uf);
    for (i = 0; device_type_list[i]; i++) {
        const char* devtype = device_type_list[i];
        struct update_info* update_info =
                this->uf->get_update_info_by_devtype(this->uf, devtype);

        LOGI("Updating device: \"%s\"\n", devtype);

        int index = 1;
        struct list_head* pos;
        list_for_each(pos, &update_info->list) {
            struct image_info* info = list_entry(pos, struct image_info, head);

            LOGI("Updating image: \"%s\"\n", info->name);

            for (int i = 1; i <= info->chunkcount; i++) {
                /*
                 * Create unzip dir
                 */
                dir_delete(prefix_local_update_path);
                if (dir_create(prefix_local_update_path) < 0) {
                    LOGE("Failed to create %s\n", prefix_local_update_path);
                    return -1;
                }

                memset(path, 0, sizeof(path));
                sprintf(path, "%s/%s/%s/%s%03d.zip", volume->mount_point,
                        prefix_storage_update_path, devtype, prefix_update_pkg, index);

                LOGI("Verifying %s\n", path);
                if (file_exist(path) < 0 || verify_update_pkg(this, path) < 0)
                    goto error;

                LOGI("Unziping %s\n", path);
                if (unzip(path, prefix_local_update_path, NULL, 1) < 0) {
                    LOGE("Failed to unzip %s to %s\n", path,
                            prefix_local_update_path);
                    goto error;
                }

                memset(path, 0, sizeof(path));
                if (info->chunkcount == 1)
                    sprintf(path, "%s/%s", prefix_local_update_path, info->name);
                else
                    sprintf(path, "%s/%s_%03d", prefix_local_update_path,
                            info->name, i);

                if (file_exist(path) < 0) {
                    LOGE("Failed to access %s: %s\n", path, strerror(errno));
                    goto error;
                }

                index++;

                LOGI("Updating \"%s\"\n", path);
                if (write_update_pkg(this, path, info) < 0) {
                    LOGE("Failed to write %s\n", path);
                    goto error;
                }
            }
        }
    }

    dir_delete(prefix_local_update_path);

    return 0;

error:
    dir_delete(prefix_local_update_path);

    return -1;
}

static int update_from_network(struct ota_manager* this) {
    int error = 0;
    char path[PATH_MAX] = {0};

    /*
     * Check network
     */
    LOGI("Checking network\n");
    error = this->ni->icmp_echo(this->ni, this->cf->server_ip, 2000);
    if (error < 0) {
        LOGE("Server \"%s\" is unreachable\n", this->cf->server_ip);
        return -1;
    }

    /*
     * Create unzip dir
     */
    dir_delete(prefix_local_update_path);
    if (dir_create(prefix_local_update_path) < 0) {
        LOGE("Failed to create %s\n", prefix_local_update_path);
        return -1;
    }

    /*
     * Download global.xml
     */
    memset(path, 0, sizeof(path));
    sprintf(path, "%s/%s", this->cf->server_url, prefix_global_xml);
    LOGI("Downloading %s\n", path);
    if (download_file(path, prefix_local_update_path) < 0) {
        LOGE("Failed to download %s\n", path);
        return -1;
    }

    /*
     * Parse global.xml
     */
    memset(path, 0, sizeof(path));
    sprintf(path, "%s/%s", prefix_local_update_path, prefix_global_xml);
    LOGI("Parsing %s\n", path);
    if (this->uf->parse_global_xml(this->uf, path) < 0) {
        LOGE("Failed to parse %s\n", path);
        return -1;
    }
    this->uf->dump_device_type_list(this->uf);

    int i;
    const char** device_type_list = this->uf->get_device_type_list(this->uf);
    for (i = 0; device_type_list[i]; i++) {
        const char* devtype = device_type_list[i];

        LOGI("Updating device: \"%s\"\n", devtype);

        /*
         * Create unzip dir
         */
        dir_delete(prefix_local_update_path);
        if (dir_create(prefix_local_update_path) < 0) {
            LOGE("Failed to create %s\n", prefix_local_update_path);
            goto error;
        }

        struct device_info* device_info =
                this->uf->get_device_info_by_devtype(this->uf, devtype);
        struct update_info* update_info =
                this->uf->get_update_info_by_devtype(this->uf, devtype);
        if (device_info == NULL || update_info == NULL) {
            LOGE("Failed to find device info or update_info for %s\n", devtype);
            goto error;
        }

        /*
         * Download update000.zip
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s/%s%s", this->cf->server_url, devtype,
                prefix_update_pkg, "000.zip");
        LOGI("Downloading %s\n", path);

        if (download_file(path, prefix_local_update_path) < 0) {
            LOGE("Failed to download %s\n", path);
            goto error;
        }

        /*
         * Verify update000.zip
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s%s", prefix_local_update_path, prefix_update_pkg, "000.zip");

        LOGI("Verifying %s\n", path);
        if (verify_update_pkg(this, path) < 0)
            goto error;

        LOGI("Unziping %s\n", path);
        if (unzip(path, prefix_local_update_path, NULL, 1) < 0) {
            LOGE("Failed to unzip %s\n", path);
            goto error;
        }

        /*
         * Parse & check device info
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s", prefix_local_update_path, prefix_device_xml);
        LOGI("Parsing %s\n", path);

        if (this->uf->parse_device_xml(this->uf, path, device_info) < 0) {
            LOGE("Failed to parse %s\n", path);
            goto error;
        }
        this->uf->dump_device_info(this->uf, device_info);

        /*
         * Check device info
         */
        LOGI("Checking device info: %s\n", devtype);
        if (check_device_info(this, device_info) < 0) {
            LOGE("Failed to check device info for %s\n", devtype);
            goto error;
        }

        /*
         * Parse update info
         */
        memset(path, 0, sizeof(path));
        sprintf(path, "%s/%s", prefix_local_update_path, prefix_update_xml);
        LOGI("Parsing %s\n", path);

        if (this->uf->parse_update_xml(this->uf, path, update_info) < 0) {
            LOGE("Failed to parse %s\n", path);
            goto error;
        }
        this->uf->dump_update_info(this->uf, update_info);

        /*
         * Get update pkg count
         */
        uint32_t chunk_count = 0;
        struct list_head* sub_pos;
        list_for_each(sub_pos, &update_info->list) {
            struct image_info * info = list_entry(sub_pos, struct image_info,
                    head);
            if (info->chunkcount == 0)
                chunk_count += 1;
            else
                chunk_count += info->chunkcount;
        }

        int index = 1;
        struct list_head* pos;
        list_for_each(pos, &update_info->list) {
            struct image_info* info = list_entry(pos, struct image_info, head);

            LOGI("Updating image: \"%s\"\n", info->name);

            for (int i = 1; i <= info->chunkcount; i++) {
                /*
                 * Create unzip dir
                 */
                dir_delete(prefix_local_update_path);
                if (dir_create(prefix_local_update_path) < 0) {
                    LOGE("Failed to create %s\n", prefix_local_update_path);
                    goto error;
                }

                memset(path, 0, sizeof(path));
                sprintf(path, "%s/%s/%s%03d.zip", this->cf->server_url, devtype,
                        prefix_update_pkg, index);

                LOGI("Downloading %s\n", path);
                if (download_file(path, prefix_local_update_path) < 0) {
                    LOGE("Failed to download %s to %s\n", path,
                            prefix_local_update_path);
                    goto error;
                }

                memset(path, 0, sizeof(path));
                sprintf(path, "%s/%s%03d.zip", prefix_local_update_path,
                        prefix_update_pkg, index);
                LOGI("Verifying %s\n", path);
                if (file_exist(path) < 0 || verify_update_pkg(this, path) < 0)
                    goto error;

                LOGI("Unziping %s\n", path);
                if (unzip(path, prefix_local_update_path, NULL, 1) < 0) {
                    LOGE("Failed to unzip %s to %s\n", path,
                            prefix_local_update_path);
                    goto error;
                }

                memset(path, 0, sizeof(path));
                if (info->chunkcount == 1)
                    sprintf(path, "%s/%s", prefix_local_update_path, info->name);
                else
                    sprintf(path, "%s/%s_%03d", prefix_local_update_path,
                            info->name, i);

                if (file_exist(path) < 0) {
                    LOGE("Failed to access %s: %s\n", path, strerror(errno));
                    goto error;
                }

                index++;

                LOGI("Updating \"%s\"\n", path);
                if (write_update_pkg(this, path, info) < 0) {
                    LOGE("Failed to write %s\n", path);
                    goto error;
                }
            }
        }
    }

    dir_delete(prefix_local_update_path);

    return 0;

error:
    dir_delete(prefix_local_update_path);

    return -1;
}

static void *main_task(void* param) {
    struct ota_manager* this = (struct ota_manager*) param;

    int error = 0;

    this->gui->show_image(this->gui, prefix_image_logo_path);

    cold_boot("/sys/block");
    cold_boot("/sys/class/net");

    mount_all(this);

    LOGI("Try update from storage\n");

    /*
     * Instance update file
     */
    this->uf = _new(struct update_file, update_file);

    error = update_from_storage(this);
    if (error < 0) {
        _delete(this->uf);

        /*
         * Instance update file
         */
        this->uf = _new(struct update_file, update_file);

        LOGI("Try update from network\n");
        error = update_from_network(this);
    }

    umount_all(this);

    _delete(this->uf);

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
     * Instance gui
     */
    this->gui = _new(struct gui, gui);
    this->gui->init(this->gui);

    /*
     * Instance net interface
     */
    this->ni = (struct net_interface*) calloc(1, sizeof(struct net_interface));
    this->ni->construct = construct_net_interface;
    this->ni->destruct = destruct_net_interface;
    this->ni->construct(this->ni, NULL);
    this->ni->init_socket(this->ni);

    /*
     * Instance block manager
     */
    this->mtd_bm = (struct block_manager*) calloc(1, sizeof(struct block_manager));
    this->mtd_bm->construct = construct_block_manager;
    this->mtd_bm->destruct = destruct_block_manager;
    this->mtd_bm->construct(this->mtd_bm, "mtdblock", bm_event_listener,
            (void *)this);
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
    _delete(this->mm);

    this->cf = NULL;
    this->start = NULL;
    this->stop = NULL;
    this->load_configure = NULL;

    /*
     * Destruct netlink_handler
     */
    this->nh->deconstruct(this->nh);
    free(this->nh);
    this->nh = NULL;

    /*
     * Destruct net_interface
     */
    this->ni->destruct(this->ni);
    free(this->ni);
    this->ni = NULL;

    /*
     * Destruct graphics drawer
     */
    this->gui->deinit(this->gui);
    _delete(this->gui);
    this->gui = NULL;

    /*
     * Destruct block manager
     */
    this->mtd_bm->destruct(this->mtd_bm);
    free(this->mtd_bm);
    this->mtd_bm = NULL;
}
