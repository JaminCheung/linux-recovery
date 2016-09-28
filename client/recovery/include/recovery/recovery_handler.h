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

#ifndef RECOVERY_HANDLER_H
#define RECOVERY_HANDLER_H

#define FS_UNKNOWN  0x0
#define FS_FAT      0x01
#define FS_YAFFS   0x02

struct recovery_handler {
    void (*construct)(struct recovery_handler* this);
    void (*destruct)(struct recovery_handler* this);
    int (*start)(struct recovery_handler* this);
    int (*stop)(struct recovery_handler* this);
    struct netlink_handler* (*get_hotplug_handler)(struct recovery_handler* this);
    struct netlink_handler* nh;
    struct net_interface* ni;
#if (defined CONFIG_BOARD_HAS_EEPROM) && (CONFIG_BOARD_HAS_EEPROM == true)
    struct eeprom_manager* em;
#endif
    struct signal_handler* sh;
    struct configure_file* cf;
    struct flash_manager* fm;

    bool storage_medium_insert;
    pthread_cond_t storage_medium_status_cond;
    pthread_mutex_t storage_medium_status_lock;
    const char* storage_medium_mount_point;
    const char* rootfs_mount_point;
    const char* userfs_mount_point;

    struct configure_data* cf_data;

    unsigned short major_code;
    unsigned char minor_code;
    unsigned char failure_flag;

    bool cable_plugin;
    pthread_cond_t cable_status_cond;
    pthread_mutex_t cable_status_lock;

    const char* if_name;
    const char* server_ip;

    unsigned int upgrade_bit_flag;
};

void construct_recovery_handler(struct recovery_handler* this);
void destruct_recovery_handler(struct recovery_handler* this);

#endif /* RECOVERY_HANDLER_H */
