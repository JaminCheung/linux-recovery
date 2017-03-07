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
#include <utils/assert.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/fs/fs_manager.h>
#include <block/fs/jffs2.h>
#include <block/sysinfo/sysinfo_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG "mtd_base"

int mtd_type_is_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_mlc_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_nor(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NORFLASH;
}

#if 0
#ifdef MTD_OPEN_DEBUG
static int mtd_bm_block_map_test(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    int total_eb = (int)(bm->get_capacity(bm) / mtd->eb_size);

    LOGI("total block count %d\n", total_eb);
    LOGI("map reading: \n");
    for (int i = 0; i < total_eb; ++i) {
        LOGI("%d is reading, value is %d\n", i, mi->es[i]);
        if (mi->es[i]) {
            assert_die_if(1, "initial value of eb%d status cannot be %d\n", i, mi->es[i]);
        }
    }
    LOGI("map reading done\n");
    return 0;
}
#endif
#endif

static int mtd_bm_block_map_init(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_block_map **mi = BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (*mi == NULL) {
        *mi = calloc(1, sizeof(struct mtd_block_map));
        if (!*mi) {
            LOGE("Cannot allocate %zd bytes of memory\n",
                 sizeof(struct mtd_block_map));
            goto out;
        }
        int total_eb = bm->get_capacity(bm) / mtd->eb_size;
        (*mi)->es = calloc(total_eb, sizeof(*((*mi)->es)));
        if ((*mi)->es == NULL) {
            LOGE("Cannot allocate %zd bytes of memory\n",
                 total_eb * sizeof((*mi)->es));
            goto out;
        }
        (*mi)->eb_start = 0;
        (*mi)->eb_cnt = total_eb;
#if 0
#ifdef MTD_OPEN_DEBUG
        mtd_bm_block_map_test(fs);
#endif
#endif
    }
    return 0;
out:
    if ((*mi)->es)
        free((*mi)->es);
    if (*mi) {
        free(*mi);
        *mi = NULL;
    }
    return -1;
}

void mtd_bm_block_map_destroy(struct block_manager *bm) {
    struct mtd_block_map **mi = BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (!(*mi))
        return;
    if ((*mi)->es)
        free((*mi)->es);
    if (*mi) {
        free(*mi);
        *mi = NULL;
    }
}

int mtd_bm_block_map_is_valid(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (mi == NULL || mi->es == NULL) {
        LOGE("Nand map table is null\n");
        return false;
    }
    return true;
}

int  mtd_bm_block_map_is_taged(struct filesystem *fs, int64_t eb) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if ((mi->es[eb] & MTD_BLK_PREFIX)
            != MTD_BLK_PREFIX) {
        return false;
    }
    return true;
}

int mtd_bm_block_map_is_bad(struct filesystem *fs, int64_t eb) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (mi->es[eb] == MTD_BLK_BAD) {
        LOGI("Skipping bad block at %llx\n", eb);
        return true;
    }
    return false;
}

int mtd_bm_block_map_is_erased(struct filesystem *fs, int64_t eb) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (mi->es[eb] == MTD_BLK_ERASED)
        return true;
    return false;
}

int mtd_bm_block_map_set(struct filesystem *fs, int64_t eb, int status) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm, struct mtd_block_map);

    if (mi->es[eb] != status) {
        if (status == MTD_BLK_BAD)
            mi->bad_cnt++;
        // else if (status == MTD_BLK_WRITEN_EIO)
        //     mi->write_eio++;
        if (mi->es[eb] == MTD_BLK_BAD)
            goto out;
        mi->es[eb] = status;
    }
out:
    return 0;
}

int mtd_boundary_is_valid(struct filesystem *fs, int64_t eb_start, int64_t eb_end) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int64_t left_limit = 0;
    int64_t right_limit = MTD_OFFSET_TO_EB_INDEX(mtd, mtd->size);
    if ((eb_start < left_limit) || (eb_end > right_limit)) {
        LOGE("start eb%lld to end eb%lld is invalid, valid boundary is from %lld to %lld\n",
             eb_start, eb_end, left_limit, right_limit);
        return false;
    }
    return true;
}

