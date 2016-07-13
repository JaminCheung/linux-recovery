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
# update file storage medium type
# 0: udisk
# 1: sdcard
#
CONFIG_STORAGE_MEDIUM_TYPE=1

#
# flash capacity unit MB
#
CONFIG_FLASH_CAPACITY=128

#
# rootfs & userfs file system type
#
CONFIG_ROOTFS_TYPE=yaffs2
CONFIG_USERFS_TYPE=yaffs2

#
# net interface name
#
CONFIG_NET_INTERFACE_NAME=eth0

#
# storage medium && rootfs & userfs mount point
#
CONFIG_STORAGE_MEDIUM_MOUNT_POINT=/mnt/sdcard
CONFIG_ROOTFS_MOUNT_POINT=/mnt/rootfs
CONFIG_USERFS_MOUNT_POINT=/mnt/mtdblock

#
# configure file path on udisk & server
#
CONFIG_CONFIGURE_FILE_PATH=autoupdate/autoupdate.txt

# server ip address
CONFIG_SERVER_IP=194.169.2.59

#
# server url
#
CONFIG_SERVER_URL=http://194.169.2.59:8008