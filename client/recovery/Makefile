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

ifneq (config.mk, $(wildcard config.mk))
$(error Could not find $(shell pwd)/config.mk !!!)
endif

include config.mk

AUTOCONF := $(INCLUDES)/autoconf.h

#
# Application Entry
#
OBJS-y += main.o

#
# OTA Manager
#
OBJS-y += ota/ota_manager.o

#
# Netlink
#
OBJS-y += netlink/netlink_manager.o                                            \
          netlink/netlink_listener.o                                           \
          netlink/netlink_handler.o                                            \
          netlink/netlink_event.o

#
# Utils
#
OBJS-y += utils/signal_handler.o                                               \
          utils/compare_string.o                                               \
          utils/assert.o                                                       \
          utils/verifier.o                                                     \
          utils/minizip.o                                                      \
          utils/file_ops.o                                                     \
          utils/common.o

#
# Block Manager
#
OBJS-y += block/block_manager.o
OBJS-y += block/blocks/mtd/mtd.o
OBJS-y += block/blocks/mmc.o
OBJS-y += block/fs/fs_manager.o
OBJS-y += block/fs/normal.o
OBJS-y += block/fs/jffs2.o
OBJS-y += block/fs/cramfs.o
OBJS-y += block/fs/ubifs.o
OBJS-y += block/fs/yaffs2.o

#
# Recovery Handler
#
OBJS-n += recovery/recovery_handler.o

#
# Net Interface
#
OBJS-y += net/net_interface.o

#
# Mount Manager
#
OBJS-y += mount/mount_manager.o

#
# EEPROM Manager
#
OBJS-n += eeprom/eeprom_manager.o

#
# Configure & Update Parser
#
OBJS-y += configure/configure_file.o                                           \
          configure/update_file.o

OBJS := $(OBJS-y)

#
# ZLib & Mini-zip Lib
#
LIBS-y += lib/zlib/zlib-1.2.8/adler32.o                                        \
          lib/zlib/zlib-1.2.8/crc32.o                                          \
          lib/zlib/zlib-1.2.8/deflate.o                                        \
          lib/zlib/zlib-1.2.8/infback.o                                        \
          lib/zlib/zlib-1.2.8/inffast.o                                        \
          lib/zlib/zlib-1.2.8/inflate.o                                        \
          lib/zlib/zlib-1.2.8/inftrees.o                                       \
          lib/zlib/zlib-1.2.8/trees.o                                          \
          lib/zlib/zlib-1.2.8/zutil.o                                          \
          lib/zlib/zlib-1.2.8/compress.o                                       \
          lib/zlib/zlib-1.2.8/uncompr.o                                        \
          lib/zip/minizip/ioapi.o                                              \
          lib/zip/minizip/unzip.o                                              \
          lib/zip/minizip/zip.o
#          lib/zlib/zlib-1.2.8/gzclose.o                                        \
          lib/zlib/zlib-1.2.8/gzlib.o                                          \
          lib/zlib/zlib-1.2.8/gzread.o                                         \
          lib/zlib/zlib-1.2.8/gzwrite.o                                        \

#
# MTD Lib
#
LIBS-y += lib/mtd/libmtd_legacy.o                                              \
          lib/mtd/libmtd.o                                                     \
          #lib/mtd/ubi/libubi.o                                                 \
          lib/mtd/ubi/libubigen.o                                              \
          lib/mtd/ubi/libscan.o

#
# XML Lib
#
LIBS-y += lib/mxml/mxml-2.10/mxml-attr.o                                       \
          lib/mxml/mxml-2.10/mxml-entity.o                                     \
          lib/mxml/mxml-2.10/mxml-file.o                                       \
          lib/mxml/mxml-2.10/mxml-get.o                                        \
          lib/mxml/mxml-2.10/mxml-index.o                                      \
          lib/mxml/mxml-2.10/mxml-node.o                                       \
          lib/mxml/mxml-2.10/mxml-search.o                                     \
          lib/mxml/mxml-2.10/mxml-set.o                                        \
          lib/mxml/mxml-2.10/mxml-private.o                                    \
          lib/mxml/mxml-2.10/mxml-string.o

#
# Config Lib
#
LIBS-y += lib/config/libconfig-1.5/lib/scanner.o                               \
          lib/config/libconfig-1.5/lib/scanctx.o                               \
          lib/config/libconfig-1.5/lib/strbuf.o                                \
          lib/config/libconfig-1.5/lib/libconfig.o                             \
          lib/config/libconfig-1.5/lib/grammar.o

#
# Mincrypt Lib (From Android 4.3)
#
LIBS-y += lib/mincrypt/rsa.o                                                   \
          lib/mincrypt/rsa_e_3.o                                               \
          lib/mincrypt/rsa_e_f4.o                                              \
          lib/mincrypt/sha.o

#
# Input Lib
#
LIBS-y +=

#
# Base64 Lib
#
LIBS-y += lib/b64/libb64-1.2/src/cencode.o                                     \
          lib/b64/libb64-1.2/src/cdecode.o

#
# INI Lib
#
LIBS-y += lib/ini/iniparser/src/dictionary.o                                   \
          lib/ini/iniparser/src/iniparser.o

#
# MD5 Lib
#
LIBS-y += lib/md5/libmd5.o

#
# I2C Lib
#
LIBS-y += lib/i2c/libsmbus.o

#
# GPIO Lib
#
LIBS-y += lib/gpio/libgpio.o

#
# CRC Lib
#
LIBS-y += lib/crc/libcrc.o

LIBS := $(LIBS-y)

#
# Targets
#
all: $(TARGET)

.PHONY : all testunit testunit_clean clean backup

#
# Test unit
#
testunit:
	make -C lib/mxml/testunit all
	make -C lib/b64/testunit all
	make -C lib/config/testunit all
	make -C lib/zlib/testunit all
	make -C lib/ini/testunit all
	make -C lib/gpio/testunit all
	make -C lib/zip/testunit all

testunit_clean:
	make -C lib/mxml/testunit clean
	make -C lib/b64/testunit clean
	make -C lib/config/testunit clean
	make -C lib/zlib/testunit clean
	make -C lib/ini/testunit clean
	make -C lib/gpio/testunit clean
	make -C lib/zip/testunit clean

$(TARGET): $(OBJS) $(LIBS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(OBJS) $(LIBS) $(LDFLAGS) $(LDLIBS)
	@$(STRIP) $(OUTDIR)/$@
	@echo -e '\n  Recovery: $(shell basename $(OUTDIR))/$@ is ready\n'

clean:
	rm -rf $(OUTDIR) $(OBJS) $(LIBS)

distclean: clean

backup: distclean
	F=`basename $(TOPDIR)` ; cd .. ; \
	tar --force-local --exclude=.git -Jcvf `date "+$$F-%Y-%m-%d-%T.tar.xz"` $$F
