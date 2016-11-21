#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <types.h>
#include <lib/mtd/mtd-user.h>
#include <lib/mtd/ubi-media.h>
#include <lib/mtd/mtd_swab.h>
#include <lib/ubi/libubigen.h>
#include <lib/ubi/libubi.h>
#include <lib/ubi/libscan.h>
#include <lib/ini/iniparser.h>
#include <autoconf.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <utils/common.h>
#include <block/fs/fs_manager.h>
#include <block/fs/ubifs.h>
#include <block/block_manager.h>
#include <block/mtd/mtd.h>

#define LOG_TAG  "fs_ubifs"

static struct ubi_params *ubi;

#ifdef UBI_OPEN_DEBUG
static void dump_ubi_vi_info(struct ubi_params *params ) {
    struct ubigen_vol_info *vi = params->vi;
    LOGI("=========vi info =========\n");
    LOGI("id = %d\n", vi->id);
    LOGI("type = %d\n", vi->type);
    LOGI("alignment = %d\n", vi->alignment);
    LOGI("data_pad = %d\n", vi->data_pad);
    LOGI("usable_leb_size = %d\n", vi->usable_leb_size);
    LOGI("name = %s\n", vi->name);
    LOGI("name_len = %d\n", vi->name_len);
    LOGI("compat = %d\n", vi->compat);
    LOGI("used_ebs = %d\n", vi->used_ebs);
    LOGI("bytes = %lld\n", vi->bytes);
    LOGI("compat = %d\n", vi->compat);
    LOGI("flags = 0x%x\n", vi->flags);
}
static void dump_ubi_si(struct ubi_params *params ) {
    struct ubi_scan_info *si = params->si;
    LOGI("=========si info =========\n");
    LOGI("ec = %p\n", si->ec);
    LOGI("mean_ec = %lld\n", si->mean_ec);
    LOGI("ok_cnt = %d\n", si->ok_cnt);
    LOGI("empty_cnt = %d\n", si->empty_cnt);
    LOGI("corrupted_cnt = %d\n", si->corrupted_cnt);
    LOGI("alien_cnt = %d\n", si->alien_cnt);
    LOGI("bad_cnt = %d\n", si->bad_cnt);
    LOGI("good_cnt = %d\n", si->good_cnt);
    LOGI("vid_hdr_offs = %d\n", si->vid_hdr_offs);
    LOGI("data_offs = %d\n", si->data_offs);
}
static void dump_ubi_ui(struct ubi_params *params ) {
    struct ubigen_info *ui = params->ui;
    LOGI("=========ui info =========\n");
    LOGI("leb_size = %d\n", ui->leb_size);
    LOGI("peb_size = %d\n", ui->peb_size);
    LOGI("min_io_size = %d\n", ui->min_io_size);
    LOGI("vid_hdr_offs = %d\n", ui->vid_hdr_offs);
    LOGI("data_offs = %d\n", ui->data_offs);
    LOGI("ubi_ver = %d\n", ui->ubi_ver);
    LOGI("vtbl_size = %d\n", ui->vtbl_size);
    LOGI("max_volumes = %d\n", ui->max_volumes);
    LOGI("image_seq = %d\n", ui->image_seq);
}
static void dump_ubi_vtbl(struct ubi_params *params) {
    struct ubi_vtbl_record *vtbl = &params->vtbl[0];
    LOGI("=========volume table record info =========\n");
    LOGI("reserved_pebs: %d\n", vtbl->reserved_pebs);
    LOGI("alignment: %d\n", vtbl->alignment);
    LOGI("data_pad: %d\n", vtbl->data_pad);
    LOGI("vol_type: %d\n", vtbl->vol_type);
    LOGI("upd_marker: %d\n", vtbl->upd_marker);
    LOGI("name_len: %d\n", vtbl->name_len);
    LOGI("name: %s\n", vtbl->name);
    LOGI("flags: 0x%x\n", vtbl->flags);
    LOGI("crc: 0x%x\n", vtbl->crc);
}
static void dump_ubi_params(struct ubi_params *params) {
    LOGI("=========device depends info=========\n");
    LOGI("peb size: %d\n", params->devinfo.peb_size);
    LOGI("page size: %d\n", params->devinfo.page_size);
    LOGI("partition size: 0x%llx\n",
         params->devinfo.partition_size);
    LOGI("device size: 0x%llx\n", params->devinfo.device_size);
    LOGI("volume name: %s\n", params->vol_name);
    LOGI("max beb per1024: %d\n", params->max_beb_per1024);
    LOGI("ubi reserved blks: %d\n", params->ubi_reserved_blks);
    LOGI("leg size: %d\n", params->lebsize);
    LOGI("volume size: %lld\n", params->vol_size);
    LOGI("override_ec: %d\n", params->override_ec);
    LOGI("ec: %lld\n", params->ec);
    LOGI("vid header offset  = %d\n", params->vid_hdr_offs);
    LOGI("ubi version  = %d\n", params->ubi_ver);
    LOGI("ui address: %p\n", params->ui);
    LOGI("si address: %p\n", params->si);
    LOGI("vtbl address: %p\n", &params->vtbl[0]);
}

static int is_power_of_2(unsigned long long n) {
    return (n != 0 && ((n & (n - 1)) == 0));
}