#ifdef MTD_OPEN_DEBUG
void mtd_scan_dump(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_block_map *mi = *BM_GET_MTD_BLOCK_MAP(bm,
                               struct mtd_block_map);
    char s_scaned[8192] = "<tag scan>:";
    char s_bad[8192] = "<tag bad>:";
    char s_erased[8192] = "<tag erased>:";
    char tmpbuf[256];
    char *buf = NULL;
    int  scaned_cnt = 0, bad_cnt = 0, erased_cnt = 0, *pointer = NULL;
    int formator = 35;
    int i;
    if ((mi == NULL) || (mi->es == NULL))  {
        LOGE("Parameter mtd block map is null\n");
        return;
    }
    LOGI("total eb count: %lld, start from %lld to %lld\n",
         (mi)->eb_cnt,  (mi)->eb_start, (mi)->eb_start + (mi)->eb_cnt);
    LOGI("bad eb count: %lld\n",  (mi)->bad_cnt);
    LOGI("eb table status table: \n");

    for (i = (mi)->eb_start; i < (mi)->eb_cnt; i++) {
        if ((mi)->es[i] & MTD_BLK_PREFIX) {
            if ((mi)->es[i] == MTD_BLK_SCAN) {
                buf = s_scaned;
                pointer = &scaned_cnt;
            } else if  ((mi)->es[i] == MTD_BLK_BAD) {
                buf = s_bad;
                pointer = &bad_cnt;
            } else if  ((mi)->es[i] == MTD_BLK_ERASED) {
                buf = s_erased;
                pointer = &erased_cnt;
            }
            if (!((*pointer) % formator)) {
                strcat(buf, "\n\t\t");
            }
            (*pointer)++;
            sprintf(tmpbuf, "%04d ", i);
            strcat(buf, tmpbuf);
        }
    }
    LOGI("total ebs %d \n%s\n", scaned_cnt, s_scaned);
    LOGI("total ebs %d \n%s\n", bad_cnt, s_bad);
    LOGI("total ebs %d \n%s\n", erased_cnt, s_erased);
    return;
}
#endif

int64_t mtd_block_scan(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    int64_t offset = fs->params->offset;
    int64_t length = fs->params->length;
    int op_method = fs->params->operation_method;
    // struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    // int64_t partition_start = MTD_DEV_INFO_TO_START(mtd);
    // int64_t partition_size = mtd->size;
    int64_t eb, start_eb, end_eb, total_bytes;
    int64_t pass = 0;
    int noskipbad = 0, retval;

    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        goto out;
    }

    if (length < 0) {
        LOGE("Parameter length is negative\n");
        goto out;
    }

    if ((op_method == BM_OPERATION_METHOD_RANDOM)
            && !length) {
        LOGE("Parameter length is zero at random mode\n");
        goto out;
    }
    fs_flags_get(fs, &noskipbad);
#if 0   //shadow boundary aligned judgement
    if (!MTD_IS_BLOCK_ALIGNED(mtd, offset)
            || !MTD_IS_BLOCK_ALIGNED(mtd, offset + length)) {
        LOGE("Boundary is not block alligned, left is %lld, right is %lld\n",
             offset, offset + length);
        LOGE("Blocksize is %d\n", mtd->eb_size);
        goto out;
    }
