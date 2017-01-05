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

#define LOG_TAG         "testcase-bm_sysinfo"
#define UPDATE_WHAT  "/mnt/recovery_test/update001/x-loader-pad-with-sleep-lib.bin"
#define CHUNKSIZE       1024*1024*2


static void bm_mtd_event_listener(struct block_manager *bm,
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

static void dump_data(int64_t offset, unsigned char *buf, int length) {
    int64_t addr = offset;
    int size = length * 4;
    char data[512];
    char *unit = NULL;
    int i;

    unit = calloc(1, size);
    if (unit == NULL) {
        LOGE("cannot alloc more memory\n");
        goto out;
    }
    for (i = 0; i < length / 2; i += 2) {
        unsigned short s = 0;
        if ((i % 16) == 0)
            sprintf(unit, "%08llx: ", addr + i);

        s = ((buf[i]) & 0xff) + ((buf[i + 1] & 0xff) << 8);

        sprintf(data, "%04x ", s);
        strcat(unit, data);
        if (((i + 2) % 16) == 0) {
            strcat(unit, "\n");
            printf("%s", unit);
        }
    }
    if (unit)
        free(unit);
    return;
out:
    if (unit)
        free(unit);
    assert_die_if(1, "crashed at %s\n", __func__);
    return;
}

int read_sysinfo(struct block_manager *bm, char *buf) {
    struct bm_operation_option bm_option;
    struct bm_operate_prepare_info* prepared = NULL;
    static int called;
    int64_t test_offset, test_length, ret;
    int64_t dump_start = SYSINFO_FLASHINFO_PARTINFO_OFFSET, dump_length = SYSINFO_FLASHINFO_PARTINFO_SIZE;
    LOGI("Reading at partition 1 <--> mtdblock0, have read %d times\n", called);
    bm->set_operation_option(bm, &bm_option,
            BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL);
    prepared = bm->prepare(bm, 0, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    test_offset = 0;
    test_length = 256 << 10;
    LOGI("read at offset 0x%llx with length 0x%llx\n", test_offset, test_length);
    memset(buf, 0, sizeof(buf));
    ret = bm->read(bm, test_offset, buf, test_length);
    LOGI("ret = 0x%llx\n", ret);
    dump_data(test_offset + dump_start,
              (unsigned char*)buf + dump_start, dump_length);

    ret = bm->finish(bm);
    LOGI("ret = 0x%llx\n", ret);

    called++;
    return 0;
out:
    assert_die_if(1, "crashed at %s\n", __func__);
    return -1;
}

int update_bootloader(struct block_manager *bm, char *buf) {
    struct bm_operation_option bm_option;
    struct bm_operate_prepare_info* prepared = NULL;
    struct stat aa_stat;
    char tmp[1024];
    int readsize;
    int fd = 0;
    int64_t part_off = 0, next_erase_offset, next_write_offset, ret;

    strcpy(tmp, UPDATE_WHAT);

    LOGI("%s is going to download\n", tmp);
    fd = open(tmp, O_RDONLY);
    if (fd <= 0) {
        LOGE("cannot open file %s\n", tmp);
        goto out;
    }
    stat(tmp, &aa_stat);
    LOGI("\"%s\" file size is %d\n", tmp, (int)aa_stat.st_size);

    readsize = read(fd, buf, aa_stat.st_size);
    if (readsize != aa_stat.st_size) {
        LOGE("read filesize error, %d, %d\n", (int)aa_stat.st_size, readsize);
        goto out;
    }
    if (fd) {
        LOGI("close opened fd \n");
        close(fd);
        fd = 0;
    }

    bm->set_operation_option(bm, &bm_option,
        BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL);
    LOGI("set_operation_option: method = %d, filetype = %s\n",
         BM_OPERATION_METHOD_PARTITION, BM_FILE_TYPE_NORMAL);

    prepared = bm->prepare(bm, part_off, 0, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    LOGI("prepared get: leb size = %d\n",
         (int)bm->get_prepare_leb_size(bm));
    LOGI("prepared get: write start = %llx\n",
         bm->get_prepare_write_start(bm));
    LOGI("prepared get: max map size = %lld\n",
         bm->get_prepare_max_mapped_size(bm));

    LOGI("Erasing at 0x%llx with length 0x%llx\n", part_off,
         bm->get_prepare_max_mapped_size(bm));
    next_erase_offset = bm->erase(bm, part_off,
                                  bm->get_prepare_max_mapped_size(bm));
    LOGI("---- > next erase offset is 0x%llx\n", next_erase_offset);

    LOGI("Writing at 0x%llx, write length is %d\n",
         bm->get_prepare_write_start(bm), readsize);
    next_write_offset = bm->write(bm,
                                  bm->get_prepare_write_start(bm), buf, readsize);
    LOGI("---- > next write offset is 0x%llx\n", next_write_offset);

    ret = bm->finish(bm);
    LOGI("ret = 0x %llx\n", ret);
    return 0;
out:
    assert_die_if(1, "crashed at %s\n", __func__);
    return -1;
}

int test_sysinfo(void) {
    char *bm_params = "test-sysinfo";
    struct block_manager *bm = (struct block_manager *)calloc(1, sizeof(*bm));
    char tmp[256];
    char *buf;

    LOGI(" == == == == == == = %s is starting == == == == = \n", __func__);
    bm->construct = construct_block_manager;
    bm->destruct = destruct_block_manager;
    bm->construct(bm, "mtd", bm_mtd_event_listener, bm_params);
    bm->get_supported_filetype(bm, tmp);
    LOGI("suported filesystem type: %s\n", tmp);
    printf("bm address %p \n", bm);

    buf = calloc(1, CHUNKSIZE);
    if (buf == NULL) {
        LOGE("malloc failed\n");
        goto out;
    }

    read_sysinfo(bm, buf);

    update_bootloader(bm, buf);

    read_sysinfo(bm , buf);

out:
    if (buf) {
        free(buf);
        buf = NULL;
    }
    bm->destruct(bm);
    printf("destruct done\n");
    if (bm)
        free(bm);
    return 0;
}
