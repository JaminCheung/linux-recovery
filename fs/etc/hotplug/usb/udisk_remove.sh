#!/bin/sh
MOUNT_POINT=/mnt/usbdisk

sync
umount $MOUNT_POINT
if [ $? -ne 0 ]; then
	echo "umount usb disk falied!" > /dev/console
else
	echo "umount usb disk successfully!" > /dev/console
fi
rm -rf $MOUNT_POINT

echo "remove usb disk mount point: $MOUNT_POINT." > /dev/console
