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

#include <autoconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <dirent.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/linux.h>
#include <utils/compare_string.h>
#include <net/net_interface.h>
#include <netlink/netlink_handler.h>
#include <netlink/netlink_event.h>
#include <utils/configure_file.h>
#include <utils/signal_handler.h>
#include <eeprom/eeprom_manager.h>
#include <block/block_manager.h>
#include <recovery/recovery_handler.h>
#include <lib/md5/libmd5.h>

#define LOG_TAG "recovery_handler"

#define STORAGE_MEDIUM_UDISK    0
#define STORAGE_MEDIUM_SDCARD   1

#define READ_DATA_SIZE  1024
#define MD5_SIZE        16
#define MD5_STR_LEN     (MD5_SIZE * 2)

#define SYSCONF_MEDIA_EEPROM    0
#define SYSCONF_MEDIA_FLASH     1

#define RECOVERY_FLAG           0x11ff
#define KERNEL_FAILURE_FLAG     'k'
#define ROOTFS_FAILURE_FLAG     'f'

#define CABLE_PLUGIN_WAIT_TIMEOUT   (5)         // 5s

#define ALARM_TIME_OUT              (20 * 60)   //20mins

#define CONFIGURE_FILE_DOWNLOAD_BIT     (1 << 0)
#define CONFIGURE_FILE_PARSE_BIT        (1 << 1)
#define BOOTLOADER_UPGRADE_BIT          (1 << 2)
#define BOOTLOADER_DOWNLOAD_BIT         (1 << 3)
#define KERNEL_UPGRADE_BIT              (1 << 4)
#define KERNEL_DOWNLOAD_BIT             (1 << 5)
#define SPLASH_UPGRADE_BIT              (1 << 6)
#define SPLASH_DOWNLOAD_BIT             (1 << 7)
#define ROOTFS_UPGRADE_BIT              (1 << 8)
#define ROOTFS_DOWNLOAD_BIT             (1 << 9)
#define USERFS_UPGRADE_BIT              (1 << 10)
#define USERFS_DOWNLOAD_BIT             (1 << 11)

static void *main_loop(void *param);

static bool is_mountpoint_mounted(const char *mp) {
    assert_die_if(mp == NULL, "mount point is NULL");

    char device[256];
    char mount_path[256];
    char rest[256];
    FILE *fp;
    char line[1024];

    if (!(fp = fopen("/proc/mounts", "r"))) {
        LOGE("Error opening /proc/mounts (%s)", strerror(errno));
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        line[strlen(line) - 1] = '\0';
        sscanf(line, "%255s %255s %255s\n", device, mount_path, rest);
        if (!strcmp(mount_path, mp)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);

    return false;
}

static bool ensure_mountpoint_unmounted(const char* mp) {
    assert_die_if(mp == NULL, "mount point is NULL");
    int timeout = 5;

    while (timeout && is_mountpoint_mounted(mp)) {
        if (umount(mp) < 0) {
            timeout--;
            sleep(1);
            continue;
        }
    }

    if (!timeout)
        return false;

    return true;
}

static void msleep(long long msec) {
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep(&ts, &ts);
    } while (err < 0 && errno == EINTR);
}

static bool check_storage_medium_capacity(const char* mount_point) {
    unsigned int free_size = 0;
    int retval = 0;
    struct statfs sb;

    retval = statfs(mount_point, &sb);
    if (retval < 0)
        return false;

    free_size = sb.f_bfree * sb.f_frsize / 1024 / 1024;

    if (free_size < CONFIG_FLASH_CAPACITY)
        return false;

    return true;
}

static void check_and_wait_storage_medium_mount(struct recovery_handler* this) {
    for (;;) {
        pthread_mutex_lock(&this->storage_medium_status_lock);
        while (!this->storage_medium_insert) {
            LOGW("Please insert storage medium.");
            pthread_cond_wait(&this->storage_medium_status_cond,
                    &this->storage_medium_status_lock);
        }
        pthread_mutex_unlock(&this->storage_medium_status_lock);

        /*
         * wait storage_medium mount
         */
        msleep(500);

        if (!is_mountpoint_mounted(this->storage_medium_mount_point)) {
            LOGW("Please mount available partition at %s.",
                    this->storage_medium_mount_point);
            continue;

        } else {
            if (!check_storage_medium_capacity(this->storage_medium_mount_point)) {
                LOGW("Please ensure storage medium capacity more than %dMB",
                        CONFIG_FLASH_CAPACITY);
                sleep(1);
                continue;
            } else
                break;
        }
    }
}

static bool check_and_wait_cable_plugin(struct recovery_handler* this) {
    struct timeval now;
    struct timespec timeout;
    int retval = 0;

    pthread_mutex_lock(&this->cable_status_lock);
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + CABLE_PLUGIN_WAIT_TIMEOUT;
    timeout.tv_nsec = now.tv_usec * 1000;
    while (!this->cable_plugin) {
        LOGW("Please plugin cable.");
        retval = pthread_cond_timedwait(&this->cable_status_cond,
                &this->cable_status_lock, &timeout);
        if (retval) {
            pthread_mutex_unlock(&this->cable_status_lock);
            return false;
        }
    }
    pthread_mutex_unlock(&this->cable_status_lock);

    return true;
}

