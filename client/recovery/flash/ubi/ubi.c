#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <lib/mtd/ubi-media.h>
#include <lib/mtd/mtd_swab.h>
#include <lib/ubi/libubigen.h>
#include <lib/ubi/libubi.h>
#include <lib/ubi/libscan.h>
#include <lib/ini/iniparser.h>
#include <flash/flash_manager.h>
#include <utils/linux.h>
#include <lib/ini/dictionary.h>
#include <utils/common.h>
#include <utils/log.h>
#include <lib/ubi/ubi.h>
#include <lib/crc/libcrc.h>

#define LOG_TAG "ubi"
#define MTD_PART_HEAD "/dev/mtd"
#define DEFAULT_CTRL_DEV "/dev/ubi_ctrl"

#define MAX_CONSECUTIVE_BAD_BLOCKS 4
#define WL_RESERVED_PEBS 1
#define EBA_RESERVED_PEBS 1

struct args {
#ifndef CONFIG_UBI_INI_PARSER_LOAD_IN_MEM
    const char *f_in;
#endif
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    const char *f_out;
    int out_fd;
#endif
    int peb_size;
    int min_io_size;
    int subpage_size;
    int vid_hdr_offs;
    long long ec;
    int ubi_ver;
    unsigned int image_seq;
    int quiet;
    dictionary *dict;

    int mtd_fd;
    int override_ec;
    int start_eb;
    int valid_eb;
    int upgrade_blks;
};

struct ubi_params {
    char *ubifs_img_name;
    char *mtd_part_name;
    char *vol_name;
    unsigned int pebsize;
    unsigned int pagesize;
    unsigned int partition_size;
    unsigned int device_size;
    unsigned int max_beb_per1024;
    unsigned int ubi_reserved_blks;
    unsigned int lebsize;
    unsigned int vol_size;
    char *vol_info;
    struct mtd_dev_info* mtd_dev;
    struct ubigen_info *ui;
    struct ubi_scan_info *si;
    struct flash_manager *this;
};

static struct args args = { .peb_size = -1, .min_io_size = -1, .subpage_size =
        -1 };

static struct flash_event process_info;

static inline int is_power_of_2(unsigned long long n) {
    return (n != 0 && ((n & (n - 1)) == 0));
}
#if 0
static void print_bad_eraseblocks(const struct mtd_dev_info *mtd,
        const struct ubi_scan_info *si) {
    int first = 1, eb;

    if (si->bad_cnt == 0)
    return;

    if (!args.quiet) {
        printf("%d bad eraseblocks found, numbers: ", si->bad_cnt);
        for (eb = 0; eb < mtd->eb_cnt; eb++) {
            if (si->ec[eb] != EB_BAD)
            continue;
            if (first) {
                printf("%d", eb);
                first = 0;
            } else
            printf(", %d", eb);
        }
        printf("\n");
    }
}
#endif
/**
 * ubiutils_srand - randomly seed the standard pseudo-random generator.
 *
 * This helper function seeds the standard libc pseudo-random generator with a
 * more or less random value to make sure the 'rand()' call does not return the
 * same sequence every time UBI utilities run. Returns zero in case of success
 * and a %-1 in case of error.
 */
int ubiutils_srand(void) {
    struct timeval tv;
    struct timezone tz;
    unsigned int seed;

    /*
     * Just assume that a combination of the PID + current time is a
     * reasonably random number.
     */
    if (gettimeofday(&tv, &tz))
        return -1;

    seed = (unsigned int) tv.tv_sec;
    seed += (unsigned int) tv.tv_usec;
    seed *= getpid();
    seed %= RAND_MAX;
    srand(seed);
    return 0;
}

