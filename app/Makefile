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

SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	else if [ -x /bin/bash ]; then echo /bin/bash; \
	else echo sh; fi; fi)

TOPDIR := $(shell pwd)

VENDOR :=

include $(TOPDIR)/common.mk

ifeq ($(VENDOR),zkteco)
include $(TOPDIR)/zkteco.mk
endif

CROSS_COMPILE := mips-linux-gnu-
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CC_VERSION := $(shell $(CC) --version | head -n 1)
HOSTOS := $(shell uname -s | tr '[:upper:]' '[:lower:]' | \
	sed -e 's/\(cygwin\).*/cygwin/')
AM_CFLAGS := -DDEBUG
CFLAGS := -O2
CFLAGS += -g
CHECKFLAGS := -Wall -Wuninitialized -Wundef -Werror
LDLIBS += -pthread

SOURCES := $(shell find $(TOPDIR) -name *.c)
TARDIR := $(TOPDIR)/install
OBJDIR := $(TOPDIR)/obj
INCLUDES := $(TOPDIR)/include

override CFLAGS := $(CHECKFLAGS) $(AM_CFLAGS) $(CFLAGS)

TARGET := recoveryd
AUTOCONF := $(INCLUDES)/autoconf.h

OBJS-y += netlink/netlink_manager.o     \
          netlink/netlink_listener.o    \
          netlink/netlink_handler.o     \
          netlink/netlink_event.o
OBJS-y += recovery/recovery_handler.o
OBJS-y += utils/configure_file.o        \
          utils/signal_handler.o        \
          utils/compare_string.o        \
          utils/assert.o		\
	  utils/common.o                \
          utils/dictionary.o
OBJS-y += net/net_interface.o
OBJS-y += flash/flash_manager.o
OBJS-y += flash/ubi/ubi.o
OBJS-y += main.o
OBJS-$(CONFIG_BOARD_HAS_EEPROM) += eeprom/eeprom_manager.o

OBJS := $(addprefix $(TOPDIR)/, $(OBJS-y))

LIBS-y += lib/libmtd_legacy.o           \
          lib/libmtd.o
LIBS-y += lib/libsmbus.o
LIBS-y += lib/libmd5.o
LIBS-y += lib/libcrc32.o
LIBS-y += lib/ubi/libubi.o              \
          lib/ubi/libubigen.o           \
          lib/ubi/libscan.o             \
          lib/ubi/libiniparser.o

LIBS := $(addprefix $(TOPDIR)/, $(LIBS-y))

all: $(TARGET)

# Attempt to create a output directory.
$(shell [ -d $(TARDIR) ] || mkdir -p $(TARDIR))

# Verify if it was successful.
TARDIR := $(shell cd $(TARDIR) && /bin/pwd)
$(if $(TARDIR),,$(error output directory "$(TARDIR)" does not exist))

.PHONY : all clean config unconfig

%.o:%.c config
	$(CC) -I$(INCLUDES) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $(TARDIR)/$@ $(OBJS) $(LIBS) $(LDFLAGS) $(LDLIBS)
	$(STRIP) $(TARDIR)/$@
	@echo -e '\n  $(shell basename $(TARDIR))/$@ is ready\n'

unconfig:
	@rm -rf $(AUTOCONF)

config: unconfig
	@echo -e "/*"  > $(AUTOCONF)
	@echo -e " *"  >> $(AUTOCONF)
	@echo -e " * Automatically generated file; DO NOT EDIT."  >> $(AUTOCONF)
	@echo -e " *"  >> $(AUTOCONF)
	@echo -e " */\n"  >> $(AUTOCONF)

ifeq ($(VENDOR),zkteco)
	@echo "#define CONFIG_VENDOR_ZKTECO" >> $(AUTOCONF)
endif

	@echo "#define CONFIG_ROOTFS_TYPE \"$(CONFIG_ROOTFS_TYPE)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_USERFS_TYPE \"$(CONFIG_USERFS_TYPE)\"" >> $(AUTOCONF)

ifdef CONFIG_SYSCONF_MEDIA
	@echo "#define CONFIG_SYSCONF_MEDIA $(CONFIG_SYSCONF_MEDIA)" >> $(AUTOCONF)
	@echo "#define CONFIG_BOOT_STATE_ADDR $(CONFIG_BOOT_STATE_ADDR)" >> $(AUTOCONF)
endif

ifeq ($(CONFIG_BOARD_HAS_EEPROM),y)
	@echo "#define CONFIG_BOARD_HAS_EEPROM true" >> $(AUTOCONF)
	@echo "#define CONFIG_EEPROM_ADDR $(CONFIG_EEPROM_ADDR)" >> $(AUTOCONF)
	@echo "#define CONFIG_EEPROM_TYPE $(CONFIG_EEPROM_TYPE)" >> $(AUTOCONF)
endif

	@echo "#define CONFIG_FLASH_CAPACITY $(CONFIG_FLASH_CAPACITY)" >> $(AUTOCONF)

ifdef CONFIG_BASE64_ENCODE_CONFIG_FILE
	@echo "#define CONFIG_BASE64_ENCODE_CONFIG_FILE $(CONFIG_BASE64_ENCODE_CONFIG_FILE)" >> $(AUTOCONF)
endif

	@echo "#define CONFIG_NET_INTERFACE_NAME \"$(CONFIG_NET_INTERFACE_NAME)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_SERVER_IP \"$(CONFIG_SERVER_IP)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_SERVER_URL \"$(CONFIG_SERVER_URL)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_CONFIGURE_FILE_PATH \"$(CONFIG_CONFIGURE_FILE_PATH)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_UDISK_MOUNT_POINT \"$(CONFIG_UDISK_MOUNT_POINT)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_ROOTFS_MOUNT_POINT \"$(CONFIG_ROOTFS_MOUNT_POINT)\"" >> $(AUTOCONF)
	@echo "#define CONFIG_USERFS_MOUNT_POINT \"$(CONFIG_USERFS_MOUNT_POINT)\"" >> $(AUTOCONF)

ifdef CONFIG_KERNEL_IMAGE_DEF_PATH
	@echo "#define CONFIG_KERNEL_IMAGE_DEF_PATH \"$(CONFIG_KERNEL_IMAGE_DEF_PATH)\"" >> $(AUTOCONF)
endif

ifdef CONFIG_ROOTFS_IMAGE_DEF_PATH
	@echo "#define CONFIG_ROOTFS_IMAGE_DEF_PATH \"$(CONFIG_ROOTFS_IMAGE_DEF_PATH)\"" >> $(AUTOCONF)
endif

ifdef CONFIG_KERNEL_PART_DEF_NAME
	@echo "#define CONFIG_KERNEL_PART_DEF_NAME \"$(CONFIG_KERNEL_PART_DEF_NAME)\"" >> $(AUTOCONF)
endif

ifdef CONFIG_ROOTFS_PART_DEF_NAME
	@echo "#define CONFIG_ROOTFS_PART_DEF_NAME \"$(CONFIG_ROOTFS_PART_DEF_NAME)\"" >> $(AUTOCONF)
endif

clean:
	@rm -rf $(TARDIR) $(OBJS) $(LIBS)
	@echo -e "  $@ is done"

distclean: clean unconfig
