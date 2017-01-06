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
#include <block/sysinfo/sysinfo_manager.h>
#include <block/block_manager.h>
#include <version.h>
#include <utils/assert.h>
#include <utils/common.h>

#define LOG_TAG         "testcase-bm_read"
#define CHUNKSIZE       1024*1024*2

static void bm_mtd_event_listener(struct block_manager *bm,
                                  struct bm_event* event, void* param) {
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

static void dump_data(int64_t offset, char *buf, int length) {
    char tmp[8196]  = {"\n"};
    char unit[4096];
    char data[512];
    int leap = 1024;
    int unit_count = 32;
    int64_t addr = offset;
    int i, j;

    for (i = 0; i < length; i++) {
        if ((i % leap) == 0) {
            sprintf(unit, "\t\toffset: 0x%08llx: ", addr + i);
            for (j = i; j < i + unit_count; j++) {
                sprintf(data, "%02x ", (unsigned char)buf[j]);
                strcat(unit, data);
            }
            strcat(unit, "\n");
            strcat(tmp, unit);
        }
    }

    LOGI("%s", tmp);
}

int test_read(void) {
    char *bm_params = "test-read";
    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    struct bm_operation_option bm_option;
    char tmp[256];
    char *buf;
    struct bm_operate_prepare_info* prepared = NULL;
    int64_t test_offset = 0x0;
    int64_t test_length = 32 << 10;
    int64_t ret = 0;

    LOGI("=============%s is starting =========\n", __func__);
    bm->construct = construct_block_manager;
    bm->destruct = destruct_block_manager;
    bm->construct(bm, "mtd", bm_mtd_event_listener, bm_params);
    bm->get_supported_filetype(bm, tmp);
    LOGI("suported filesystem type: %s\n", tmp);


    buf = calloc(1, CHUNKSIZE);
    if (buf == NULL) {
        LOGE("malloc failed\n");
        goto out;
    }

    LOGI("First of all: operation option set with method 0x%x, type %s\n",
         BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL);
    bm->set_operation_option(bm, &bm_option, BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL);

    LOGI("Operation at partition 1 <--> mtdblock0\n");
    prepared = bm->prepare(bm, 0, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    LOGI("prepared max length = 0x%llx\n", prepared->max_size_mapped_in_partition);
    test_offset = 0;
    test_length = 32 << 10;
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset, buf, 32 << 10);

    test_offset = 200 << 10;
    test_length = 32 << 10;
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset, buf, 32 << 10);

    ret = bm->finish(bm);
    LOGI("ret = 0x%llx\n", ret);

    LOGI("Operation at partition 2 <--> mtdblock1\n");
    test_offset = 1 << 20;
    prepared = bm->prepare(bm, 0x40000, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    LOGI("prepared max length = 0x%llx\n", prepared->max_size_mapped_in_partition);
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset, buf, 32 << 10);
    ret = bm->finish(bm);
    LOGI("ret = 0x%llx\n", ret);


    LOGI("Operation at partition 5<--> mtdblock4\n");
    test_offset = 7 << 20;
    test_length = 2 << 20;
    prepared = bm->prepare(bm, 0x700000, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    LOGI("prepared max length = 0x%llx\n", prepared->max_size_mapped_in_partition);
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset + (1 << 20), buf + (1 << 20), 32 << 10);

    test_offset = 9 << 20;
    test_length = 1 << 20;
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset + (512 << 10), buf + (512 << 10), 32 << 10);

    ret = bm->finish(bm);
    LOGI("ret = 0x%llx\n", ret);

out:
    if (buf) {
        free(buf);
        buf = NULL;
    }
    bm->destruct(bm);
    if (bm)
        free(bm);
    return 0;
}
