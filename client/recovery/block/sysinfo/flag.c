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
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <utils/common.h>
#include <block/block_manager.h>
#include <block/fs/fs_manager.h>
#include <block/mtd/mtd.h>
#include <block/sysinfo/sysinfo_manager.h>

#define    LOG_TAG     "sysinfo_flag"

static char global_buffer[SYSINFO_FLAG_SIZE];

static struct sysinfo_flag_layout layout[] = {
    {SYSINFO_FLAG_UPDATE_DONE_OFFSET,  SYSINFO_FLAG_UPDATE_DONE_SIZE},
};

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

static int basic_read(int64_t offset, char *buf, int64_t length) {
    struct bm_operate_prepare_info* prepared = NULL;
    struct bm_operation_option bm_option;
    struct block_manager *bm;
    int64_t ret;

    bm = GET_SYSINFO_MANAGER()->binder;
    if (bm == NULL) {
        LOGE("Bind your block manager firstly\n");
        goto out;
    }
    bm->set_operation_option(bm, &bm_option,
            BM_OPERATION_METHOD_RANDOM, BM_FILE_TYPE_NORMAL);
    prepared = bm->prepare(bm, offset, length, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }
    LOGD("read at offset 0x%llx with length 0x%llx\n", offset, length);
    ret = bm->read(bm, offset, buf, length);
    if (ret < 0) {
        LOGE("read at offset 0x%llx failed\n", offset);
        goto out;
    }
    dump_data(offset, (unsigned char*)buf, length);

    ret = bm->finish(bm);
    if (ret < 0) {
        LOGE("block finish failed\n");
        goto out;
    }
    return 0;
out:
    return-1;
}

/* restriction:  write length must smaller than one block size */
static int basic_write(int64_t offset, char *buf, int64_t length) {
    struct bm_operate_prepare_info* prepared = NULL;
    struct bm_operation_option bm_option;
    struct block_manager *bm;
    int64_t ret, blkaligned_addr;
    char *tmpbuf = global_buffer;
    uint32_t blksize;

    bm = GET_SYSINFO_MANAGER()->binder;
    if (bm == NULL) {
        LOGE("Bind your block manager firstly\n");
        goto out;
    }
    blksize = bm->get_blocksize(bm, 0);
    bm->set_operation_option(bm, &bm_option,
            BM_OPERATION_METHOD_RANDOM, BM_FILE_TYPE_NORMAL);

    prepared = bm->prepare(bm, offset, length, &bm_option);
    if (prepared == NULL) {
        LOGE("Block manager prepare failed\n");
        goto out;
    }

    tmpbuf = calloc(1, blksize);
    if (tmpbuf == NULL) {
        LOGE("Cannot alloc more memory\n");
        goto out;
    }

    blkaligned_addr = offset & (~(blksize - 1));
    LOGD("<1> read at offset 0x%llx with length 0x%x\n",
                    blkaligned_addr,
                    blksize);
    ret = bm->read(bm, blkaligned_addr,
                tmpbuf, blksize);
    if (ret < 0) {
        LOGE("read at offset 0x%llx failed\n", blkaligned_addr);
        goto out;
    }
    LOGD("<2> erase at offset 0x%llx with length 0x%x\n",
                    blkaligned_addr,
                    blksize);
    ret = bm->erase(bm, blkaligned_addr, blksize);
    if (ret < 0) {
        LOGE("erase at offset 0x%llx failed\n", blkaligned_addr);
        goto out;
    }
    LOGD("<2.5> merge data at buffer offset 0x%llx with length %lld\n",
                    offset - blkaligned_addr,
                    length);
    memcpy(tmpbuf + (offset - blkaligned_addr), buf, length);

    LOGD("<3> write at offset 0x%llx with length 0x%x\n",
                    blkaligned_addr,
                    blksize);
    ret = bm->write(bm, blkaligned_addr, tmpbuf, blksize);
    if (ret < 0) {
        LOGE("write at offset 0x%llx failed\n", blkaligned_addr);
        goto out;
    }
    ret = bm->finish(bm);
    if (ret < 0) {
        LOGE("block finish failed\n");
        goto out;
    }
    if (tmpbuf) {
        free(tmpbuf);
        tmpbuf = NULL;
    }
    return 0;
out:
    if (tmpbuf) {
        free(tmpbuf);
        tmpbuf = NULL;
    }
    return-1;
}

int64_t sysinfo_get_flag_size(int id) {
    if (id < 0 || id >= ARRAY_SIZE(layout)) {
        LOGE("id%d is overlowed with predefined value\n", id);
        return -1;
    }
    return layout[id].length;
}

int sysinfo_read_flag(int id, void *flag) {
    if (id < 0 || id >= ARRAY_SIZE(layout)) {
        LOGE("id%d is overlowed with predefined value\n", id);
        return -1;
    }
    if (flag == NULL) {
        LOGE("Parameter flag is null\n");
        return -1;
    }
    char *buf = global_buffer;
    if (basic_read(SYSINFO_FLAG_OFFSET, buf, SYSINFO_FLAG_SIZE) < 0) {
        LOGE("basic read is failed\n");
        return -1;
    }
    struct sysinfo_flag_layout *l = NULL;
    l =  &layout[id];

    memcpy(flag, buf + l->offset, l->length);
    return 0;
}

int sysinfo_write_flag(int id, void *flag) {
    if (id < 0 || id >= ARRAY_SIZE(layout)) {
        LOGE("id%d is overlowed with predefined value\n", id);
        return -1;
    }
    if (flag == NULL) {
        LOGE("Parameter flag is null\n");
        return -1;
    }
    char *buf = global_buffer;
    struct sysinfo_flag_layout *l = NULL;
    l =  &layout[id];

    memcpy(buf + l->offset, flag, l->length);
    if (basic_write(SYSINFO_FLAG_OFFSET, buf, SYSINFO_FLAG_SIZE) < 0) {
        LOGE("basic write is failed\n");
        return -1;
    }
    return 0;
}
