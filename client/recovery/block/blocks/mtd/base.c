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
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG "mtd_base"

#if 0
static char *mkpath(const char *path, const char *name)
{
    char *n;
    size_t len1 = strlen(path);
    size_t len2 = strlen(name);

    n = malloc(len1 + len2 + 2);

    memcpy(n, path, len1);
    if (n[len1 - 1] != '/')
        n[len1++] = '/';

    memcpy(n + len1, name, len2 + 1);
    return n;
}
#endif

int mtd_type_is_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_mlc_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_nor(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NORFLASH;
}

static int mtd_nand_map_init(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map **mi = BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);
    if (*mi == NULL) {
        *mi = calloc(1, sizeof(struct mtd_nand_map));
        if (!*mi) {
            LOGE("Cannot allocate %zd bytes of memory",
                 sizeof(struct mtd_nand_map));
            goto out;
        }
        int total_eb = bm->get_capacity(bm) / mtd->eb_size;
        (*mi)->es = calloc(total_eb, sizeof(*((*mi)->es)));
        if ((*mi)->es == NULL) {
            LOGE("Cannot allocate %zd bytes of memory",
                 total_eb * sizeof((*mi)->es));
            goto out;
        }
        (*mi)->eb_start = 0;
        (*mi)->eb_cnt = total_eb;
    }
    return true;
out:
    if ((*mi)->es)
        free((*mi)->es);
    if (*mi){
        free(*mi);
        *mi = NULL;
    }
    return false;
}

int mtd_nand_map_is_valid(struct filesystem *fs, int64_t eb){
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    int64_t mtd_start = MTD_DEV_INFO_TO_START(mtd);
    if (nand_map->es == NULL) {
        LOGE("Nand map table is null\n");
        return false;
    }
    if ((nand_map->es[eb+mtd_start] & MTD_BLK_PREFIX) 
        != MTD_BLK_PREFIX) {
        LOGE("Nand map table state is error at eb%lld\n", eb+mtd_start);
        return false;
    }
    return true; 
}

int mtd_nand_block_is_bad(struct filesystem *fs, int64_t eb){
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    int64_t mtd_start = MTD_DEV_INFO_TO_START(mtd);
    // if (nand_map->es == NULL) {
    //     LOGE("Nand map table is null\n");
    //     return false;
    // }

    // if ((nand_map->es[eb+mtd_start] & MTD_BLK_PREFIX) 
    //     != MTD_BLK_PREFIX) {
    //     LOGE("Nand map table state is error at eb%d\n", eb+mtd_start);
    //     return false;
    // } 
    if (nand_map->es[eb+mtd_start] == MTD_BLK_BAD) {
        LOGI("Skipping bad block at %llx", (eb+mtd_start));
        return true;
    }
    return false;
}

int mtd_nand_block_is_erased(struct filesystem *fs, int64_t eb) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    int64_t mtd_start = MTD_DEV_INFO_TO_START(mtd);
    if (nand_map->es[eb+mtd_start] == MTD_BLK_ERASED)
        return true;
    return false;
}

int mtd_nand_block_is_set(struct filesystem *fs, int64_t eb, int status) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    int64_t mtd_start = MTD_DEV_INFO_TO_START(mtd);

    if (nand_map->es == NULL) {
        LOGE("Nand map table is null\n");
        return false;
    }
    if ((nand_map->es[eb+mtd_start] & MTD_BLK_PREFIX) 
        != MTD_BLK_PREFIX) {
        LOGE("Nand map table state is error at eb%lld\n", eb+mtd_start);
        return false;
    }
    if (nand_map->es[eb+mtd_start] != status) {
        nand_map->es[eb+mtd_start] = status;
        if (status == MTD_BLK_WRITEN_EIO)
            nand_map->dirty++;
    }
    return true;
}

