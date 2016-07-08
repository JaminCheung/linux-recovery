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

#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

struct eeprom_manager {
    void (*construct)(struct eeprom_manager* this);
    void (*destruct)(struct eeprom_manager* this);
    int (*write)(struct eeprom_manager* this, unsigned char* buf, int addr, int count);
    int (*read)(struct eeprom_manager* this, unsigned char* buf, int addr, int count);
    int fd;
    unsigned long funcs;
};

void construct_eeprom_manager(struct eeprom_manager* this);
void destruct_eeprom_manager(struct eeprom_manager* this);

#endif /* EEPROM_MANAGER_H */