static bool check_network_state(struct recovery_handler* this) {
    return this->ni->icmp_echo(this->ni, this->server_ip, 2000);
}

static void signal_storage_medium_status_changed(struct recovery_handler* this,
        bool status) {
    pthread_mutex_lock(&this->storage_medium_status_lock);

    this->storage_medium_insert = status;
    pthread_cond_signal(&this->storage_medium_status_cond);

    pthread_mutex_unlock(&this->storage_medium_status_lock);
}

static void signal_cable_status_changed(struct recovery_handler* this,
        bool status) {
    pthread_mutex_lock(&this->cable_status_lock);

    if (this->cable_plugin != status) {
        this->cable_plugin = status;
        pthread_cond_signal(&this->cable_status_cond);
    }

    pthread_mutex_unlock(&this->cable_status_lock);
}

static void cable_plugin_listener(void* param, bool state) {
    struct recovery_handler* this = (struct recovery_handler*) param;

    LOGD("Cable status %splungin", state ? "" : "un");
    signal_cable_status_changed(this, state);
}

#ifdef CONFIG_VENDOR_ZKTECO

static bool get_boot_state(struct recovery_handler* this) {
#if (defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_EEPROM)
#if (defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)
    unsigned char boot_state[4] = {0};
    unsigned int addr = CONFIG_BOOT_STATE_ADDR;

    this->em->read(this->em, boot_state, addr, sizeof(boot_state));

    this->major_code = (boot_state[3] << 8) | boot_state[2];
    this->minor_code = boot_state[1];
    this->failure_flag = boot_state[0];

#else
#error "Please ensure eeprom on board and check CONFIG_BOARD_HAS_EEPROM=true"
#endif //(defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)

#elif (defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_FLASH)

#error "Implement me"

#else //(defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_FLASH)

    return false;

#endif //(defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_EEPROM)

    LOGI(
            "Boot state: major code: 0x%02x minor code: 0x%01x, failure flag: \'%c\'",
            this->major_code, this->minor_code, this->failure_flag);

    return true;
}

static bool clear_boot_state(struct recovery_handler* this) {
#if (defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_EEPROM)
#if (defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)
    unsigned char write_buf[4] = {0};
    unsigned char read_buf[4] = {0};
    unsigned int addr = CONFIG_BOOT_STATE_ADDR;

    this->em->write(this->em, write_buf, addr, sizeof(write_buf));

    this->em->read(this->em, read_buf, addr, sizeof(read_buf));

    this->major_code = (read_buf[3] << 8) | read_buf[2];
    this->minor_code = read_buf[1];
    this->failure_flag = read_buf[0];

    LOGI(
            "Boot state read back: major code: 0x%02x minor code: 0x%01x, failure flag: \'%c\'",
            this->major_code, this->minor_code, this->failure_flag);

    if (memcmp(write_buf, read_buf, sizeof(write_buf))) {
        LOGE("Failed to clear boot state.");
        return false;
    }

    return true;

#else
#error "Please ensure eeprom on board and check CONFIG_BOARD_HAS_EEPROM=true"
#endif
#elif (defined CONFIG_SYSCONF_MEDIA) && (CONFIG_SYSCONF_MEDIA == SYSCONF_MEDIA_FLASH)

#error "Implement me"

#else
    return true;

#endif
}
#endif // CONFIG_VENDOR_ZKTECO

static void handle_net_event(struct netlink_handler* nh,
        struct netlink_event* event) {

    struct recovery_handler* this =
            (struct recovery_handler *) nh->get_private_data(nh);
    const char* if_name = event->find_param(event, "INTERFACE");
    const int action = event->get_action(event);

    if (action == NLACTION_ADD && !strcmp(if_name, CONFIG_NET_INTERFACE_NAME)) {
        /*
         * instance net interface
         */
        this->ni = (struct net_interface*) malloc(sizeof(struct net_interface));
        this->ni->construct = construct_net_interface;
        this->ni->destruct = destruct_net_interface;
        this->ni->construct(this->ni, this->if_name);
        this->ni->init_socket(this->ni);
        this->ni->start_cable_detector(this->ni, cable_plugin_listener, this);

    } else if (action == NLACTION_REMOVE
            && !strcmp(if_name, CONFIG_NET_INTERFACE_NAME)) {

        /*
         * destruct net interface
         */
        this->ni->destruct(this->ni);
    }
}

