TOPDIR ?= ../..
#CROSS_COMPILE ?=

include ../../config.mk

TESTUNIT := test_png_decoder
TESTUNIT_OBJS := test_png_decoder.o                                            \
          $(TOPDIR)/lib/png/libpng-1.6.26/png.o                                \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngerror.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngget.o                             \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngmem.o                             \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngpread.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngread.o                            \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngrio.o                             \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngrtran.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngrutil.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngset.o                             \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngtrans.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngwio.o                             \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngwrite.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngwtran.o                           \
          $(TOPDIR)/lib/png/libpng-1.6.26/pngwutil.o                           \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/adler32.o                              \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/crc32.o                                \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/deflate.o                              \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/infback.o                              \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/inffast.o                              \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/inflate.o                              \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/inftrees.o                             \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/trees.o                                \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/zutil.o                                \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/compress.o                             \
          $(TOPDIR)/lib/zlib/zlib-1.2.8/uncompr.o                              \
          $(TOPDIR)/utils/assert.o                                             \
          $(TOPDIR)/fb/fb_manager.o

.PHONY : all clean

all: $(TESTUNIT)

$(TESTUNIT): $(TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TESTUNIT_OBJS)