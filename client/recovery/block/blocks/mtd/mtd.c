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

#define LOG_TAG BM_BLOCK_TYPE_MTD
#define BM_GET_MTD_DESC(bm) (&bm->mtd_desc)
#define BM_GET_MTD_INFO(bm) (&bm->mtd_info)
#define BM_GET_PARTINFO(bm) (bm->part_info)
#define BM_GET_PARTINFO_START(bm, i) (&(bm->part_info[i].start))
#define BM_GET_PARTINFO_FD(bm, i) (&(bm->part_info[i].fd))
#define BM_GET_PARTINFO_PATH(bm, i) (bm->part_info[i].path)
#define BM_GET_PARTINFO_MTD_DEV(bm, i)  (&(bm->part_info[i].part.mtd_dev_info))
#define MTD_DEV_INFO_TO_FD(mtd)   container_of(mtd, struct bm_part_info, part.mtd_dev_info)->fd
#define MTD_DEV_INFO_TO_START(mtd)   container_of(mtd, struct bm_part_info, part.mtd_dev_info)->start
#define MTD_OFFSET_TO_EB_INDEX(mtd, off)   off/mtd->eb_size
#define MTD_BOUNDARY_IS_ALIGNED(mtd, off)  (off%mtd->eb_size == 0)

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

static inline struct mtd_dev_info* mtd_get_dev_info_by_offset(
    struct block_manager* this,
    long long offset) {
    long long size = 0;
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        size += mtd_dev_info->size;
        LOGI("%d start: 0x%llx, size: 0x%llx\n", i, size, mtd_dev_info->size);
        if (offset < size)
            return mtd_dev_info;
    }
    return NULL;
}

static inline struct mtd_dev_info* mtd_get_dev_info_by_node(
    struct block_manager* this,
    char *mtdchar) {
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        if (mtdchar && !strcmp(mtd_dev_info->name, mtdchar))
            return mtd_dev_info;
    }
    return NULL;
}

void mtd_scan_dump(struct block_manager* this, long long offset,
                    struct mtd_scan_info* mi) {
    int i;
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return;
    }
    if (mi || mi->es) {
        LOGE("Parameter mtd_scan_info is null\n");
        return;
    }
    for (i = 0; i < mtd->eb_cnt; i++) {
        if (mi->es[i] == MTD_BLK_BAD)
            LOGI("Block%d is bad\n", i);
    }
    return;
}

static long long mtd_get_partition_size_by_offset(struct block_manager* this,
        long long offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->size;
}

static long long mtd_get_partition_size_by_node(struct block_manager* this,
        char *mtdchar) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_node(this, mtdchar);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo by name:\"%s\"", mtdchar);
        return 0;
    }
    return mtd->size;
}

static long long mtd_get_capacity(struct block_manager* this) {
    long long size = 0;
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        size += mtd_dev_info->size;
    }
    return size;
}

int mtd_get_blocksize(struct block_manager* this, long long offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->eb_size;
}

static int mtd_type_is_nand_user(struct block_manager* this, long long offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

static int mtd_type_is_nor_user(struct block_manager* this, long long offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->type == MTD_NORFLASH;
}

long long mtd_nand_block_scan(struct block_manager* this,
                                     long long offset, long long length) {
    static struct mtd_scan_info *mi = NULL;
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    int fd = MTD_DEV_INFO_TO_FD(mtd);
    // long long partition_start = MTD_DEV_INFO_TO_START(mtd);
    // long long partition_size = mtd->size;
    long long eb, start, end;
    long long pass = 0;
    int retval;


    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx", offset);
        goto out;
    }
    if (mi == NULL) {
        mi = calloc(1, sizeof(struct mtd_scan_info));
        if (!mi) {
            LOGE("Cannot allocate %zd bytes of memory",
                 sizeof(struct mtd_scan_info));
            goto out;
        }
        int total_eb = mtd_get_capacity(this) / mtd->eb_size;
        mi->es = calloc(total_eb, sizeof(*mi->es));
        if (!mi->es) {
            LOGE("Cannot allocate %zd bytes of memory",
                 total_eb * sizeof(*mi->es));
            goto out;
        }
    }
    if (!MTD_BOUNDARY_IS_ALIGNED(mtd, offset) 
     || !MTD_BOUNDARY_IS_ALIGNED(mtd, offset+length)) {
        LOGE("Boundary is not alligned, left is %lld, right is %lld\n",
                 offset, offset+length);
        goto out;        
    }
    start = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    end = MTD_OFFSET_TO_EB_INDEX(mtd, offset+length);
    LOGI("Scan \"%s\" from eb%d to eb%d, total scaned length is %lld\n",
                mtd->name, (int)start, (int)end, (end-start)*mtd->eb_size);

    for (eb = start; eb < mtd->eb_cnt; eb++) {
        // if (mi->es[eb] & MTD_BLK_PREFIX)
        //     continue;
        if ((mi->es[eb] & MTD_BLK_PREFIX) 
            && mi->es[eb] != MTD_BLK_BAD) {
            pass += mtd->eb_size;
            if (pass >= length)
                break;
        }
        mi->es[eb] = MTD_BLK_SCAN;
        retval = mtd_is_bad(mtd, fd, eb);
        if (retval == -1)
            goto out;
        if (retval) {
            mi->bad_cnt += 1;
            mi->es[eb] = MTD_BLK_BAD;
            LOGI("Block%d is bad\n", (int)eb);
            continue;
        }
        pass += mtd->eb_size;
        if (pass >= length)
            break;
    }

    LOGI("Actually total %lld bytes is scaned\n", (eb-start)*mtd->eb_size);
    return (eb-start)*mtd->eb_size;
out:
    if (mi->es)
        free(mi->es);
    if (mi)
        free(mi);
    return 0;
}