#ifdef MTD_OPEN_DEBUG
#if 0
void mtd_scan_dump(struct block_manager* this, int64_t offset,
                    struct mtd_nand_map* mi) {
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    struct mtd_nand_map **mi = BM_GET_MTD_NAND_MAP(this, 
                                                            struct mtd_nand_map);
    int64_t scaned, bad, erased, writen, writen_eio;
    int64_t a_scaned[2048], 
    if (((*mi) == NULL) || ((*mi)->es == NULL))  {
        LOGE("Parameter mtd_nand_map is null\n");
        return;
    }
    LOGI("total eb count: %lld, start from %lld to %lld\n", 
            *mi->eb_cnt,  *mi->eb_start, *mi->eb_start + *mi->eb_cnt);
    LOGI("bad eb count: %lld\n",  *mi->bad_cnt);
    LOGI("eb table status:\n")
    for (i = *mi->eb_start; i < *mi->eb_cnt; i++) {
        if (*mi->es[i] & MTD_BLK_PREFIX) {
            if (*mi->es[i] == MTD_BLK_SCAN) {

                scaned++;
            } else if  (*mi->es[i] == MTD_BLK_BAD) {

            } else if  (*mi->es[i] == MTD_BLK_ERASED) {

            } else if  (*mi->es[i] == MTD_BLK_WRITEN_EIO) {
                
            } else if  (*mi->es[i] == MTD_BLK_WRITEN) {
                
            }
        }
        LOGI("eb%d: "); 
    }
    return;
}
#endif
#endif

int64_t mtd_block_scan(struct filesystem *fs) {
    struct block_manager *this = FS_GET_BM(fs);
    int64_t offset = fs->params->offset;
    int64_t length = fs->params->length;
    int op_method = fs->params->operation_method;
    struct mtd_nand_map **mi = BM_GET_MTD_NAND_MAP(this, struct mtd_nand_map);
    // struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    // int64_t partition_start = MTD_DEV_INFO_TO_START(mtd);
    // int64_t partition_size = mtd->size;
    int64_t eb, mtd_start, start, end, total_bytes;
    int64_t pass = 0;
    int retval;

    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        goto out;
    }

    if (!length) {
        LOGE("Parameter length is zero\n");
        goto out;
    }

#if 0   //shadow boundary aligned judgement temporarily
    if (!MTD_IS_BLOCK_ALIGNED(mtd, offset)
     || !MTD_IS_BLOCK_ALIGNED(mtd, offset+length)) {
        LOGE("Boundary is not block alligned, left is %lld, right is %lld\n",
                 offset, offset+length);
        LOGE("Blocksize is %d\n", mtd->eb_size);
        goto out;
    }
#endif
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = MTD_OFFSET_TO_EB_INDEX(mtd, offset+length+mtd->eb_size-1);

    end = (op_method == BM_OPERATION_METHOD_PARTITION)
        ?MTD_OFFSET_TO_EB_INDEX(mtd,
                        BM_GET_PARTINFO_START(this, (MTD_DEV_INFO_TO_ID(mtd)+1)))
        :MTD_OFFSET_TO_EB_INDEX(mtd,
                        offset + length + mtd->eb_size - 1);

    total_bytes = (end - start)*mtd->eb_size;
    LOGI("Scan \"%s\" from eb%d to eb%d, total scaned length is %lld\n",
                MTD_DEV_INFO_TO_PATH(mtd), (int)start, (int)end, total_bytes);
    mtd_start = MTD_OFFSET_TO_EB_INDEX(mtd, MTD_DEV_INFO_TO_START(mtd));

    if (mtd_type_is_nor(mtd)) {
        LOGI("Actually total %lld bytes is returned on faked scaning for norflash\n",
            total_bytes);
        return total_bytes;
    }

    if (!mtd_nand_map_init(fs))
        goto out;

    for (eb = start - mtd_start; eb <= mtd->eb_cnt; eb++) {
        if ((((*mi)->es[eb] & MTD_BLK_PREFIX) == MTD_BLK_PREFIX) 
            && (*mi)->es[eb] != MTD_BLK_BAD) {
            pass += mtd->eb_size;
            if (pass >= total_bytes)
                break;
        }
        (*mi)->es[eb] = MTD_BLK_SCAN;
        // LOGI("Bad block detection is starting\n");
        retval = mtd_is_bad(mtd, fd, eb);
        if (retval == -1)
            goto out;
        if (retval) {
             if (errno == EOPNOTSUPP) {
                LOGE("Bad block check not available on %s", mtd->name);
                goto out;
            }
            (*mi)->bad_cnt += 1;
            (*mi)->es[eb] = MTD_BLK_BAD;
            LOGI("Block%d is bad\n", (int)eb);
            continue;
        }
        pass += mtd->eb_size;
        if (pass >= total_bytes)
            break;
    }

    if (eb > mtd->eb_cnt) {
        LOGI("total bytes to be operated is too large to hold on MTD \'%s\'\n", 
                MTD_DEV_INFO_TO_PATH(mtd));
        goto out;
    }
    LOGI("Actually total %lld bytes is scaned\n", (eb-start + mtd_start)*mtd->eb_size);
    return (eb-start + mtd_start)*mtd->eb_size;
