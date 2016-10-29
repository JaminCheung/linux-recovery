TOPDIR ?= ../../../../
#CROSS_COMPILE ?=

include ../../../../config.mk

TESTUNIT := test_blockmtd
TESTUNIT_OBJS := main.o                                 \
          $(TOPDIR)/block/block_manager.o		\
	  $(TOPDIR)/block/blocks/mtd/mtd.o		\
	  $(TOPDIR)/block/blocks/mtd/base.o		\
	  $(TOPDIR)/block/blocks/mmc.o		\
	$(TOPDIR)/block/fs/fs_manager.o		\
	$(TOPDIR)/block/fs/normal.o		\
	$(TOPDIR)/block/fs/jffs2.o		\
	$(TOPDIR)/block/fs/cramfs.o		\
	$(TOPDIR)/block/fs/ubifs.o		\
	$(TOPDIR)/block/fs/yaffs2.o		\
	$(TOPDIR)/utils/assert.o		\
	$(TOPDIR)/lib/mtd/libmtd_legacy.o       \
        $(TOPDIR)/lib/mtd/libmtd.o $(TOPDIR)/lib/crc/libcrc.o 

.PHONY : all clean

all: $(TESTUNIT)

$(TESTUNIT): $(TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TESTUNIT_OBJS)
