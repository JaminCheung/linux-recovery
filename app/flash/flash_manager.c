/*
 *  Copyright (C) 2016, Zhang YanMing <jamincheung@126.com>
 *
 *  Linux recovery updater
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/log.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/libcrc32.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <flash/flash_manager.h>

#define LOG_TAG "recovery--->flash_manager"
#define MTD_PART_HEAD "/dev/mtd"

#if 0
#define DEBUG_FLASH_MANAGER
#define DEBUG_FLASH_NOR
#endif

#define YAFFS2_TAG_SIZE 28

enum{
	YAFFS2 = 210,
	UBIFS,
	JFFS2,
	CRAMFS,
};

struct erase_params {
	char *partition;		/* mtd char device name, /dev/mtdN */
	unsigned long mtdoffset;/* start address in one partition, unit: bytes*/
	unsigned long size;		/* erase size, unit: bytes, value 0 is represent for erase all */
	int jffs2;				/* format for jffs2 usage, default is 0 */
	int noskipbad;			/* do not skip bad blocks, default is 0 */
	int unlock;				/* unlock sectors before erasing, default is 0 */
};

struct flashwrite_params {
	char *partition;		/* mtd char device name, /dev/mtdN */
	char *image;			/* file name to be writen */
	unsigned long mtdoffset;/* start address in one partition, unit: bytes*/
	unsigned long inputskip;/* Skip bytes form the head of the input file */
	unsigned long inputsize;/* Only read fix bytes of the input file */

	int writeoob;			/* whether Input contains oob data or not, default is 0*/
	int onlyoob;			/* input contains oob data and only write the oob part, default is 0 */
	int markbad;			/* mark blocks bad if write fails, default is 0 */
	int noecc;				/* write without ecc, default is 0 */
	int autoplace;			/* use auto OOB layout, default is 0 */
	int noskipbad;			/* write without bad block skipping, default is 0 */
	int pad;				/* pad writes to page size, default is 0 */
	int blockalign; 		/* set multiple of eraseblocks to align to, default is 1, 1|2|4 is optional*/
};

struct filesystem_part {
	char *part_name;
	char *fstype;
};

#ifndef DEBUG_FLASH_MANAGER
static struct filesystem_part filesystem_partition_name[] = {
	{"rootfs", CONFIG_ROOTFS_TYPE},
	{"data", CONFIG_USERFS_TYPE},
};
#else
#ifndef DEBUG_FLASH_NOR
static struct filesystem_part filesystem_partition_name[] = {
    {"rootfs", CONFIG_ROOTFS_TYPE},
    {"data", "jffs2"},
    {"recovery", "cramfs"}
};
#else
static struct filesystem_part filesystem_partition_name[] = {
    {"userfs", "cramfs"},
    {"usrdata", "jffs2"},
};
#endif
#endif

int target_endian = __BYTE_ORDER;
static int quiet;
static struct erase_params erase_params;
static struct flashwrite_params flashwrite_params;
static struct jffs2_unknown_node cleanmarker;
static char partition_name[64];
static char mtd_part_name[64];
struct flash_event process_info;