static void print_bad_eraseblocks(const struct mtd_dev_info *mtd,
                                  const struct ubi_scan_info *si) {
    int first = 1, eb;
    char unitbuf[128];
    char *buf;
    if (si->bad_cnt == 0)
        return;

    buf = calloc(1, 1024);
    sprintf(buf, "%d bad eraseblocks found, numbers: ",
            si->bad_cnt);
    for (eb = 0; eb < mtd->eb_cnt; eb++) {
        if (si->ec[eb] != EB_BAD)
            continue;
        if (first) {
            sprintf(unitbuf, "%d", eb);
            strcat(buf, unitbuf);
            first = 0;
        } else {
            sprintf(unitbuf, ", %d", eb);
            strcat(buf, unitbuf);
        }
    }
    LOGI("%s\n", buf);
    free(buf);
}
#endif

static int ubiutils_srand(void) {
    struct timeval tv;
    struct timezone tz;
    unsigned int seed;

    if (gettimeofday(&tv, &tz))
        return -1;

    seed = (unsigned int) tv.tv_sec;
    seed += (unsigned int) tv.tv_usec;
    seed *= getpid();
    seed %= RAND_MAX;
    srand(seed);
    return 0;
}

static int get_bad_peb_limit(struct ubi_params *params)
{
    unsigned int limit, device_pebs, max_beb_per1024;

    max_beb_per1024 = params->max_beb_per1024;
    if (!max_beb_per1024)
        return 0;

    device_pebs = params->devinfo.device_size / params->devinfo.peb_size;
    limit = mult_frac(device_pebs, max_beb_per1024, 1024);
    if (mult_frac(limit, 1024, max_beb_per1024) < device_pebs)
        limit += 1;

    return limit;
}

static int consecutive_bad_check(int eb) {
    static int consecutive_bad_blocks = 1;
    static int prev_bb = -1;

    if (prev_bb == -1)
        prev_bb = eb;

    if (eb == prev_bb + 1)
        consecutive_bad_blocks += 1;
    else
        consecutive_bad_blocks = 1;

    prev_bb = eb;

    if (consecutive_bad_blocks >= MAX_CONSECUTIVE_BAD_BLOCKS) {
        LOGE("consecutive bad blocks exceed limit: %d, bad flash?",
             MAX_CONSECUTIVE_BAD_BLOCKS);
        return -1;
    }
    return 0;
}

static int mark_bad(const struct mtd_dev_info *mtd, struct ubi_scan_info *si,
                    int eb) {
    int mtd_fd = MTD_DEV_INFO_TO_FD(mtd);
    int err;

    LOGI("marking block %d bad\n", eb);

    if (!mtd->bb_allowed) {
        LOGI("bad blocks not supported by this flash");
        return -1;
    }

    err = mtd_mark_bad(mtd, mtd_fd, eb);
    if (err)
        return err;

    si->bad_cnt += 1;
    si->ec[eb] = EB_BAD;

    return consecutive_bad_check(eb);
}

static int ubi_generate_vi_info(struct ubi_params *params, int64_t image_length) {
    struct ubigen_info *ui = params->ui;
    int vol_id = UBI_VOLUME_DEFAULT_ID;
    char *vol_name = params->vol_name;
    long long vol_size = (long long)params->vol_size;
    char *volume_type = UBI_VOLUME_DEFAULT_TYPE;
    int autoresize_flag = UBI_VOLUME_ENABLE_AUTORESIZE;
    int sects = UBI_VOLUME_SECTION_CNT;
    int alignment = UBI_VOLUME_DEFAULT_ALIGNMENT;
    struct ubigen_vol_info *vi = NULL;
    int retval = 0;

    vi = calloc(sects, sizeof(struct ubigen_vol_info));

    vi->type = !strcmp(volume_type, "static") ? UBI_VID_STATIC : UBI_VID_DYNAMIC;
    // LOGI("volume type: %s\n", vi->type == UBI_VID_DYNAMIC ? "dynamic" : "static");
    vi->id = vol_id;
    if (vi->id < 0) {
        LOGE("negative volume ID %d in volume \"%s\"\n", vi->id, vol_name);
        goto out;
    }
    if (vi->id >= ui->max_volumes) {
        LOGE("too high volume ID %d in volume \"%s\", max. is %d\n", vi->id,
             vol_name, ui->max_volumes);
        goto out;
    }
    // LOGI("volume ID: %d\n", vi->id);

    vi->bytes = vol_size;
    // LOGI("volume size: %lld bytes\n", vi->bytes);

    vi->name = vol_name;
    vi->name_len = strlen(vol_name);
    if (vi->name_len > UBI_VOL_NAME_MAX) {
        LOGE("too long volume name in volume \"%s\", max. is %d characters\n",
             vol_name, UBI_VOL_NAME_MAX);
        goto out;
    }
    // LOGI("volume name: %s\n", vi->name);

    vi->alignment = alignment;
    if (vi->alignment >= ui->leb_size) {
        LOGE("too large alignment %d, max is %d (LEB size)\n",
             vi->alignment, ui->leb_size);
        goto out;
    }
    // LOGI("volume alignment: %d\n", vi->alignment);

    if (autoresize_flag)
        vi->flags |= UBI_VTBL_AUTORESIZE_FLG;

    vi->data_pad = ui->leb_size % vi->alignment;
    vi->usable_leb_size = ui->leb_size - vi->data_pad;
    if (image_length % vi->usable_leb_size) {
        LOGE("image length must be leb size alignment,  pass length is %lld, leb size is %d\n",
             image_length, vi->usable_leb_size);
        goto out;
    }
    if (vi->type == UBI_VID_DYNAMIC)
        vi->used_ebs = (vi->bytes + vi->usable_leb_size - 1)
                       / vi->usable_leb_size;
    else {
        if (!image_length) {
            LOGE("image length cannot be zero when volume type is static\n");
            goto out;
        }
        vi->used_ebs = (image_length + vi->usable_leb_size - 1)
                       / vi->usable_leb_size;
    }
    vi->compat = 0;

    retval = ubigen_add_volume(ui, vi, params->vtbl);
    if (retval) {
        LOGE("Cannot add volume \"%s\"", vi->name);
        goto out;
    }
    params->vi = vi;
#ifdef UBI_OPEN_DEBUG
    dump_ubi_vi_info(params);
    dump_ubi_vtbl(params);
#endif
    return 0;
out:
    if (vi) {
        free(vi);
        vi = NULL;
        if (params->vi)
            params->vi = NULL;
    }
    return -1;
}

