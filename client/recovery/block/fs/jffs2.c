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
#include <block/fs/fs_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG  "fs_jffs2"

static void jffs2_dump_cleanmaker(struct jffs2_unknown_node *maker,
        int *pos, int *len) {
    unsigned int magic = *(unsigned int *)&maker->magic;
    unsigned int nodetype = *(unsigned int *)&maker->nodetype;
    unsigned int totlen = *(unsigned int *)&maker->totlen;
    unsigned int hdr_crc = *(unsigned int *)&maker->hdr_crc;
    LOGI("clmpos = %d\n", *pos);
    LOGI("clmlen = %d\n", *len);
    LOGI("cleanmarker magic = 0x%x\n", magic);
    LOGI("cleanmarker nodetype = 0x%x\n", nodetype);
    LOGI("cleanmarker totlen = 0x%x\n", totlen);
    LOGI("cleanmarker hdr_crc = 0x%x\n", hdr_crc);
    return;
}

int jffs2_init_cleanmaker(struct filesystem *fs,
                         struct jffs2_unknown_node *maker,
                         int *pos, int *len) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    int clmpos = 0, clmlen = 8;
    struct jffs2_unknown_node cleanmarker;
    int is_nand = mtd_type_is_nand(mtd);

    if (!strcmp(fs->name, BM_FILE_TYPE_JFFS2)) {
        cleanmarker.magic = cpu_to_je16 (JFFS2_MAGIC_BITMASK);
        cleanmarker.nodetype = cpu_to_je16 (JFFS2_NODETYPE_CLEANMARKER);
        if (!is_nand)
            cleanmarker.totlen = cpu_to_je32(sizeof(cleanmarker));
        else {
            struct nand_oobinfo oobinfo;

            if (ioctl(fd, MEMGETOOBSEL, &oobinfo) != 0){
                LOGE("Unable to get NAND oobinfo");
                return false;
            }

            /* Check for autoplacement */
            if (oobinfo.useecc == MTD_NANDECC_AUTOPLACE) {
                /* Get the position of the free bytes */
                if (!oobinfo.oobfree[0][1]){
                    LOGE("Eeep. Autoplacement selected and no empty space in oob");
                    return false;
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
    memcpy(maker, &cleanmarker, sizeof(cleanmarker));

    jffs2_dump_cleanmaker(maker, pos, len);
    return true;
}

int jffs2_write_cleanmaker(struct filesystem *fs,
                           long long offset,
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
            return false;
        }
    } else {
        if (pwrite(fd, cleanmarker, sizeof(*cleanmarker), (loff_t)offset) != sizeof(*cleanmarker)) {
            LOGE("MTD \"%s\" write failure", mtd->name);
            return false;
        }
    }
    return true;
}

static int jffs2_init(struct filesystem *fs) {
    if (!fs_init(fs))
        return false;
    FS_FLAG_SET(fs, PAD);
    FS_FLAG_SET(fs, MARKBAD);
    return true;
};

static long long jffs2_erase(struct filesystem *fs) {
    return mtd_basic_erase(fs);
}

static long long jffs2_read(struct filesystem *fs) {
    return mtd_basic_read(fs);
}

static long long jffs2_write(struct filesystem *fs) {
    return mtd_basic_write(fs);
}

static long long jffs2_get_operate_start_address(struct filesystem *fs) {
    return fs->params->offset;
}

static unsigned long jffs2_get_leb_size(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    return mtd->eb_size;
}
static long long jffs2_get_max_mapped_size_in_partition(struct filesystem *fs) {
    return mtd_block_scan(fs);
}

struct filesystem fs_jffs2 = {
    .name = BM_FILE_TYPE_JFFS2,
    .init = jffs2_init,
    .erase = jffs2_erase,
    .read = jffs2_read,
    .write = jffs2_write,
    .get_operate_start_address = jffs2_get_operate_start_address,
    .get_leb_size = jffs2_get_leb_size,
    .get_max_mapped_size_in_partition = 
            jffs2_get_max_mapped_size_in_partition,
};