static int mtd_type_is_nand_user(const struct mtd_dev_info *mtd)
{
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

static int mtd_type_is_nor_user(const struct mtd_dev_info *mtd)
{
    return mtd->type == MTD_NORFLASH;
}

int ftype_convert(char *name)
{
	if (!strcmp(name, "yaffs2"))
		return YAFFS2;
	else if (!strcmp(name, "ubifs"))
		return UBIFS;
	else if (!strcmp(name, "jffs2"))
		return JFFS2;
	else if (!strcmp(name, "cramfs"))
	    return CRAMFS;

	return 0;
}

static int get_fs_type(char *pname)
{
    int i;
    int listsize = sizeof(filesystem_partition_name)/sizeof(filesystem_partition_name[0]);
    int ftype;
    for (i = 0; i < listsize; i++){
        if (!strcmp(filesystem_partition_name[i].part_name, pname)){
            ftype = ftype_convert(filesystem_partition_name[i].fstype);
            break;
        }
    }
    if (i == listsize)
        ftype = 0;

    return ftype;
}

static int init_libmtd(struct flash_manager* this) {
    int retval = 0;
    int i;

    this->mtd_desc = libmtd_open();
    if (!this->mtd_desc) {
        LOGE("Failed to open libmtd");
        return -1;
    }

    retval = mtd_get_info(this->mtd_desc, &this->mtd_info);
    if (retval < 0) {
        LOGE("Failed to get mtd info.");
        return -1;
    }

    this->mtd_dev_info = (struct mtd_dev_info*) malloc(
            sizeof(struct mtd_dev_info) * this->mtd_info.mtd_dev_cnt);
    memset(this->mtd_dev_info, 0,
            sizeof(struct mtd_dev_info) * this->mtd_info.mtd_dev_cnt);
    for (i = 0; i < this->mtd_info.mtd_dev_cnt; i++) {
        retval = mtd_get_dev_info1(this->mtd_desc, i, &this->mtd_dev_info[i]);
        if (retval < 0)
            continue;
    }

#ifdef DEBUG_FLASH_MANAGER
#ifndef DEBUG_FLASH_NOR
    retval = this->partition_erase(this, "rootfs");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "rootfs", "system.yaffs");
    LOGI("retval = %d", retval);

    retval = this->partition_erase(this, "kernel");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "kernel", "uImage.gz");
    LOGI("retval = %d", retval);

    retval = this->partition_erase(this, "data");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "data", "userdata_test");
    LOGI("retval = %d", retval);

    retval = this->partition_erase(this, "recovery");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "recovery", "cramfs_test");
    LOGI("retval = %d", retval);
    exit(0);
#else
    retval = this->partition_erase(this, "kernel");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "kernel", "zImage");
    LOGI("retval = %d", retval);

    retval = this->partition_erase(this, "userfs");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "userfs", "updater.cramfs");
    LOGI("retval = %d", retval);

    retval = this->partition_erase(this, "usrdata");
    LOGI("retval = %d", retval);
    retval = this->partition_write(this, "usrdata", "usrdata.jffs2");
    LOGI("retval = %d", retval);
    exit(0);
#endif
#endif
    return 0;
}

static void set_process_info(struct flash_manager* this,
        struct flash_event *info, int type, int progress, char *name) {
	info->type = type;
	info->progress = progress;
	info->name = name;

	if (this->listener)
	    this->listener(this->param, info);
}

static int close_libmtd(struct flash_manager* this) {
    if (this->mtd_desc)
        libmtd_close(this->mtd_desc);

    if (this->mtd_dev_info)
        free(this->mtd_dev_info);

    return 0;
}

static struct mtd_dev_info* get_mtd_dev_info_by_name(struct flash_manager* this, const char* params) {
    int i;

    for (i = 0; i < this->mtd_info.mtd_dev_cnt; i++) {
        if (!strcmp(params, this->mtd_dev_info[i].name))
            return &this->mtd_dev_info[i];
    }

    return NULL;
}

static int flash_erase_params_judge(struct flash_manager* this, struct erase_params *params)
{
	if (params->partition == NULL)
		return -1;
	return 0;
}

static void erase_show_progress(struct flash_manager* this, struct mtd_dev_info *mtd,
		off_t start, int eb, int eb_start, int eb_cnt)
{
	if (!quiet)
		LOGI("Erasing %d Kibyte @ %"PRIxoff_t" -- %2i %% complete ",
				mtd->eb_size / 1024, start, ((eb - eb_start) * 100) / eb_cnt);

	set_process_info(this, &process_info, ACTION_ERASE,
	        ((eb - eb_start) * 100) / eb_cnt, partition_name);
}