static int ubi_volume_params_set(struct ubi_params *params,  int64_t image_length) {
    struct ubigen_info *ui = params->ui;
    unsigned int logic_blkcnt = 0;
    unsigned int logic_blksize = 0;
    unsigned int vol_size = 0;

    params->vtbl = ubigen_create_empty_vtbl(ui);
    if (!params->vtbl) {
        LOGE("volume table records created failed\n");
        goto out;
    }

    logic_blksize = params->devinfo.peb_size - 2 * params->devinfo.page_size;
    params->max_beb_per1024 = CONFIG_MTD_UBI_BEB_LIMIT;
    params->ubi_reserved_blks = UBI_LAYOUT_VOLUME_EBS + WL_RESERVED_PEBS +
                                EBA_RESERVED_PEBS + get_bad_peb_limit(params);
    logic_blkcnt = params->si->good_cnt - params->ubi_reserved_blks;
    vol_size = logic_blkcnt * logic_blksize;
    params->lebsize = logic_blksize;
    params->vol_size = vol_size;

    if (ubi_generate_vi_info(params, image_length) < 0) {
        LOGE("Cannot generate vi info for volume\n");
        goto out;
    }
    return 0;
out:
    if (params->vtbl) {
        free(params->vtbl);
        params->vtbl = NULL;
    }
    return -1;
}

static int ubi_mtd_part_check(struct filesystem* fs,
                              struct ubi_params *ubi_params) {
    struct mtd_dev_info *mtd_dev = FS_GET_MTD_DEV(fs);
    int mtd_fd = MTD_DEV_INFO_TO_FD(mtd_dev);
    char *mtd_path = MTD_DEV_INFO_TO_PATH(mtd_dev);
    struct ubi_scan_info *si = NULL;
    libubi_t libubi;
    int err;

    if (!is_power_of_2(mtd_dev->min_io_size)) {
        LOGE("min. I/O size is %d, but should be power of 2\n",
             mtd_dev->min_io_size);
        goto out;
    }
    if (!mtd_dev->writable) {
        LOGE("mtd%d (%s) is a read-only device\n", mtd_dev->mtd_num,
             mtd_path);
        goto out;
    }

    /* Make sure this MTD device is not attached to UBI */
    libubi = libubi_open();
    if (libubi) {
        int ubi_dev_num;

        err = mtd_num2ubi_dev(libubi, mtd_dev->mtd_num, &ubi_dev_num);
        if (!err) {
            if (ubi_detach(libubi, UBI_DEFAULT_CTRL_DEV, mtd_path)
                    < 0) {
                LOGE("please, failed to detach mtd%d (%s) from ubi%d\n",
                     mtd_dev->mtd_num, UBI_DEFAULT_CTRL_DEV, ubi_dev_num);
                libubi_close(libubi);
                goto out;
            }
        }
        libubi_close(libubi);
    }

    err = ubi_scan(mtd_dev, mtd_fd, &si, 0);
    if (err) {
        LOGE("failed to scan mtd%d (%s)\n", mtd_dev->mtd_num,
             mtd_path);
        goto out;
    }
    if (si->good_cnt < 2) {
        if (si != NULL)
            free(si);
        LOGE("too few non-bad eraseblocks (%d) on mtd%d\n", si->good_cnt,
             mtd_dev->mtd_num);
        goto out;
    }
    if (si->ok_cnt)
        LOGI("%d eraseblocks have valid erase counter, mean value is %lld\n",
             si->ok_cnt, si->mean_ec);
    if (si->empty_cnt)
        LOGI("%d eraseblocks are supposedly empty\n", si->empty_cnt);
    if (si->corrupted_cnt)
        LOGI("%d corrupted erase counters\n", si->corrupted_cnt);
#ifdef  UBI_OPEN_DEBUG
    print_bad_eraseblocks(mtd_dev, si);
#endif
    if (si->alien_cnt) {
        LOGW("%d of %d eraseblocks contain non-UBI data\n", si->alien_cnt,
             si->good_cnt);
    }
    if (!ubi_params->override_ec && si->empty_cnt < si->good_cnt) {
        int percent = ((double) si->ok_cnt) / si->good_cnt * 100;
        /*
         * Make sure the majority of eraseblocks have valid
         * erase counters.
         */
        if (percent < 50) {
            LOGW("only %d of %d eraseblocks have valid erase counter\n",
                 si->ok_cnt, si->good_cnt);
            ubi_params->ec = 0;
            ubi_params->override_ec = 1;
        } else if (percent < 95) {
            LOGW("only %d of %d eraseblocks have valid erase counter\n",
                 si->ok_cnt, si->good_cnt);
            LOGI("mean erase counter %lld will be used for the rest of eraseblock\n",
                 si->mean_ec);
            ubi_params->ec = si->mean_ec;
            ubi_params->override_ec = 1;
        }
    }

    if (ubi_params->override_ec)
        LOGI("Use erase counter %lld for all eraseblocks\n", ubi_params->ec);

    ubi_params->si = si;
#ifdef UBI_OPEN_DEBUG
    dump_ubi_si(ubi_params);
#endif
    return 0;
out:
    return -1;
}

