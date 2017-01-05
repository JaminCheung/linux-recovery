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
#include <block/sysinfo/sysinfo_manager.h>

#define LOG_TAG "test_flag"

#define UPDATE_FLAG_WRITE_DATA  0x11223344
int test_flag(void) {
    int ret = -1;
    int64_t  flag_size = 0;
    char *flag_update_done = NULL;
    LOGI(" == == == == == == = %s is starting == == == == = \n", __func__);

    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    if (bm == NULL) {
        LOGE("Cannot alloc more memory \n");
        goto out;
    }

    bm->construct = construct_block_manager;
    bm->destruct = destruct_block_manager;
    bm->construct(bm, "mtd", NULL, NULL);

    flag_size = GET_SYSINFO_MANAGER()->get_flag_size(SYSINFO_FLAG_ID_UPDATE_DONE);
    if (flag_size <= 0) {
        LOGE("Cannot get flag%d size\n", SYSINFO_FLAG_ID_UPDATE_DONE);
        goto out;
    }
    LOGD("flag%d size = %lld\n", SYSINFO_FLAG_ID_UPDATE_DONE, flag_size);

    flag_update_done = malloc(flag_size);
    if (flag_update_done == NULL) {
        LOGE("Cannot alloc more memory \n");
        goto out;
    }

    ret = GET_SYSINFO_MANAGER()->read_flag(SYSINFO_FLAG_ID_UPDATE_DONE, flag_update_done);
    if (ret < 0) {
        LOGE("Cannot read flag %d\n", SYSINFO_FLAG_ID_UPDATE_DONE);
        goto out;
    }
    LOGI("read-1 flag%d  with value 0x%x\n", SYSINFO_FLAG_ID_UPDATE_DONE, *(uint32_t *)flag_update_done);

    *(uint32_t *)flag_update_done = UPDATE_FLAG_WRITE_DATA;
    ret = GET_SYSINFO_MANAGER()->write_flag(SYSINFO_FLAG_ID_UPDATE_DONE, flag_update_done);
    if (ret < 0) {
        LOGE("Cannot read flag %d\n", SYSINFO_FLAG_ID_UPDATE_DONE);
        goto out;
    }
    LOGI("write flag%d  with value 0x%x\n", SYSINFO_FLAG_ID_UPDATE_DONE, *(uint32_t *)flag_update_done);
    *(uint32_t *)flag_update_done = 0x0;
    ret = GET_SYSINFO_MANAGER()->read_flag(SYSINFO_FLAG_ID_UPDATE_DONE, flag_update_done);
    if (ret < 0) {
        LOGE("Cannot read flag %d\n", SYSINFO_FLAG_ID_UPDATE_DONE);
        goto out;
    }
    LOGI("read-2 flag%d with value 0x%x\n", SYSINFO_FLAG_ID_UPDATE_DONE, *(uint32_t *)flag_update_done);

    ret = 0;
out:
    if (bm) {
        free(bm);
    }
    if (flag_update_done) {
        free(flag_update_done);
    }
    return ret;
}