#endif
    start_eb = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end_eb = MTD_OFFSET_TO_EB_INDEX(mtd, offset + length + mtd->eb_size - 1);

    end_eb = (op_method == BM_OPERATION_METHOD_PARTITION)
             ? MTD_OFFSET_TO_EB_INDEX(mtd,
                                      BM_GET_PARTINFO_START(bm, (MTD_DEV_INFO_TO_ID(mtd) + 1)))
             : MTD_OFFSET_TO_EB_INDEX(mtd,
                                      offset + length + mtd->eb_size - 1);

    total_bytes = (end_eb - start_eb) * mtd->eb_size;
    LOGI("Scan mtdchar dev \"%s\" from eb%lld to eb%lld, total scaned bytes is %lld\n",
         MTD_DEV_INFO_TO_PATH(mtd), start_eb, end_eb, total_bytes);

    start_eb = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, start_eb);
    end_eb = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, end_eb);
    if (!mtd_boundary_is_valid(fs, start_eb, end_eb)) {
        LOGE("mtd boundary start eb %lld to end eb %lld is invalid\n",
             start_eb, end_eb);
        goto out;
    }

    if (mtd_bm_block_map_init(fs) < 0)
        goto out;

    if (!mtd_bm_block_map_is_valid(fs)) {
        LOGE("mtd block map on MTD \'%s\' is invalid\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto out;
    }

    for (eb = start_eb; eb < mtd->eb_cnt; eb++) {
        if (mtd_bm_block_map_is_taged(fs, MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb))
                && !mtd_bm_block_map_is_bad(fs, MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb))) {
            pass += mtd->eb_size;
            //exit1: The block has been scaned before.
            if (pass >= total_bytes) {
                // printf("exit1: The block has been scaned before. Leaving eb %lld\n", eb);
                break;
            }
            continue;
        }
        mtd_bm_block_map_set(fs,
                             MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_SCAN);
        if (mtd_type_is_nor(mtd)) {
            pass += mtd->eb_size;
            //exit2: nor flash has no requirement for bad block detecting
            if (pass >= total_bytes) {
                // printf("exit2: nor flash has no requirement for bad block detecting. Leaving eb %lld\n", eb);
                break;
            }
            continue;
        }

        if (noskipbad) {
            pass += mtd->eb_size;
            if (pass >= total_bytes) {
                // printf("exit3: explicitly for nand flash. Leaving eb %lld\n", eb);
                break;
            }
            continue;
        }
        retval = mtd_is_bad(mtd, fd, eb);
        if (retval == -1) {
            LOGE("mtd block %lld bad detecting wrong\n", eb);
            goto out;
        }
        if (retval) {
            if (errno == EOPNOTSUPP) {
                LOGE("Bad block check not available on MTD \"%s\"",
                     MTD_DEV_INFO_TO_PATH(mtd));
                goto out;
            }
            mtd_bm_block_map_set(fs,
                                 MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_BAD);
            LOGI("Block%lld is bad\n", eb);
            continue;
        }
        //exit3: explicitly for nand flash
        pass += mtd->eb_size;
        if (pass >= total_bytes) {
            // printf("exit3: explicitly for nand flash. Leaving eb %lld\n", eb);
            break;
        }
    }
    if ((op_method != BM_OPERATION_METHOD_PARTITION)
            && (eb >= mtd->eb_cnt)) {
        LOGE("total bytes to be operated is too large to hold on MTD \"%s\"\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto out;
    }
    LOGI("Actually total %lld bytes is scaned\n",
         (eb - start_eb + 1)*mtd->eb_size);
    return (eb - start_eb + 1) * mtd->eb_size;
out:
    return -1;
}

void set_process_info(struct filesystem *fs,
                      int type, int64_t eboff, int64_t ebcnt) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int progress = (int)(eboff * 100 / ebcnt);
    struct bm_event info;

    info.part_name = (char*)mtd->name;
    info.operation = type;
    info.progress = progress;

    if (BM_GET_LISTENER(bm))
        BM_GET_LISTENER(bm)(bm, &info, bm->param);
}