static void handle_block_event(struct netlink_handler* nh,
        struct netlink_event* event) {

    struct recovery_handler* this =
            (struct recovery_handler *) nh->get_private_data(nh);

    const char* dev_type = event->find_param(event, "DEVTYPE");
    const char* dev_name = event->find_param(event, "DEVNAME");
    const int action = event->get_action(event);

#if (defined CONFIG_STORAGE_MEDIUM_TYPE) && (CONFIG_STORAGE_MEDIUM_TYPE == STORAGE_MEDIUM_UDISK)
    const char* dev_name_prefix = "sd";
#elif (defined CONFIG_STORAGE_MEDIUM_TYPE) && (CONFIG_STORAGE_MEDIUM_TYPE == STORAGE_MEDIUM_SDCARD)
    const char* dev_name_prefix = "mmcblk";
#endif

    if (!strcmp(dev_type, "disk") && is_prefixed_with(dev_name, dev_name_prefix)) {
        const char* nparts_str = event->find_param(event, "NPARTS");
        int nparts = -1;

        if (nparts_str)
            nparts = atoi(event->find_param(event, "NPARTS"));

        if (action == NLACTION_ADD) {
            LOGI("storage medium \"%s\" insert, %d partitions", dev_name, nparts);
            if (nparts == 0)
                signal_storage_medium_status_changed(this, true);

        } else if (action == NLACTION_REMOVE) {
            LOGI("storage medium \"%s\" remove, %d partitions", dev_name, nparts);
            if (nparts == 0)
                signal_storage_medium_status_changed(this, false);
        }

    } else if (!strcmp(dev_type, "partition")
            && is_prefixed_with(dev_name, dev_name_prefix)) {
        const char* part_str = event->find_param(event, "PARTN");
        int part = -1;
        if (part_str)
            part = atoi(event->find_param(event, "PARTN"));

        if (action == NLACTION_ADD) {
            LOGI("storage medium partition %d found", part);
            signal_storage_medium_status_changed(this, true);

        } else if (action == NLACTION_REMOVE) {
            LOGI("storage medium partition %d remove", part);
            signal_storage_medium_status_changed(this, false);
        }
    }
}

static struct netlink_handler* get_hotplug_handler(
        struct recovery_handler* this) {
    return this->nh;
}

static void handle_event(struct netlink_handler* nh,
        struct netlink_event* event) {
    if (!strcmp(event->get_subsystem(event), "block")) {
        event->dump(event);
        handle_block_event(nh, event);

    } else if (!strcmp(event->get_subsystem(event), "net")) {
        handle_net_event(nh, event);
    }
}

