#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>
#include <types.h>
#include <utils/assert.h>
#include <utils/log.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/sysinfo/sysinfo_manager.h>
#include <block/fs/fs_manager.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG BM_BLOCK_TYPE_MTD

static pthread_mutex_t mtd_block_lock = PTHREAD_MUTEX_INITIALIZER;

struct mtd_dev_info* mtd_get_dev_info_by_offset( struct block_manager* this,
        int64_t offset) {

    int64_t size = 0;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);

    for (int i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        size += mtd_dev_info->size;
        if (offset < size)
            return mtd_dev_info;
    }

    return NULL;
}

static inline struct mtd_dev_info* mtd_get_dev_info_by_node(
    struct block_manager* this, char *mtdchar) {

    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    for (int i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        if (mtdchar && !strcmp(mtd_dev_info->name, mtdchar))
            return mtd_dev_info;
    }

    return NULL;
}

static inline struct mtd_dev_info* mtd_get_dev_info_by_mtdstr(
    struct block_manager* this, char *name) {
    struct mtd_info *mtd_info;
    int len =  strlen(BM_BLOCK_TYPE_MTD);
    int i;

    mtd_info = BM_GET_MTD_INFO(this);
    if ((name == NULL) || strncmp(name,
                                  BM_BLOCK_TYPE_MTD, len)) {
        LOGE("Parameter %s is wrong\n", name);
        return NULL;
    }

    if (strlen(name) != (len + 1)) {
        LOGE("Parameter length must be %d\n", (len + 1));
        return NULL;
    }
    if (!isdigit(name[len])) {
        LOGE("Parameter %s is wrong\n", name);
        return NULL;
    }

    i = strtoul(&name[len], NULL, 10);
    if (i > mtd_info->mtd_dev_cnt) {
        LOGE("Parameter %s is over the right limit of mtd\n", name);
        return NULL;
    }

    return BM_GET_PARTINFO_MTD_DEV(this, i);
}

static int64_t mtd_get_partition_size_by_offset(struct block_manager* this,
        int64_t offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return -1;
    }

    return mtd->size;
}

static int mtd_get_partition_count(struct block_manager* this) {
    struct mtd_info *mtd_info;
    if (this == NULL) {
        LOGE("Parameter block_manager is null\n");
        return -1;
    }

    mtd_info = BM_GET_MTD_INFO(this);

    return mtd_info->mtd_dev_cnt;
}

static int64_t mtd_get_partition_size_by_name(struct block_manager* this,
        char *name) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_mtdstr(this, name);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo by name \'%s'\n", name);
        return -1;
    }

    return mtd->size;
}

static int64_t mtd_get_partition_start_by_offset(struct block_manager* this,
        int64_t offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return -1;
    }

    return MTD_DEV_INFO_TO_START(mtd);
}

static int64_t mtd_get_partition_start_by_name(struct block_manager* this,
        char *name) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_mtdstr(this, name);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo by name \'%s'\n", name);
        return -1;
    }

    return MTD_DEV_INFO_TO_START(mtd);
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
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return -1;
    }

    return mtd->eb_size;
}

int mtd_get_pagesize_by_offset(struct block_manager* this, int64_t offset) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx\n", offset);
        return -1;
    }

    return mtd->min_io_size;
}

static int mtd_install_filesystem(struct block_manager* this) {
    BM_MTD_FILE_TYPE_INIT(user_list);
    struct list_head *head = &this->list_fs_head;

    if (head->next == NULL || head->prev == NULL)
        INIT_LIST_HEAD(head);

    for (int i = 0; i < ARRAY_SIZE(user_list); i++) {
        struct filesystem* fs = fs_get_suppoted_by_name(user_list[i]);
        if (fs == NULL) {
            LOGE("Cannot get filesystem \"%s\"\n", user_list[i]);
            return -1;
        }

        if (fs_register(head, fs) < 0) {
            LOGE("Failed in register filesystem \"%s\"\n", user_list[i]);
            return -1;
        }

        LOGD("filesystem \"%s\" is installed\n", user_list[i]);
    }

    return 0;
}