static void ubi_params_free(struct ubi_params **params) {
    if (*params == NULL)
        return;
    if ((*params)->vtbl) {
        free((*params)->vtbl);
        (*params)->vtbl = NULL;
    }
    if ((*params)->si) {
        ubi_scan_free((*params)->si);
        (*params)->si = NULL;
    }
    if ((*params)->ui) {
        free((*params)->ui);
        (*params)->ui = NULL;
    }
    if ((*params)->outbuf) {
        free((*params)->outbuf);
        (*params)->outbuf = NULL;
    }
    if ((*params)->vi) {
        free((*params)->vi);
        (*params)->vi = NULL;
    }
    if ((*params)) {
        free((*params));
        (*params) = NULL;
    }
}

static int ubi_params_init(struct filesystem* fs,
                           struct ubi_params **ubi_params) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct block_manager *bm = FS_GET_BM(fs);
    struct ubi_params *params = NULL;
    struct ubigen_info *ui = NULL;
    unsigned int image_seq = 0;

    params = calloc(1, sizeof(*params));
    if (params == NULL) {
        LOGE("Cannot alloc more memory space for ubi parameter\n");
        goto out;
    }
    ui = calloc(1, sizeof(*ui));
    if (ui == NULL) {
        LOGE("Cannot alloc more memory space for ubigen info\n");
        goto out;
    }
    ubiutils_srand();
    image_seq = rand();
    params->devinfo.peb_size = mtd->eb_size;
    params->devinfo.page_size = mtd->min_io_size;
    params->devinfo.subpage_size = mtd->subpage_size;
    params->devinfo.partition_size = mtd->size;
    params->devinfo.device_size = bm->get_capacity(bm);
    params->vol_name = (char*)mtd->name;
    params->vid_hdr_offs = UBI_VID_HDR_OFFSET_INIT;
    params->ubi_ver = UBI_VERSION_DEFAULT;
    params->override_ec = UBI_OVERRIDE_EC;
    ubigen_info_init(ui, params->devinfo.peb_size, params->devinfo.page_size,
                     params->devinfo.subpage_size, params->vid_hdr_offs,
                     params->ubi_ver, image_seq);
    params->ui = ui;
#ifdef UBI_OPEN_DEBUG
    dump_ubi_ui(params);
#endif
    if (ubi_mtd_part_check(fs, params) < 0) {
        LOGE("ubi mtd part checking is failed\n");
        goto out;
    }

    params->outbuf = malloc(ui->peb_size);
    if (params->outbuf == NULL) {
        LOGE("cannot allocate %d bytes of memory\n", ui->peb_size);
        goto out;
    }
    memset(params->outbuf, 0xFF, ui->data_offs);
    ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)params->outbuf, params->ec);

    *ubi_params = params;
    return 0;
out:
    ubi_params_free(&params);
    return -1;
}

