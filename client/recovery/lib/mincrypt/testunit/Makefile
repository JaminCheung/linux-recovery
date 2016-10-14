TOPDIR ?= ../../..
#CROSS_COMPILE ?=

include ../../../config.mk

LIB_OBJS += $(TOPDIR)/lib/mincrypt/dsa_sig.o                                   \
        $(TOPDIR)/lib/mincrypt/p256.o                                          \
        $(TOPDIR)/lib/mincrypt/p256_ec.o                                       \
        $(TOPDIR)/lib/mincrypt/p256_ecdsa.o                                    \
        $(TOPDIR)/lib/mincrypt/rsa.o                                           \
        $(TOPDIR)/lib/mincrypt/sha.o                                           \
        $(TOPDIR)/lib/mincrypt/sha256.o

RSA_TESTUNIT := test_rsa
RSA_TESTUNIT_OBJS := rsa_test.o $(LIB_OBJS)

ECDSA_TESTUNIT := test_ecdsa
ECDSA_TESTUNIT_OBJS := ecdsa_test.o $(LIB_OBJS)

.PHONY : all clean

all: $(RSA_TESTUNIT) $(ECDSA_TESTUNIT)

$(RSA_TESTUNIT): $(RSA_TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(RSA_TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

$(ECDSA_TESTUNIT): $(ECDSA_TESTUNIT_OBJS)
	$(QUIET_LINK)$(LINK_OBJS) -o $(OUTDIR)/$@ $(ECDSA_TESTUNIT_OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(RSA_TESTUNIT_OBJS) $(ECDSA_TESTUNIT_OBJS)