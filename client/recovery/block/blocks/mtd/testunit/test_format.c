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

#define LOG_TAG         "testcase-bm_flush"
#define CHUNKSIZE       1024*1024*2
#define JFFS2     0
#define CRAMFS    1
#define UBIFS     2
#define FS_TYPE   UBIFS

static void bm_mtd_event_listener(struct block_manager *bm,
                                  struct bm_event* event, void* param) {
    return;
}


static int format(struct block_manager *bm) {
    struct bm_operation_option bm_option;
    struct bm_operate_prepare_info* prepared = NULL;
    char *fs_type;
    int64_t start;
    int64_t ret;

#if FS_TYPE == JFFS2
    fs_type = BM_FILE_TYPE_JFFS2;
    start = 0xe00000;    //Partition 'data' with offset 0xe00000
#elif FS_TYPE == UBIFS
    fs_type = BM_FILE_TYPE_UBIFS;
    start = 0x3780000;
#endif

    ret = bm->set_operation_option(bm, &bm_option, BM_OPERATION_METHOD_PARTITION, fs_type);
    LOGI("set option ret = 0x%llx\n", ret);
    prepared = bm->prepare(bm, start, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    ret = bm->format(bm);
    LOGI("format ret = 0x%llx\n", ret);
    ret = bm->finish(bm);
    LOGI("finish ret = 0x%llx\n", ret);

    return 0;
out:
    assert_die_if(1, "crashed at %s\n", __func__);
    return -1;
}

int test_format(void) {
    char *bm_params = "test-format";
    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    char tmp[512];

    LOGI(" == == == == == == = %s is starting == == == == = \n", __func__);
    bm->construct = construct_block_manager;
    bm->destruct = destruct_block_manager;
    bm->construct(bm, "mtd", bm_mtd_event_listener, bm_params);
    bm->get_supported_filetype(bm, tmp);
    LOGI("suported filesystem type: %s\n", tmp);

    format(bm);

    bm->destruct(bm);
    printf("destruct done\n");
    if (bm)
        free(bm);
    return 0;
}
