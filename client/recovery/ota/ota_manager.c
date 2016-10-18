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

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/linux.h>
#include <ota/ota_manager.h>

#define LOG_TAG "ota_manager"

void construct_ota_manager(struct ota_manager* this) {
    LOGE("%s\n", __FUNCTION__);

}

void destruct_ota_manager(struct ota_manager* this) {
    LOGE("%s\n", __FUNCTION__);
}