static int ubi_bypass_layout_vol(struct filesystem *fs,
                                 int64_t eb_off, struct ubi_params *params) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct block_manager *bm = FS_GET_BM(fs);
    libmtd_t libmtd = BM_GET_MTD_DESC(bm);
    int mtd_fd = MTD_DEV_INFO_TO_FD(mtd);
    struct ubigen_info *ui = params->ui;
    struct ubi_scan_info *si = params->si;
    int volume_table_cnt = UBI_LAYOUT_VOLUME_DEFAULT_COUNT;
    int64_t eb = eb_off, valid_eb = 0;
    char *tmp_buf = NULL;
    int err;

    if (params->layout_volume_start_eb > 0) {
        return params->layout_volume_start_eb;
    }

    tmp_buf = calloc(1, ui->peb_size);
    if (tmp_buf == NULL) {
        LOGE("Cannot alloc more space for temp buffer\n");
        goto out;
    }

    LOGI("Bypass start at eb %lld, bypass layout eb count is %d\n",
         eb, volume_table_cnt);
    while (eb < mtd->eb_cnt) {
        if (si->ec[eb] == EB_BAD) {
            eb++;
            continue;
        }
        err = mtd_erase(libmtd, mtd, mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %lld\n", eb);
            if (errno != EIO) {
                LOGE("fatal error on eraseblock %lld\n", eb);
                goto out;
            }

            if (mark_bad(mtd, si, eb))
                goto out;
            eb++;
            continue;
        }
        err = mtd_write(libmtd, mtd, mtd_fd, eb, 0, tmp_buf,
                        ui->peb_size, NULL, 0, 0);
        if (err) {
            LOGE("cannot write data (%d bytes buffer) to eraseblock %lld\n",
                 ui->peb_size, eb);

            if (errno != EIO) {
                LOGE("fatal error on writeblock %lld\n", eb);
                goto out;
            }
            err = mtd_torture(libmtd, mtd, mtd_fd, eb);
            if (err) {
                if (mark_bad(mtd, si, eb))
                    goto out;
                eb++;
                continue;
            }
        }
        err = mtd_erase(libmtd, mtd, mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %lld\n", eb);
            if (errno != EIO) {
                LOGE("fatal error on eraseblock %lld\n", eb);
                goto out;
            }

            if (mark_bad(mtd, si, eb))
                goto out;
            eb++;
            continue;
        }
        eb++;
        valid_eb++;
        if (valid_eb >= volume_table_cnt)
            break;
    }

    if (eb > mtd->eb_cnt) {
        LOGE("Cannot bypass ubi layout volume\n");
        goto out;
    }
    ubi->layout_volume_start_eb = eb;
    LOGI("Volume will be writen start at eb %lld\n",
         ubi->layout_volume_start_eb);

    if (tmp_buf)
        free(tmp_buf);
    return eb;
out:
    if (tmp_buf)
        free(tmp_buf);
    return -1;
}

static int change_ech(struct ubi_ec_hdr *hdr, uint32_t image_seq, long long ec) {
    uint32_t crc;

    /* Check the EC header */
    if (be32_to_cpu(hdr->magic) != UBI_EC_HDR_MAGIC) {
        LOGE("bad UBI magic %#08x, should be %#08x",
             be32_to_cpu(hdr->magic), UBI_EC_HDR_MAGIC);
        return -1;
    }

    crc = local_crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
    if (be32_to_cpu(hdr->hdr_crc) != crc) {
        LOGE("bad CRC %#08x, should be %#08x\n",
             crc, be32_to_cpu(hdr->hdr_crc));
        return -1;
    }

    hdr->image_seq = cpu_to_be32(image_seq);
    hdr->ec = cpu_to_be64(ec);
    crc = local_crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
    hdr->hdr_crc = cpu_to_be32(crc);

    return 0;
}

static int drop_ffs(const struct mtd_dev_info *mtd, const void *buf, int len) {
    int i;

    for (i = len - 1; i >= 0; i--)
        if (((const uint8_t *) buf)[i] != 0xFF)
            break;

    /* The resulting length must be aligned to the minimum flash I/O size */
    len = i + 1;
    len = (len + mtd->min_io_size - 1) / mtd->min_io_size;
    len *= mtd->min_io_size;
    return len;
}

int64_t ubi_write_one_peb(struct filesystem *fs,
                          libmtd_t libmtd, struct mtd_dev_info *mtd,
                          struct ubigen_info *ui, struct ubi_scan_info *si,
                          int64_t eb, void *buf) {
    int mtd_fd = MTD_DEV_INFO_TO_FD(mtd);
    int write_len;
    int write_flag = 0;
    long long ec;
    int err;

    while (eb < mtd->eb_cnt) {
        if (si->ec[eb] == EB_BAD) {
            LOGI("block %lld is bad on mtd(%d) \n", eb, mtd->mtd_num);
            eb++;
            continue;
        }

        err = mtd_erase(libmtd, mtd, mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %lld\n", eb);
            if (errno != EIO) {
                LOGE("fatal error occured on %lld\n", eb);
                return -1;
            }
            if (mark_bad(mtd, si, eb)) {
                LOGE("mark bad block failed on %lld\n", eb);
                return -1;
            }
            eb++;
            mtd_bm_block_map_set(fs,
                                 MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_BAD);
            continue;
        }
        mtd_bm_block_map_set(fs,
                             MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_ERASED);
        if (ubi->override_ec)
            ec = ubi->ec;
        else if (si->ec[eb] <= EC_MAX)
            ec = si->ec[eb] + 1;
        else
            ec = si->mean_ec;

        err = change_ech((struct ubi_ec_hdr *) buf, ui->image_seq, ec);
        if (err) {
            LOGI("bad EC header at eraseblock %lld\n", eb);
            return -1;
        }
        write_len = drop_ffs(mtd, buf, mtd->eb_size);
        LOGI("write eb %lld with ec %lld\n", eb, ec);
        err = mtd_write(libmtd, mtd, mtd_fd, eb, 0, buf, write_len, NULL,
                        0, 0);
        if (err) {
            LOGE("cannot write eraseblock %lld\n", eb);

            if (errno != EIO) {
                LOGE("fatal error occured on %lld\n", eb);
                return -1;
            }

            err = mtd_torture(libmtd, mtd, mtd_fd, eb);
            if (err) {
                if (mark_bad(mtd, si, eb)) {
                    LOGE("mark bad block failed on %lld\n", eb);
                    return -1;
                }
            }
            eb++;
            continue;
        }
        eb++;
        write_flag = 1;
        break;
    }

    if (eb > mtd->eb_cnt) {
        LOGE("write size is overflowed, eb = %lld, mtd eb total: %d\n", eb, mtd->eb_cnt);
        return -1;
    }

    if (write_flag) {
        return eb;
    }
    return -1;
}

