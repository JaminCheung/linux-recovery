#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/log.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <types.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/ubi/ubi.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/block_manager.h>
#include <version.h>
#include <utils/assert.h>
#include <utils/common.h>

#define DEVNOR          0
#define DEVNAND        1
#define DEVTYPE         DEVNOR
#define LOG_TAG         "testcase-blockmanager"
#define MOUNT_POINT   "/mnt/"
#define PACKAGE_PREFIX "updateXXX/"
#define CHUNKSIZE       1024*1024*2
struct offset_tlb {
    int64_t part_off;
    int operation_method;
    char* imgtype;
    char *image;
    int size;
};

#if DEVTYPE == DEVNOR
struct offset_tlb global_offset_tlb[] = {
    {0x0, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "x-loader-pad-with-sleep-lib.bin", 24576},
    {0x40000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "xImage_001", 3018816},
    {0x340000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "sn.txt", 23456},
    {0x700000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_JFFS2, "system.jffs2_001", 11730944},
};

#elif DEVTYPE == DEVNAND
struct offset_tlb global_offset_tlb[] = {
    {0x0, "x-loader-pad-with-sleep-lib"},
    {0x100000, "xImage_001"},
    {0x980000, "system.ubifs_001"},
};
#endif

char *mtd_nor_files [] = {
    "update001/x-loader-pad-with-sleep-lib.bin",
    "update002/xImage_001",
    "update003/xImage_002",
    "update004/xImage_003",
    #if 0
    "update005/system.jffs2_001",
    "update006/system.jffs2_002",
    "update007/system.jffs2_003",
    "update008/system.jffs2_004",
    "update009/system.jffs2_005",
    #else
    "update005/sn.txt",
    "update006/mac.txt",
    "update010/system.jffs2_001",
    "update011/system.jffs2_002",
    "update012/system.jffs2_003",
    "update013/system.jffs2_004",
    "update014/system.jffs2_005",
    #endif
};


int64_t filesizes[100];

void bm_mtd_event_listener(struct block_manager *bm,
                           struct bm_event* event) {
    LOGI("block_manager parser\n");
    LOGI("block_manager name = %s\n", bm->name);
    LOGI("block_manager param = %p\n", bm->param);

    LOGI("Event parser\n");
    LOGI("Event  mtdchar = %s \n", event->mtdchar);
    LOGI("Event  operation = %d \n", event->operation);
    LOGI("Event  progress = %d \n", event->progress);
    return;
}

void init_filesizes(void) {
    int i;
    char tmp[256];
    struct stat aa_stat;

    LOGI("%s is going on\n", __func__);
    for (i = 0; i < sizeof(mtd_nor_files) / sizeof(mtd_nor_files[0]); i++) {
        strcpy(tmp, MOUNT_POINT);
        strcat(tmp, mtd_nor_files[i]);
        stat(tmp, &aa_stat);
        filesizes[i] = aa_stat.st_size;
        LOGI("%s: %lld\n", mtd_nor_files[i], filesizes[i]);
    }

    return;
}

static struct offset_tlb* is_file_recognize(char *path) {
    char n[256];
    int j;
    char *subfix;

    strcpy(n, MOUNT_POINT);
    strcat(n, PACKAGE_PREFIX);

    subfix = &path[strlen(n)];

    for (j = 0; j < sizeof(global_offset_tlb) / sizeof(global_offset_tlb[0]); ++j)
    {
        if (!strcmp(global_offset_tlb[j].image, subfix))
            return &global_offset_tlb[j];
    }
    return NULL;
}

