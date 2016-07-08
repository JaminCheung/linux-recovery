 #
 #  Copyright (C) 2016, Zhang YanMing <jamincheung@126.com>
 #
 #  Linux recovery updater
 #
 #  This program is free software; you can redistribute it and/or modify it
 #  under  the terms of the GNU General  Public License as published by the
 #  Free Software Foundation;  either version 2 of the License, or (at your
 #  option) any later version.
 #
 #  You should have received a copy of the GNU General Public License along
 #  with this program; if not, write to the Free Software Foundation, Inc.,
 #  675 Mass Ave, Cambridge, MA 02139, USA.
 #
 #


#
# 0: eeprom
# 1: mtd device
#
CONFIG_SYSCONF_MEDIA=0

#
# boot state address on eeprom or flash
#
ifdef CONFIG_SYSCONF_MEDIA
CONFIG_BOOT_STATE_ADDR=0x3fc
endif

#
# board support eeprom
#
CONFIG_BOARD_HAS_EEPROM=y
ifeq ($(CONFIG_BOARD_HAS_EEPROM), y)
#
# 1: 8bits address
# 2: 16bits address
#
CONFIG_EEPROM_TYPE=2

#
# chip address
#
CONFIG_EEPROM_ADDR=0x51
endif

#
# enable base64 decode configure file
#
CONFIG_BASE64_ENCODE_CONFIG_FILE=true

#
# kernel & rootfs default path while kernel or rootfs fail
#
CONFIG_KERNEL_IMAGE_DEF_PATH=autoupdate/uImage
CONFIG_ROOTFS_IMAGE_DEF_PATH=autoupdate/system.yaffs

#
# kernel & rootfs default partition name while kernel or rootfs fail
#
CONFIG_KERNEL_PART_DEF_NAME=kernel
CONFIG_ROOTFS_PART_DEF_NAME=rootfs