int64_t mtd_basic_erase(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);

    int is_nand = mtd_type_is_nand(mtd);
    int noskipbad = 0;
    // int op_method = FS_GET_PARAM(fs)->operation_method;
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);

    int64_t start, end, total_bytes, erased_bytes;
    int64_t offset, eb;

    struct jffs2_unknown_node cleanmarker;
    int clmpos, clmlen;
    int is_jffs2 = !strcmp(fs->name, BM_FILE_TYPE_JFFS2);

    int64_t bad_unlock_nerase_ebs = 0, nerase_size = 0;
    int err;

    fs_flags_get(fs, &noskipbad);

    if (is_jffs2 && mtd_type_is_mlc_nand(mtd)) {
        LOGE("JFFS2 cannot support MLC NAND\n");
        goto closeall;
    }

    if (is_jffs2 && (jffs2_init_cleanmarker(fs, &cleanmarker, &clmpos, &clmlen) < 0))
        goto closeall;

    offset = MTD_DEV_INFO_TO_START(mtd);
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = MTD_OFFSET_TO_EB_INDEX(mtd,
                                 MTD_BLOCK_ALIGN(mtd, offset + fs->params->length + mtd->eb_size - 1));

    total_bytes = (end - start) * mtd->eb_size;
    start = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, start);
    end = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, end);
    if (!mtd_boundary_is_valid(fs, start, end)) {
        LOGE("mtd boundary start eb %lld to end eb %lld is invalid\n",
             start, end);
        goto closeall;
    }
    if (!mtd_bm_block_map_is_valid(fs)) {
        LOGE("mtd block map on MTD \'%s\' is invalid\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto closeall;
    }
    LOGI("MTD \"%s\" is going to erase from eb%lld to eb%lld, total length is %lld bytes\n",
         MTD_DEV_INFO_TO_PATH(mtd), start, end, total_bytes);

    eb = start;
    erased_bytes = 0;
    while ((erased_bytes < total_bytes)
            && (eb < mtd->eb_cnt)) {
        offset = eb * mtd->eb_size;
        set_process_info(fs, BM_OPERATION_ERASE, erased_bytes, total_bytes);
        if ((is_nand && !noskipbad)
                && mtd_bm_block_map_is_bad(fs, MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb))) {
            eb++;
            bad_unlock_nerase_ebs++;
            continue;
        }
        if (FS_FLAG_IS_SET(fs, UNLOCK)) {
            if (mtd_unlock(mtd, *fd, eb) != 0) {
                LOGE("MTD \"%s\" unlock failure", MTD_DEV_INFO_TO_PATH(mtd));
                eb++;
                bad_unlock_nerase_ebs++;
                continue;
            }
        }
        LOGI("MTD %s: erase at block%lld with fd%d\n", MTD_DEV_INFO_TO_PATH(mtd), eb, *fd);

#ifdef BM_SYSINFO_SUPPORT
        if (bm->sysinfo) {
            err = bm->sysinfo->traversal_save(bm->sysinfo,
                                              offset + MTD_DEV_INFO_TO_START(mtd), mtd->eb_size);
            if (err < 0) {
                LOGE("MTD \"%s\" failed to save system info\n", MTD_DEV_INFO_TO_PATH(mtd));
                goto closeall;
            }
        }
#endif

        err = mtd_erase(mtd_desc, mtd, *fd, eb);
        if (err) {
            LOGE("MTD \"%s\" failed to erase eraseblock %lld\n",
                 MTD_DEV_INFO_TO_PATH(mtd), eb);
            if (errno != EIO) {
                LOGE("MTD \"%s\" fatal error occured on %lld\n",
                     MTD_DEV_INFO_TO_PATH(mtd), eb);
                goto closeall;
            }
            if (mtd_mark_bad(mtd, *fd, eb)) {
                LOGE("MTD \"%s\" mark bad block failed on %lld\n",
                     MTD_DEV_INFO_TO_PATH(mtd), eb);
                goto closeall;
            }
            mtd_bm_block_map_set(fs,
                                 MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_BAD);
            eb++;
            bad_unlock_nerase_ebs++;
            continue;
        }
        mtd_bm_block_map_set(fs,
                             MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_ERASED);
        /* format for JFFS2 ? */
        if (is_jffs2) {
            LOGI("Cleanmarker is writing at %x\n", (unsigned int)offset);
            if (jffs2_write_cleanmarker(fs, offset, &cleanmarker, clmpos, clmlen) < 0) {
                LOGE("MTD \"%s\" cannot write cleanmarker at offset 0x%llx\n",
                     MTD_DEV_INFO_TO_PATH(mtd), offset);
                goto closeall;
            }
        }
        erased_bytes += mtd->eb_size;
        eb++;
    }

    nerase_size = bad_unlock_nerase_ebs * mtd->eb_size;
    if ((eb >= mtd->eb_cnt)
            && (erased_bytes + nerase_size < total_bytes)) {
        LOGE("The erase length you have requested is too large to issuing\n");
        LOGE("Request length: %lld, erase length: %lld, non erase length: %lld\n",
             total_bytes, erased_bytes, nerase_size);
        goto closeall;
    }
    set_process_info(fs, BM_OPERATION_ERASE, erased_bytes, total_bytes);
    start = MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb) * mtd->eb_size;
    return start;
closeall:
    LOGE("%s has crashed\n", __func__);
    if (*fd) {
        close(*fd);
        *fd = 0;
    }
    return -1;
}

static void erase_buffer(void *buffer, size_t size)
{
    const uint8_t bytes = 0xff;

    if (buffer != NULL && size > 0)
        memset(buffer, bytes, size);
}

