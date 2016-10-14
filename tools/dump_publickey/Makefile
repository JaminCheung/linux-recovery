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

OUTDIR := out
$(shell [ -d $(OUTDIR) ] || mkdir -p $(OUTDIR))
OUTDIR := $(shell cd $(OUTDIR) && /bin/pwd)
$(if $(OUTDIR),,$(error output directory "$(OUTDIR)" does not exist))

CLASSES_DIR := classes
$(shell [ -d $(CLASSES_DIR) ] || mkdir -p $(CLASSES_DIR))
CLASSES_DIR := $(shell cd $(CLASSES_DIR) && /bin/pwd)
$(if $(CLASSES_DIR),,$(error classes directory "$(CLASSES_DIR)" does not exist))

TARGET := $(OUTDIR)/dumpkey.jar

all:
	javac DumpPublicKey.java -d $(CLASSES_DIR)
	jar cvfm $(TARGET) DumpPublicKey.mf -C $(CLASSES_DIR) .
	rm -rf $(CLASSES_DIR)

clean:
	rm -rf $(OUTDIR)