static int64_t ubi_write_volume(struct filesystem *fs,
                                libmtd_t libmtd, struct mtd_dev_info *mtd,
                                char *buf, int64_t eb_off, int64_t length,
                                struct ubi_params *params) {
    struct ubigen_info *ui = params->ui;
    struct ubi_scan_info *si = params->si;
    struct ubigen_vol_info *vi = params->vi;
    long long bytes = length;
    char *inbuf = buf;
    char *outbuf = params->outbuf;
    long long eb = eb_off;
    int usable_leb_len = vi->usable_leb_size;
    int lnum = params->vid_hdr_lnum;
    struct ubi_vid_hdr *vid_hdr;
    int64_t err = 0;

    if (outbuf == NULL) {
        LOGE("ubi parameter \'outbuf\' must be initialed firstly\n");
        errno = EINVAL;
        goto out;
    }
    if (bytes % usable_leb_len) {
        LOGE("Passing parameter of write length must be leb aligned\n");
        errno = EINVAL;
        goto out;
    }

    LOGI("MTD:%s write volume chunk start eb at %lld, total eb count is %lld\n",
         MTD_DEV_INFO_TO_PATH(mtd),
         eb, bytes / usable_leb_len);
    while (bytes > 0) {
        set_process_info(fs, BM_OPERATION_ERASE_WRITE, fs->params->progress_size, fs->params->max_size);
        vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
        ubigen_init_vid_hdr(ui, vi, vid_hdr, lnum, inbuf, usable_leb_len);

        memcpy(outbuf + ui->data_offs, inbuf, usable_leb_len);
        memset(outbuf + ui->data_offs + usable_leb_len, 0xFF,
               ui->peb_size - ui->data_offs - usable_leb_len);

        err = ubi_write_one_peb(fs, libmtd, mtd, ui, si, eb, outbuf);
        if (err < 0) {
            LOGE("failed to write eraseblock %lld", eb);
            goto out;
        }
        eb = err;
        bytes -= usable_leb_len;
        inbuf += usable_leb_len;
        lnum += 1;
        fs->params->progress_size += usable_leb_len;
    }
    set_process_info(fs, BM_OPERATION_ERASE_WRITE, fs->params->progress_size, fs->params->max_size);
    params->vid_hdr_lnum = lnum;
    ubi->format_eb = eb;
    return  eb;
out:
    return -1;
}

int ubi_write_layout_vol(struct filesystem *fs,
                         struct ubigen_info *ui, int64_t eb,
                         int64_t ec1, int64_t ec2,
                         struct ubi_vtbl_record *vtbl, libmtd_t *libmtd,
                         struct mtd_dev_info *mtd, struct ubi_scan_info *si)
{
    int ret;
    struct ubigen_vol_info vi;
    char *outbuf  = NULL;
    struct ubi_vid_hdr *vid_hdr;
    int64_t start_eb  = eb;

    vi.bytes = ui->leb_size * UBI_LAYOUT_VOLUME_EBS;
    vi.id = UBI_LAYOUT_VOLUME_ID;
    vi.alignment = UBI_LAYOUT_VOLUME_ALIGN;
    vi.data_pad = ui->leb_size % UBI_LAYOUT_VOLUME_ALIGN;
    vi.usable_leb_size = ui->leb_size - vi.data_pad;
    vi.data_pad = ui->leb_size - vi.usable_leb_size;
    vi.type = UBI_LAYOUT_VOLUME_TYPE;
    vi.name = UBI_LAYOUT_VOLUME_NAME;
    vi.name_len = strlen(UBI_LAYOUT_VOLUME_NAME);
    vi.compat = UBI_LAYOUT_VOLUME_COMPAT;

    outbuf = malloc(ui->peb_size);
    if (!outbuf) {
        LOGE("failed to allocate %d bytes",
             ui->peb_size);
        return -1;
    }

    memset(outbuf, 0xFF, ui->data_offs);
    vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
    memcpy(outbuf + ui->data_offs, vtbl, ui->vtbl_size);
    memset(outbuf + ui->data_offs + ui->vtbl_size, 0xFF,
           ui->peb_size - ui->data_offs - ui->vtbl_size);

    ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf, ec1);
    ubigen_init_vid_hdr(ui, &vi, vid_hdr, 0, NULL, 0);

    LOGI("write layout volume0 to eb%lld\n", start_eb);
    if ((ret = ubi_write_one_peb(fs, libmtd, mtd, ui, si, start_eb, outbuf)) < 0) {
        LOGE("cannot write %d bytes to eb %lld\n", ui->peb_size, start_eb);
        goto out_free;
    }

    start_eb = ret;
    ubigen_init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf, ec2);
    ubigen_init_vid_hdr(ui, &vi, vid_hdr, 1, NULL, 0);
    LOGI("write layout volume1 to eb%lld\n", start_eb);
    if ((ret = ubi_write_one_peb(fs, libmtd, mtd, ui, si, start_eb, outbuf)) < 0) {
        LOGE("cannot write %d bytes to eb %lld\n", ui->peb_size, start_eb);
        goto out_free;
    }
    if (outbuf)
        free(outbuf);
    return 0;

