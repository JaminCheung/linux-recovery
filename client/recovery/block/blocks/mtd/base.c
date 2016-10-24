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

#define LOG_TAG "mtd_base"

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

int mtd_nand_map_is_valid(struct filesystem *fs, long long eb){
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    long long mtd_start = MTD_DEV_INFO_TO_START(mtd);
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

int mtd_nand_block_is_bad(struct filesystem *fs, long long eb){
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    long long mtd_start = MTD_DEV_INFO_TO_START(mtd);
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

int mtd_nand_block_is_erased(struct filesystem *fs, long long eb) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    long long mtd_start = MTD_DEV_INFO_TO_START(mtd);
    if (nand_map->es[eb+mtd_start] == MTD_BLK_ERASED)
        return true;
    return false;
}

int mtd_nand_block_is_set(struct filesystem *fs, long long eb, int status) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);

    long long mtd_start = MTD_DEV_INFO_TO_START(mtd);

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

long long mtd_block_scan(struct filesystem *fs) {
    struct block_manager *this = FS_GET_BM(fs);
    long long offset = fs->params->offset;
    long long length = fs->params->length;
    struct mtd_nand_map **mi = BM_GET_MTD_NAND_MAP(this, struct mtd_nand_map);
    // struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    // long long partition_start = MTD_DEV_INFO_TO_START(mtd);
    // long long partition_size = mtd->size;
    long long eb, mtd_start, start, end, total_bytes;
    long long pass = 0;
    int retval;

    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        goto out;
    }

    if (!length) {
        LOGE("Parameter length is zero\n");
        goto out;
    }

    if (!mtd_nand_map_init(fs))
        goto out;
    // if (*mi == NULL) {
    //     *mi = calloc(1, sizeof(struct mtd_nand_map));
    //     if (!*mi) {
    //         LOGE("Cannot allocate %zd bytes of memory",
    //              sizeof(struct mtd_nand_map));
    //         goto out;
    //     }
    //     int total_eb = mtd_get_capacity(this) / mtd->eb_size;
    //     (*mi)->es = calloc(total_eb, sizeof(*((*mi)->es)));
    //     if (!(*mi)->es) {
    //         LOGE("Cannot allocate %zd bytes of memory",
    //              total_eb * sizeof((*mi)->es));
    //         goto out;
    //     }
    // }
    if (!MTD_BOUNDARY_IS_ALIGNED(mtd, offset)
     || !MTD_BOUNDARY_IS_ALIGNED(mtd, offset+length)) {
        LOGE("Boundary is not block alligned, left is %lld, right is %lld\n",
                 offset, offset+length);
        LOGE("Blocksize is %d\n", mtd->eb_size);
        goto out;
    }
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = MTD_OFFSET_TO_EB_INDEX(mtd, offset+length+mtd->eb_size-1);
    total_bytes = (end - start)*mtd->eb_size;
    LOGI("Scan \"%s\" from eb%d to eb%d, total scaned length is %lld\n",
                mtd->name, (int)start, (int)end, total_bytes);
    mtd_start = MTD_OFFSET_TO_EB_INDEX(mtd, MTD_DEV_INFO_TO_START(mtd));

    if (mtd_type_is_nor(mtd)) {
        LOGI("Actually total %lld bytes is returned on faked scaning\n", 
            total_bytes);
        return total_bytes;
    }
    for (eb = start - mtd_start; eb <= mtd->eb_cnt; eb++) {
        // if ((*mi)->es[eb] & MTD_BLK_PREFIX)
        //     continue;
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

    LOGI("Actually total %lld bytes is scaned\n", (eb-start + mtd_start)*mtd->eb_size);
    return (eb-start + mtd_start)*mtd->eb_size;
out:
    return 0;
}

static void set_process_info(struct filesystem *fs, 
        int type, long long eboff, long long ebcnt) {
    struct block_manager *bm = FS_GET_BM(fs);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int progress = (int)(eboff * 100 / ebcnt);
    struct bm_event info;
    char *op_name = NULL;

    info.mtdchar = (char*)mtd->name;
    info.operation = type;
    info.progress = progress;

    if (type == BLOCK_OPERATION_ERASE) {
        op_name = "Erasing";
    }else if (type == BLOCK_OPERATION_WRITE) {
        op_name = "Writing";
    }else if (type == BLOCK_OPERATION_READ) {
        op_name = "Reading";
    }

    LOGI("%s procent is %d%% at %s\n", op_name, progress, mtd->name);
    if (BM_GET_LISTENER(bm))
        BM_GET_LISTENER(bm)(bm, &info);
}

