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

TARGET := recovery

#
# Top directory
#
TOPDIR ?= $(shell pwd)

#
# Out & Tools directory
#
OUTDIR := $(TOPDIR)/out
$(shell [ -d $(OUTDIR) ] || mkdir -p $(OUTDIR))
OUTDIR := $(shell cd $(OUTDIR) && /bin/pwd)
$(if $(OUTDIR),,$(error output directory "$(OUTDIR)" does not exist))

#
# Cross compiler
#
CROSS_COMPILE ?= mips-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
STRIP := $(CROSS_COMPILE)strip

#
# Compiler & Linker options
#
ARFLAGS := rcv
INCLUDES := -I$(TOPDIR)/include                                                \
            -I$(TOPDIR)/include/lib                                            \
            -I$(TOPDIR)/include/lib/zlib                                       \
            -I$(TOPDIR)/include/lib/zip/minizip

CFLAGS := -std=gnu11 $(INCLUDES)
CHECKFLAGS := -Wall -Wuninitialized -Wundef

ifndef DEBUG
CFLAGS += -O2
else
CFLAGS += -g -DLOCAL_DEBUG -DDEBUG
endif

#
# For Mini-zip lib crypt
#
# open to disable encryption support
#
#CFLAGS += -DNOUNCRYPT -DNOCRYPT

#
# For open large file > 2GB
#
#CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGE_FILE

LDLIBS += -pthread -lm
override CFLAGS := $(CHECKFLAGS) $(CFLAGS)

#
# Quiet compile
#
COMPILE_SRC := $(CC) $(CFLAGS) -c
LINK_OBJS   := $(CC) $(CFLAGS)
ifndef V
QUIET_AR        = @echo -e "  AR\t$@";
QUIET_CC        = @echo -e "  CC\t$@";
QUIET_LINK      = @echo -e "  LINK\t$@";
endif

%.o:%.c
	$(QUIET_CC)$(COMPILE_SRC) $< -o $@