long long mtd_calculate_max_mapped_size (struct block_manager* this,
        long long offset, long long length) {
    // struct mtd_scan_info*  mi = mtd_block_scan(this, offset);
    long long size = length;

    if (!mtd_type_is_nand_user(this, offset))
        goto exit;


exit:
    return size;
}

static int mtd_install_filesystem(struct block_manager* this) {
    BM_FILE_TYPE_INIT(user_list);
    int i;
    for (i = 0; i < sizeof(user_list) / sizeof(user_list[0]); i++) {
        if (!fs_register(&this->list_fs_head, fs_get_suppoted_by_name(user_list[i]))) {
            LOGE("Failed in register filesystem \"%s\"\n", user_list[i]);
            return false;
        }
    }
    return true;
}

static int mtd_uninstall_filesystem(struct block_manager* this) {
    int i;
    BM_FILE_TYPE_INIT(user_list);
    for (i = sizeof(user_list) / sizeof(user_list[0]) - 1; i >= 0; i--) {
        if (!fs_unregister(&this->list_fs_head, fs_get_suppoted_by_name(user_list[i]))) {
            LOGE("Failed in unregister filesystem \"%s\"\n", user_list[i]);
            return false;
        }
    }
    return true;
}

static int mtd_block_init(struct block_manager* this) {
    struct bm_part_info **part_info = &BM_GET_PARTINFO(this);
    libmtd_t *mtd_desc = BM_GET_MTD_DESC(this);
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;
    int retval = 0;
    long long size = 0;

    *mtd_desc = libmtd_open();
    if (!*mtd_desc) {
        LOGE("Failed to open libmtd");
        return false;
    }

    retval = mtd_get_info(*mtd_desc, mtd_info);
    if (retval < 0) {
        LOGE("Failed to get mtd info.");
        return false;
    }

    *part_info = (struct bm_part_info *)calloc(sizeof(struct bm_part_info), 
                                               mtd_info->mtd_dev_cnt);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        retval = mtd_get_dev_info1(*mtd_desc, i, mtd_dev_info);
        if (retval < 0) {
            LOGE("Can\'t get dev info at \"mtd%d\"\n", i);
            return false;
        }
        int *fd = BM_GET_PARTINFO_FD(this, i);
        char *path = BM_GET_PARTINFO_PATH(this, i);
        strcpy(path, mkpath(MTD_DEV_ROOT, mtd_dev_info->name));
        LOGI("Open mtd path: %s\n", path);
        *fd = open(path, O_RDWR);
        if (*fd < 0) {
            LOGE("Can't open mtd device %s: %s\n", path, strerror(errno));
            return false;
        }
        long long* off = BM_GET_PARTINFO_START(this, i);
        *off = size;
        size += mtd_dev_info->size;
    }

    return true;
}

static int mtd_block_exit(struct block_manager* this) {
    struct bm_part_info **part_info = &BM_GET_PARTINFO(this);
    libmtd_t *mtd_desc = BM_GET_MTD_DESC(this);
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;

    if (*part_info) {
        for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
            int *fd = BM_GET_PARTINFO_FD(this, i);
            if (*fd) {
                LOGI("Close file descriptor %d\n", *fd);
                close(*fd);
                *fd= 0;
            }
        }
        free(*part_info);
        *part_info = NULL;  
    }

    if (*mtd_desc) {
        libmtd_close(*mtd_desc);
        *mtd_desc = NULL;
    }

    return true;
}

static struct fs_operation_params* convert_params(struct block_manager* this, char *buf,
        long long offset, long long length, struct bm_operation_option *option) {
    static struct fs_operation_params  fs_params;

    memset(&fs_params, 0, sizeof(fs_params));
    fs_params.buf = buf;
    fs_params.offset = offset;
    fs_params.length = length;
    if (option) {
        fs_params.operation_method = option->method;
        fs_params.fstype = option->filetype;
    }
    fs_params.blksize = mtd_get_blocksize(this, offset);
    fs_params.priv = this;
    return &fs_params;
}

static int mtd_block_erase(struct block_manager* this, long long offset,
                           long long length, struct bm_operation_option *option) {
    char *buf = NULL;
    char *default_filetype = BM_FILE_TYPE_NORMAL;
    struct filesystem *fs = NULL;

    if (this == NULL) {
        LOGE("Parameter \'block_manager\' is null\n");
        return false;
    }
    if (option && strcmp(option->filetype, default_filetype)) {
        default_filetype = option->filetype;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head , default_filetype);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             default_filetype);
        return false;
    }
    fs_set_parameter(fs, convert_params(this, buf, offset, length, option));
    return fs->erase(fs);
}