out_free:
    if (outbuf)
        free(outbuf);
    return -1;
}

static int64_t format(struct filesystem *fs, libmtd_t libmtd,
                      struct mtd_dev_info *mtd, struct ubigen_info *ui,
                      struct ubi_scan_info *si, int64_t start_eb, int novtbl) {
    int64_t eb, err, write_size;
    struct ubi_ec_hdr *hdr = NULL;
    struct ubi_vtbl_record *vtbl;
    int eb1 = -1, eb2 = -1;
    long long ec1 = -1, ec2 = -1;
    int override_ec = ubi->override_ec;
    int64_t preset_ec = ubi->ec;
    int mtd_fd = MTD_DEV_INFO_TO_FD(mtd);

    write_size = UBI_EC_HDR_SIZE + mtd->subpage_size - 1;
    write_size /= mtd->subpage_size;
    write_size *= mtd->subpage_size;

    hdr = malloc(write_size);
    if (hdr == NULL) {
        LOGE("cannot allocate %lld bytes of memory\n", write_size);
        return -1;
    }
    memset(hdr, 0xFF, write_size);

    LOGI("MTD \"%s\"  volume tailing format from eb %lld to eb %d\n",
         MTD_DEV_INFO_TO_PATH(mtd), start_eb, mtd->eb_cnt);
    for (eb = start_eb; eb < mtd->eb_cnt; eb++) {
        int64_t ec;

        set_process_info(fs, BM_OPERATION_FORMAT,
                         eb - start_eb, mtd->eb_cnt - start_eb);
        if (si->ec[eb] == EB_BAD) {
            LOGI("MTD \"%s\"  volume format bypass bad eb %lld\n",
                 MTD_DEV_INFO_TO_PATH(mtd), eb);
            continue;
        }
        if (override_ec)
            ec = preset_ec;
        else if (si->ec[eb] <= EC_MAX)
            ec = si->ec[eb] + 1;
        else
            ec = si->mean_ec;
        ubigen_init_ec_hdr(ui, hdr, ec);

        err = mtd_erase(libmtd, mtd, mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %lld\n", eb);
            if (errno != EIO) {
                LOGE("fatal error on eraseblock %lld\n", eb);
                goto out_free;
            }

            if (mark_bad(mtd, si, eb))
                goto out_free;

            mtd_bm_block_map_set(fs,
                                 MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_BAD);
            continue;
        }
        mtd_bm_block_map_set(fs,
                             MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb), MTD_BLK_ERASED);
        if ((eb1 == -1 || eb2 == -1) && !novtbl) {
            if (eb1 == -1) {
                eb1 = eb;
                ec1 = ec;
            } else if (eb2 == -1) {
                eb2 = eb;
                ec2 = ec;
            }
            continue;
        }

        LOGI("write eb %lld with ec %lld\n", eb, ec);
        err = mtd_write(libmtd, mtd, mtd_fd, eb, 0, hdr,
                        write_size, NULL, 0, 0);
        if (err) {
            LOGE("cannot write EC header (%lld bytes buffer) to eraseblock %lld\n",
                 write_size, eb);

            if (errno != EIO) {
                LOGE("fatal error on writeblock %lld\n", eb);
                goto out_free;
            }
            err = mtd_torture(libmtd, mtd, mtd_fd, eb);
            if (err) {
                if (mark_bad(mtd, si, eb))
                    goto out_free;
            }
            continue;
        }
    }

    if (!novtbl) {
        if (eb1 == -1 || eb2 == -1) {
            LOGE("no eraseblocks for volume table\n");
            goto out_free;
        }

        LOGI("write volume table to eraseblocks %d and %d\n", eb1, eb2);
        vtbl = ubigen_create_empty_vtbl(ui);
        if (!vtbl)
            goto out_free;

        err = ubi_write_layout_vol(fs, ui, eb1, ec1, ec2, vtbl, &libmtd, mtd, si);
        free(vtbl);
        if (err) {
            LOGI("cannot write layout volume");
            goto out_free;
        }
    }
    set_process_info(fs, BM_OPERATION_FORMAT,
                     eb - start_eb, mtd->eb_cnt - start_eb);
    free(hdr);
    return eb;

out_free: free(hdr);
    return -1;
}

static int ubifs_init(struct filesystem *fs) {
    return 0;
};

static int ubi_format(struct filesystem *fs) {
    if (ubi == NULL) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    ubi->format_eb = ubi->layout_volume_start_eb;
    return 0;
out:
    return -1;
}

static int64_t ubifs_erase(struct filesystem *fs) {
    int64_t offset = fs->params->offset;
    int64_t length = fs->params->length;
    int64_t retval;

    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    retval = (offset + length + ubi->ui->peb_size)
             & (~(ubi->ui->peb_size) + 1);
    return retval;
out:
    return -1;
}

