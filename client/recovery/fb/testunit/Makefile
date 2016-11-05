TOPDIR ?= ../..
#CROSS_COMPILE ?=

include ../../config.mk

TESTUNIT := test_fb_manager
TESTUNIT_OBJS := main.o                                                        \
          $(TOPDIR)/utils/assert.o                                             \
          $(TOPDIR)/fb/fb_manager.o

.PHONY : all clean

all: $(TESTUNIT)

$(TESTUNIT): $(TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TESTUNIT_OBJS)
