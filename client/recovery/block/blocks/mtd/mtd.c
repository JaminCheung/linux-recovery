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

#define LOG_TAG BM_BLOCK_TYPE_MTD

static inline struct mtd_dev_info* mtd_get_dev_info_by_offset(
    struct block_manager* this,
    int64_t offset) {
    int64_t size = 0;
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        LOGI("%d start: 0x%llx, size: 0x%llx\n", i, size, mtd_dev_info->size);
        size += mtd_dev_info->size;
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

static int64_t mtd_get_partition_size_by_offset(struct block_manager* this,
        int64_t offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->size;
}

static int64_t mtd_get_partition_size_by_node(struct block_manager* this,
        char *mtdchar) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_node(this, mtdchar);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo by name:\"%s\"", mtdchar);
        return 0;
    }
    return mtd->size;
}

static int64_t mtd_get_capacity(struct block_manager* this) {
    int64_t size = 0;
    int i;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        size += mtd_dev_info->size;
    }
    return size;
}

int mtd_get_blocksize_by_offset(struct block_manager* this, int64_t offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGI("Cannot get mtd devinfo at 0x%llx", offset);
        return 0;
    }
    return mtd->eb_size;
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

#ifdef MTD_OPEN_DEBUG
static void dump_mtd_dev_info(struct block_manager* this) {
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;

    LOGI("Partinfo dumped:\n");
    for (i = 0; i < mtd_info->mtd_dev_cnt + 1; i++) {
        LOGI("id: %d, path: %s, fd: %d, start: 0x%llx\n", 
                BM_GET_PARTINFO_ID(this, i), 
                BM_GET_PARTINFO_PATH(this, i),
                BM_GET_PARTINFO_FD(this, i)
                BM_GET_PARTINFO_START(this, i));
    }
}
#endif

static int mtd_block_init(struct block_manager* this) {
    struct bm_part_info **part_info = &BM_GET_PARTINFO(this);
    libmtd_t *mtd_desc = &BM_GET_MTD_DESC(this);
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;
    int retval = 0;
    int64_t size = 0;

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
                 mtd_info->mtd_dev_cnt + 1);
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {

        // if (i == (mtd_info->mtd_dev_cnt - 1)) {
        //     printf("bypass rootfs system temporarily\n");
        //     continue;
        // }
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        retval = mtd_get_dev_info1(*mtd_desc, i, mtd_dev_info);
        if (retval < 0) {
            LOGE("Can\'t get dev info at \"mtd%d\"\n", i);
            return false;
        }
        int *fd = BM_GET_PARTINFO_FD(this, i);
        char *path = BM_GET_PARTINFO_PATH(this, i);
        sprintf(path, "%s%d", MTD_CHAR_HEAD, mtd_dev_info->mtd_num);
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
#ifdef MTD_OPEN_DEBUG
    dump_mtd_dev_info(this);
#endif
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
                *fd = 0;
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

#ifdef MTD_OPEN_DEBUG
static void dump_convert_params(struct block_manager* this,
                                struct fs_operation_params  *params) {
    LOGI("operation_method = %d\n", params->operation_method);
    LOGI("buf = 0x%x\n", (unsigned int)params->buf);
    LOGI("offset = 0x%llx\n", params->offset);
    LOGI("offset = %lld\n", params->length);
    LOGI("mtd = 0x%x\n", (unsigned int)params->mtd);
}
#endif

static struct filesystem * prepare_convert_params(struct block_manager* this, struct filesystem *fs,
        int64_t offset, int64_t length, struct bm_operation_option *option) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);

    assert_die_if(!fs->alloc_params(fs),
                  "Cannot alloc parameters for fs \'%s\'\n", fs->name);

    fs->params->offset = offset;
    fs->params->length = length;
    if (option) {
        fs->params->operation_method = option->method;
    }
    fs->params->mtd = mtd;
    return fs;
}

static int mtd_chip_erase(struct block_manager *this) {
    struct filesystem *fs  = NULL;
    char *fs_get_name = "normal";
    int64_t offset, length;
    struct bm_operation_option *option;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;

    option = this->set_operation_option(this,
                                        BM_OPERATION_METHOD_PARTITION, fs_get_name);
    if (option == NULL) {
        LOGE("Cannot set block option\n");
        goto out;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head , fs_get_name);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             fs_get_name);
        goto out;
    }

    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd = BM_GET_PARTINFO_MTD_DEV(this, i);
        length = mtd->size;
        offset = BM_GET_PARTINFO_START(this, i);
        prepare_convert_params(this, fs, offset, length, option);
        if (mtd_type_is_nand(mtd)) {
            FS_FLAG_SET(fs, NOSKIPBAD);
        }
        if (fs->erase(fs) <= 0) {
            LOGE("Cannot erase block with starting at 0x%llx, size 0x%llx \n", offset, length);
            goto out;
        }
    }
    fs->free_params(fs);
    return true;
out:
    if (fs != NULL)
        fs->free_params(fs);
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return false;
}


static struct filesystem* data_transfer_params_set(struct block_manager* this, int64_t offset,
        char *buf, int64_t length) {
    struct filesystem *fs = NULL;

    fs = BM_GET_PREPARE_INFO_CONTEXT(this);
    if (fs == NULL) {
        LOGE("Prepare info context_handle is lost\n");
        return NULL;
    }
    fs->params->offset = offset;
    fs->params->buf = buf;
    fs->params->length = length;
    return fs;
}

