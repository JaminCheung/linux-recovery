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

TARGET := linux-recovery
OUTDIR := out
CLIENT_DIR := $(OUTDIR)/client-side
SERVER_DIR := $(OUTDIR)/server-side
TOOLS_DIR := $(OUTDIR)/tools
DOC_DIR := $(OUTDIR)/document
RES_DIR := $(OUTDIR)/resource

.PHONY: all client server tools document resource

all: clean $(TARGET)

$(TARGET): client server tools document resource
	F=`basename $(shell pwd)`; \
	cd $(OUTDIR) && tar --force-local --exclude=.git -Jcvf ../`date "+$$F-%Y-%m-%d-%T.tar.xz"` .

	@echo -e "\n==================================="
	@echo -e "  $(TARGET).tar.xz is ready."
	@echo -e "===================================\n"

#
# For client side
#
client:
	@mkdir -p $(CLIENT_DIR)
	@cp -av client/root-nand.cpio $(CLIENT_DIR)
	@cp -av client/root-nor.cpio $(CLIENT_DIR)
	@cp -av client/root-mmc.cpio $(CLIENT_DIR)
	@cp -av client/root-addition.tar.xz $(CLIENT_DIR)

	@make clean -C client/recovery
	@make -C client/recovery -j4
	@cp -av client/recovery/out/recovery $(CLIENT_DIR)

	@make clean -C client/recovery
	@make -C client/recovery DEBUG=1 -j4
	@cp -av client/recovery/out/recovery $(CLIENT_DIR)/recovery-for-debug

	@echo -e "========================"
	@echo -e "  Client side is ready."
	@echo -e "========================\n"

#
# For server side
#
server:
	@mkdir -p $(SERVER_DIR)
	@cp -arv server/* $(SERVER_DIR)
	@mkdir -p $(SERVER_DIR)/image
	@echo -e "========================"
	@echo -e "  Server side is ready."
	@echo -e "========================\n"

#
#
# For tools
#
tools:
	@mkdir -p $(TOOLS_DIR)
	@make clean -C tools/dump_publickey
	@make all -C tools/dump_publickey
	@cp -av tools/dump_publickey/out/dumpkey.jar $(TOOLS_DIR)
	@cp -arv server/otapackage/depmod/signature/makekey/* $(TOOLS_DIR)/

	@echo -e "========================"
	@echo -e "  Host tools is ready."
	@echo -e "========================\n"

#
# For document
#
document:
	@mkdir -p $(DOC_DIR)
	@cp -av document/linux\ recovery设计说明.pdf $(DOC_DIR)
	@cp -av document/linux\ recovery部署说明.pdf $(DOC_DIR)
	@echo -e "========================"
	@echo -e "  Document is ready."
	@echo -e "========================\n"

#
# For resource
#
resource:
	@mkdir -p $(RES_DIR)
	@cp -av resource/* $(RES_DIR)
	@echo -e "========================"
	@echo -e "  Resource is ready."
	@echo -e "========================\n"

clean:
	@rm -rf $(OUTDIR) $(TARGET)*.tar.xz
