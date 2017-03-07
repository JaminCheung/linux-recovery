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

/*
 * The total size reserved for flag area is 1KB
 */
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

static int is_id_valid(int id) {
    if (id < 0 || id >= ARRAY_SIZE(layout)) {
        return false;
    }
    return true;
}

static int64_t sysinfo_get_flag_size(int id) {
    if (id < 0 || id >= ARRAY_SIZE(layout)) {
        LOGE("id%d is overlowed with predefined value\n", id);
        return -1;
    }
    return layout[id].length;
}

static int sysinfo_read_flag(int id, void *flag) {
    struct sysinfo_manager *sys_m = GET_SYSINFO_MANAGER();
    char *sysinfo_buf = NULL;
    struct sysinfo_flag_layout *l = NULL;
    char mode;
    if (!is_id_valid(id)) {
        LOGE("id%d is not defined\n", id);
        return -1;
    }
    if (flag == NULL) {
        LOGE("Parameter flag is null\n");
        return -1;
    }

    l =  &layout[id];
    mode = SYSINFO_OPERATION_DEV;
    if (sys_m->get_value(sys_m, SYSINFO_FLAG, &sysinfo_buf, mode) < 0) {
            LOGE("Cannot get sysinfo by operation mode %d\n", mode);
            return -1;
    }
    if (sysinfo_buf == NULL) {
        LOGE("Cannot get sysinfo buffer\n");
        return -1;
    }
    dump_data(sys_m->get_offset(sys_m, SYSINFO_FLAG),
                        (unsigned char*)sysinfo_buf,
                        sys_m->get_length(sys_m, SYSINFO_FLAG));
    memcpy(flag, sysinfo_buf + l->offset, l->length);
    return 0;
}

static int sysinfo_write_flag(int id, void *flag) {
    struct sysinfo_manager *sys_m = GET_SYSINFO_MANAGER();
    char *sysinfo_buf = NULL;
    struct sysinfo_flag_layout *l = NULL;
    char mode;
    int reserve_org;
    if (!is_id_valid(id)) {
        LOGE("id%d is not defined\n", id);
        return -1;
    }
    if (flag == NULL) {
        LOGE("Parameter flag is null\n");
        return -1;
    }
    l =  &layout[id];
    mode = SYSINFO_OPERATION_RAM;
    if (sys_m->get_value(sys_m, SYSINFO_FLAG, &sysinfo_buf, mode) < 0) {
            LOGE("Cannot get value by operation mode %d\n", mode);
            return -1;
    }
    memcpy(sysinfo_buf + l->offset, flag, l->length);

    mode = SYSINFO_OPERATION_DEV;
    reserve_org = sys_m->get_reserve(sys_m, SYSINFO_FLAG);
    sys_m->set_reserve(sys_m, SYSINFO_FLAG, SYSINFO_NO_RESERVED);
    if (sys_m->set_value(sys_m, SYSINFO_FLAG, NULL, mode)) {
            LOGE("Cannot set value by operation mode %d\n", mode);
            return -1;
    }
    sys_m->set_reserve(sys_m, SYSINFO_FLAG, reserve_org);
    return 0;
}

struct sysinfo_flag sysinfo_flags = {
    .get_size = sysinfo_get_flag_size,
    .read = sysinfo_read_flag,
    .write = sysinfo_write_flag,
};