long long mtd_basic_erase(struct filesystem *fs) {
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t mtd_desc = BM_GET_MTD_DESC(bm);
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);

    struct mtd_nand_map *nand_map = *BM_GET_MTD_NAND_MAP(bm, struct mtd_nand_map);
    int is_nand = mtd_type_is_nand(mtd);

    int op_type = fs->params->operation;
    int *fd = &MTD_DEV_INFO_TO_FD(mtd);
    
    long long start, end, total_bytes, mtd_start;
    long long offset, eb;

    struct jffs2_unknown_node cleanmarker;
    int clmpos, clmlen;
    int is_jffs2 = !strcmp(fs->name, BM_FILE_TYPE_JFFS2);

    if (is_jffs2 && mtd_type_is_mlc_nand(mtd)){
        LOGE("JFFS2 cannot support MLC NAND\n");
        goto closeall;
    }

    if (!jffs2_init_cleanmaker(fs, &cleanmarker, &clmpos, &clmlen))
        goto closeall;

    offset = MTD_DEV_INFO_TO_START(mtd);
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = (fs->params->operation_method == BLOCK_OPERATION_METHOD_PARTITION)
        ?BM_GET_PARTINFO_START(bm, (MTD_DEV_INFO_TO_ID(mtd)+1))-1
        :MTD_OFFSET_TO_EB_INDEX(mtd, start*mtd->eb_size + fs->params->max_mapped_size);
    total_bytes = (end - start)*mtd->eb_size;
    LOGI("Scan \"%s\" from eb%lld to eb%lld, total scaned length is %lld\n",
                mtd->name, start, end, total_bytes);
    mtd_start = MTD_OFFSET_TO_EB_INDEX(mtd, MTD_DEV_INFO_TO_START(mtd));
    start -= mtd_start;
    end -= mtd_start;
    LOGI("Going to erase from eb%lld to eb%lld, total length is %lld bytes\n",
            start+mtd_start, end+mtd_start, total_bytes);
    for (eb = start; eb <= end; eb++) {
        offset = (long long)eb * mtd->eb_size;

        set_process_info(fs, op_type, eb-start, end-start);
        if (is_nand){
            if (mtd_nand_map_is_valid(fs, eb))
                goto closeall;
            if (mtd_nand_block_is_bad(fs, eb))
                continue;
            // if (nand_map->es == NULL) {
            //     LOGE("Nand map table is null\n");
            //     goto closeall;
            // }

            // if ((nand_map->es[eb+mtd_start] & MTD_BLK_PREFIX) 
            //     != MTD_BLK_PREFIX) {
            //     LOGE("Nand map table state is error at eb%d\n", eb+mtd_start);
            //     goto closeall;
            // } 
            // if (nand_map->es[eb+mtd_start] == MTD_BLK_BAD) {
            //     LOGI("Skipping bad block at %llx", (eb+mtd_start));
            //     continue;
            // }
        }

        if (FS_FLAG_IS_SET(fs, UNLOCK)) {
            if (mtd_unlock(mtd, *fd, eb) != 0) {
                LOGE("MTD \"%s\" unlock failure", mtd->name);
                continue;
            }
        }
        if (mtd_erase(mtd_desc, mtd, *fd, eb) != 0) {
            LOGE("MTD \"%s\" Erase unlock failure", mtd->name);
            continue;
        }
        nand_map->es[eb+mtd_start] = MTD_BLK_ERASED;
        /* format for JFFS2 ? */
        if (!is_jffs2)
            continue;

        if (!jffs2_write_cleanmaker(fs, offset, &cleanmarker, clmpos, clmlen))
            continue;

        LOGI("Cleanmarker written at %x", (unsigned int)offset);
    }
    set_process_info(fs, op_type, eb-start, end-start);

    start = (eb+mtd_start)*mtd->eb_size;
    LOGI("Next erase offset will start at 0x%llx\n", start);
    return start;
closeall:
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

long long mtd_basic_write(struct filesystem *fs) {
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
    LOGI("Write at 0x%llx with length %lld, source buffer %p\n"
         "mtd start at 0x%llx, relative write offset 0x%llx\n", 
            w_offset,w_length, w_buffer, mtd_start, w_offset);
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
            mtd->name, w_offset, w_length);
    while (w_length > 0 && w_offset < mtd->size) {
        while (blockstart != MTD_BLOCK_ALIGN(mtd, w_offset)) {
            blockstart = MTD_BLOCK_ALIGN(mtd, w_offset);
            LOGI("Writing data to block %lld at offset 0x%llx",
                     blockstart, blockstart);
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
    LOGI("write done\n");
    LOGI("Next write offset will start at 0x%llx\n", w_offset + mtd_start);
    return w_offset + mtd_start;
closeall:
    if (*fd){
        close(*fd);
        *fd = 0;
    }
    return 0;
}


long long mtd_basic_read(struct filesystem *fs) {
    long long ret;
    return ret;
}