out:
    return 0;
}

static void set_process_info(struct filesystem *fs, 
        int type, int64_t eboff, int64_t ebcnt) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int progress = (int)(eboff * 100 / ebcnt);
    struct bm_event info;
    char *op_name = NULL;

    info.mtdchar = (char*)mtd->name;
    info.operation = type;
    info.progress = progress;

    if (type == BM_OPERATION_ERASE) {
        op_name = "Erasing";
    }else if (type == BM_OPERATION_WRITE) {
        op_name = "Writing";
    }else if (type == BM_OPERATION_READ) {
        op_name = "Reading";
    }

    LOGI("Partition[%s] %s procent is %d%%\n", 
            mtd->name, op_name, progress);
    if (BM_GET_LISTENER(bm))
        BM_GET_LISTENER(bm)(bm, &info);
}

int64_t mtd_basic_erase(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);

    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);
    int is_nand = mtd_type_is_nand(mtd);

    int op_method = FS_GET_PARAM(fs)->operation_method;
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);
    
    int64_t start, end, total_bytes, mtd_start;
    int64_t offset, eb;

    struct jffs2_unknown_node cleanmarker;
    int clmpos, clmlen;
    int is_jffs2 = !strcmp(fs->name, BM_FILE_TYPE_JFFS2);

    if (is_jffs2 && mtd_type_is_mlc_nand(mtd)){
        LOGE("JFFS2 cannot support MLC NAND\n");
        goto closeall;
    }

    if (is_jffs2 && !jffs2_init_cleanmarker(fs, &cleanmarker, &clmpos, &clmlen))
        goto closeall;

    offset = MTD_DEV_INFO_TO_START(mtd);
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = (op_method == BM_OPERATION_METHOD_PARTITION)
        ?MTD_OFFSET_TO_EB_INDEX(mtd,
                        BM_GET_PARTINFO_START(bm, (MTD_DEV_INFO_TO_ID(mtd)+1)))
        :MTD_OFFSET_TO_EB_INDEX(mtd,
                        offset + fs->params->length);

    // printf("%s: %d, end = %d\n", __func__, __LINE__, end);
    // printf("%s: %d, op_method = %d\n", __func__, __LINE__, op_method);
    // printf("%s: %d, eb 1 = %d\n", __func__, __LINE__, MTD_OFFSET_TO_EB_INDEX(mtd, 
    //                     BM_GET_PARTINFO_START(bm, (MTD_DEV_INFO_TO_ID(mtd)+1))-1));
    // printf("%s: %d, eb 2 = %lld\n", __func__, __LINE__, MTD_OFFSET_TO_EB_INDEX(mtd, 
    //                     offset + fs->params->max_mapped_size));
    //     printf("%s: %d, offset = 0x%llx, size = %lld\n", __func__, __LINE__, 
    //                     offset, fs->params->max_mapped_size);
    //     printf("%s: %d, offset = 0x%llx, mtdid = %d\n", __func__, __LINE__, 
    //                     BM_GET_PARTINFO_START(bm, (MTD_DEV_INFO_TO_ID(mtd)+1))-1, 
    //                     MTD_DEV_INFO_TO_ID(mtd)+1);

    total_bytes = (end - start)*mtd->eb_size;
    LOGI("Scan \"%s\" from eb%lld to eb%lld, total scaned length is %lld\n",
                MTD_DEV_INFO_TO_PATH(mtd), start, end, total_bytes);
    mtd_start = MTD_OFFSET_TO_EB_INDEX(mtd, MTD_DEV_INFO_TO_START(mtd));
    start -= mtd_start;
    end -= mtd_start;
    LOGI("Going to erase from eb%lld to eb%lld, total length is %lld bytes\n",
            start+mtd_start, end+mtd_start, total_bytes);
    for (eb = start; eb < end; eb++) {
        offset = (int64_t)eb * mtd->eb_size;

        set_process_info(fs, BM_OPERATION_ERASE, eb-start, end-start);
        if (is_nand && !FS_FLAG_IS_SET(fs, NOSKIPBAD)){
            if (mtd_nand_map_is_valid(fs, eb))
                goto closeall;
            if (mtd_nand_block_is_bad(fs, eb))
                continue;
        }

        if (FS_FLAG_IS_SET(fs, UNLOCK)) {
            if (mtd_unlock(mtd, *fd, eb) != 0) {
                LOGE("MTD \"%s\" unlock failure", MTD_DEV_INFO_TO_PATH(mtd));
                continue;
            }
        }

        printf("mtd_erase %s: block%lld is erasing  fd = %d\n", MTD_DEV_INFO_TO_PATH(mtd), eb, *fd);
        if (mtd_erase(mtd_desc, mtd, *fd, eb) != 0) {
            LOGE("MTD \"%s\" Erase unlock failure", MTD_DEV_INFO_TO_PATH(mtd));
            continue;
        }
        if (is_nand)
            nand_map->es[eb+mtd_start] = MTD_BLK_ERASED;
        /* format for JFFS2 ? */
        if (!is_jffs2)
            continue;
        LOGI("Cleanmarker is writing at %x\n", (unsigned int)offset);
        if (!jffs2_write_cleanmarker(fs, offset, &cleanmarker, clmpos, clmlen))
            continue;
    }
    set_process_info(fs, BM_OPERATION_ERASE, eb-start, end-start);
    start = (eb+mtd_start)*mtd->eb_size;
    LOGI("Next erase offset will start at 0x%llx\n", start);
    return start;
