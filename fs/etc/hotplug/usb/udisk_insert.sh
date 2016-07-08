#!/bin/sh
MOUNT_POINT=/mnt/usbdisk

if [ ! -f $MDEV ]; then
	if [ ! -d $MOUNT_POINT ]; then
		mkdir -p $MOUNT_POINT
	fi

	echo "create usb disk mount point: $MOUNT_POINT." > /dev/console

	mount -t vfat /dev/$MDEV $MOUNT_POINT
	if [ $? -ne 0 ]; then
		echo "mount usb disk falied!" > /dev/console

	else
		echo "mount usb disk successfully!" > /dev/console
	fi
fi

