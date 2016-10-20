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

#ifndef CONFIGURE_FILE_H
#define CONFIGURE_FILE_H

struct configure_file {
    void (*construct)(struct configure_file* this);
    void (*destruct)(struct configure_file* this);
    int (*parse)(struct configure_file* this, const char* path);
    void (*dump)(struct configure_file* this);
    char *version;
    char *server_ip;
    char *server_url;
};

void construct_configure_file(struct configure_file* this);
void destruct_configure_file(struct configure_file* this);

#endif /* CONFIGURE_FILE_H */