closeall:
    LOGE("%s has crashed\n", __func__);
    if (*fd){
        close(*fd);
        *fd = 0;
    }
    return 0;
}

static void erase_buffer(void *buffer, size_t size)
{
    const uint8_t bytes = 0xff;

    if (buffer != NULL && size > 0)
        memset(buffer, bytes, size);
}

static void mtd_write_flags_get(struct filesystem *fs, 
    int *noecc, int *autoplace, int *writeoob, 
    int *oobsize, int *pad, int *markbad) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    *noecc = FS_FLAG_IS_SET(fs, NOECC);
    *autoplace = FS_FLAG_IS_SET(fs, AUTOPLACE);
    *writeoob = FS_FLAG_IS_SET(fs, WRITEOOB);
    *oobsize = fs->tagsize?fs->tagsize:mtd->oob_size;
    *pad = FS_FLAG_IS_SET(fs, PAD);
    *markbad = FS_FLAG_IS_SET(fs, MARKBAD);
    LOGI("noecc = %d\n", *noecc);
    LOGI("autoplace = %d\n", *autoplace);
    LOGI("writeoob = %d\n", *writeoob);
    LOGI("oobsize = %d\n", *oobsize);
    LOGI("pad = %d\n", *pad);
    LOGI("markbad = %d\n", *markbad);
}

