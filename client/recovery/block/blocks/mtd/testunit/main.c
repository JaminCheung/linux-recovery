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
#define DEVTYPE         DEVNAND
#define LOG_TAG         "testcase-blockmanager"
#define MOUNT_POINT   "/mnt/recovery_test/"
#define PACKAGE_PREFIX "updateXXX/"
#define CHUNKSIZE       1024*1024*2
struct offset_tlb {
    int64_t part_off;
    int operation_method;
    char* imgtype;
    char *image;
};

#if DEVTYPE == DEVNOR
#define MAX_WRITE_OFFSET 0x341000
#elif DEVTYPE == DEVNAND
#define MAX_WRITE_OFFSET 0xf01000
#endif
struct offset_tlb global_offset_tlb[] = {
#if DEVTYPE == DEVNOR
    {0x0, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "x-loader-pad-with-sleep-lib.bin"},
    {0x40000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "xImage_001"},
    {0x340000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "sn.txt"},
    {0x700000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_JFFS2, "system.jffs2_001"},
#elif DEVTYPE == DEVNAND
    {0x0, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "x-loader-pad-with-sleep-lib.bin"},
    {0x100000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "xImage_001"},
    {0xf00000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL, "sn.txt"},
    {0xf80000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_UBIFS, "system.ubi_001"},
    {0x3780000, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_UBIFS, "data.ubi_001"},
#endif
};
char *mtd_files [] = {
#if DEVTYPE == DEVNOR
    "update001/x-loader-pad-with-sleep-lib.bin",
    "update002/xImage_001",
    "update003/xImage_002",
    "update004/xImage_003",
    "update005/sn.txt",
    "update006/mac.txt",
    "update010/system.jffs2_001",
    "update011/system.jffs2_002",
    "update012/system.jffs2_003",
    "update013/system.jffs2_004",
    "update014/system.jffs2_005",
#elif DEVTYPE == DEVNAND
    "update001/x-loader-pad-with-sleep-lib.bin",
    "update002/xImage_001",
    "update003/xImage_002",
    "update004/xImage_003",
    "update005/xImage_004",
    "update006/sn.txt",
    "update007/mac.txt",
    "update008/system.ubi_001",
    "update009/system.ubi_002",
    "update010/system.ubi_003",
    "update011/system.ubi_004",
    "update012/system.ubi_005",
    "update013/system.ubi_006",
    "update014/system.ubi_007",
    "update015/system.ubi_008",
    "update016/system.ubi_009",
    "update017/system.ubi_010",
    "update018/system.ubi_011",
    "update019/system.ubi_012",
    "update020/system.ubi_013",
    "update021/system.ubi_014",
    "update022/data.ubi_001",
    "update023/data.ubi_002",
    "update024/data.ubi_003",
    "update025/data.ubi_004",
    "update026/data.ubi_005",
    "update027/data.ubi_006",
    "update028/data.ubi_007",
    "update029/data.ubi_008",
    "update030/data.ubi_009",
    "update031/data.ubi_010",
    "update032/data.ubi_011",
    "update033/data.ubi_012",
    "update034/data.ubi_013",
    "update035/data.ubi_014",
#endif
};

int64_t filesizes[100];

void bm_mtd_event_listener(struct block_manager *bm,
                           struct bm_event* event) {
#if 0
    LOGI("block_manager parser\n");
    LOGI("block_manager name = %s\n", bm->name);
    LOGI("block_manager param = %s\n", bm->param);

    LOGI("Event parser\n");
    LOGI("Event  mtdchar = %s \n", event->mtdchar);
    LOGI("Event  operation = %d \n", event->operation);
    LOGI("Event  progress = %d \n", event->progress);
#endif
    return;
}

