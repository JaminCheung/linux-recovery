#ifndef MTD_H
#define MTD_H

#define MTD_LARGEST_PARTITION_NUM  10
#define MTD_DEV_ROOT    "/dev/"
#define MTD_PART_HEAD "/dev/mtd"

enum {
    MTD_BLK_PREFIX = 0xFFFFFF00,
    MTD_BLK_BAD = 0xFFFFFFFC,
    MTD_BLK_SCAN = 0xFFFFFFFF
};
/**
 * struct mtd_scan_info - mtd scanning information.
 * @ec: eraseblock status for all eraseblocks
 * @bad_cnt: count of bad eraseblocks
 */
struct mtd_scan_info {
    unsigned int *es;
    int bad_cnt;
};

struct mtd_part_char {
    char *path;
    int fd;
};

extern int mtd_is_block_scaned(struct block_manager* this, long long offset);
extern void mtd_scan_dump (struct block_manager* this, long long offset, 
                        struct mtd_scan_info* mi);

extern long long mtd_nand_block_scan(struct block_manager* this, 
                        long long offset, long long length);
extern int mtd_get_blocksize(struct block_manager* this, long long offset);
#endif