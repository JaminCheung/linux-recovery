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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <configure/configure_file.h>
#include <utils/file_ops.h>
#include <lib/config/libconfig.h>
#include <version.h>

#define LOG_TAG "configure_file"

static const char* prefix_application = "Application";
static const char* prefix_version = "Version";
static const char* prefix_server_setting = "Server";
static const char* prefix_server_ip = "ip";
static const char* prefix_server_url = "url";

static void dump(struct configure_file* this) {
    LOGI("=========================\n");
    LOGI("Dump configure file\n");
    LOGI("Version:    %s\n", this->version);
    LOGI("Server IP:  %s\n", this->server_ip);
    LOGI("Server URL: %s\n", this->server_url);
    LOGI("=========================\n");
}

static int parse(struct configure_file* this, const char* path) {
    assert_die_if(path == NULL, "path is NULL");

    config_t cfg;
    config_setting_t *setting;
    char *buf = NULL;
    const char* version = NULL;
    const char* server_ip = NULL;
    const char* server_url = NULL;

    if (file_exist(path) < 0)
        return -1;

    config_init(&cfg);

    if(!config_read_file(&cfg, path)) {
        LOGE("Failed to read %s: line %d - %s\n",config_error_file(&cfg),
              config_error_line(&cfg), config_error_text(&cfg));
        goto error;
    }

    if(!config_lookup_string(&cfg, prefix_version, &version)) {
        LOGE("Failed to lookup %s: line %d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        goto error;
    }
    this->version = strdup(version);

    if (strcmp(this->version, VERSION)) {
        LOGE("Configure file version error: %s - %s", this->version, VERSION);
        goto error;
    }

    asprintf(&buf, "%s.%s", prefix_application, prefix_server_setting);
    setting = config_lookup(&cfg, buf);
    if (setting == NULL) {
        LOGE("Failed to lookup %s: line %d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        goto error;
    }

    int count = config_setting_length(setting);
    for (int i = 0; i < count; i++) {
        if(!(config_setting_lookup_string(setting, prefix_server_ip, &server_ip)
             && config_setting_lookup_string(setting, prefix_server_url, &server_url)))
          continue;
    }

    if (server_ip == NULL || server_url == NULL)
        goto error;

    this->server_ip = strdup(server_ip);
    this->server_url = strdup(server_url);

    free(buf);
    buf = NULL;

    config_destroy(&cfg);

    return 0;

error:
    if (buf)
        free(buf);
    if (this->version)
        free(this->version);
    if (this->server_ip)
        free(this->server_ip);
    if (this->server_url)
        free(this->server_url);

    config_destroy(&cfg);

    return -1;
}

void construct_configure_file(struct configure_file* this) {
    this->parse = parse;
    this->dump = dump;
}

void destruct_configure_file(struct configure_file* this) {
    if (this->version)
        free(this->version);
    if (this->server_ip)
        free(this->server_ip);
    if (this->server_url)
        free(this->server_url);

    this->parse = NULL;
    this->dump = NULL;
}
