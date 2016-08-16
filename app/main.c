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

#include <autoconf.h>
#include <utils/log.h>
#include <netlink/netlink_manager.h>
#include <utils/signal_handler.h>
#include <utils/assert.h>
#include <recovery/recovery_handler.h>

#define LOG_TAG "recovery--->main"

#if 0
static const char *TEMPORARY_LOG_FILE = "/dev/tty0";
#endif

static void print_version() {
    LOGI("Linux recovery updater. Version: 1.0-beta. Build: %s %s", __DATE__,
            __TIME__);
    LOGI(
            "Copyright (c) 2016, ZhangYanMing <jamincheung@126.com> and contributors.\n\n\n");
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

int main(int argc, char* argv[]) {
#if 0
    if (freopen(TEMPORARY_LOG_FILE, "a", stdout))
        setbuf(stdout, NULL);
    else
        LOGW("Failed to redirect stdout to file %s: %s", TEMPORARY_LOG_FILE,
                strerror(errno));

    if (freopen(TEMPORARY_LOG_FILE, "a", stderr))
        setbuf(stderr, NULL);
    else
        LOGW("Failed to redirect stderr to file %s: %s", TEMPORARY_LOG_FILE,
                strerror(errno));
#endif

    print_version();

    /*
     * instances netlink manager
     */
    struct netlink_manager *nm = (struct netlink_manager*) malloc(
            sizeof(struct netlink_manager));
    nm->construct = construct_netlink_manager;
    nm->destruct = destruct_netlink_manager;
    nm->construct(nm);

    /*
     * instances recovery_handler
     */
    struct recovery_handler *rh = (struct recovery_handler*) malloc(
            sizeof(struct recovery_handler));
    rh->construct = construct_recovery_handler;
    rh->destruct = destruct_recovery_handler;
    rh->construct(rh);

    /*
     * register netlink handler to netlink manager
     */
    nm->register_handler(nm, rh->get_hotplug_handler(rh));

    /*
     * start recovery handler
     */
    if (rh->start(rh)) {
        LOGE("Unable to start recovery_handler.");
        goto error;
    }

    /*
     * start netlink manager
     */
    if (nm->start(nm)) {
        LOGE("Unable to start netlink_manager.");
        goto error;
    }

    cold_boot("/sys/block");
    cold_boot("/sys/class/net");
    //cold_boot("/sys/class");
    //cold_boot("/sys/devices");

    while (true)
        sleep(1000);

    return 0;

    error: return -1;
}
