#ifndef UBIFS_H
#define UBIFS_H

#define UBI_OPEN_DEBUG

#define UBI_DEFAULT_CTRL_DEV "/dev/ubi_ctrl"
#define UBI_VID_HDR_OFFSET_INIT      0
#define UBI_VERSION_DEFAULT           UBI_VERSION
#define UBI_OVERRIDE_EC                  0
#define CONFIG_MTD_UBI_BEB_LIMIT     20
#define UBI_VOLUME_SECTION_CNT       1
#define UBI_VOLUME_DEFAULT_ID        0
#define UBI_VOLUME_DEFAULT_TYPE      "dynamic"
#define UBI_VOLUME_DEFAULT_ALIGNMENT  UBI_LAYOUT_VOLUME_ALIGN
#define UBI_VOLUME_ENABLE_AUTORESIZE   0
#define UBI_LAYOUT_VOLUME_DEFAULT_COUNT      UBI_LAYOUT_VOLUME_EBS
#define MAX_CONSECUTIVE_BAD_BLOCKS 4
#define WL_RESERVED_PEBS                    1
#define EBA_RESERVED_PEBS                   1

struct ubi_mtd_device_info {
    unsigned int peb_size;
    unsigned int page_size;
    unsigned int subpage_size;
    int64_t partition_size;
    int64_t device_size;
};

struct ubi_params {
    char *vol_name;
    unsigned int max_beb_per1024;
    unsigned int ubi_reserved_blks;
    unsigned int lebsize;
    int64_t vol_size;
    int override_ec;
    int64_t ec;
    int vid_hdr_offs;
    int ubi_ver;
    int vid_hdr_lnum;
    int64_t start_eb;
    int64_t layout_volume_start_eb;
    int64_t format_eb;
    int64_t has_writen_size;
    int64_t total_write_size;
    char *outbuf;
    struct ubi_mtd_device_info devinfo;
    struct ubigen_info *ui;
    struct ubi_scan_info *si;
    struct ubi_vtbl_record *vtbl;
    struct ubigen_vol_info *vi;
};
#endif