static int flash_erase(struct flash_manager* this, struct erase_params *params) {
	struct mtd_dev_info *mtd;
	unsigned long start;
	unsigned int eb, eb_start, eb_cnt;
	int isNAND;
	int fd, clmpos = 0, clmlen = 8;
	unsigned long offset = 0;
	int retval = -1;

	if (!this || !params->partition){
		LOGE("Parameter errors");
		return -1;
	}

	if (flash_erase_params_judge(this, params) < 0){
		LOGE("Parameter judge errors");
		return -1;
	}

	if ((fd = open(params->partition, O_RDWR)) < 0){
		LOGE("Can't open mtd device %s: %s", params->partition, strerror(errno));
		retval = fd;
		goto closeall;
	}

	mtd = get_mtd_dev_info_by_name(this, partition_name);
	if (mtd == NULL){
		LOGE("Can't get mtd device on %s", partition_name);
		goto closeall;
	}

	if (params->jffs2 && mtd->type == MTD_MLCNANDFLASH){
		LOGE("JFFS2 cannot support MLC NAND");
		goto closeall;
	}

	start = params->mtdoffset;
	eb_cnt = (params->size + mtd->eb_size - 1) / mtd->eb_size;
	eb_start = start / mtd->eb_size;

	isNAND = mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;

	if (params->jffs2) {
		cleanmarker.magic = cpu_to_je16 (JFFS2_MAGIC_BITMASK);
		cleanmarker.nodetype = cpu_to_je16 (JFFS2_NODETYPE_CLEANMARKER);
		if (!isNAND)
			cleanmarker.totlen = cpu_to_je32(sizeof(cleanmarker));
		else {
			struct nand_oobinfo oobinfo;

			if (ioctl(fd, MEMGETOOBSEL, &oobinfo) != 0){
				LOGE("%s: unable to get NAND oobinfo", params->partition);
				goto closeall;
			}

			/* Check for autoplacement */
			if (oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
				/* Get the position of the free bytes */
				if (!oobinfo.oobfree[0][1]){
					LOGE(" Eeep. Autoplacement selected and no empty space in oob");
					goto closeall;
				}
				clmpos = oobinfo.oobfree[0][0];
				clmlen = oobinfo.oobfree[0][1];
				if (clmlen > 8)
					clmlen = 8;
			} else {
				/* Legacy mode */
				switch (mtd->oob_size) {
					case 8:
						clmpos = 6;
						clmlen = 2;
						break;
					case 16:
						clmpos = 8;
						clmlen = 8;
						break;
					case 64:
						clmpos = 16;
						clmlen = 8;
						break;
				}
			}
			cleanmarker.totlen = cpu_to_je32(8);
		}
		cleanmarker.hdr_crc = cpu_to_je32(crc32(0, &cleanmarker, sizeof(cleanmarker) - 4));
	}
	/*
	 * Now do the actual erasing of the MTD device
	 */
	if (eb_cnt == 0)
		eb_cnt = (mtd->size / mtd->eb_size) - eb_start;

	for (eb = eb_start; eb < eb_start + eb_cnt; eb++) {
		offset = (unsigned long)eb * mtd->eb_size;

		if (!params->noskipbad){
			int ret = mtd_is_bad(mtd, fd, eb);
			if (ret > 0) {
				if (!quiet){
					LOGI("Skipping bad block at %x", (unsigned int)offset);
					continue;
				}
			}else if (ret < 0) {
				if (errno == EOPNOTSUPP) {
					params->noskipbad = 1;
					if (isNAND){
						LOGE("Bad block check not available on %s", params->partition);
						goto closeall;
					}
				} else{
					LOGE("MTD get bad block failed on %s", params->partition);
					goto closeall;
				}
			}
		}
		erase_show_progress(this, mtd, offset, eb, eb_start, eb_cnt);

		if (params->unlock) {
			if (mtd_unlock(mtd, fd, eb) != 0) {
				LOGE("%s: MTD unlock failure", params->partition);
				continue;
			}
		}
		if (mtd_erase(this->mtd_desc, mtd, fd, eb) != 0) {
			LOGE("%s: MTD Erase failure", params->partition);
			continue;
		}
		/* format for JFFS2 ? */
		if (!params->jffs2)
			continue;

		/* write cleanmarker */
		if (isNAND) {
			if (mtd_write_oob(this->mtd_desc, mtd, fd, (uint64_t)offset + clmpos, clmlen, &cleanmarker) != 0) {
				LOGE("%s: MTD writeoob failure", params->partition);
				continue;
			}
		} else {
			if (pwrite(fd, &cleanmarker, sizeof(cleanmarker), (loff_t)offset) != sizeof(cleanmarker)) {
				LOGE("%s: MTD write failure", params->partition);
				continue;
			}
		}
		if (!quiet){
			LOGI(" Cleanmarker written at %x", (unsigned int)offset);
		}
	}
	erase_show_progress(this, mtd, offset, eb, eb_start, eb_cnt);

	retval = 0;
closeall:
	if (fd)
		close(fd);
	return retval;
}

