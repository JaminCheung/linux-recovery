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
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <version.h>
#include <utils/log.h>
#include <netlink/netlink_manager.h>
#include <utils/signal_handler.h>
#include <utils/file_ops.h>
#include <utils/assert.h>
#include <utils/common.h>
#include <ota/ota_manager.h>
#include <configure/configure_file.h>

#define LOG_TAG "main"

static const char* temporary_log_file = "/tmp/recovery.log";
static const char* opt_string = "vhc:k:";

const char* public_key_path;

static void print_version() {
    fprintf(stderr, "Linux recovery updater. Version: %s. Build: %s %s\n", VERSION,
            __DATE__, __TIME__);
}

static void print_help() {
    fprintf(stderr, "Usage: recovery [options] file\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -c <configure file>\tConfigure file\n");
    fprintf(stderr, "    -k <RSA public key>\tRSA public key file\n");
    fprintf(stderr, "    -v\t\t\tDisplay version infomation\n");
    fprintf(stderr, "    -h\t\t\tDisplay this information\n");
}

static void do_cold_boot(DIR *d, int lvl) {
    struct dirent *de;
    int dfd, fd;

    dfd = dirfd(d);

    fd = openat(dfd, "uevent", O_WRONLY);
    if (fd >= 0) {
        if (write(fd, "add\n", 4) < 0)
            close(fd);
        else
            close(fd);
    }

    while ((de = readdir(d))) {
        DIR *d2;

        if (de->d_name[0] == '.')
            continue;

        if (de->d_type != DT_DIR && lvl > 0)
            continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY);
        if (fd < 0)
            continue;

        d2 = fdopendir(fd);
        if (d2 == 0)
            close(fd);
        else {
            do_cold_boot(d2, lvl + 1);
            closedir(d2);
        }
    }
}

static void cold_boot(const char *path) {
    DIR *d = opendir(path);
    if (d) {
        do_cold_boot(d, 0);
        closedir(d);
    }
}

__attribute__((__unused__)) static void redirect_stdio() {
    if (freopen(temporary_log_file, "a", stderr))
        setbuf(stderr, NULL);
    else
        LOGW("Failed to redirect stderr to file %s: %s\n", temporary_log_file,
                strerror(errno));

    if (freopen(temporary_log_file, "a", stderr))
        setbuf(stderr, NULL);
    else
        LOGW("Failed to redirect stderr to file %s: %s\n", temporary_log_file,
                strerror(errno));
}

int main(int argc, char* argv[]) {
    const char* configure_file_path = NULL;
    int opt = 0;

    if (argc < 2) {
        print_help();
        return -1;
    }

    while ((opt = getopt(argc, argv, opt_string)) != -1) {
        switch (opt) {
        case 'c':
            if (file_exist(optarg) < 0) {
                fprintf(stderr, "error: %s: %s\n", optarg, strerror(errno));
                print_help();
                return -1;
            }

            configure_file_path = optarg;
            break;

        case 'k':
            if (file_exist(optarg) < 0) {
                fprintf(stderr, "error: %s: %s\n", optarg, strerror(errno));
                print_help();
                return -1;
            }
            public_key_path = optarg;
            break;

        case 'v':
            print_version();
            return 0;

        case 'h':
        default:
            print_help();
            return 0;
        }
    }

    if (configure_file_path == NULL || public_key_path == NULL) {
        print_help();
        return -1;
    }

    /*
     * Instance configure_file
     */
    struct configure_file* cf = _new(struct configure_file, configure_file);
    if (cf->parse(cf, configure_file_path) < 0) {
        LOGE("Failed to parse %s: %s\n", configure_file_path, strerror(errno));
        return -1;
    }

    /*
     * Instances netlink manager
     */
    struct netlink_manager *nm = _new(struct netlink_manager, netlink_manager);

    /*
     * Instances ota_manager
     */
    struct ota_manager *om = _new(struct ota_manager, ota_manager);

    nm->register_handler(nm, om->get_hotplug_handler(om));

    /*
     * Start ota manager
     */
    if (om->start(om) < 0) {
        LOGE("Unable to start ota_manager\n");
        goto error;
    }

    /*
     * start netlink manager
     */
    if (nm->start(nm)) {
        LOGE("Unable to start netlink_manager\n");
        goto error;
    }

    cold_boot("/sys/block");
    cold_boot("/sys/class/net");

    while (true)
        sleep(1000);

    return 0;

    error: return -1;
}