int block_manager_testcase(void) {

    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    char *bm_params = "bm_params";
    struct stat aa_stat;
    char tmp[256];
    struct bm_operate_prepare_info* prepared = NULL;
    char *buf = NULL;
    int64_t next_erase_offset = 0, next_write_offset = 0;
    struct bm_operation_option *bm_option;
    struct offset_tlb *cur, *save;
    int readsize;
    int fd = 0;
    int i;
    static int w;
    int ret;

    bm->construct = construct_block_manager;
    bm->destruct = destruct_block_manager;
    bm->construct(bm, "mtd", bm_mtd_event_listener, bm_params);

    bm->get_supported(bm, tmp);
    LOGI("suported blocks : %s\n", tmp);
    bm->get_supported_filetype(bm, tmp);
    LOGI("suported filesystem type: %s\n", tmp);

    buf = malloc(CHUNKSIZE);
    if (buf == NULL) {
        LOGE("malloc failed\n");
        goto exit;
    }

    init_filesizes();
    for (i = 0; i < sizeof(mtd_nor_files) / sizeof(mtd_nor_files[0]); i++) {
        strcpy(tmp, MOUNT_POINT);
        strcat(tmp, mtd_nor_files[i]);

        LOGI("%s is going to download\n", tmp);
        fd = open(tmp, O_RDONLY);
        if (fd <= 0) {
            LOGE("cannot open file %s\n", tmp);
            goto exit;
        }
        stat(tmp, &aa_stat);
        LOGI("\"%s\" file size is %d\n", tmp, (int)aa_stat.st_size);

        readsize = read(fd, buf, aa_stat.st_size);
        if (readsize != aa_stat.st_size) {
            LOGE("read filesize error, %d, %d\n", (int)aa_stat.st_size, readsize);
            goto exit;
        }
        if (fd) {
            LOGI("close opened fd \n");
            close(fd);
            fd = 0;
        }
        if ((cur = is_file_recognize(tmp)) != NULL) {
            if (i != 0) {
                LOGI("====<finish start at update process %d,  in %d step \n", i, w + 1);
                ret = bm->finish(bm);
                if (!ret) {
                    LOGE("Block manager finish failed\n");
                    goto exit;
                }
            }
            LOGI("==================Update process %d is begining==============\n", w + 1);
            LOGI("part offset is 0x%llx\n", cur->part_off);
            LOGI("operation method is %d\n", cur->operation_method);
            LOGI("image is %s\n", cur->image);
            LOGI("size is %d\n", cur->size);
            save = cur;

            bm_option = bm->set_operation_option(bm, save->operation_method,
                                                 save->imgtype);
            LOGI("set_operation_option: method = %d, filetype = %s\n",
                 bm_option->method, bm_option->filetype);

            prepared = bm->prepare(bm, save->part_off, save->size, bm_option);
            if (prepared == NULL) {
                LOGE("Block manager prepare failed\n");
                goto exit;
            }
            LOGI("prepared get: leb size = %d\n",
                 (int)bm->get_prepare_leb_size(bm));
            LOGI("prepared get: write start = %llx\n",
                 bm->get_prepare_write_start(bm));
            LOGI("prepared get: max map size = %lld\n",
                 bm->get_prepare_max_mapped_size(bm));

            LOGI("%d: Erasing at 0x%llx\n", i , save->part_off);
            next_erase_offset = bm->erase(bm, save->part_off,
                                          bm->get_prepare_max_mapped_size(bm));
            LOGI("----> next erase offset is 0x%llx\n", next_erase_offset);

            LOGI("%d: Writing at 0x%llx, write length is %d\n",
                 i, bm->get_prepare_write_start(bm), readsize);
            next_write_offset = bm->write(bm,
                                          bm->get_prepare_write_start(bm), buf, readsize);
            LOGI("----> next write offset is 0x%llx\n", next_write_offset);

            w++;
            continue;
        }
        LOGI("%d: Writing at 0x%llx\n", i, next_write_offset);
        next_write_offset = bm->write(bm, next_write_offset, buf, readsize);
        LOGI("----> next write offset is 0x%llx\n", next_write_offset);
    }

    return 0;

exit:
    if (buf) {
        free(buf);
        buf = NULL;
    }
    if (fd > 0){
        close(fd);
        fd = 0;
    }
    return -1;
}