static int flash_write_params_judge(struct flash_manager* this, struct flashwrite_params *params)
{
	if ((params->partition == NULL)
		|| (params->image == NULL))
		return -1;

	return 0;
}

static void write_show_progress(struct flash_manager* this, struct mtd_dev_info *mtd,
		long long total, long long remained)
{
	set_process_info(this, &process_info, ACTION_WRITE,
	        (total - remained) * 100 / total, partition_name);
}

static void erase_buffer(void *buffer, size_t size)
{
	const uint8_t kEraseByte = 0xff;

	if (buffer != NULL && size > 0)
		memset(buffer, kEraseByte, size);
}

/* derive from mtd-utils nandwrite */
static int flash_write(struct flash_manager* this, struct flashwrite_params* params) {

	int fd = -1;
	int ifd = -1;
	int pagelen;
	long long imglen = 0;
	long long imglen_org = 0;
	int baderaseblock = false;
	long long blockstart = -1;
	struct mtd_dev_info *mtd;
	long long offs;
	int ret;
	int failed = true;
	/* contains all the data read from the file so far for the current eraseblock */
	unsigned char *filebuf = NULL;
	size_t filebuf_max = 0;
	size_t filebuf_len = 0;
	/* points to the current page inside filebuf */
	unsigned char *writebuf = NULL;
	/* points to the OOB for the current page in filebuf */
	unsigned char *oobbuf = NULL;
	int ebsize_aligned;
	uint8_t write_mode;
	int oob_size_org = 0;
	const char	*standard_input = "-";
	int ftype = get_fs_type(partition_name);

	if (!this || !params){
		LOGE("Parameter errors");
		return -1;
	}

	if (flash_write_params_judge(this, params) < 0){
		LOGE("Parameter judge errors");
		return -1;
	}

	if ((fd = open(params->partition, O_RDWR)) < 0){
		LOGE("Can't open mtd device %s: %s", params->partition, strerror(errno));
		return fd;
	}
	mtd = get_mtd_dev_info_by_name(this, partition_name);
	if (mtd == NULL){
		LOGE("Can't get mtd device on %s.", partition_name);
		goto closeall;
	}

	oob_size_org = mtd->oob_size;
	/*
	 * Pretend erasesize is specified number of blocks - to match jffs2
	 *   (virtual) block size
	 * Use this value throughout unless otherwise necessary
	 */
	ebsize_aligned = mtd->eb_size * params->blockalign;

	if (params->mtdoffset & (mtd->min_io_size - 1)){
		LOGE("The start address is not page-aligned !"
			   "The pagesize of this NAND Flash is 0x%x.",
			   mtd->min_io_size);
		goto closeall;
	}
	/* Select OOB write mode */
	if (params->noecc)
		write_mode = MTD_OPS_RAW;
	else if (params->autoplace)
		write_mode = MTD_OPS_AUTO_OOB;
	else
		write_mode = MTD_OPS_PLACE_OOB;

	if (params->noecc)  {
		ret = ioctl(fd, MTDFILEMODE, MTD_FILE_MODE_RAW);
		if (ret) {
			switch (errno) {
			case ENOTTY:
				LOGE("ioctl MTDFILEMODE is missing");
				goto closeall;
			default:
				LOGE("MTDFILEMODE");
				goto closeall;
			}
		}
	}

	/* Determine if we are reading from standard input or from a file. */
	if (strcmp(params->image, standard_input) == 0)
		ifd = STDIN_FILENO;
	else
		ifd = open(params->image, O_RDONLY);
	if (ifd == -1) {
		LOGE("Can't open image %s: %s", params->image, strerror(errno));
		goto closeall;
	}

	if (ftype == YAFFS2)
		mtd->oob_size = YAFFS2_TAG_SIZE;

	pagelen = mtd->min_io_size + ((params->writeoob) ? mtd->oob_size : 0);
	if (ifd == STDIN_FILENO) {
		imglen = params->inputsize ? : pagelen;
		if (params->inputskip) {
			LOGE("seeking stdin not supported");
			goto closeall;
		}
	} else {
		if (!params->inputsize) {
			struct stat st;
			if (fstat(ifd, &st)) {
				LOGE("unable to stat input image");
				goto closeall;
			}
			imglen = st.st_size - params->inputskip;
		} else
			imglen = params->inputsize;

		if (params->inputskip && lseek(ifd, params->inputskip, SEEK_CUR) == -1) {
			LOGE("lseek input by %ld failed", params->inputskip);
			goto closeall;
		}
	}

	imglen_org = imglen;

	/* Check, if file is page-aligned */
	if (!params->pad && (imglen % pagelen) != 0) {
		LOGE("Input file is not page-aligned. Use the padding "
				 "option.");
		goto closeall;
	}

	/* Check, if length fits into device */
	if ((imglen / pagelen) * mtd->min_io_size > mtd->size - params->mtdoffset) {
		LOGE("Image %lld bytes, NAND page %d bytes, OOB area %d"
				" bytes, device size %lld bytes",
				imglen, pagelen, mtd->oob_size, mtd->size);
		LOGE("Input file does not fit into device");
		goto closeall;
	}

	/*
	 * Allocate a buffer big enough to contain all the data (OOB included)
	 * for one eraseblock. The order of operations here matters; if ebsize
	 * and pagelen are large enough, then "ebsize_aligned * pagelen" could
	 * overflow a 32-bit data type.
	 */
	filebuf_max = ebsize_aligned / mtd->min_io_size * pagelen;
	filebuf = malloc(filebuf_max);
	if (filebuf == NULL){
		LOGE("Buffer malloc error");
		goto closeall;
	}
	erase_buffer(filebuf, filebuf_max);

	/*
	 * Get data from input and write to the device while there is
	 * still input to read and we are still within the device
	 * bounds. Note that in the case of standard input, the input
	 * length is simply a quasi-boolean flag whose values are page
	 * length or zero.
	 */
	while ((imglen > 0 || writebuf < filebuf + filebuf_len)
		&& params->mtdoffset < mtd->size) {
		/*
		 * New eraseblock, check for bad block(s)
		 * Stay in the loop to be sure that, if mtdoffset changes because
		 * of a bad block, the next block that will be written to
		 * is also checked. Thus, we avoid errors if the block(s) after the
		 * skipped block(s) is also bad (number of blocks depending on
		 * the blockalign).
		 */
		while (blockstart != (params->mtdoffset & (~ebsize_aligned + 1))) {
			blockstart = params->mtdoffset & (~ebsize_aligned + 1);
			offs = blockstart;

			/*
			 * if writebuf == filebuf, we are rewinding so we must
			 * not reset the buffer but just replay it
			 */
			if (writebuf != filebuf) {
				erase_buffer(filebuf, filebuf_len);
				filebuf_len = 0;
				writebuf = filebuf;
			}

			baderaseblock = false;
			if (!quiet)
				LOGI("Writing data to block %lld at offset 0x%llx",
						 blockstart / ebsize_aligned, blockstart);

			write_show_progress(this, mtd, imglen_org, imglen);
			/* Check all the blocks in an erase block for bad blocks */
			if (params->noskipbad)
				continue;

			do {
				ret = mtd_is_bad(mtd, fd, offs / ebsize_aligned);
				if (ret < 0) {
					LOGE("%s: MTD get bad block failed", params->partition);
					goto closeall;
				} else if (ret == 1) {
					baderaseblock = true;
					if (!quiet)
						LOGW("Bad block at %llx, %u block(s) "
								"from %llx will be skipped",
								offs, params->blockalign, blockstart);
				}

				if (baderaseblock) {
					params->mtdoffset = blockstart + ebsize_aligned;

					if (params->mtdoffset > mtd->size) {
						LOGE("too many bad blocks, cannot complete request");
						goto closeall;
					}
				}

				offs +=  ebsize_aligned / params->blockalign;
			} while (offs < blockstart + ebsize_aligned);

		}

		/* Read more data from the input if there isn't enough in the buffer */
		if (writebuf + mtd->min_io_size > filebuf + filebuf_len) {
			size_t readlen = mtd->min_io_size;
			size_t alreadyread = (filebuf + filebuf_len) - writebuf;
			size_t tinycnt = alreadyread;
			ssize_t cnt = 0;

			while (tinycnt < readlen) {
				cnt = read(ifd, writebuf + tinycnt, readlen - tinycnt);
				if (cnt == 0) { /* EOF */
					break;
				} else if (cnt < 0) {
					LOGE("File I/O error on input");
					goto closeall;
				}
				tinycnt += cnt;
			}

			/* No padding needed - we are done */
			if (tinycnt == 0) {
				/*
				 * For standard input, set imglen to 0 to signal
				 * the end of the "file". For nonstandard input,
				 * leave it as-is to detect an early EOF.
				 */
				if (ifd == STDIN_FILENO)
					imglen = 0;

				break;
			}

			/* Padding */
			if (tinycnt < readlen) {
				if (!params->pad) {
					LOGE("Unexpected EOF. Expecting at least "
							"%zu more bytes. Use the padding option.",
							readlen - tinycnt);
					goto closeall;
				}
				erase_buffer(writebuf + tinycnt, readlen - tinycnt);
			}

			filebuf_len += readlen - alreadyread;
			if (ifd != STDIN_FILENO) {
				imglen -= tinycnt - alreadyread;
			} else if (cnt == 0) {
				/* No more bytes - we are done after writing the remaining bytes */
				imglen = 0;
			}
		}

		if (params->writeoob) {
			oobbuf = writebuf + mtd->min_io_size;

			/* Read more data for the OOB from the input if there isn't enough in the buffer */
			if (oobbuf + mtd->oob_size > filebuf + filebuf_len) {
				size_t readlen = mtd->oob_size;
				size_t alreadyread = (filebuf + filebuf_len) - oobbuf;
				size_t tinycnt = alreadyread;
				ssize_t cnt = 0;

				while (tinycnt < readlen) {
					cnt = read(ifd, oobbuf + tinycnt, readlen - tinycnt);
					if (cnt == 0) { /* EOF */
						break;
					} else if (cnt < 0) {
						LOGE("File I/O error on input");
						goto closeall;
					}
					tinycnt += cnt;
				}

				if (tinycnt < readlen) {
					LOGE("Unexpected EOF. Expecting at least "
							"%zu more bytes for OOB", readlen - tinycnt);
					goto closeall;
				}

				filebuf_len += readlen - alreadyread;
				if (ifd != STDIN_FILENO) {
					imglen -= tinycnt - alreadyread;
				} else if (cnt == 0) {
					/* No more bytes - we are done after writing the remaining bytes */
					imglen = 0;
				}
			}
		}

		/* Write out data */
		ret = mtd_write(this->mtd_desc, mtd, fd, params->mtdoffset / mtd->eb_size,
				params->mtdoffset % mtd->eb_size,
				params->onlyoob ? NULL : writebuf,
				params->onlyoob ? 0 : mtd->min_io_size,
				params->writeoob ? oobbuf : NULL,
				params->writeoob ? mtd->oob_size : 0,
				write_mode);
		if (ret) {
			long long i;
			if (errno != EIO) {
				LOGE("%s: MTD write failure", params->partition);
				goto closeall;
			}

			/* Must rewind to blockstart if we can */
			writebuf = filebuf;

			LOGW("Erasing failed write from %#08llx to %#08llx",
				blockstart, blockstart + ebsize_aligned - 1);
			for (i = blockstart; i < blockstart + ebsize_aligned; i += mtd->eb_size) {
				if (mtd_erase(this->mtd_desc, mtd, fd, i / mtd->eb_size)) {
					int errno_tmp = errno;
					LOGE("%s: MTD Erase failure", params->partition);
					if (errno_tmp != EIO)
						goto closeall;
				}
			}

			if (params->markbad) {
				LOGW("Marking block at %08lx bad",
						params->mtdoffset & (~mtd->eb_size + 1));
				if (mtd_mark_bad(mtd, fd, params->mtdoffset / mtd->eb_size)) {
					LOGE("%s: MTD Mark bad block failure", params->partition);
					goto closeall;
				}
			}
			params->mtdoffset = blockstart + ebsize_aligned;

			continue;
		}
		params->mtdoffset += mtd->min_io_size;
		writebuf += pagelen;
	}
	write_show_progress(this, mtd, imglen_org, imglen);
	failed = false;
closeall:
	if (ifd > 0)
		close(ifd);
	if (filebuf != NULL)
		free(filebuf);
	if (fd > 0)
		close(fd);

	if (oob_size_org != mtd->oob_size)
		mtd->oob_size = oob_size_org;

	if (failed || (ifd != STDIN_FILENO && imglen > 0)
		   || (writebuf < filebuf + filebuf_len)){
		LOGE("Data was only partially written due to error");
		return -1;
	}
	/* Return happy */
	return 0;
}