static int start(struct recovery_handler* this) {
    int retval = 0;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    retval = pthread_create(&tid, &attr, main_loop, (void *) this);
    if (retval) {
        LOGE("pthread_create failed: %s", strerror(errno));
        pthread_attr_destroy(&attr);
        return -1;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

static int stop(struct recovery_handler* this) {
    return 0;
}

static void recovery_upgrade_complete(struct recovery_handler* this,
        bool status) {
    sync();

    if (status) {
#ifdef CONFIG_VENDOR_ZKTECO
        clear_boot_state(this);
#endif
        LOGI("Upgrade success, restarting system...");
    }

    reboot(RB_AUTOBOOT);

    for (;;) {
        LOGE("Should not come here...");
        sleep(1);
    }
}

static bool file_exist(const char* file_path) {
    if (access(file_path, F_OK | R_OK)) {
        LOGE("Failed to access \"%s\": %s", file_path, strerror(errno));
        return false;
    }

    return true;
}

static bool check_file_md5(struct recovery_handler* this, const char* file_path,
        const char* md5) {
    int fd = 0;
    int i;
    int retval = 0;
    unsigned char buf[READ_DATA_SIZE] = { 0 };
    unsigned char md5_value[MD5_SIZE] = { 0 };
    char md5_str[MD5_STR_LEN + 1] = { 0 };

    MD5_CTX md5_ctx;

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        LOGE("Failed to open \"%s\" for check md5: %s", file_path,
                strerror(errno));
        return false;
    }

    MD5Init(&md5_ctx);

    for (;;) {
        retval = read(fd, buf, sizeof(buf));
        if (retval < 0) {
            LOGE("Failed to read \"%s\" for check md5: %s", file_path,
                    strerror(errno));
            goto error;
        }

        MD5Update(&md5_ctx, buf, retval);

        if (retval == 0 || retval < READ_DATA_SIZE)
            break;
    }

    close(fd);

    MD5Final(&md5_ctx, md5_value);

    for (i = 0; i < MD5_SIZE; i++)
        snprintf(md5_str + i * 2, 2 + 1, "%02x", md5_value[i]);

    md5_str[MD5_STR_LEN] = '\0';

    if (!strcmp(md5_str, md5))
        return true;

    return false;

    error: close(fd);

    return false;
}

static void set_flag(struct recovery_handler* this, int flag) {
    this->upgrade_bit_flag |= flag;
}

static void clear_flag(struct recovery_handler* this) {
    this->upgrade_bit_flag = 0;
}

static bool parse_configure_file(struct recovery_handler* this,
        const char* file_path) {
    LOGI("Going to parse configure file.");

    this->cf_data = this->cf->load_configure_file(this->cf, file_path);

    if (!this->cf_data) {
        set_flag(this, CONFIGURE_FILE_PARSE_BIT);
        return false;
    }

    return true;
}

static bool upgrade_bootloader(struct recovery_handler* this, const char* path) {
    int error;

    if (!file_exist(path))
        return false;

    error = check_file_md5(this, path, this->cf_data->bootloader_md5);
    if (!error) {
        LOGE("Failed to check bootloader image md5 for %s", path);
        goto error;
    }

    char* part_name = this->cf_data->bootloader_name;

    struct mtd_dev_info* info = this->bm->get_mtd_dev_info_by_name(this->bm,
            part_name);

    if (!info) {
        LOGE("Failed to get mtd info by name: %s", part_name);
        goto error;
    }

    LOGI("Going to upgrade bootloader from: %s", path);

    error = this->bm->partition_erase(this->bm, part_name);
    if (error < 0) {
        LOGE("Failed to earse partition: %s", part_name);
        goto error;
    }

    error = this->bm->partition_write(this->bm, part_name, path);
    if (error < 0) {
        LOGE("Failed to write partition: %s", part_name);
        goto error;
    }

    return true;

    error: set_flag(this, BOOTLOADER_UPGRADE_BIT);
    return false;
}

static bool upgrade_kernel(struct recovery_handler* this, const char* path) {
    int error;
    char* part_name = NULL;

    if (!file_exist(path))
        return false;

    if (!this->cf_data) {
#ifdef CONFIG_VENDOR_ZKTECO
        /*
         * treat as kernel fail
         */
        part_name = CONFIG_KERNEL_PART_DEF_NAME;
#else
        assert_die_if(true, "configure file is null\n");

#endif
    } else {
        error = check_file_md5(this, path, this->cf_data->kernel_md5);
        if (!error) {
            LOGE("Failed to check kernel image md5 for %s", path);
            goto error;
        }

        part_name = this->cf_data->kernel_name;
    }

    struct mtd_dev_info* info = this->bm->get_mtd_dev_info_by_name(this->bm,
            part_name);

    if (!info) {
        LOGE("Failed to get mtd info by name: %s", part_name);
        goto error;
    }

    LOGI("Going to upgrade kernel from: %s", path);

    error = this->bm->partition_erase(this->bm, part_name);
    if (error < 0) {
        LOGE("Failed to earse partition: %s", part_name);
        goto error;
    }

    error = this->bm->partition_write(this->bm, part_name, path);
    if (error < 0) {
        LOGE("Failed to write partition: %s", part_name);
        goto error;
    }

    return true;

    error: set_flag(this, KERNEL_UPGRADE_BIT);
    return false;
}

static bool upgrade_splash(struct recovery_handler* this, const char* path) {
    int error;

    if (!file_exist(path))
        return false;

    error = check_file_md5(this, path, this->cf_data->splash_md5);
    if (!error) {
        LOGE("Failed to check splash image md5 for %s", path);
        goto error;
    }

    char* part_name = this->cf_data->splash_name;

    struct mtd_dev_info* info = this->bm->get_mtd_dev_info_by_name(this->bm,
            part_name);

    if (!info) {
        LOGE("Failed to get mtd info by name: %s", part_name);
        goto error;
    }

    LOGI("Going to upgrade splash from: %s", path);

    error = this->bm->partition_erase(this->bm, part_name);
    if (error < 0) {
        LOGE("Failed to earse partition: %s", part_name);
        goto error;
    }

    error = this->bm->partition_write(this->bm, part_name, path);
    if (error < 0) {
        LOGE("Failed to write partition: %s", part_name);
        goto error;
    }

    return true;

    error: set_flag(this, SPLASH_UPGRADE_BIT);
    return false;
}

static bool upgrade_rootfs(struct recovery_handler* this, const char* path,
        bool full_upgrade) {
    int error;
    char* part_name = NULL;

    if (!file_exist(path))
        return false;

    LOGI("Going to upgrade rootfs from %s", path);

    if (full_upgrade) {
        if (!ensure_mountpoint_unmounted(this->rootfs_mount_point)) {
            LOGE("Failed to umount %s: %s", this->rootfs_mount_point,
                    strerror(errno));
            goto error;
        }

        if (!this->cf_data) {
#ifdef CONFIG_VENDOR_ZKTECO
            /*
             * treat as rootfs fail
             */
            part_name = CONFIG_ROOTFS_PART_DEF_NAME;
#else
            assert_die_if(true, "configure file is null\n");
#endif
        } else {
            error = check_file_md5(this, path, this->cf_data->rootfs_md5);
            if (!error) {
                LOGE("Failed to check rootfs image md5 for %s", path);
                goto error;
            }

            part_name = this->cf_data->rootfs_name;
        }

        struct mtd_dev_info* info = this->bm->get_mtd_dev_info_by_name(this->bm,
                part_name);

        if (!info) {
            LOGE("Failed to get mtd info by name: %s", part_name);
            goto error;
        }

        error = this->bm->partition_erase(this->bm, part_name);
        if (error < 0) {
            LOGE("Failed to earse partition: %s", part_name);
            goto error;
        }

        error = this->bm->partition_write(this->bm, part_name, path);
        if (error < 0) {
            LOGE("Failed to write partition: %s", part_name);
            goto error;
        }

    } else {
        if (!is_mountpoint_mounted(this->rootfs_mount_point)) {
            LOGE("Please ensure rootfs already mounted at %s",
                    this->rootfs_mount_point);
            return false;
        }

        char cmd[512] = { 0 };
        int retval = 0;

        error = check_file_md5(this, path, this->cf_data->rootfs_md5);
        if (!error) {
            LOGE("Failed to check rootfs image md5 for %s", path);
            goto error;
        }

        sprintf(cmd, "tar -xvf %s -C %s", path, this->rootfs_mount_point);

        retval = system(cmd);
        if (retval) {
            LOGE("Failed to exec \"%s\"", cmd);
            goto error;
        }
    }

    return true;

    error: set_flag(this, ROOTFS_UPGRADE_BIT);
    return false;
}

static bool upgrade_userfs(struct recovery_handler* this, const char* path,
        bool full_upgrade) {
    int error;

    if (!file_exist(path))
        return false;

    error = check_file_md5(this, path, this->cf_data->userfs_md5);
    if (!error) {
        LOGE("Failed to check userfs image md5 for %s", path);
        goto error;
    }

    LOGI("Going to upgrade userfs from %s", path);

    if (full_upgrade) {
        if (!ensure_mountpoint_unmounted(this->userfs_mount_point)) {
            LOGE("Failed to umount %s: %s", this->userfs_mount_point,
                    strerror(errno));
            goto error;
        }

        char* part_name = this->cf_data->userfs_name;

        struct mtd_dev_info* info = this->bm->get_mtd_dev_info_by_name(this->bm,
                part_name);

        if (!info) {
            LOGE("Failed to get mtd info by name: %s", part_name);
            goto error;
        }

        error = this->bm->partition_erase(this->bm, part_name);
        if (error < 0) {
            LOGE("Failed to earse partition: %s", part_name);
            goto error;
        }

        error = this->bm->partition_write(this->bm, part_name, path);
        if (error < 0) {
            LOGE("Failed to write partition: %s", part_name);
            goto error;
        }

    } else {
        if (!is_mountpoint_mounted(this->userfs_mount_point)) {
            LOGE("Please ensure userfs already mounted at %s",
                    this->userfs_mount_point);
            return false;
        }

        char cmd[512] = { 0 };
        int retval = 0;
        sprintf(cmd, "tar -xvf %s -C %s", path, this->userfs_mount_point);

        retval = system(cmd);
        if (retval) {
            LOGE("Failed to exec \"%s\"", cmd);
            goto error;
        }
    }

    return true;

    error: set_flag(this, USERFS_UPGRADE_BIT);
    return false;
}

static void free_cached_mem() {
    int retval = 0;
    char cmd[128] = { 0 };

    sprintf(cmd, "echo 3 > /proc/sys/vm/drop_caches");
    retval = system(cmd);
    if (retval)
        LOGE("Failed to exec \"%s\"", cmd);

    sync();
}

static bool create_download_directory(struct recovery_handler* this,
        const char* file_path) {
    const char* start = file_path;
    char* pos = NULL;
    char buf[NAME_MAX] = { 0 };
    char path[PATH_MAX] = { 0 };
    struct stat sb;
    int retval = 0;
    DIR* dp;

    start += strlen(this->storage_medium_mount_point) + 1;

    pos = strchr(start, '/');
    if (!pos) {
        LOGE("%s is invalid", file_path);
        return false;
    }

    memcpy(buf, start, pos - start);

    sprintf(path, "%s/%s", this->storage_medium_mount_point, buf);

    retval = access(path, F_OK);
    if (!retval) {
        retval = stat(path, &sb);
        if (retval < 0)
            return false;

        if (!S_ISDIR(sb.st_mode))
            unlink(path);
    }

    dp = opendir(path);
    if (!dp) {
        umask(0000);
        retval = mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        if (retval < 0)
            return false;
    }

    return true;
}

static bool download_file(struct recovery_handler* this, const char* src_path,
        const char* dest_path) {
    int retval = 0;
    int timeout = 5;
    char cmd[NAME_MAX] = { 0 };

    if (!create_download_directory(this, dest_path))
        return false;

    free_cached_mem();

    sprintf(cmd, "wget -c %s -O %s", src_path, dest_path);
    retval = system(cmd);
    while (retval != 0 && timeout != 0) {
        free_cached_mem();
        sleep(2);
        retval = system(cmd);
        timeout--;
    }

    free_cached_mem();

    if (!retval)
        return true;

    return false;
}

static bool download_bootloader(struct recovery_handler* this,
        const char* src_path, const char* dest_path) {

    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, BOOTLOADER_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

static bool download_kernel(struct recovery_handler* this, const char* src_path,
        const char* dest_path) {

    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, KERNEL_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

static bool download_splash(struct recovery_handler* this, const char* src_path,
        const char* dest_path) {

    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, SPLASH_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

static bool download_rootfs(struct recovery_handler* this, const char* src_path,
        const char* dest_path) {

    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, ROOTFS_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

static bool download_userfs(struct recovery_handler* this, const char* src_path,
        const char* dest_path) {

    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, USERFS_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

static bool download_configure_file(struct recovery_handler* this,
        const char* src_path, const char* dest_path) {
    if (!download_file(this, src_path, dest_path)) {
        set_flag(this, CONFIGURE_FILE_DOWNLOAD_BIT);
        return false;
    }

    return true;
}

#ifdef CONFIG_VENDOR_ZKTECO
static bool upgrade_for_kernel_fail(struct recovery_handler* this) {
    LOGI("Going to upgrade kernel for boot kernel fail");

    char server_path[PATH_MAX] = { 0 };
    char local_path[PATH_MAX] = { 0 };

    sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
    CONFIG_KERNEL_IMAGE_DEF_PATH);

    restart: if (file_exist(local_path)) {
        /*
         * upgrade kernel from local storage
         */
        return upgrade_kernel(this, local_path);

    } else {
        /*
         * upgrade kernel from network
         */
        if (!check_and_wait_cable_plugin(this))
            goto restart;

        if (!check_network_state(this)) {
            LOGW("Network not available.");
            return false;
        }

        sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
        CONFIG_KERNEL_IMAGE_DEF_PATH);

        if (download_kernel(this, server_path, local_path))
            goto restart;
    }

    return false;
}

static bool upgrade_for_rootfs_fail(struct recovery_handler* this) {
    LOGI("Going to upgrade rootfs for mount rootfs fail");

    char server_path[PATH_MAX] = { 0 };
    char local_path[PATH_MAX] = { 0 };

    sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
    CONFIG_ROOTFS_IMAGE_DEF_PATH);

    restart: if (file_exist(local_path)) {
        /*
         * upgrade rootfs from local storage
         */
        return upgrade_rootfs(this, local_path, true);

    } else {
        /*
         * upgrade rootfs from network
         */
        if (!check_and_wait_cable_plugin(this))
            goto restart;

        if (!check_network_state(this)) {
            LOGW("Network not available.");
            return false;
        }

        sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
        CONFIG_ROOTFS_IMAGE_DEF_PATH);

        if (download_rootfs(this, server_path, local_path))
            goto restart;
    }

    return false;
}
#endif //CONFIG_VENDOR_ZKTECO

static bool do_custom_upgrade(struct recovery_handler* this) {
    char path[PATH_MAX] = { 0 };

    if (this->cf_data->bootloader_upgrade) {
        sprintf(path, "%s/%s", this->storage_medium_mount_point,
                this->cf_data->bootloader_path);
        if (!upgrade_bootloader(this, path))
            return false;

        memset(path, 0, sizeof(path));
    }

    free_cached_mem();

    if (this->cf_data->kernel_upgrade) {
        sprintf(path, "%s/%s", this->storage_medium_mount_point,
                this->cf_data->kernel_path);
        if (!upgrade_kernel(this, path))
            return false;

        memset(path, 0, sizeof(path));
    }

    free_cached_mem();

    if (this->cf_data->splash_upgrade) {
        sprintf(path, "%s/%s", this->storage_medium_mount_point,
                this->cf_data->splash_path);
        if (!upgrade_splash(this, path))
            return false;

        memset(path, 0, sizeof(path));
    }

    free_cached_mem();

    if (this->cf_data->rootfs_upgrade) {
        sprintf(path, "%s/%s", this->storage_medium_mount_point,
                this->cf_data->rootfs_path);
        if (!upgrade_rootfs(this, path, this->cf_data->rootfs_full_upgrade))
            return false;

        memset(path, 0, sizeof(path));
    }

    free_cached_mem();

    if (this->cf_data->userfs_upgrade) {
        sprintf(path, "%s/%s", this->storage_medium_mount_point,
                this->cf_data->userfs_path);
        if (!upgrade_userfs(this, path, this->cf_data->userfs_full_upgrade))
            return false;

        memset(path, 0, sizeof(path));
    }

    return true;
}

static bool upgrade_for_custom(struct recovery_handler* this) {
    LOGI("Going to upgrade for custom");

    char server_path[PATH_MAX] = { 0 };
    char local_path[PATH_MAX] = { 0 };

    sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
    CONFIG_CONFIGURE_FILE_PATH);

    if (file_exist(local_path)) {
        /*
         * parse configure file from local storage
         */
        if (!parse_configure_file(this, local_path))
            return false;

    } else {
        /*
         * parse configure file from network
         */
        if (!check_and_wait_cable_plugin(this))
            return false;

        if (!check_network_state(this)) {
            LOGW("Network not available.");
            return false;
        }

        sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
        CONFIG_CONFIGURE_FILE_PATH);

        LOGI("Going to download configure from %s", server_path);

        /*
         * download configure file to local storage
         */
        if (download_configure_file(this, server_path, local_path)) {
            if (!parse_configure_file(this, local_path))
                return false;

            memset(server_path, 0, sizeof(server_path));

        } else {
            return false;
        }

        /*
         * download bootloader to local storage
         */
        if (this->cf_data->bootloader_upgrade) {
            sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
                    this->cf_data->bootloader_path);

            LOGI("Going to download bootloader from %s", server_path);

            sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
                    this->cf_data->bootloader_path);
            if (!download_bootloader(this, server_path, local_path))
                return false;

            memset(server_path, 0, sizeof(server_path));
            memset(local_path, 0, sizeof(local_path));
        }

        /*
         * download kernel to local storage
         */
        if (this->cf_data->kernel_upgrade) {
            sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
                    this->cf_data->kernel_path);

            LOGI("Going to download kernel from %s", server_path);

            sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
                    this->cf_data->kernel_path);
            if (!download_kernel(this, server_path, local_path))
                return false;

            memset(server_path, 0, sizeof(server_path));
            memset(local_path, 0, sizeof(local_path));
        }

        /*
         * download kernel to local storage
         */
        if (this->cf_data->splash_upgrade) {
            sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
                    this->cf_data->splash_path);

            LOGI("Going to download splash from %s", server_path);

            sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
                    this->cf_data->splash_path);
            if (!download_splash(this, server_path, local_path))
                return false;

            memset(server_path, 0, sizeof(server_path));
            memset(local_path, 0, sizeof(local_path));
        }

        /*
         * download rootfs to local storage
         */
        if (this->cf_data->rootfs_upgrade) {
            sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
                    this->cf_data->rootfs_path);

            LOGI("Going to download rootfs from %s", server_path);

            sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
                    this->cf_data->rootfs_path);
            if (!download_rootfs(this, server_path, local_path))
                return false;

            memset(server_path, 0, sizeof(server_path));
            memset(local_path, 0, sizeof(local_path));
        }

        /*
         * download userfs image to local storage
         */
        if (this->cf_data->userfs_upgrade) {
            sprintf(server_path, "%s/%s", CONFIG_SERVER_URL,
                    this->cf_data->userfs_path);

            LOGI("Going to download userfs from %s", server_path);

            sprintf(local_path, "%s/%s", this->storage_medium_mount_point,
                    this->cf_data->userfs_path);
            if (!download_userfs(this, server_path, local_path))
                return false;
        }
    }

    return do_custom_upgrade(this);
}

