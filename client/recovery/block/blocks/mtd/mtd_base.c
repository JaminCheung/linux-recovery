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
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/fs/fs_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>


static int mtd_basic_erase(struct filesystem *fs) {

    struct block_manager  *bm = FS_GET_TYPE(struct block_manager, fs->params->priv);

    struct mtd_dev_info *mtd;
    unsigned long start;
    unsigned int eb, eb_start, eb_cnt;
    int isNAND;
    int fd, clmpos = 0, clmlen = 8;
    unsigned long offset = 0;
    int retval = -1;

    int filetype = FS_GET_PARAM(fs)->fstype;
    int fd = FS_GET_PARAM(fs)->fd;


    if (!this || !params->partition){
        LOGE("Parameter errors");
        return -1;
    }

    if (flash_erase_params_judge(this, params) < 0){
        LOGE("Parameter judge errors");
        return -1;
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

    eb_cnt = 

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
        cleanmarker.hdr_crc = cpu_to_je32(local_crc32(0, &cleanmarker, sizeof(cleanmarker) - 4));
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
            if (mtd_write_oob(this->mtd_desc, mtd, fd, (uint64_t_t)offset + clmpos, clmlen, &cleanmarker) != 0) {
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