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

int mtd_get_blocksize_by_offset(struct block_manager* this, long long offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->eb_size;
}


int mtd_type_is_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_mlc_nand(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_MLCNANDFLASH;
}

int mtd_type_is_nor(struct mtd_dev_info *mtd) {
    return mtd->type == MTD_NORFLASH;
}

void mtd_scan_dump(struct block_manager* this, long long offset,
                    struct mtd_nand_map* mi) {
    int i;
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return;
    }
    if (mi || mi->es) {
        LOGE("Parameter mtd_nand_map is null\n");
        return;
    }
    for (i = 0; i < mtd->eb_cnt; i++) {
        if (mi->es[i] == MTD_BLK_BAD)
            LOGI("Block%d is bad\n", i);
    }
    return;
}

static int mtd_install_filesystem(struct block_manager* this) {
    BM_FILE_TYPE_INIT(user_list);
    int i;
    for (i = 0; i < sizeof(user_list) / sizeof(user_list[0]); i++) {
        struct filesystem* fs = fs_get_suppoted_by_name(user_list[i]);
        if (fs == NULL) {
            LOGE("Cannot get filesystem \"%s\"\n", user_list[i]);
            return false;
        }
        if (!fs_register(&this->list_fs_head, fs)) {
            LOGE("Failed in register filesystem \"%s\"\n", user_list[i]);
            return false;
        }
        LOGI("filesystem \"%s\" is installed\n", user_list[i]);
        FS_SET_PRIVATE(fs, this);
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
    libmtd_t *mtd_desc = &BM_GET_MTD_DESC(this);
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
                                               mtd_info->mtd_dev_cnt+1);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        retval = mtd_get_dev_info1(*mtd_desc, i, mtd_dev_info);
        if (retval < 0) {
            LOGE("Can\'t get dev info at \"mtd%d\"\n", i);
            return false;
        }
        int *fd = BM_GET_PARTINFO_FD(this, i);
        char *path = BM_GET_PARTINFO_PATH(this, i);
        strcpy(BM_GET_PARTINFO_PATH(this, i), 
            mkpath(MTD_DEV_ROOT, mtd_dev_info->name));
        LOGI("Open mtd path: %s\n", path);
        *fd = open(path, O_RDWR);
        if (*fd < 0) {
            LOGE("Can't open mtd device %s: %s\n", path, strerror(errno));
            return false;
        }
        BM_GET_PARTINFO_ID(this, i) = i;
        BM_GET_PARTINFO_START(this, i) = size;
        size += mtd_dev_info->size;
    }
    BM_GET_PARTINFO_ID(this, i) = mtd_info->mtd_dev_cnt;
    BM_GET_PARTINFO_START(this, i) = size;
    return true;
}

static int mtd_block_exit(struct block_manager* this) {
    struct bm_part_info **part_info = &BM_GET_PARTINFO(this);
    libmtd_t *mtd_desc = &BM_GET_MTD_DESC(this);
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

static void dump_convert_params(struct block_manager* this, 
                        struct fs_operation_params  *params) {
    LOGI("operation_method = %d\n", params->operation_method);
    LOGI("buf = 0x%x\n", (unsigned int)params->buf);
    LOGI("offset = 0x%llx\n", params->offset);
    LOGI("offset = %lld\n", params->length);
    LOGI("mtd = 0x%x\n", (unsigned int)params->mtd);
}

static struct fs_operation_params* convert_params(struct block_manager* this, char *buf,
        long long offset, long long length, struct bm_operation_option *option) {
    static struct fs_operation_params fs_params;
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    memset(&fs_params, 0, sizeof(fs_params));

    
    fs_params.buf = buf;
    fs_params.offset = offset;
    fs_params.length = length;
    if (option) {
        fs_params.operation_method = option->method;
    }
    fs_params.mtd = mtd;
    return &fs_params;
}

static long long mtd_block_erase(struct block_manager* this, long long offset,
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
    FS_SET_PARAMS(fs, convert_params(this, buf, offset, length, option));
    return fs->erase(fs);
}

static long long mtd_block_write(struct block_manager* this, long long offset, char* buf,
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
    FS_SET_PARAMS(fs, convert_params(this, buf, offset, length, option));
    return fs->write(fs);
}

static long long mtd_block_read(struct block_manager* this, long long offset, char* buf,
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
    FS_SET_PARAMS(fs, convert_params(this, buf, offset, length, option));
    return fs->read(fs);
}

static struct bm_operate_prepare_info* mtd_get_prepare_info(
    struct block_manager* this, ...) {
    static struct bm_operate_prepare_info prepare_info;
    struct filesystem *fs = NULL;
    va_list args;
    if (this->prepared == NULL) {
        va_start(args, this);
        fs = va_arg(args, struct filesystem *);
        prepare_info.write_start = va_arg(args, long long);
        prepare_info.physical_unit_size = va_arg(args, unsigned long);
        prepare_info.logical_unit_size = va_arg(args, unsigned long);
        prepare_info.max_size_mapped_in_partition = va_arg(args, long long);
        // fs_set_content_boundary(fs, prepare_info.max_size_mapped_in_partition,
        //                         prepare_info.write_start);
        
        fs->init(fs);
        FS_GET_PARAM(fs)->content_start = prepare_info.write_start;
        FS_GET_PARAM(fs)->max_mapped_size = prepare_info.max_size_mapped_in_partition;
        this->prepared = &prepare_info;
        va_end(args);
        LOGI("Set info bm_operate_prepare_info \n");
    }
    return &prepare_info;
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

    FS_SET_PARAMS(fs, convert_params(this, buf, offset, length, option));
    return mtd_get_prepare_info(this,
                                fs,
                                fs->get_operate_start_address(fs),
                                mtd_get_blocksize_by_offset(this, offset),
                                fs->get_leb_size(fs),
                                fs->get_max_mapped_size_in_partition(fs));
}

void mtd_switch_prepare_context(struct block_manager* this, 
            struct bm_operate_prepare_info* prepared) {
    this->prepared = prepared;
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
    .switch_prepare_context = mtd_switch_prepare_context,
    .get_prepare_leb_size = mtd_get_prepare_leb_size,
    .get_prepare_max_mapped_size = mtd_get_max_size_mapped_in,
    .finish = mtd_block_finish,
    .get_partition_size_by_offset = mtd_get_partition_size_by_offset,
    .get_partition_size_by_node = mtd_get_partition_size_by_node,
    .get_capacity = mtd_get_capacity,
    .get_blocksize = mtd_get_blocksize_by_offset,
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