int64_t mtd_basic_write(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);

    int is_nand = mtd_type_is_nand(mtd);
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);

    int noecc, autoplace, writeoob, oobsize, pad, markbad, pagelen;
    unsigned int write_mode;
    long long mtd_start, w_length, w_offset, blockstart = -1, writen;
    char *w_buffer, *oobbuf;
    char *pad_buffer = NULL;
    int ret;

    w_length = fs->params->length;
    w_buffer = fs->params->buf;
    w_offset = fs->params->offset;
    mtd_start = MTD_DEV_INFO_TO_START(mtd);
    w_offset -= mtd_start;

    fs_write_flags_get(fs, &noecc, &autoplace, &writeoob, &oobsize, &pad, &markbad);

    if (w_offset < 0) {
        LOGE("The start address is negative %lld.\n",
             w_offset);
        goto closeall;
    }
    if (w_offset & (mtd->min_io_size - 1)) {
        LOGE("The start address is not page-aligned !"
             "The pagesize of this Flash is 0x%x.\n",
             mtd->min_io_size);
        goto closeall;
    }

    if (noecc)  {
        ret = ioctl(*fd, MTDFILEMODE, MTD_FILE_MODE_RAW);
        if (ret) {
            switch (errno) {
            case ENOTTY:
                LOGE("ioctl MTDFILEMODE is missing\n");
                goto closeall;
            default:
                LOGE("MTDFILEMODE\n");
                goto closeall;
            }
        }
    }

    pagelen = mtd->min_io_size;
    if (is_nand)
        pagelen = mtd->min_io_size + ((writeoob) ? oobsize : 0);

    if (!pad && (w_length % pagelen) != 0) {
        LOGE("Writelength is not page-aligned. Use the padding "
             "option.\n");
        goto closeall;
    }

    if ((w_length / pagelen) * mtd->min_io_size
            > mtd->size - w_offset) {
        LOGE("MTD name \"%s\" overlow, write %lld bytes, page %d bytes, OOB area %d"
             " bytes, device size %lld bytes\n",
             MTD_DEV_INFO_TO_PATH(mtd), w_length, pagelen, oobsize, mtd->size);
        LOGE("Write length does not fit into device\n");
        goto closeall;
    }

    if (!mtd_bm_block_map_is_valid(fs)) {
        LOGE("mtd block map on MTD \'%s\' is invalid\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto closeall;
    }
    if (noecc)
        write_mode = MTD_OPS_RAW;
    else if (autoplace)
        write_mode = MTD_OPS_AUTO_OOB;
    else
        write_mode = MTD_OPS_PLACE_OOB;

    LOGI("MTD \"%s\" write at 0x%llx, totally %lld bytes is starting\n",
         MTD_DEV_INFO_TO_PATH(mtd), w_offset, w_length);
    while (w_length > 0 && w_offset < mtd->size) {
        while (blockstart != MTD_BLOCK_ALIGN(mtd, w_offset)) {
            blockstart = MTD_BLOCK_ALIGN(mtd, w_offset);
            LOGI("Writing data to block %lld at offset 0x%llx\n",
                 blockstart / mtd->eb_size, w_offset);

            if (!is_nand)
                continue;
            do {
                if (mtd_bm_block_map_is_bad(fs, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                    w_offset += mtd->eb_size;
                    w_offset = MTD_BLOCK_ALIGN(mtd, w_offset);
                    continue;
                }
                break;
            } while (w_offset < mtd->size);

            if (w_offset >= mtd->size) {
                LOGE("write boundary is overlow\n");
                goto closeall;
            }
            blockstart = MTD_BLOCK_ALIGN(mtd, w_offset);
            continue;
        }
        writen = (w_length > pagelen) ? pagelen : w_length;
        if (writen % pagelen) {
            LOGI("Padding\n");
            if (pad_buffer == NULL) {
                pad_buffer = malloc(pagelen);
                if (pad_buffer == NULL) {
                    LOGE("Buffer malloc error\n");
                    goto closeall;
                }
            }
            erase_buffer(pad_buffer, pagelen);
            memcpy(pad_buffer, w_buffer, writen);
            w_buffer = pad_buffer;
        }
        if ((writen > mtd->min_io_size)
                && writeoob) {
            oobbuf = writeoob ? w_buffer + mtd->min_io_size : NULL;
        } else {
            oobbuf = NULL;
            writeoob = 0;
        }
        set_process_info(fs, BM_OPERATION_WRITE,
                         fs->params->progress_size, fs->params->max_size);
#ifdef BM_SYSINFO_SUPPORT
        if (bm->sysinfo) {
            ret = bm->sysinfo->traversal_merge(bm->sysinfo, w_buffer,
                                               w_offset + MTD_DEV_INFO_TO_START(mtd),
                                               mtd->min_io_size);
            if (ret < 0) {
                LOGE("MTD \"%s\" failed to merge system info\n", MTD_DEV_INFO_TO_PATH(mtd));
                goto closeall;
            }
        }
#endif
        ret = mtd_write(mtd_desc, mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset),
                        w_offset % mtd->eb_size,
                        w_buffer,
                        mtd->min_io_size,
                        writeoob ? oobbuf : NULL,
                        writeoob ? oobsize : 0,
                        write_mode);
        if (ret) {
            if (errno != EIO) {
                LOGE("MTD \"%s\" write failure\n", MTD_DEV_INFO_TO_PATH(mtd));
                goto closeall;
            }
            LOGW("Erasing failed write at 0x%llx\n", MTD_OFFSET_TO_EB_INDEX(mtd, w_offset));
            if (mtd_erase(mtd_desc, mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                int errno_tmp = errno;
                LOGE("MTD \"%s\" Erase failure\n", MTD_DEV_INFO_TO_PATH(mtd));
                if (errno_tmp != EIO)
                    goto closeall;
            }
            if (markbad) {
                LOGW("Marking block at %llx bad\n", MTD_BLOCK_ALIGN(mtd, w_offset));
                if (mtd_mark_bad(mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                    LOGE("MTD \"%s\" Mark bad block failure\n", MTD_DEV_INFO_TO_PATH(mtd));
                    goto closeall;
                }
            }
            if (mtd_bm_block_map_set(fs,
                                     MTD_OFFSET_TO_EB_INDEX(mtd, w_offset), MTD_BLK_BAD) < 0) {
                LOGE("MTD \"%s\" block map wrong at eb %lld\n", MTD_DEV_INFO_TO_PATH(mtd),
                     MTD_OFFSET_TO_EB_INDEX(mtd, w_offset));
                goto closeall;
            }
            w_offset += mtd->eb_size;
            w_offset = MTD_BLOCK_ALIGN(mtd, w_offset);
            continue;
        }
        w_offset += mtd->min_io_size;
        w_buffer += pagelen;
        w_length -= writen;
        fs->params->progress_size += writen;
    }
    set_process_info(fs, BM_OPERATION_WRITE,
                     fs->params->progress_size, fs->params->max_size);
    if (pad_buffer)
        free(pad_buffer);
    return w_offset + mtd_start;
closeall:
    LOGE("%s has crashed\n", __func__);
    if (pad_buffer)
        free(pad_buffer);
    if (*fd) {
        close(*fd);
        *fd = 0;
    }
    return -1;
}

int64_t mtd_basic_read(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);
    long long mtd_start, length, offset, blockstart = -1, read_unit = 0;
    char *buffer;
    int is_nand = mtd_type_is_nand(mtd);
    int ret;

    length = fs->params->length;
    buffer = fs->params->buf;
    offset = fs->params->offset;
    mtd_start = MTD_DEV_INFO_TO_START(mtd);
    offset -= mtd_start;

    if (!mtd_bm_block_map_is_valid(fs)) {
        LOGE("mtd block map on MTD \'%s\' is invalid\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto closeall;
    }

    LOGI("MTD \"%s\" read at 0x%llx, totally %lld bytes is starting\n",
         MTD_DEV_INFO_TO_PATH(mtd), offset, length);
    while (length > 0 && offset < mtd->size) {
        while (blockstart != MTD_BLOCK_ALIGN(mtd, offset)) {
            blockstart = MTD_BLOCK_ALIGN(mtd, offset);
            // LOGI("Reading data from block %lld at offset 0x%llx\n",
            //      blockstart / mtd->eb_size, offset);
            if (!is_nand)
                continue;
            do {
                if (mtd_bm_block_map_is_bad(fs, MTD_OFFSET_TO_EB_INDEX(mtd, offset))) {
                    offset += mtd->eb_size;
                    offset = MTD_BLOCK_ALIGN(mtd, offset);
                    continue;
                }
                break;
            } while (offset < mtd->size);

            if (offset >= mtd->size) {
                LOGE("read boundary is overlow\n");
                goto closeall;
            }
            blockstart = MTD_BLOCK_ALIGN(mtd, offset);
            continue;
        }
        read_unit = ((length > (mtd->eb_size - (offset % mtd->eb_size))) ?
                     (mtd->eb_size - (offset % mtd->eb_size)) : length);
        LOGI("Reading data at offset 0x%llx with size %lld\n", offset, read_unit);
        ret = mtd_read(mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, offset),
                       offset % mtd->eb_size, buffer, read_unit);
        if (ret) {
            LOGE("MTD \"%s\" read failure at address 0x%llx with length %lld\n",
                 MTD_DEV_INFO_TO_PATH(mtd), offset, length);
            goto closeall;
        }
        length -= read_unit;
        buffer += read_unit;
        offset += read_unit;
    }

    return offset + mtd_start;
closeall:
    LOGE("%s has crashed\n", __func__);
    if (*fd) {
        close(*fd);
        *fd = 0;
    }
    return -1;
}