static void check_error_reason(struct recovery_handler* this) {
    if (this->upgrade_bit_flag & CONFIGURE_FILE_DOWNLOAD_BIT) {
        LOGE("Failed to download configure file.");
    }

    if (this->upgrade_bit_flag & CONFIGURE_FILE_PARSE_BIT) {
        LOGE("Failed to parse configure file");
    }

    if (this->upgrade_bit_flag & BOOTLOADER_DOWNLOAD_BIT) {
        LOGE("Failed to download bootloader");
    }

    if (this->upgrade_bit_flag & BOOTLOADER_UPGRADE_BIT) {
        LOGE("Failed to upgrade bootloader.");
    }

    if (this->upgrade_bit_flag & KERNEL_DOWNLOAD_BIT) {
        LOGE("Failed to download kernel");
    }

    if (this->upgrade_bit_flag & KERNEL_UPGRADE_BIT) {
        LOGE("Failed to upgrade kernel");
    }

    if (this->upgrade_bit_flag & SPLASH_DOWNLOAD_BIT) {
        LOGE("Failed to download splash");
    }

    if (this->upgrade_bit_flag & SPLASH_UPGRADE_BIT) {
        LOGE("Failed to upgrade splash");
    }

    if (this->upgrade_bit_flag & ROOTFS_DOWNLOAD_BIT) {
        LOGE("Failed to download rootfs");
    }

    if (this->upgrade_bit_flag & ROOTFS_UPGRADE_BIT) {
        LOGE("Failed to upgrade rootfs");
    }

    if (this->upgrade_bit_flag & USERFS_DOWNLOAD_BIT) {
        LOGE("Failed to download userfs");
    }

    if (this->upgrade_bit_flag & USERFS_UPGRADE_BIT) {
        LOGE("Failed to upgrade userfs");
    }
}