void init_filesizes(void) {
    int i;
    char tmp[256];
    struct stat aa_stat;

    LOGI("%s is going on\n", __func__);
    for (i = 0; i < sizeof(mtd_files) / sizeof(mtd_files[0]); i++) {
        strcpy(tmp, MOUNT_POINT);
        strcat(tmp, mtd_files[i]);
        stat(tmp, &aa_stat);
        filesizes[i] = aa_stat.st_size;
        LOGI("%s: %lld\n", mtd_files[i], filesizes[i]);
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

static void api_interface_test(struct block_manager *this){
    int i;
    char buf[256];
    long long s;

    LOGI("partition count = %d\n", this->get_partition_count(this));
    for (i = 0; i < this->get_partition_count(this); i++) {
        sprintf(buf, "mtd%d", i);
        LOGI("partition = %s\n", buf);
        s = this->get_partition_start_by_name(this, buf);
        LOGI("parrition_start = 0x%llx\n", s);
        s = this->get_partition_size_by_name(this, buf);
        LOGI("partition_size = 0x%llx\n", s);
    }
}

int main(int argc, char **argv) {

    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    char *bm_params = "parameter1-passed-by-main";
    struct stat aa_stat;
    char tmp[256];
    struct bm_operate_prepare_info* prepared = NULL;
    char *buf = NULL;
    int64_t next_erase_offset = 0, next_write_offset = 0, mac_write_offset = MAX_WRITE_OFFSET;
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

    api_interface_test(bm);

    bm->get_supported_filetype(bm, tmp);
    LOGI("suported filesystem type: %s\n", tmp);

    buf = malloc(CHUNKSIZE);
    if (buf == NULL) {
        LOGE("malloc failed\n");
        goto exit;
    }

    init_filesizes();
    for (i = 0; i < sizeof(mtd_files) / sizeof(mtd_files[0]); i++) {

        strcpy(tmp, MOUNT_POINT);
        strcat(tmp, mtd_files[i]);
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
                LOGI("====<finish start at update process %d,  in %d step \n", i, w);
                ret = bm->finish(bm);
                if (ret < 0) {
                    LOGE("Block manager finish failed\n");
                    goto exit;
                }
                LOGI("====<finsh next start address at 0x%x,  in %d step \n", ret,  w);
            }
            LOGI("==================Update process %d is begining==============\n",
                    w + 1);
            LOGI("part offset is 0x%llx\n", cur->part_off);
            LOGI("operation method is %d\n", cur->operation_method);
            LOGI("image is %s\n", cur->image);
            save = cur;

            bm_option = bm->set_operation_option(bm, save->operation_method,
                                                 save->imgtype);
            LOGI("set_operation_option: method = %d, filetype = %s\n",
                 bm_option->method, bm_option->filetype);

            prepared = bm->prepare(bm, save->part_off, readsize, bm_option);
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

        if (!strcmp(mtd_files[i], "update006/mac.txt")) {
            next_write_offset = (mac_write_offset > next_write_offset)?mac_write_offset:next_write_offset;
            LOGI("mac %d: Writing at 0x%llx\n", i, next_write_offset);
            next_write_offset = bm->write(bm, next_write_offset, buf, readsize);
            LOGI("----> next write offset is 0x%llx\n", next_write_offset);
            continue;
        }

        LOGI("%d: Writing at 0x%llx\n", i, next_write_offset);
        next_write_offset = bm->write(bm, next_write_offset, buf, readsize);
        LOGI("----> next write offset is 0x%llx\n", next_write_offset);
    }
    LOGI("====<finish start at update process %d,  in %d step \n", i, w);
    ret = bm->finish(bm);
    if (ret < 0) {
        LOGE("Block manager finish failed\n");
        goto exit;
    }
    LOGI("====<finsh next start address at 0x%x,  in %d step \n", ret,  w);
    LOGI("destruct is starting\n");
exit:
    if (buf) {
        free(buf);
        buf = NULL;
    }
    if (fd > 0){
        close(fd);
        fd = 0;
    }
    bm->destruct(bm);
    if (bm)
        free(bm);
    return -1;
}