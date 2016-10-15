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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <utils/assert.h>

#define LOG_TAG "file_ops"

int dir_delete(const char *path) {
    struct stat st;
    DIR *dir;
    struct dirent *de;
    int fail = 0;

    assert_die_if(path == NULL, "path is NULL");

    /* is it a file or directory? */
    if (lstat(path, &st) < 0)
        return -1;

    /* a file, so unlink it */
    if (!S_ISDIR(st.st_mode))
        return unlink(path);

    /* a directory, so open handle */
    dir = opendir(path);
    if (dir == NULL)
        return -1;

    /* recurse over components */
    errno = 0;
    while ((de = readdir(dir)) != NULL) {
        char dn[PATH_MAX];
        if (!strcmp(de->d_name, "..") || !strcmp(de->d_name, "."))
            continue;

        snprintf(dn, sizeof(dn), "%s/%s", path, de->d_name);
        if (dir_delete(dn) < 0) {
            fail = 1;
            break;
        }
        errno = 0;
    }

    /* in case readdir or unlink_recursive failed */
    if (fail || errno < 0) {
        int save = errno;
        closedir(dir);
        errno = save;
        return -1;
    }

    /* close directory handle */
    if (closedir(dir) < 0)
        return -1;

    /* delete target directory */
    return rmdir(path);
}
