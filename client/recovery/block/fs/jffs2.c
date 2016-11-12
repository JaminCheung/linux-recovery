#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <utils/log.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <types.h>
#include <linux/jffs2.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <utils/assert.h>
#include <block/fs/fs_manager.h>
#include <block/fs/jffs2.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG  "fs_jffs2"

static void jffs2_dump_cleanmarker(struct jffs2_unknown_node *marker,
                                   int *pos, int *len) {
    unsigned int magic;
    unsigned int nodetype;
    unsigned int totlen;
    unsigned int hdr_crc;

    memcpy(&magic, &marker->magic, sizeof(magic));
    memcpy(&nodetype, &marker->nodetype, sizeof(nodetype));
    memcpy(&totlen, &marker->totlen, sizeof(totlen));
    memcpy(&hdr_crc, &marker->hdr_crc, sizeof(hdr_crc));
    // LOGI("clmpos = %d\n", *pos);
    // LOGI("clmlen = %d\n", *len);
    // LOGI("cleanmarker magic = 0x%x\n", magic);
    // LOGI("cleanmarker nodetype = 0x%x\n", nodetype);
    // LOGI("cleanmarker totlen = 0x%x\n", totlen);
    // LOGI("cleanmarker hdr_crc = 0x%x\n", hdr_crc);
    return;
}

int jffs2_init_cleanmarker(struct filesystem *fs,
                           struct jffs2_unknown_node *marker,
                           int *pos, int *len) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    int clmpos = 0, clmlen = 8;
    struct jffs2_unknown_node cleanmarker;
    int is_nand = mtd_type_is_nand(mtd);

    if (!strcmp(fs->name, BM_FILE_TYPE_JFFS2)) {
        cleanmarker.magic = cpu_to_je16 (JFFS2_MAGIC_BITMASK);
        cleanmarker.nodetype = cpu_to_je16 (JFFS2_NODETYPE_CLEANMARKER);
        if (!is_nand) {
            cleanmarker.totlen = cpu_to_je32(sizeof(cleanmarker));
        }
        else {
            struct nand_oobinfo oobinfo;

            if (ioctl(fd, MEMGETOOBSEL, &oobinfo) != 0) {
                LOGE("Unable to get NAND oobinfo");
                return -1;
            }

            /* Check for autoplacement */
            if (oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
                /* Get the position of the free bytes */
                if (!oobinfo.oobfree[0][1]) {
                    LOGE("Eeep. Autoplacement selected and no empty space in oob");
                    return -1;
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
        cleanmarker.hdr_crc = cpu_to_je32(local_crc32(0, &cleanmarker, sizeof(cleanmarker) - 4));
    }

    *pos = clmpos;
    *len = clmlen;
    memcpy(marker, &cleanmarker, sizeof(cleanmarker));
    jffs2_dump_cleanmarker(marker, pos, len);
    return 0;
}

int jffs2_write_cleanmarker(struct filesystem *fs,
                            int64_t offset,
                            struct jffs2_unknown_node *cleanmarker,
                            int clmpos, int clmlen) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    int is_nand = mtd_type_is_nand(mtd);
    /* write cleanmarker */
    if (is_nand) {
        if (mtd_write_oob(mtd_desc, mtd, fd, (uint64_t)offset + clmpos, clmlen, cleanmarker) != 0) {
            LOGE("MTD \"%s\" writeoob failure", mtd->name);
            return -1;
        }
    } else {
        if (pwrite(fd, cleanmarker, sizeof(*cleanmarker), (loff_t)offset) != sizeof(*cleanmarker)) {
            LOGE("MTD \"%s\" write failure", mtd->name);
            return -1;
        }
    }
    return 0;
}

static int jffs2_init(struct filesystem *fs) {
    FS_FLAG_SET(fs, PAD);
    FS_FLAG_SET(fs, MARKBAD);
    return 0;
};

static int jffs2_format(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    fs->params->length = mtd->size;
    if (mtd_basic_erase(fs) < 0) {
        LOGE("Cannot format on fs\'%s\'\n", fs->name);
        goto out;
    }
    return 0;
out:
    return -1;
}
static int64_t jffs2_erase(struct filesystem *fs) {
    return mtd_basic_erase(fs);
}

static int64_t jffs2_read(struct filesystem *fs) {
    assert_die_if(1, "%s is not served temporarily\n", __func__);
    return 0;
}

static int64_t jffs2_write(struct filesystem *fs) {
    return mtd_basic_write(fs);
}

static int64_t jffs2_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long jffs2_get_leb_size(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    return mtd->eb_size;
}
static int64_t jffs2_get_max_mapped_size_in_partition(struct filesystem *fs) {
    return mtd_block_scan(fs);
}

struct filesystem fs_jffs2 = {
    .name = BM_FILE_TYPE_JFFS2,
    .init = jffs2_init,
    .alloc_params = fs_alloc_params,
    .free_params = fs_free_params,
    .set_params = fs_set_params,
    .erase = jffs2_erase,
    .read = jffs2_read,
    .write = jffs2_write,
    .get_operate_start_address = jffs2_get_operate_start_address,
    .get_leb_size = jffs2_get_leb_size,
    .get_max_mapped_size_in_partition =
    jffs2_get_max_mapped_size_in_partition,
    .format = jffs2_format,
};
