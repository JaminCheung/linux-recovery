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

#include <version.h>
#include <utils/log.h>
#include <netlink/netlink_manager.h>
#include <utils/signal_handler.h>
#include <utils/file_ops.h>
#include <utils/assert.h>
#include <utils/common.h>
#include <utils/signal_handler.h>
#include <ota/ota_manager.h>
#include <configure/configure_file.h>

#define LOG_TAG "main"

static const char* temporary_log_file = "/tmp/recovery.log";
static const char* opt_string = "vh";

static void print_version() {
    fprintf(stderr, "Linux recovery updater. Version: %s. Build: %s %s\n", VERSION,
            __DATE__, __TIME__);
}

static void print_help() {
    fprintf(stderr, "Usage: recovery [options] file\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -v\t\t\tDisplay version infomation\n");
    fprintf(stderr, "    -h\t\t\tDisplay this information\n");
}

__attribute__((__unused__)) static void redirect_stdio() {
    if (freopen(temporary_log_file, "w", stderr))
        setbuf(stderr, NULL);
    else
        LOGW("Failed to redirect stderr to file %s: %s\n", temporary_log_file,
                strerror(errno));

    if (freopen(temporary_log_file, "w", stderr))
        setbuf(stderr, NULL);
    else
        LOGW("Failed to redirect stderr to file %s: %s\n", temporary_log_file,
                strerror(errno));
}

static int init_gdata(void) {
    if (file_exist(g_data.configure_file_path) < 0) {
        LOGI("Failed to open: %s: %s\n", g_data.configure_file_path,
                strerror(errno));
        return -1;
    }

    if (file_exist(g_data.public_key_path) < 0) {
        LOGI("Failed to open: %s: %s\n", g_data.public_key_path,
                strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    int opt = 0;

    while ((opt = getopt(argc, argv, opt_string)) != -1) {
        switch (opt) {
        case 'v':
            print_version();
            return 0;

        case 'h':
        default:
            print_help();
            return 0;
        }
    }

#ifndef LOCAL_DEBUG
    //TODO
    //redirect_stdio();
#endif

    if (init_gdata() < 0)
        return -1;

    /*
     * Instance configure_file
     */
    struct configure_file* cf = _new(struct configure_file, configure_file);
    if (cf->parse(cf, g_data.configure_file_path) < 0) {
        LOGE("Failed to parse %s\n", g_data.configure_file_path);
        return -1;
    }

    if (strcmp(cf->version, VERSION)) {
        LOGE("Configure file version error: %s - %s", cf->version, VERSION);
        return -1;
    }

    /*
     * Instance signal handler
     */
    struct signal_handler *sh = _new(struct signal_handler, signal_handler);
    sh->set_signal_handler(sh, SIGINT, NULL);
    sh->set_signal_handler(sh, SIGQUIT, NULL);
    sh->set_signal_handler(sh, SIGTERM, NULL);

    /*
     * Instances netlink manager
     */
    struct netlink_manager *nm = _new(struct netlink_manager, netlink_manager);

    /*
     * Instances ota_manager
     */
    struct ota_manager *om = _new(struct ota_manager, ota_manager);
    om->load_configure(om, cf);

    nm->register_handler(nm, om->nh);

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

    while (1)
        sleep(1000);

    return 0;

error:
    return -1;
}