static int64_t ubifs_write(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct block_manager * bm = FS_GET_BM(fs);
    libmtd_t libmtd = BM_GET_MTD_DESC(bm);
    int64_t offset = fs->params->offset;
    int64_t eb = MTD_OFFSET_TO_EB_INDEX(mtd, offset);
    int64_t length = fs->params->length;
    char *buf = fs->params->buf;

    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    eb = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, eb);
    eb = ubi_write_volume(fs, libmtd, mtd, buf, eb, length, ubi);
    if (eb < 0) {
        LOGE("ubi write volume wrong\n");
        goto out;
    }
    return (MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, eb)) * ubi->ui->peb_size;
out:
    ubi_params_free(&ubi);
    return -1;
}

static int64_t ubifs_read(struct filesystem *fs) {
    assert_die_if(1, "%s is not served temporarily\n", __func__);
    return 0;
}

static int64_t ubifs_done(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    struct block_manager * bm = FS_GET_BM(fs);
    libmtd_t libmtd = BM_GET_MTD_DESC(bm);
    struct ubigen_info *ui = ubi->ui;
    int64_t ec1 = ubi->ec, ec2 = ubi->ec;
    struct ubi_vtbl_record *vtbl = ubi->vtbl;
    struct ubi_scan_info *si = ubi->si;
    int64_t start_eb = ubi->start_eb;
    int64_t retval;

    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    if (ubi_write_layout_vol(fs, ui, start_eb, ec1, ec2, vtbl, &libmtd, mtd, si) < 0) {
        LOGE("ubi write layout volume failed\n");
        goto out;
    }

    if ((retval = format(fs, libmtd, mtd, ui, si, ubi->format_eb, 1)) < 0) {
        LOGE("Cannot format the tailing ebs in mtd \"%s\"\n", mtd->name);
        goto out;
    }

    if (retval > 0)
        retval = MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, retval);

    retval *= ui->peb_size;

    LOGI("Total lnum is %d, Next mtd partition write start at 0x%llx\n",
         ubi->vid_hdr_lnum, retval);
    ubi_params_free(&ubi);
    return retval;
out:
    ubi_params_free(&ubi);
    return -1;
}

static int64_t ubifs_get_operate_start_address(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int64_t eb, err;

    if (ubi_params_init(fs, &ubi) < 0) {
        LOGE("ubi parameter init failed\n");
        goto out;
    }
    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    eb = fs->params->offset / ubi->ui->peb_size;
    eb = MTD_EB_ABSOLUTE_TO_RELATIVE(mtd, eb);
    err = ubi_bypass_layout_vol(fs, eb, ubi);
    if (err < 0) {
        LOGE("Cannot bypass default layout volume\n");
        goto out;
    }

    ubi->vid_hdr_lnum = 0;
    ubi->start_eb = eb;
    ubi->layout_volume_start_eb = err;
    LOGI("ubifs start at eb %lld, bypassed layout eb count is %lld"
         " volume layout will start at eb %lld\n",
         ubi->start_eb, err - eb,
         ubi->layout_volume_start_eb);

    return (MTD_EB_RELATIVE_TO_ABSOLUTE(mtd, err)) * ubi->ui->peb_size;
out:
    ubi_params_free(&ubi);
    return -1;
}

static unsigned long ubifs_get_leb_size(struct filesystem *fs) {
    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    return ubi->ui->leb_size;
out:
    ubi_params_free(&ubi);
    return 0;
}

static int64_t ubifs_get_max_mapped_size_in_partition(struct filesystem *fs) {
    struct mtd_dev_info *mtd = FS_GET_MTD_DEV(fs);
    int64_t max_mapped_size, bypassed_bytes;
    int64_t length = fs->params->length;
    // int64_t eb, err;

    if ((ubi == NULL)
            || (ubi->ui == NULL)) {
        LOGE("ubi parameter is null\n");
        goto out;
    }
    if (ubi_volume_params_set(ubi, length) < 0) {
        LOGE("ubi volume paramters setting is failed\n");
        goto out;
    }

    if (!ubi->layout_volume_start_eb) {
        LOGE("The function ubi_bypass_layout_vol must be called firstly\n");
        goto out;
    }

    bypassed_bytes =  ubi->ui->peb_size *
                      (ubi->layout_volume_start_eb -  ubi->start_eb);

    fs->params->length += bypassed_bytes;
    max_mapped_size = mtd_block_scan(fs);
    fs->params->length = length;
    if (!max_mapped_size) {
        LOGE("Failed to scan mtd block at mtd '%s'\n",
             MTD_DEV_INFO_TO_PATH(mtd));
        goto out;
    }
#ifdef UBI_OPEN_DEBUG
    dump_ubi_params(ubi);
#endif
    return max_mapped_size;
out:
    ubi_params_free(&ubi);
    return -1;
}

struct filesystem fs_ubifs = {
    .name = BM_FILE_TYPE_UBIFS,
    .init = ubifs_init,
    .alloc_params = fs_alloc_params,
    .free_params = fs_free_params,
    .set_params = fs_set_params,
    .format = ubi_format,
    .erase = ubifs_erase,
    .read = ubifs_read,
    .write = ubifs_write,
    .done = ubifs_done,
    .get_operate_start_address = ubifs_get_operate_start_address,
    .get_leb_size = ubifs_get_leb_size,
    .get_max_mapped_size_in_partition =
    ubifs_get_max_mapped_size_in_partition,
};