static void dump_erase_params(struct erase_params *params)
{
#ifdef DEBUG_FLASH_MANAGER
    LOGD("partition=%s", params->partition);
    LOGD("mtdoffset=%ld", params->mtdoffset);
    LOGD("size=%ld", params->size);
    LOGD("jffs2=%d", params->jffs2);
    LOGD("noskipbad=%d", params->noskipbad);
    LOGD("unlock=%d", params->unlock);
#endif
}

static void dump_flashwrite_params(struct flashwrite_params *params)
{
#ifdef DEBUG_FLASH_MANAGER
    LOGD("partition=%s", params->partition);
    LOGD("image=%s", params->image);
    LOGD("mtdoffset=%ld", params->mtdoffset);
    LOGD("inputskip=%ld", params->inputskip);
    LOGD("inputsize=%ld", params->inputsize);
    LOGD("writeoob=%d", params->writeoob);
    LOGD("onlyoob=%d", params->onlyoob);
    LOGD("markbad=%d", params->markbad);
    LOGD("noecc=%d", params->noecc);
    LOGD("autoplace=%d", params->autoplace);
    LOGD("noskipbad=%d", params->noskipbad);
    LOGD("pad=%d", params->pad);
    LOGD("blockalign=%d", params->blockalign);
#endif
}