static int mtd_uninstall_filesystem(struct block_manager* this) {
    struct filesystem* fs;

    BM_MTD_FILE_TYPE_INIT(user_list);

    for (int i = ARRAY_SIZE(user_list) - 1; i >= 0; i--) {

        fs = fs_get_registered_by_name(&this->list_fs_head, user_list[i]);
        if (fs == NULL)
            continue;

        if (fs_unregister(&this->list_fs_head, fs) < 0) {
            LOGE("Failed in unregister filesystem \"%s\"\n", user_list[i]);
            return -1;
        }

    }

    return 0;
}

#ifdef MTD_OPEN_DEBUG
static void dump_mtd_dev_info(struct block_manager* this) {
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    int i;

    LOGD("Partinfo dumped:\n");
    for (i = 0; i < mtd_info->mtd_dev_cnt + 1; i++) {
        LOGD("id: %d, path: %s, fd: %d, start: 0x%llx\n",
             BM_GET_PARTINFO_ID(this, i),
             BM_GET_PARTINFO_PATH(this, i),
             *BM_GET_PARTINFO_FD(this, i),
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

    pthread_mutex_lock(&mtd_block_lock);
    *mtd_desc = libmtd_open();
    if (!*mtd_desc) {
        LOGE("Failed to open libmtd\n");
        goto out;
    }
    retval = mtd_get_info(*mtd_desc, mtd_info);
    if (retval < 0) {
        LOGE("Failed to get mtd info\n");
        goto out;
    }
    *part_info = (struct bm_part_info *)calloc(mtd_info->mtd_dev_cnt + 1,
                 sizeof(struct bm_part_info));
    for (i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        struct mtd_dev_info *mtd_dev_info = BM_GET_PARTINFO_MTD_DEV(this, i);
        retval = mtd_get_dev_info1(*mtd_desc, i, mtd_dev_info);
        if (retval < 0) {
            LOGE("Cannot get dev info at \"mtd%d\"\n", i);
            goto out;
        }

        int *fd = BM_GET_PARTINFO_FD(this, i);
        char *path = BM_GET_PARTINFO_PATH(this, i);
        sprintf(path, "%s%d", MTD_CHAR_HEAD, mtd_dev_info->mtd_num);
        *fd = open(path, O_RDWR);
        if (*fd < 0) {
            LOGE("Cannot open mtd device %s: %s\n", path, strerror(errno));
            goto out;
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

    LOGD("mtd block init successfully\n");

    return 0;
out:
    pthread_mutex_unlock(&mtd_block_lock);

    return -1;
}

static int mtd_block_exit(struct block_manager* this) {

    struct bm_part_info **part_info = &BM_GET_PARTINFO(this);
    libmtd_t *mtd_desc = &BM_GET_MTD_DESC(this);
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);

    if (*part_info) {
        if (mtd_info) {
            for (int i = 0; i < mtd_info->mtd_dev_cnt; i++) {
                int *fd = BM_GET_PARTINFO_FD(this, i);
                if (*fd) {
                    LOGD("Close file descriptor %d\n", *fd);
                    close(*fd);
                    *fd = 0;
                }
            }
        }

        free(*part_info);
        *part_info = NULL;
    }

    if (*mtd_desc) {
        libmtd_close(*mtd_desc);
        *mtd_desc = NULL;
    }

    pthread_mutex_unlock(&mtd_block_lock);

    LOGD("mtd block exit successfully\n");

    return 0;
}

#if 0
#ifdef MTD_OPEN_DEBUG
static void dump_convert_params(struct block_manager* this,
                                struct fs_operation_params  *params) {
    LOGI("operation_method = %d\n", params->operation_method);
    LOGI("buf = 0x%x\n", (unsigned int)params->buf);
    LOGI("offset = 0x%llx\n", params->offset);
    LOGI("offset = %lld\n", params->length);
    LOGI("mtd = 0x%x\n", (unsigned int)params->priv);
}
#endif
#endif

static struct filesystem * prepare_convert_params(struct block_manager* this,
        struct filesystem *fs, int64_t offset, int64_t length,
        struct bm_operation_option *option) {
    struct mtd_dev_info* mtd = mtd_get_dev_info_by_offset(this, offset);
    int op_method = BM_OPERATION_METHOD_RANDOM;

    if (mtd == NULL) {
        LOGE("Cannot get mtd devinfo at 0x%llx", offset);
        goto out;
    }

    if (fs->params == NULL) {
        LOGE("filesystem %s parameter has not been allocated yet\n",
             fs->name);
        goto out;
    }

    if (fs->set_params == NULL) {
        LOGE("filesystem %s set_params is not initialed\n",
             fs->name);
        goto out;
    }

    if (option && (option->method != op_method))
        op_method = option->method;

    fs->set_params(fs, NULL, offset, length, op_method, mtd, this);

    return fs;

out:
    return NULL;
}

static int mtd_chip_erase(struct block_manager *this) {
    struct filesystem *fs  = NULL;
    int64_t offset, length, max_mapped;
    struct mtd_info *mtd_info = BM_GET_MTD_INFO(this);
    struct block_manager *bm = this;
    struct mtd_dev_info *mtd = NULL;

    fs = fs_new(BM_FILE_TYPE_NORMAL);
    if (fs == NULL) {
        LOGE("Cannot instance filesystem %s\n", BM_FILE_TYPE_NORMAL);
        goto out;
    }

    if (bm == NULL) {
        LOGE("Cannot get binder bm\n");
        goto  out;
    }

    if (mtd_info == NULL) {
        LOGE("Cannot get mtd info by block manager %p\n", this);
        goto out;
    }

    for (int i = 0; i < mtd_info->mtd_dev_cnt; i++) {
        mtd = BM_GET_PARTINFO_MTD_DEV(this, i);
        offset = BM_GET_PARTINFO_START(this, i);
        if (mtd == NULL) {
            LOGE("Cannot get mtd devinfo by index%d\n", i);
            goto out;
        }
        length = mtd->size;
        fs->set_params(fs, NULL, offset,
                       length, BM_OPERATION_METHOD_PARTITION, mtd, bm);

        if (!strcmp(bm->name, BM_BLOCK_TYPE_MTD)) {
            FS_FLAG_CLEAR(fs, NOSKIPBAD);
            if ((max_mapped = fs->get_max_mapped_size_in_partition(fs)) <= 0) {
                LOGE("Cannot get max mapped size by fs \'%s\'\n", fs->name);
                goto out;
            }

            FS_FLAG_SET(fs, NOSKIPBAD);
            if (fs->erase(fs) < 0) {
                LOGE("Cannot erase at offset 0x%llx by length %lld\n", offset, length);
                goto out;
            }
        }
    }

    if (fs)
        fs_destroy(&fs);

    return 0;

out:
    if (fs)
        fs_destroy(&fs);

    return -1;
}


static struct filesystem* data_transfer_params_set(struct block_manager* this,
        int64_t offset, char *buf, int64_t length) {

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
    int retval;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;

    retval = fs->erase(fs);
    if (retval < 0)
        goto out;

    return  retval;

out:
    return -1;
}

static int64_t mtd_block_write(struct block_manager* this, int64_t offset,
                               char* buf, int64_t length) {
    struct filesystem *fs = NULL;
    int retval;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;

    retval = fs->write(fs);
    if (retval < 0)
        goto out;

    return retval;

out:
    return -1;
}

static int64_t mtd_block_read(struct block_manager* this, int64_t offset,
                              char* buf, int64_t length) {

    struct filesystem *fs = NULL;
    int retval;

    if ((fs = data_transfer_params_set(this, offset, buf, length)) == NULL)
        goto out;

    retval = fs->read(fs);
    if (retval < 0)
        goto out;

    return retval;

out:
    return -1;
}

int mtd_block_format(struct block_manager* this) {
    struct filesystem *fs = NULL;

    fs = BM_GET_PREPARE_INFO_CONTEXT(this);
    if (fs == NULL) {
        LOGE("Prepare info context_handle is lost\n");
        goto out;
    }

    if (fs->format == NULL) {
        LOGE("\'%s\' not support method \'format\' yet\n",
             fs->name);
        goto out;
    }

    return fs->format(fs);

out:
    return -1;
}

#ifdef MTD_OPEN_DEBUG
static void dump_prepared_info(struct bm_operate_prepare_info *prepare) {
    LOGI("write_start = 0x%llx\n", prepare->write_start);
    LOGI("physical_unit_size = %u\n", prepare->physical_unit_size);
    LOGI("logical_unit_size = %u\n", prepare->logical_unit_size);
    LOGI("max_size_mapped_in_partition = %lld\n", prepare->max_size_mapped_in_partition);
    return;
}
#endif

static struct bm_operate_prepare_info* mtd_get_prepare_info(
    struct block_manager* this, ...) {

    static struct bm_operate_prepare_info prepare_info;
    struct filesystem *fs = NULL;
    va_list args;

    if (this->prepared == NULL)
        this->prepared = &prepare_info;

    va_start(args, this);
    fs = va_arg(args, struct filesystem *);

    prepare_info.write_start = va_arg(args, int64_t);
    prepare_info.physical_unit_size = va_arg(args, uint32_t);
    prepare_info.logical_unit_size = va_arg(args, uint32_t);
    prepare_info.max_size_mapped_in_partition = va_arg(args, int64_t);
    if ((prepare_info.write_start < 0)
            || !prepare_info.physical_unit_size
            || !prepare_info.logical_unit_size
            || (prepare_info.max_size_mapped_in_partition < 0))
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
    return NULL;
}

static int mtd_put_prepare_info(struct block_manager* this) {
    struct filesystem *fs = NULL;
    fs = BM_GET_PREPARE_INFO_CONTEXT(this);

    if (fs == NULL) {
        LOGE("Prepare info context_handle is lost\n");
        return -1;
    }

    if (this->prepared)
        this->prepared = NULL;

    return 0;
}

static struct bm_operate_prepare_info* mtd_block_prepare(
    struct block_manager* this, int64_t offset, int64_t length,
    struct bm_operation_option *option) {

    char *default_filetype = BM_FILE_TYPE_NORMAL;

    struct filesystem *fs = NULL;

    if (option && strcmp(option->filetype, default_filetype))
        default_filetype = option->filetype;

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
    return NULL;
}

static uint32_t mtd_get_prepare_leb_size(struct block_manager* this) {
    if (this->prepared == NULL)
        return 0;

    return this->prepared->logical_unit_size;
}

static int64_t mtd_get_prepare_write_start(struct block_manager* this) {
    if (this->prepared == NULL)
        return -1;

    return this->prepared->write_start;
}

static int64_t mtd_get_max_size_mapped_in(struct block_manager* this) {
    if (this->prepared == NULL)
        return -1;

    return this->prepared->max_size_mapped_in_partition;
}

static int64_t mtd_block_finish(struct block_manager* this) {
    struct filesystem *fs = BM_GET_PREPARE_INFO_CONTEXT(this);
    int64_t retval = 0;

    if (fs == NULL) {
        LOGE("Prepare info context_handle is lost\n");
        return -1;
    }

    if (fs->done)
        retval = fs->done(fs);

#ifdef MTD_OPEN_DEBUG
    mtd_scan_dump(fs);
#endif

    mtd_put_prepare_info(this);

    return retval;
}

static struct block_manager mtd_manager =  {
    .name = BM_BLOCK_TYPE_MTD,
    .chip_erase = mtd_chip_erase,
    .erase = mtd_block_erase,
    .read = mtd_block_read,
    .write = mtd_block_write,
    .format = mtd_block_format,
    .prepare = mtd_block_prepare,
    .get_prepare_leb_size = mtd_get_prepare_leb_size,
    .get_prepare_write_start = mtd_get_prepare_write_start,
    .get_prepare_max_mapped_size = mtd_get_max_size_mapped_in,
    .finish = mtd_block_finish,
    .get_partition_count = mtd_get_partition_count,
    .get_partition_size_by_name = mtd_get_partition_size_by_name,
    .get_partition_size_by_offset = mtd_get_partition_size_by_offset,
    .get_partition_start_by_name =  mtd_get_partition_start_by_name,
    .get_partition_start_by_offset =  mtd_get_partition_start_by_offset,
    .get_capacity = mtd_get_capacity,
    .get_blocksize = mtd_get_blocksize_by_offset,
    .get_iosize = mtd_get_pagesize_by_offset,
};

int mtd_manager_init(void) {
    if (!mtd_block_init(&mtd_manager)
            && !register_block_manager(&mtd_manager)
            && !mtd_install_filesystem(&mtd_manager))
        return 0;

    return -1;
}

int mtd_manager_destroy(void) {
    if (!mtd_uninstall_filesystem(&mtd_manager)
            && !unregister_block_manager(&mtd_manager)
            && !mtd_block_exit(&mtd_manager))
        return 0;

    return -1;
}