static int64_t mtd_block_erase(struct block_manager* this, int64_t offset,
                               int64_t length) {
    struct filesystem *fs = NULL;
    char *buf = NULL;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;
    return fs->erase(fs);
out:
    if (fs != NULL)
        fs->free_params(fs);
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return 0;
}

static int64_t mtd_block_write(struct block_manager* this, int64_t offset, char* buf,
                               int64_t length) {
    struct filesystem *fs = NULL;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;
    return fs->write(fs);
out:
    if (fs != NULL)
        fs->free_params(fs);
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return 0;
}

static int64_t mtd_block_read(struct block_manager* this, int64_t offset, char* buf,
                              int64_t length) {
    struct filesystem *fs = NULL;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;
    return fs->read(fs);
out:
    if (fs != NULL)
        fs->free_params(fs);
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return 0;
}

#ifdef MTD_OPEN_DEBUG
static void dump_prepared_info(struct bm_operate_prepare_info *prepare) {
    LOGI("write_start = 0x%llx\n", prepare->write_start);
    LOGI("physical_unit_size = %ld\n", prepare->physical_unit_size);
    LOGI("logical_unit_size = %ld\n", prepare->logical_unit_size);
    LOGI("max_size_mapped_in_partition = %lld\n", prepare->max_size_mapped_in_partition);
    return;
}
#endif
static struct bm_operate_prepare_info* mtd_get_prepare_info(
    struct block_manager* this, ...) {
    static struct bm_operate_prepare_info prepare_info;
    struct filesystem *fs = NULL;
    va_list args;
    if (this->prepared == NULL) {
        this->prepared = &prepare_info;
    }

    va_start(args, this);
    fs = va_arg(args, struct filesystem *);
    prepare_info.write_start = va_arg(args, int64_t);
    prepare_info.physical_unit_size = va_arg(args, uint32_t);
    prepare_info.logical_unit_size = va_arg(args, uint32_t);
    prepare_info.max_size_mapped_in_partition = va_arg(args, int64_t);
    if (!prepare_info.physical_unit_size
            || !prepare_info.logical_unit_size
            || !prepare_info.max_size_mapped_in_partition
       ) 
        goto out;
    
    prepare_info.context_handle = fs;
    FS_GET_PARAM(fs)->content_start = prepare_info.write_start;
    FS_GET_PARAM(fs)->max_mapped_size = prepare_info.max_size_mapped_in_partition;

    this->prepared = &prepare_info;
    va_end(args);

#ifdef MTD_OPEN_DEBUG
    dump_prepared_info(&prepare_info);
#endif
    return &prepare_info;
out:
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return NULL;
}

static int mtd_put_prepare_info(struct block_manager* this) {
    struct filesystem *fs = NULL;
    fs = BM_GET_PREPARE_INFO_CONTEXT(this);

    if (fs == NULL) {
        LOGE("Prepare info context_handle is lost\n");
        return false;
    }

    fs->free_params(fs);
    if (this->prepared) {
        this->prepared = NULL;
    }
    return true;
}

static struct bm_operate_prepare_info* mtd_block_prepare(
    struct block_manager* this,
    int64_t offset,
    int64_t length,
    struct bm_operation_option *option) {

    char *default_filetype = BM_FILE_TYPE_NORMAL;

    struct filesystem *fs = NULL;
    if (this == NULL) {
        LOGE("Parameter \'block_manager\' is null\n");
        goto out;
    }
    if (option && strcmp(option->filetype, default_filetype)) {
        default_filetype = option->filetype;
    }
    fs = fs_get_registered_by_name(&this->list_fs_head, default_filetype);
    if (fs == NULL) {
        LOGE("Filetype:\"%s\" is not supported yet\n",
             default_filetype);
        goto out;
    }

    prepare_convert_params(this, fs, offset, length, option);
    return mtd_get_prepare_info(this,
                                fs,
                                fs->get_operate_start_address(fs),
                                mtd_get_blocksize_by_offset(this, offset),
                                fs->get_leb_size(fs),
                                fs->get_max_mapped_size_in_partition(fs));
out:
    if (fs != NULL)
        fs->free_params(fs);
    assert_die_if(1, "%s:%s:%d run crashed\n", __FILE__, __func__, __LINE__);
    return false;
}

// void mtd_switch_prepare_context(struct block_manager* this,
//             struct bm_operate_prepare_info* prepared) {
//     this->prepared = prepared;
// }

uint32_t mtd_get_prepare_leb_size(struct block_manager* this) {
    return this->prepared->logical_unit_size;
}

int64_t mtd_get_prepare_write_start(struct block_manager* this) {
    return this->prepared->write_start;
}

int64_t mtd_get_max_size_mapped_in(struct block_manager* this) {
    return this->prepared->max_size_mapped_in_partition;
}

static int mtd_block_finish(struct block_manager* this) {

    // struct bm_operate_prepare_info *info = mtd_get_prepare_info(this);

    mtd_put_prepare_info(this);
    return true;
}

static struct block_manager mtd_manager =  {
    .name = BM_BLOCK_TYPE_MTD,
    .chip_erase = mtd_chip_erase,
    .erase = mtd_block_erase,
    .read = mtd_block_read,
    .write = mtd_block_write,
    .prepare = mtd_block_prepare,
    // .switch_prepare_context = mtd_switch_prepare_context,
    .get_prepare_leb_size = mtd_get_prepare_leb_size,
    .get_prepare_write_start = mtd_get_prepare_write_start,
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
