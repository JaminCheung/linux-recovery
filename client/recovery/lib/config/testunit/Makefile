TOPDIR ?= ../../..
#CROSS_COMPILE ?=

include ../../../config.mk

TESTUNIT := test_libconfig
TESTUNIT_OBJS := main.o                                                        \
          $(TOPDIR)/lib/config/libconfig-1.5/lib/scanner.o                     \
          $(TOPDIR)/lib/config/libconfig-1.5/lib/scanctx.o                     \
          $(TOPDIR)/lib/config/libconfig-1.5/lib/strbuf.o                      \
          $(TOPDIR)/lib/config/libconfig-1.5/lib/libconfig.o                   \
          $(TOPDIR)/lib/config/libconfig-1.5/lib/grammar.o

.PHONY : all clean

all: $(TESTUNIT)

$(TESTUNIT): $(TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(TESTUNIT_OBJS)