static void *main_loop(void* param) {
    struct recovery_handler* this = (struct recovery_handler*) param;
    int error = 0;

#ifdef CONFIG_VENDOR_ZKTECO
    int retval;
    retval = get_boot_state(this);
#endif

    alarm(ALARM_TIME_OUT);

    for (;;) {
        check_and_wait_storage_medium_mount(this);

        clear_flag(this);

#ifdef CONFIG_VENDOR_ZKTECO
        if (retval) {
            switch (this->failure_flag) {
            case KERNEL_FAILURE_FLAG:
                if (!upgrade_for_kernel_fail(this))
                    error++;
                break;

            case ROOTFS_FAILURE_FLAG:
                if (!upgrade_for_rootfs_fail(this))
                    error++;
                break;

            default:
                if (!upgrade_for_custom(this))
                    error++;
                break;
            }

        }
#else // CONFIG_VENDOR_ZKTECO

        if (!upgrade_for_custom(this))
            error++;
#endif

        if (error) {
            error = 0;
            check_error_reason(this);
            sleep(1);
            continue;
        }

        if (!error)
            recovery_upgrade_complete(this, true);
    }

    return NULL;
}

static void signal_handler(int signal) {
    LOGE("Upgrade wait %ds timeout, restarting system...\n", ALARM_TIME_OUT);
    recovery_upgrade_complete(NULL, false);
}