static int set_erase_optional_params(struct erase_params *params, int jffs2, int noskipbad, int unlock)
{
	params->jffs2 = jffs2;
	params->noskipbad = noskipbad;
	params->unlock = unlock;
	return 0;
}

static int set_erase_fixed_params(struct erase_params *params, char *mtd_part,
		unsigned long offset, unsigned long size)
{
	params->partition = mtd_part;
	params->mtdoffset = offset;
	params->size = size;
	return 0;
}

static int set_write_optional_params(struct flashwrite_params *params,
									int inputskip, int inputsize, int writeoob,
									int onlyoob, int markbad, int noecc, int autoplace,
									int noskipbad, int pad, int blockalign)
{
	params->inputskip = inputskip;
	params->inputsize = inputsize;
	params->writeoob = writeoob;
	params->onlyoob = onlyoob;
	params->markbad = markbad;
	params->noecc = noecc;
	params->autoplace = autoplace;
	params->noskipbad = noskipbad;
	params->pad = pad;
	params->blockalign = blockalign;
	return 0;
}

static int set_write_fixed_params(struct flashwrite_params *params,
		char *mtd_part, unsigned long offset,
		char *image)
{
	params->partition = mtd_part;
	params->mtdoffset = offset;
	params->image = image;
	return 0;
}

