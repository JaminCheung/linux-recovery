TOPDIR ?= ../../../../
#CROSS_COMPILE ?=

include ../../../../config.mk

TESTUNIT := test_blockmtd
TESTUNIT_OBJS := main.o                                                        \
	  test_read.o							       \
	  test_sysinfo.o						       \
	  test_format.o							       \
	  test_update.o							       \
          $(TOPDIR)/block/block_manager.o                                      \
          $(TOPDIR)/block/blocks/mtd/mtd.o                                     \
          $(TOPDIR)/block/blocks/mtd/base.o                                    \
          $(TOPDIR)/block/blocks/mmc.o                                         \
          $(TOPDIR)/block/sysinfo/sysinfo_manager.o                            \
          $(TOPDIR)/block/fs/fs_manager.o                                      \
          $(TOPDIR)/block/fs/normal.o                                          \
          $(TOPDIR)/block/fs/jffs2.o                                           \
          $(TOPDIR)/block/fs/cramfs.o                                          \
          $(TOPDIR)/block/fs/ubifs.o                                           \
          $(TOPDIR)/block/fs/yaffs2.o                                          \
          $(TOPDIR)/utils/assert.o                                             \
          $(TOPDIR)/lib/mtd/libmtd_legacy.o                                    \
          $(TOPDIR)/lib/mtd/libmtd.o                                           \
          $(TOPDIR)/lib/crc/libcrc.o                                           \
          $(TOPDIR)/lib/mtd/ubi/libubi.o                                       \
          $(TOPDIR)/lib/mtd/ubi/libubigen.o                                    \
          $(TOPDIR)/lib/mtd/ubi/libscan.o                                      \
          $(TOPDIR)/utils/common.o                                             \
          $(TOPDIR)/utils/file_ops.o                                           \
          $(TOPDIR)/lib/md5/libmd5.o

.PHONY : all clean

all: $(TESTUNIT)

$(TESTUNIT): $(TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TESTUNIT_OBJS)