static int read_section(const struct ubigen_info *ui, const char *sname,
        struct ubigen_vol_info *vi, const char **img, struct stat *st) {
    char buf[256];
    const char *p;

    *img = NULL;

    if (strlen(sname) > 128) {
        LOGE("too long section name \"%s\"", sname);
        return -1;
    }

    /* Make sure mode is UBI, otherwise ignore this section */
    sprintf(buf, "%s:mode", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (!p) {
        LOGE("\"mode\" key not found in section \"%s\"", sname);
        LOGE("the \"mode\" key is mandatory and has to be "
                "\"mode=ubi\" if the section describes an UBI volume");
        return -1;
    }

    /* If mode is not UBI, skip this section */
    if (strcmp(p, "ubi")) {
        LOGI("skip non-ubi section \"%s\"", sname);
        return 1;
    }

    LOGI("mode=ubi, keep parsing");

    /* Fetch volume type */
    sprintf(buf, "%s:vol_type", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (!p) {
        LOGI("volume type was not specified in "
                "section \"%s\", assume \"dynamic\"\n", sname);
        vi->type = UBI_VID_DYNAMIC;
    } else {
        if (!strcmp(p, "static"))
            vi->type = UBI_VID_STATIC;
        else if (!strcmp(p, "dynamic"))
            vi->type = UBI_VID_DYNAMIC;
        else {
            LOGE("invalid volume type \"%s\" in section  \"%s\"", p, sname);
            return -1;
        }
    }

    LOGI("volume type: %s", vi->type == UBI_VID_DYNAMIC ? "dynamic" : "static");
    /* Fetch the name of the volume image file */
    sprintf(buf, "%s:image", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (p) {
        *img = p;
        if (stat(p, st)) {
            LOGE("cannot stat \"%s\" referred from section \"%s\"", p, sname);
            return -1;
        }
        if (st->st_size == 0) {
            LOGE("empty file \"%s\" referred from section \"%s\"", p, sname);
            return -1;
        }
    } else if (vi->type == UBI_VID_STATIC) {
        LOGE("image is not specified for static volume in section \"%s\"",
                sname);
        return -1;
    }

    /* Fetch volume id */
    sprintf(buf, "%s:vol_id", sname);
    vi->id = iniparser_getint(args.dict, buf, -1);
    if (vi->id == -1) {
        LOGE("\"vol_id\" key not found in section  \"%s\"", sname);
        return -1;
    }
    if (vi->id < 0) {
        LOGE("negative volume ID %d in section \"%s\"", vi->id, sname);
        return -1;
    }

    if (vi->id >= ui->max_volumes) {
        LOGE("too high volume ID %d in section \"%s\", max. is %d", vi->id,
                sname, ui->max_volumes);
        return -1;
    }

    LOGI("volume ID: %d", vi->id);

    /* Fetch volume size */
    sprintf(buf, "%s:vol_size", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (p) {
        vi->bytes = get_bytes(p);
        if (vi->bytes <= 0) {
            LOGE("bad \"vol_size\" key value \"%s\" (section \"%s\")", p,
                    sname);
            return -1;
        }

        /* Make sure the image size is not larger than volume size */
        if (*img && st->st_size > vi->bytes) {
            LOGE("error in section \"%s\": size of the image file "
                    "\"%s\" is %lld, which is larger than volume size %lld",
                    sname, *img, (long long )st->st_size, vi->bytes);
            return -1;
        }
        LOGI("volume size: %lld bytes", vi->bytes);
    } else {
        struct stat st;

        if (!*img) {
            LOGE("neither image file (\"image=\") nor volume size "
                    "(\"vol_size=\") specified in section \"%s\"", sname);
            return -1;
        }

        if (stat(*img, &st)) {
            LOGE("cannot stat \"%s\"", *img);
            return -1;
        }

        vi->bytes = st.st_size;

        if (vi->bytes == 0) {
            LOGE("file \"%s\" referred from section \"%s\" is empty", *img,
                    sname);
            return -1;
        }

        LOGI("volume size was not specified in section \"%s\", assume"
                " minimum to fit image \"%s\", size is %lld bytes", sname,
                *img, vi->bytes);
    }

    /* Fetch volume name */
    sprintf(buf, "%s:vol_name", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (!p) {
        LOGE("\"vol_name\" key not found in section \"%s\"", sname);
        return -1;
    }

    vi->name = p;
    vi->name_len = strlen(p);
    if (vi->name_len > UBI_VOL_NAME_MAX) {
        LOGE("too long volume name in section \"%s\", max. is %d characters",
                vi->name, UBI_VOL_NAME_MAX);
        return -1;
    }

    LOGI("volume name: %s", p);

    /* Fetch volume alignment */
    sprintf(buf, "%s:vol_alignment", sname);
    vi->alignment = iniparser_getint(args.dict, buf, -1);
    if (vi->alignment == -1)
        vi->alignment = 1;
    else if (vi->id < 0) {
        LOGE("negative volume alignement %d in section \"%s\"", vi->alignment,
                sname);
        return -1;
    }

    LOGI("volume alignment: %d", vi->alignment);

    /* Fetch volume flags */
    sprintf(buf, "%s:vol_flags", sname);
    p = iniparser_getstring(args.dict, buf, NULL);
    if (p) {
        if (!strcmp(p, "autoresize")) {
            LOGI("autoresize flags found");
            vi->flags |= UBI_VTBL_AUTORESIZE_FLG;
        } else {
            LOGE("unknown flags \"%s\" in section \"%s\"", p, sname);
            return -1;
        }
    }

    /* Initialize the rest of the volume information */
    vi->data_pad = ui->leb_size % vi->alignment;
    vi->usable_leb_size = ui->leb_size - vi->data_pad;
    if (vi->type == UBI_VID_DYNAMIC)
        vi->used_ebs = (vi->bytes + vi->usable_leb_size - 1)
                / vi->usable_leb_size;
    else
        vi->used_ebs = (st->st_size + vi->usable_leb_size - 1)
                / vi->usable_leb_size;
    vi->compat = 0;
    return 0;
}

void dump_ubi_params(struct ubi_params *params) {
    LOGI("ubifs image: %s", params->ubifs_img_name);
    LOGI("mtd char name: %s", params->mtd_part_name);
    LOGI("volume name: %s", params->vol_name);
    LOGI("peb size: %d", params->pebsize);
    LOGI("page size: %d", params->pagesize);
    LOGI("partition size: %d", params->partition_size);
    LOGI("device size: %d", params->device_size);
    LOGI("max beb per1024: %d", params->max_beb_per1024);
    LOGI("ubi reserved blks: %d", params->ubi_reserved_blks);
    LOGI("leg size: %d", params->lebsize);
    LOGI("volume size: %d", params->vol_size);
    LOGI("volume info address: %p", params->vol_info);
    LOGI("ui address: %p", params->ui);
    LOGI("si address: %p", params->si);
    return;
}

static void ubi_params_free(struct ubi_params *params) {

    if (params->ui) {
        free(params->ui);
        params->ui = NULL;
    }
    if (params->vol_info) {
        free(params->vol_info);
        params->vol_info = NULL;
    }
    if (params->si) {
        free(params->si);
        params->si = NULL;
    }
    if (params) {
        free(params);
        params = NULL;
    }
}

static int get_bad_peb_limit(struct ubi_params *params)
{
    unsigned int limit, device_pebs, max_beb_per1024;

    max_beb_per1024 = params->max_beb_per1024;
    if (!max_beb_per1024)
        return 0;

    /*
     * Here we are using size of the entire flash chip and
     * not just the MTD partition size because the maximum
     * number of bad eraseblocks is a percentage of the
     * whole device and bad eraseblocks are not fairly
     * distributed over the flash chip. So the worst case
     * is that all the bad eraseblocks of the chip are in
     * the MTD partition we are attaching (ubi->mtd).
     */
    device_pebs = params->device_size / params->pebsize;
    limit = mult_frac(device_pebs, max_beb_per1024, 1024);
    /* Round it up */
    if (mult_frac(limit, 1024, max_beb_per1024) < device_pebs)
        limit += 1;

    return limit;
}

static int ubi_volume_params_set(struct ubi_params *params) {
    char *buf = NULL;
    static int vol_id;
    unsigned int logic_blkcnt = 0;
    unsigned int logic_blksize = 0;
    unsigned int vol_size = 0;

    buf = malloc(1024);
    if (buf == NULL) {
        LOGE("malloc error");
        return -1;
    }

    logic_blksize = params->pebsize - 2 * params->pagesize;
    params->max_beb_per1024 = CONFIG_MTD_UBI_BEB_LIMIT;
    params->ubi_reserved_blks = UBI_LAYOUT_VOLUME_EBS + WL_RESERVED_PEBS +
                EBA_RESERVED_PEBS + get_bad_peb_limit(params);
    logic_blkcnt = params->si->good_cnt - params->ubi_reserved_blks;
    vol_size = logic_blkcnt * logic_blksize;
    params->lebsize = logic_blksize;
    params->vol_size = vol_size;
    params->vol_info = buf;

    sprintf(buf, "[%s-volume]\n"
        "mode=ubi\n"
        "image=%s\n"
        "vol_id=%d\n"
        "vol_size=%d\n"
        "vol_type=dynamic\n"
        "vol_name=%s\n", params->vol_name, params->ubifs_img_name, vol_id,
            params->vol_size, params->vol_name);

    vol_id++;
    return 0;
}

static int ubi_mtd_part_check(struct mtd_dev_info* mtd_dev,
        struct ubi_params *ubi_params) {
    struct ubi_scan_info *si = NULL;
    libubi_t libubi;
    int err;

    if (!is_power_of_2(mtd_dev->min_io_size)) {
        LOGE("min. I/O size is %d, but should be power of 2",
                mtd_dev->min_io_size);
        return -1;
    }
    if (!mtd_dev->writable) {
        LOGE("mtd%d (%s) is a read-only device", mtd_dev->mtd_num,
                ubi_params->mtd_part_name);
        return -1;
    }

    args.mtd_fd = open(ubi_params->mtd_part_name, O_RDWR);
    if (args.mtd_fd == -1) {
        LOGE("cannot open \"%s\"", ubi_params->mtd_part_name);
        return -1;
    }

    /* Make sure this MTD device is not attached to UBI */
    libubi = libubi_open();
    if (libubi) {
        int ubi_dev_num;

        err = mtd_num2ubi_dev(libubi, mtd_dev->mtd_num, &ubi_dev_num);
        if (!err) {
            if (ubi_detach(libubi, DEFAULT_CTRL_DEV, ubi_params->mtd_part_name)
                    < 0) {
                LOGE("please, failed to detach mtd%d (%s) from ubi%d",
                        mtd_dev->mtd_num, DEFAULT_CTRL_DEV, ubi_dev_num);
                libubi_close(libubi);
                return -1;
            }
        }
        libubi_close(libubi);
    }
    //    if (!args.quiet){
    //        printf("mtd%d (%s), size ", mtd_dev->mtd_num, mtd_dev->type_str);
    //        print_bytes(mtd_dev->size, 1);
    //        printf(", %d eraseblocks of ", mtd_dev->eb_cnt);
    //        print_bytes(mtd_dev->eb_size, 1);
    //        printf(", min. I/O size %d bytes\n", mtd_dev->min_io_size);
    //    }
    err = ubi_scan(mtd_dev, args.mtd_fd, &si, 0);
    if (err) {
        LOGE("failed to scan mtd%d (%s)", mtd_dev->mtd_num,
                ubi_params->mtd_part_name);
        return -1;
    }
    if (si->good_cnt < 2) {
        if (si != NULL)
            free(si);
        LOGE("too few non-bad eraseblocks (%d) on mtd%d", si->good_cnt,
                mtd_dev->mtd_num);
        return -1;
    }
    if (si->ok_cnt)
        LOGI("%d eraseblocks have valid erase counter, mean value is %lld",
                si->ok_cnt, si->mean_ec);
    if (si->empty_cnt)
        LOGI("%d eraseblocks are supposedly empty", si->empty_cnt);
    if (si->corrupted_cnt)
        LOGI("%d corrupted erase counters", si->corrupted_cnt);
    //    print_bad_eraseblocks(mtd_dev, si);

    if (si->alien_cnt) {
        LOGW("%d of %d eraseblocks contain non-UBI data", si->alien_cnt,
                si->good_cnt);
    }
    if (!args.override_ec && si->empty_cnt < si->good_cnt) {
        int percent = ((double) si->ok_cnt) / si->good_cnt * 100;

        /*
         * Make sure the majority of eraseblocks have valid
         * erase counters.
         */
        if (percent < 50) {
            LOGW("only %d of %d eraseblocks have valid erase counter",
                    si->ok_cnt, si->good_cnt);
            args.ec = 0;
            args.override_ec = 1;
        } else if (percent < 95) {
            LOGW("only %d of %d eraseblocks have valid erase counter",
                    si->ok_cnt, si->good_cnt);
            LOGI(
                    "mean erase counter %lld will be used for the rest of eraseblock",
                    si->mean_ec);
            args.ec = si->mean_ec;
            args.override_ec = 1;
        }
    }

    if (args.override_ec)
        LOGI("use erase counter %lld for all eraseblocks", args.ec);

    ubi_params->si = si;
    return 0;
}

static unsigned int get_flash_size(struct flash_manager* this)
{
    struct mtd_dev_info* mtd_dev_info;
    unsigned int size = 0;
    int i;

    for (i = 0; i < this->mtd_info.mtd_dev_cnt; i++){
        mtd_dev_info = &this->mtd_dev_info[i];
        size += (unsigned int)mtd_dev_info->size;
    }

    return size;
}


int ubi_params_set(struct flash_manager* this, struct ubi_params **ubi,
        char *mtd_part, char *ubifsimg, char *volname) {

    struct ubi_params *params = NULL;
    struct ubigen_info *ui;
    struct mtd_dev_info* mtd_dev;
    int mtdnum = -1;

    params = malloc(sizeof(*params));
    if (params == NULL) {
        LOGE("malloc failed on ubi_params");
        goto out;
    }
    ui = malloc(sizeof(*ui));
    if (ui == NULL) {
        LOGE("malloc failed on ubigen_info");
        goto out_free_params;
    }

    mtdnum = strtoul(&mtd_part[strlen(MTD_PART_HEAD)], NULL, 10);
    mtd_dev = &(this->mtd_dev_info[mtdnum]);

    args.upgrade_blks = 0;
    args.peb_size = mtd_dev->eb_size;
    args.min_io_size = mtd_dev->min_io_size;
    args.subpage_size = mtd_dev->subpage_size;
    args.ubi_ver = 1;
    args.vid_hdr_offs = 0;

    ubiutils_srand();
    args.image_seq = rand();

    ubigen_info_init(ui, args.peb_size, args.min_io_size, args.subpage_size,
            args.vid_hdr_offs, args.ubi_ver, args.image_seq);

    params->ui = ui;
    params->ubifs_img_name = ubifsimg;
    params->mtd_part_name = mtd_part;
    params->vol_name = volname;
    params->pebsize = mtd_dev->eb_size;
    params->pagesize = mtd_dev->min_io_size;
    params->partition_size = mtd_dev->eb_size * mtd_dev->eb_cnt;
    params->device_size = get_flash_size(this);
    params->mtd_dev = mtd_dev;
    params->this = this;

    if (ubi_mtd_part_check(mtd_dev, params) < 0) {
        LOGE("ubi mtd part checking is failed");
        goto out_free_ui;
    }

    if (ubi_volume_params_set(params) < 0) {
        LOGE("ubi volume paramters setting is failed");
        goto out_free_ui;
    }

    *ubi = params;
    dump_ubi_params(params);

    return 0;
    out_free_ui: free(params->ui);
    params->ui = NULL;
    out_free_params: free(params);
    params = NULL;
    out: return -1;
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

static void ubi_start_eb_set(int eb) {
    args.start_eb = eb;
}

static int ubi_bypass_layout_vol(struct mtd_dev_info *mtd,
        struct ubi_scan_info *si, int volume_table_size) {
    int eb = 0;
    int start_eb = 0;

    while (eb < mtd->eb_cnt) {
        if (si->ec[eb] == EB_BAD) {
            eb++;
            continue;
        }
        start_eb++;
        eb++;
        if (start_eb >= volume_table_size)
            break;
    }

    if (eb >= mtd->eb_cnt) {
        LOGE("cannot bypass ubi layout volume");
        return -1;
    }
    ubi_start_eb_set(eb);
    LOGI("write volume start at block %d", args.start_eb);
    return 0;
}

/*
 * Returns %-1 if consecutive bad blocks exceeds the
 * MAX_CONSECUTIVE_BAD_BLOCKS and returns %0 otherwise.
 */
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

/* TODO: we should actually torture the PEB before marking it as bad */
static int mark_bad(const struct mtd_dev_info *mtd, struct ubi_scan_info *si,
        int eb) {
    int err;

    LOGI("marking block %d bad\n", eb);

    if (!mtd->bb_allowed) {
        LOGI("bad blocks not supported by this flash");
        return -1;
    }

    err = mtd_mark_bad(mtd, args.mtd_fd, eb);
    if (err)
        return err;

    si->bad_cnt += 1;
    si->ec[eb] = EB_BAD;

    return consecutive_bad_check(eb);
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

static void set_process_info(struct flash_manager* this,
        struct flash_event *info, int type, int progress, char *name) {
    info->type = type;
    info->progress = progress;
    info->name = name;

    if (this->listener)
        this->listener(this->param, info);
}

static void update_show_progress(struct flash_manager* this,
        struct mtd_dev_info *mtd, long long total, long long remained) {
    static int percent_s;
    int percent = (total - remained) * 100 / total;

    if (percent_s != percent) {
        if (!args.quiet)
            LOGI("upgrading process %d%%",(int)((total - remained) * 100 / total));
        set_process_info(this, &process_info, ACTION_WRITE, percent,
                (char *) mtd->name);
        percent_s = percent;
    }
}

int ubi_write_volume_to_mtd(libmtd_t *libmtd, struct mtd_dev_info *mtd,
        const struct ubigen_info *ui, struct ubi_scan_info *si, void *buf,
        int len) {
    int eb = args.start_eb;
    int new_len;
    int write_flag = 0;
    long long ec;
    int err;
    struct flash_manager *this =
            container_of(libmtd ,struct flash_manager, mtd_desc);

    while (eb < mtd->eb_cnt) {
        args.upgrade_blks++;
        update_show_progress(this, mtd, mtd->eb_cnt,
                mtd->eb_cnt - args.upgrade_blks);

        if (si->ec[eb] == EB_BAD) {
            LOGI("block %d is bad on mtd(%d) \n", eb, mtd->mtd_num);
            eb++;
            continue;
        }

        err = mtd_erase(*libmtd, mtd, args.mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %d", eb);
            if (errno != EIO) {
                LOGE("fatal error occured on %d", eb);
                return -1;
            }
            if (mark_bad(mtd, si, eb)) {
                LOGE("mark bad block failed on %d", eb);
                return -1;
            }

            eb++;
            continue;
        }
        if (args.override_ec)
            ec = args.ec;
        else if (si->ec[eb] <= EC_MAX)
            ec = si->ec[eb] + 1;
        else
            ec = si->mean_ec;

        //        printf(", change EC to %lld", ec);
        err = change_ech((struct ubi_ec_hdr *) buf, ui->image_seq, ec);
        if (err) {
            LOGI("bad EC header at eraseblock %d", eb);
            return -1;
        }
        new_len = drop_ffs(mtd, buf, mtd->eb_size);
        err = mtd_write(*libmtd, mtd, args.mtd_fd, eb, 0, buf, new_len, NULL,
                0, 0);
        if (err) {
            LOGE("cannot write eraseblock %d", eb);

            if (errno != EIO) {
                LOGE("fatal error occured on %d", eb);
                return -1;
            }

            err = mtd_torture(*libmtd, mtd, args.mtd_fd, eb);
            if (err) {
                if (mark_bad(mtd, si, eb)) {
                    LOGE("mark bad block failed on %d", eb);
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

    if ((eb >= mtd->eb_cnt) && !write_flag) {
        LOGE("write size is overflowed, eb = %d, mtd eb total: %d\n", eb, mtd->eb_cnt);
        return -1;
    }

    if (write_flag) {
        args.start_eb = eb;
        return 0;
    }
    return -1;
}

static int format(struct flash_manager *this, struct ubi_params *ubi_params,
        int start_eb, int novtbl) {
    int eb, err, write_size;
    struct ubi_ec_hdr *hdr;
    struct ubi_vtbl_record *vtbl;
    int eb1 = -1, eb2 = -1;
    long long ec1 = -1, ec2 = -1;
    struct mtd_dev_info *mtd = ubi_params->mtd_dev;
    struct ubigen_info *ui = ubi_params->ui;
    struct ubi_scan_info *si = ubi_params->si;

    write_size = UBI_EC_HDR_SIZE + mtd->subpage_size - 1;
    write_size /= mtd->subpage_size;
    write_size *= mtd->subpage_size;
    hdr = malloc(write_size);
    if (!hdr) {
        LOGE("cannot allocate %d bytes of memory", write_size);
        return -1;
    }
    memset(hdr, 0xFF, write_size);

    for (eb = start_eb; eb < mtd->eb_cnt; eb++) {
        long long ec;
        args.upgrade_blks++;
        update_show_progress(this, mtd, mtd->eb_cnt,
                mtd->eb_cnt - args.upgrade_blks);

        if (si->ec[eb] == EB_BAD)
            continue;

        if (args.override_ec)
            ec = args.ec;
        else if (si->ec[eb] <= EC_MAX)
            ec = si->ec[eb] + 1;
        else
            ec = si->mean_ec;
        ubigen_init_ec_hdr(ui, hdr, ec);

        err = mtd_erase(this->mtd_desc, mtd, args.mtd_fd, eb);
        if (err) {
            LOGE("failed to erase eraseblock %d", eb);
            if (errno != EIO) {
                LOGE("fatal error on eraseblock %d", eb);
                goto out_free;
            }

            if (mark_bad(mtd, si, eb))
                goto out_free;
            continue;
        }

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

        err = mtd_write(this->mtd_desc, mtd, args.mtd_fd, eb, 0, hdr,
                write_size, NULL, 0, 0);
        if (err) {
            LOGE("cannot write EC header (%d bytes buffer) to eraseblock %d",
                    write_size, eb);

            if (errno != EIO) {
                LOGE("fatal error on writeblock %d", eb);
                if (!args.subpage_size != mtd->min_io_size)
                    LOGI("may be sub-page size is "
                            "incorrect?");
                goto out_free;
            }
            err = mtd_torture(this->mtd_desc, mtd, args.mtd_fd, eb);
            if (err) {
                if (mark_bad(mtd, si, eb))
                    goto out_free;
            }
            continue;
        }
    }

    if (!novtbl) {
        if (eb1 == -1 || eb2 == -1) {
            LOGE("no eraseblocks for volume table");
            goto out_free;
        }

        LOGI("write volume table to eraseblocks %d and %d", eb1, eb2);
        vtbl = ubigen_create_empty_vtbl(ui);
        if (!vtbl)
            goto out_free;

        err = ubigen_write_layout_vol(ui, eb1, eb2, ec1, ec2, vtbl,
                &this->mtd_desc, mtd, si);
        free(vtbl);
        if (err) {
            LOGI("cannot write layout volume");
            goto out_free;
        }
    }

    free(hdr);
    return 0;

    out_free: free(hdr);
    return -1;
}

int ubi_partition_update(struct flash_manager* this, char *mtd_part,
        char *imgname, char *volname) {

    struct ubi_params *ubi = NULL;
    struct ubi_vtbl_record *vtbl;
    struct ubigen_vol_info *vi;
    int err = -1, sects, i, autoresize_was_already = 0;
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    off_t seek;
#endif

    if (ubi_params_set(this, &ubi, mtd_part, imgname, volname) < 0) {
        LOGE("ubinize parameters setting failed\n");
        return -1;
    }

    vtbl = ubigen_create_empty_vtbl(ubi->ui);
    if (!vtbl) {
        LOGE("volume table records created faield\n");
        return -1;
    }

#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    args.f_out = "ubi.img";
    args.out_fd = open(args.f_out, O_CREAT | O_TRUNC | O_WRONLY,
            S_IWUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (args.out_fd == -1) {
        LOGE("cannot open file \"%s\"", args.f_out);
        return -1;
    }
#endif

#ifndef CONFIG_UBI_INI_PARSER_LOAD_IN_MEM
    args.f_in = "./ubinize.cfg";
    args.dict = iniparser_load(args.f_in);
#else
    args.dict = iniparser_load(ubi->vol_info);
#endif
    if (!args.dict) {
        LOGE("cannot parse config info.");
        goto out_vtbl;
    }

    LOGI("parsed the ini-config.");

    /* Each section describes one volume */
    sects = iniparser_getnsec(args.dict);
    if (sects == -1) {
        LOGE("ini-file parsing error (iniparser_getnsec)");
        goto out_dict;
    }

    LOGI("count of sections: %d", sects);
    if (sects == 0) {
        LOGE("no sections found the ini-config");
        goto out_dict;
    }

    if (sects > ubi->ui->max_volumes) {
        LOGE("too many sections (%d) in the ini-config", sects);
        LOGE("each section corresponds to an UBI volume, maximum "
                "count of volumes is %d", ubi->ui->max_volumes);
        goto out_dict;
    }

    vi = calloc(sizeof(struct ubigen_vol_info), sects);
    if (!vi) {
        LOGE("cannot allocate memory");
        goto out_dict;
    }

    /*
     * Skip 2 PEBs at the beginning of the file for the volume table which
     * will be written later.
     */
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    seek = ubi->ui->peb_size * 2;
    if (lseek(args.out_fd, seek, SEEK_SET) != seek) {
        LOGE("cannot seek file \"%s\"", args.f_out);
        goto out_free;
    }
#else
    ubi_bypass_layout_vol(this->mtd_desc, ubi->si, 2);
#endif

    for (i = 0; i < sects; i++) {
        const char *sname = iniparser_getsecname(args.dict, i);
        const char *img = NULL;
        struct stat st;
        int fd, j;

        if (!sname) {
            LOGE("ini-file parsing error (iniparser_getsecname)");
            goto out_free;
        }

        LOGI("parsing section \"%s\"", sname);

        err = read_section(ubi->ui, sname, &vi[i], &img, &st);
        if (err == -1)
            goto out_free;
        /*
         * Make sure that volume ID and name is unique and that only
         * one volume has auto-resize flag
         */
        for (j = 0; j < i; j++) {
            if (vi[i].id == vi[j].id) {
                LOGE("volume IDs must be unique, but ID %d "
                        "in section \"%s\" is not", vi[i].id, sname);
                goto out_free;
            }

            if (!strcmp(vi[i].name, vi[j].name)) {
                LOGE("volume name must be unique, but name "
                        "\"%s\" in section \"%s\" is not", vi[i].name, sname);
                goto out_free;
            }
        }

        if (vi[i].flags & UBI_VTBL_AUTORESIZE_FLG) {
            if (autoresize_was_already) {
                LOGE("only one volume is allowed "
                        "to have auto-resize flag");
                return -1;
            }
            autoresize_was_already = 1;
        }

        err = ubigen_add_volume(ubi->ui, &vi[i], vtbl);
        if (err) {
            LOGE("cannot add volume for section \"%s\"", sname);
            goto out_free;
        }

        if (img) {
            fd = open(img, O_RDONLY);
            if (fd == -1) {
                LOGE("cannot open \"%s\"", img);
                goto out_free;
            }
            LOGI("image file: %s", img);
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
            err = ubigen_write_volume(ubi->ui, &vi[i], args.ec, st.st_size, fd,
                    args.out_fd);
#else
            err = ubigen_write_volume(ubi->ui, &vi[i], args.ec, st.st_size, fd,
                    &this->mtd_desc, ubi->mtd_dev, ubi->si);
#endif
            close(fd);
            if (err) {
                LOGE("cannot write volume for section \"%s\"", sname);
                goto out_free;
            }
        }
    }
    args.valid_eb = args.start_eb - 1;
    ubi_start_eb_set(0);
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    err = ubigen_write_layout_vol(ubi->ui, 0, 1, args.ec, args.ec, vtbl,
            args.out_fd);
#else
    err = ubigen_write_layout_vol(ubi->ui, 0, 1, args.ec, args.ec, vtbl,
            &this->mtd_desc, ubi->mtd_dev, ubi->si);
#endif
    if (err) {
        LOGE("cannot write layout volume");
        goto out_free;
    }

    ubi_start_eb_set(args.valid_eb + 1);
    err = format(this, ubi, args.start_eb, 1);
    if (err)
        goto out_free;

    LOGI("done");

    ubi_params_free(ubi);
    free(vi);
    iniparser_freedict(args.dict);
    free(vtbl);
#ifndef CONFIG_UBI_VOLUME_WRITE_MTD
    close(args.out_fd);
#endif
    return 0;

    out_free: free(vi);
    out_dict: iniparser_freedict(args.dict);
    out_vtbl: free(vtbl);
    ubi_params_free(ubi);
    return err;
}