int64_t mtd_basic_write(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);

    int is_nand = mtd_type_is_nand(mtd);
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);
    
    int noecc, autoplace, writeoob, oobsize, pad, markbad,pagelen;
    unsigned int write_mode;
    long long mtd_start, w_length, w_offset, blockstart = -1, writen;
    char *w_buffer, *oobbuf;
    static char *pad_buffer;
    int ret;

    w_length = fs->params->length;
    w_buffer = fs->params->buf;
    w_offset = fs->params->offset;
    mtd_start = MTD_DEV_INFO_TO_START(mtd);
    w_offset -= mtd_start;
    mtd_write_flags_get(fs, &noecc, &autoplace, &writeoob, &oobsize, &pad, &markbad);

    if (w_offset & (mtd->min_io_size - 1)){
        LOGE("The start address is not page-aligned !"
           "The pagesize of this Flash is 0x%x.",
           mtd->min_io_size);
        goto closeall;
    }

    if (noecc)  {
        ret = ioctl(*fd, MTDFILEMODE, MTD_FILE_MODE_RAW);
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

    pagelen = mtd->min_io_size;
    if (is_nand)
        pagelen = mtd->min_io_size + ((writeoob) ? oobsize : 0);

    if (!pad && (w_length % pagelen) != 0) {
        LOGE("Writelength is not page-aligned. Use the padding "
                 "option.");
        goto closeall;
    }

    if ((w_length / pagelen) * mtd->min_io_size 
            > mtd->size - w_offset) {
        LOGE("MTD name \"%s\", write %lld bytes, page %d bytes, OOB area %d"
                " bytes, device size %lld bytes",
                mtd->name, w_length, pagelen, oobsize, mtd->size);
        LOGE("Write length does not fit into device");
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
                if (!mtd_nand_map_is_valid(fs, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset)))
                    goto closeall;
                if (mtd_nand_block_is_bad(fs, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                    w_offset += mtd->eb_size;
                    w_offset = MTD_BLOCK_ALIGN(mtd, w_offset);
                    continue;
                }
                if (!mtd_nand_block_is_erased(fs, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                    LOGE("Erase should be executed before write at \"%s\" eb%lld\n",
                         mtd->name, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset));           
                    goto closeall;
                }
                break;
            }while(w_offset < mtd->size);

            if (w_offset >= mtd->size) {
                LOGE("write boundary is overlow\n");
                goto closeall;
            }
            blockstart = MTD_BLOCK_ALIGN(mtd, w_offset);
            continue;
        }
        writen = (w_length>pagelen)?pagelen:w_length;
        printf("page writen size %lld,  remain size %lld\n", writen, w_length);
        if (writen % pagelen) {
            LOGI("Pad is starting\n");
            if (pad_buffer == NULL) {
                pad_buffer = malloc(pagelen);
                if (pad_buffer == NULL) {
                    LOGE("Buffer malloc error");
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
        }else {
            oobbuf = NULL;
            writeoob = 0;
        }
        // printf("mtd_write %s: fd = %d, write_mode = %d, w_offset = 0x%llx, w_buffer= 0x%x, oobbuf = %p, writeoob = %d\n",
        //         MTD_DEV_INFO_TO_PATH(mtd), *fd, w_offset, w_buffer, oobbuf, writeoob);
        // printf("buf: ");
        // for (int k =0; k < 10; k++) {
        //     printf(" 0x%x ", w_buffer[k]);
        // }
        // printf("\n");
        set_process_info(fs, BM_OPERATION_WRITE,
                fs->params->length - w_length, fs->params->length);
        ret = mtd_write(mtd_desc, mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset),
                w_offset % mtd->eb_size,
                w_buffer,
                mtd->min_io_size,
                writeoob ? oobbuf : NULL,
                writeoob ? oobsize : 0,
                write_mode);
        if (ret) {
            if (errno != EIO) {
                LOGE("MTD \"%s\" write failure", mtd->name);
                goto closeall;
            }
            LOGW("Erasing failed write at 0x%llx\n", MTD_OFFSET_TO_EB_INDEX(mtd, w_offset));
            if (mtd_erase(mtd_desc, mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                int errno_tmp = errno;
                LOGE("%s: MTD Erase failure", mtd->name);
                if (errno_tmp != EIO)
                    goto closeall;
            }
            if (markbad) {
                LOGW("Marking block at %llx bad", MTD_BLOCK_ALIGN(mtd, w_offset));
                if (mtd_mark_bad(mtd, *fd, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset))) {
                    LOGE("%s: MTD Mark bad block failure", mtd->name);
                    goto closeall;
                }
            }
            mtd_nand_block_is_set(fs, MTD_OFFSET_TO_EB_INDEX(mtd, w_offset), MTD_BLK_WRITEN_EIO);
            w_offset += mtd->eb_size;
            w_offset = MTD_BLOCK_ALIGN(mtd, w_offset);
            continue;
        }
        w_offset += mtd->min_io_size;
        w_buffer += pagelen;
        w_length -= writen;
    }
    set_process_info(fs, BM_OPERATION_WRITE,
                fs->params->length - w_length, fs->params->length);
    LOGI("Next write offset will start at 0x%llx\n", w_offset + mtd_start);

    return w_offset + mtd_start;
closeall:
    LOGE("%s has crashed\n", __func__);
    if (*fd){
        close(*fd);
        *fd = 0;
    }
    return 0;
}


int64_t mtd_basic_read(struct filesystem *fs) {
//     int64_t ret = true;
//     return ret;
// closeall:
//     LOGE("%s has crashed\n", __func__);
    return 0;
}