void construct_recovery_handler(struct recovery_handler* this) {
    this->start = start;
    this->stop = stop;

    this->major_code = 0;
    this->minor_code = 0;
    this->failure_flag = 0;

    this->cable_plugin = false;

    this->upgrade_bit_flag = 0;

    this->if_name = CONFIG_NET_INTERFACE_NAME;
    this->server_ip = CONFIG_SERVER_IP;
    this->storage_medium_mount_point = CONFIG_STORAGE_MEDIUM_MOUNT_POINT;

    this->rootfs_mount_point = CONFIG_ROOTFS_MOUNT_POINT;
    this->userfs_mount_point = CONFIG_USERFS_MOUNT_POINT;

    pthread_mutex_init(&this->storage_medium_status_lock, NULL);
    pthread_cond_init(&this->storage_medium_status_cond, NULL);

    pthread_mutex_init(&this->cable_status_lock, NULL);
    pthread_cond_init(&this->cable_status_cond, NULL);

    /*
     * instances signal handler
     */
    this->sh = (struct signal_handler *) malloc(sizeof(struct signal_handler));
    this->sh->construct = construct_signal_handler;
    this->sh->destruct = destruct_signal_handler;
    this->sh->construct(this->sh);
    this->sh->init(this->sh, SIGALRM, signal_handler);

    /*
     * instance netlink handler
     */
    this->nh = (struct netlink_handler *) malloc(
            sizeof(struct netlink_handler));
    this->nh->construct = construct_netlink_handler;
    this->nh->deconstruct = destruct_netlink_handler;
    this->nh->construct(this->nh, "all sub-system", 0, handle_event, this);
    this->get_hotplug_handler = get_hotplug_handler;

    /*
     * instance configure file parser
     */
    this->cf = (struct configure_file*) malloc(sizeof(struct configure_file));
    this->cf->construct = construct_configure_file;
    this->cf->destruct = destruct_configure_file;
    this->cf->construct(this->cf);

    /*
     * instance flash manager
     */
    this->bm = (struct block_manager*) malloc(sizeof(struct block_manager));
    this->bm->construct = construct_block_manager;
    this->bm->destruct = destruct_block_manager;
    this->bm->construct(this->bm, NULL, NULL);
    this->bm->init_libmtd(this->bm);

#if (defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)
    /*
     * instance eeprom manager
     */
    this->em = (struct eeprom_manager*) malloc(sizeof(struct eeprom_manager));
    this->em->construct = construct_eeprom_manager;
    this->em->destruct = destruct_eeprom_manager;
    this->em->construct(this->em);
#endif
}

void destruct_recovery_handler(struct recovery_handler* this) {
    this->if_name = NULL;
    this->server_ip = NULL;
    this->storage_medium_mount_point = NULL;
    this->rootfs_mount_point = NULL;
    this->userfs_mount_point = NULL;
    this->cf_data = NULL;

    this->cable_plugin = false;

    this->upgrade_bit_flag = 0;

    pthread_cond_destroy(&this->storage_medium_status_cond);
    pthread_mutex_destroy(&this->storage_medium_status_lock);

    pthread_cond_destroy(&this->cable_status_cond);
    pthread_mutex_destroy(&this->cable_status_lock);

    /*
     * destruct netlink handler
     */
    this->nh->deconstruct(this->nh);

    /*
     * destruct configure file parser
     */
    this->cf->destruct(this->cf);

    /*
     * destruct flash manager
     */
    this->bm->destruct(this->bm);

#if (defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)
    /*
     * destruct eeprom manager
     */
    this->em->destruct(this->em);
#endif

}