static int set_erase_normal_params(struct flash_manager* this, char *mtd_part)
{
    int ftype = get_fs_type(partition_name);
    struct mtd_dev_info* mtd_dev_info =
                            get_mtd_dev_info_by_name(this, partition_name);

    int jffs2 = 0, noskipbad = 0, unlock = 0;

    set_erase_fixed_params(&erase_params, mtd_part, 0, 0);

	if (mtd_type_is_nor_user(mtd_dev_info))
	    noskipbad = 1;

	if (ftype == JFFS2)
	    jffs2 = 1;

	set_erase_optional_params(&erase_params, jffs2, noskipbad, unlock);
	return 0;
}

static int set_write_normal_params(struct flash_manager* this, char *mtd_part, unsigned long offset,
		char *image)
{
    struct mtd_dev_info* mtd_dev_info =
                            get_mtd_dev_info_by_name(this, partition_name);

    set_write_fixed_params(&flashwrite_params, mtd_part, offset, image);
    if (mtd_type_is_nand_user(mtd_dev_info)){
        set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
    }
    else if (mtd_type_is_nor_user(mtd_dev_info)){
        set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
    }
    return 0;
}

static int set_write_filesystem_params(struct flash_manager* this, char *mtd_part, unsigned long offset,
		char *image, int ftype)
{
    struct mtd_dev_info* mtd_dev_info =
                        get_mtd_dev_info_by_name(this, partition_name);

    set_write_fixed_params(&flashwrite_params, mtd_part, offset, image);

    if (ftype == YAFFS2){
        if (mtd_type_is_nand_user(mtd_dev_info))
            set_write_optional_params(&flashwrite_params, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1);
    }else if (ftype == JFFS2){
        if (mtd_type_is_nand_user(mtd_dev_info))
            set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
        else if (mtd_type_is_nor_user(mtd_dev_info))
            set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
    }else if (ftype == CRAMFS){
        if (mtd_type_is_nand_user(mtd_dev_info))
            set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
        else if (mtd_type_is_nor_user(mtd_dev_info))
            set_write_optional_params(&flashwrite_params, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1);
    }else if (ftype  == UBIFS){
    }
    return 0;
}