static int mtd_block_write(struct block_manager* this, long long offset, char* buf,
                           long long length,  struct bm_operation_option *option) {
    char *default_filetype = BM_FILE_TYPE_NORMAL;
    struct filesystem *fs = NULL;

    if (this == NULL) {
        LOGE("Parameter \'block_manager\' is null\n");
        return false;
    }
    if (buf == NULL) {
        LOGE("Parameter \'buf\' is null\n");
        return false;
    }
    if (option && strcmp(option->filetype, default_filetype)) {
        default_filetype = option->filetype;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head, default_filetype);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             default_filetype);
        return false;
    }
    fs_set_parameter(fs, convert_params(this, buf, offset, length, option));
    return fs->write(fs);
}

static int mtd_block_read(struct block_manager* this, long long offset, char* buf,
                          long long length, struct bm_operation_option *option) {
    char *default_filetype = BM_FILE_TYPE_NORMAL;
    struct filesystem *fs = NULL;

    if (this == NULL) {
        LOGE("Parameter \'block_manager\' is null\n");
        return false;
    }
    if (buf == NULL) {
        LOGE("Parameter \'buf\' is null\n");
        return false;
    }
    if (option && strcmp(option->filetype, default_filetype)) {
        default_filetype = option->filetype;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head, default_filetype);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             default_filetype);
        return false;
    }
    fs_set_parameter(fs, convert_params(this, buf, offset, length, option));
    return fs->read(fs);
}

static struct bm_operate_prepare_info* mtd_get_prepare_info(
    struct block_manager* this, ...) {
    static struct bm_operate_prepare_info prepare_info;
    va_list args;
    if (this->prepared == NULL) {
        va_start(args, this);
        prepare_info.write_start = va_arg(args, long long);
        prepare_info.physical_unit_size = va_arg(args, unsigned long);
        prepare_info.logical_unit_size = va_arg(args, unsigned long);
        prepare_info.max_size_mapped_in_partition = va_arg(args, long long);
        this->prepared = &prepare_info;
        va_end(args);
        LOGI("Set info bm_operate_prepare_info \n");
    }
    return this->prepared;
}

static void mtd_put_prepare_info(struct block_manager* this) {
    if (this->prepared) {
        this->prepared = NULL;
    }
    return;
}

static struct bm_operate_prepare_info* mtd_block_prepare(
    struct block_manager* this,
    long long offset,
    long long length,
    struct bm_operation_option *option) {
    char *buf = NULL;

    char *default_filetype = BM_FILE_TYPE_NORMAL;

    struct filesystem *fs = NULL;
    if (this == NULL) {
        LOGE("Parameter \'block_manager\' is null\n");
        return NULL;
    }
    if (option && strcmp(option->filetype, default_filetype)) {
        default_filetype = option->filetype;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head, default_filetype);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             default_filetype);
        return NULL;
    }

    fs_set_parameter(fs, convert_params(this, buf, offset, length, option));
    return mtd_get_prepare_info(this,
                                fs->get_operate_start_address(fs),
                                mtd_get_blocksize(this, offset),
                                fs->get_leb_size(fs),
                                fs->get_max_mapped_size_in_partition(fs));
}

unsigned long mtd_get_prepare_leb_size(struct block_manager* this) {
    return this->prepared->logical_unit_size;
}

unsigned long mtd_get_max_size_mapped_in(struct block_manager* this) {
    return this->prepared->max_size_mapped_in_partition;
}

static int mtd_block_finish(struct block_manager* this) {

    // struct bm_operate_prepare_info *info = mtd_get_prepare_info(this);

    // mtd_put_prepare_info(this);
    return true;
}

static struct block_manager mtd_manager = {
    .name = BM_BLOCK_TYPE_MTD,
    .erase = mtd_block_erase,
    .read = mtd_block_read,
    .write = mtd_block_write,
    .prepare = mtd_block_prepare,
    .get_prepare_leb_size = mtd_get_prepare_leb_size,
    .get_max_size_mapped_in = mtd_get_max_size_mapped_in,
    .finish = mtd_block_finish,
    .get_partition_size_by_offset = mtd_get_partition_size_by_offset,
    .get_partition_size_by_node = mtd_get_partition_size_by_node,
    .get_capacity = mtd_get_capacity,
    .get_blocksize = mtd_get_blocksize,
};

int mtd_manager_init(void) {
    if (mtd_install_filesystem(&mtd_manager)
            && register_block_manager(&mtd_manager)
            && mtd_block_init(&mtd_manager)) {
        return true;
    }
    return false;
}

int mtd_manager_destroy(void) {
    if (mtd_block_exit(&mtd_manager)
            && unregister_block_manager(&mtd_manager)
            && mtd_uninstall_filesystem(&mtd_manager)) {
        return true;
    }
    return false;
}