static int set_write_params(struct flash_manager* this, char *mtd_part,
        unsigned long offset, char *image)
{
    int ftype = get_fs_type(partition_name);

    if (ftype == YAFFS2
        || ftype == UBIFS
        || ftype == JFFS2
        || ftype == CRAMFS){
        set_write_filesystem_params(this, mtd_part, 0, (char*)image, ftype);
    }else
        set_write_normal_params(this, mtd_part, 0, (char*)image);

    return 0;
}

static int flash_random_write(struct flash_manager* this, char* buf,
        unsigned int offset, unsigned int count)
{
	return 0;
}

static int flash_random_read(struct flash_manager* this, char* buf,
        unsigned int offset, unsigned int count) {
    return 0;
}

static int flash_random_erase(struct flash_manager* this,
        unsigned int start_blk, int count) {
	return 0;
}

static int mtd_name_convert(struct flash_manager* this, const char* partition)
{
	struct mtd_dev_info* mtd;
	if (!this || !partition)
		return -1;
#ifdef	DEBUG_FLASH_MANAGER
	LOGI("get fstype %d", get_fs_type((char*)partition));
#endif
	strcpy(partition_name, partition);
	mtd = get_mtd_dev_info_by_name(this, partition);
	if (!mtd)
		return -1;
	sprintf(mtd_part_name, "%s%d", MTD_PART_HEAD, mtd->mtd_num);
	return 0;
}

static int flash_partition_erase(struct flash_manager* this, const char* partition)
{
	mtd_name_convert(this, partition);
	set_erase_normal_params(this, (char*)mtd_part_name);

	dump_erase_params(&erase_params);
	return flash_erase(this, &erase_params);
}

static int flash_partition_write(struct flash_manager* this, const char* partition,
        const char* image) {

    mtd_name_convert(this, partition);
    set_write_params(this, mtd_part_name, 0, (char*)image);

    dump_flashwrite_params(&flashwrite_params);
    return flash_write(this, &flashwrite_params);
}

static int flash_partition_read(struct flash_manager* this, const char* params,
        const char* image_name) {
    return 0;
}

void construct_flash_manager(struct flash_manager* this,
        flash_event_listener_t listener, void* param) {
    this->init_libmtd = init_libmtd;
    this->close_libmtd = close_libmtd;

    this->partition_write = flash_partition_write;
    this->partition_read = flash_partition_read;
    this->partition_erase = flash_partition_erase;

    this->random_write = flash_random_write;
    this->random_read = flash_random_read;
    this->random_erase = flash_random_erase;

    this->get_mtd_dev_info_by_name = get_mtd_dev_info_by_name;
    this->mtd_desc = NULL;
    this->mtd_dev_info = NULL;

    this->listener = listener;
    this->param = param;
}

void destruct_flash_manager(struct flash_manager* this) {
    close_libmtd(this);

    this->init_libmtd = NULL;
    this->close_libmtd = NULL;
    this->get_mtd_dev_info_by_name = NULL;
    this->partition_write = NULL;
    this->partition_read = NULL;
    this->partition_erase = NULL;
    this->random_write = NULL;
    this->random_read = NULL;
    this->random_erase = NULL;
    this->mtd_desc = NULL;
    this->mtd_dev_info = NULL;
    this->listener = NULL;
    this->param = NULL;
}
