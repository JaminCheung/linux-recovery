#ifndef MTD_H
#define MTD_H

#define MTD_LARGEST_PARTITION_NUM  10
#define MTD_DEV_ROOT    "/dev/"
#define MTD_PART_HEAD "/dev/mtd"

enum {
    MTD_BLK_PREFIX = 0x5A5A5A00,
    MTD_BLK_SCAN,
    MTD_BLK_BAD,
    MTD_BLK_ERASED,
    MTD_BLK_WRITEN_EIO,
    MTD_BLK_WRITEN,
};
/**
 * struct mtd_scan_info - mtd scanning information.
 * @ec: eraseblock status for all eraseblocks
 * @bad_cnt: count of bad eraseblocks
 */
struct mtd_nand_map {
    char *name;
    unsigned int *es;
    long long start;
    long long eb_cnt;
    int bad_cnt;
    int dirty;
};

#define MTD_NAND_MAP_SET(mm, i, s)   (mm->es[i] = s)

struct mtd_part_char {
    char *path;
    int fd;
};

int mtd_get_blocksize_by_offset(struct block_manager* this, long long offset);
int mtd_type_is_nand(struct mtd_dev_info *mtd);
int mtd_type_is_mlc_nand(struct mtd_dev_info *mtd);
int mtd_type_is_nor(struct mtd_dev_info *mtd);
int mtd_is_block_scaned(struct block_manager* this, long long offset);
void mtd_scan_dump(struct block_manager* this, long long offset, 
                        struct mtd_nand_map* mi);
long long mtd_block_scan(struct filesystem *fs);
long long mtd_basic_erase(struct filesystem *fs);
long long mtd_basic_write(struct filesystem *fs);
long long mtd_basic_read(struct filesystem *fs);

#define MTD_DEV_INFO_TO_FD(mtd)   container_of(mtd, struct bm_part_info, part.mtd_dev_info)->fd
#define MTD_DEV_INFO_TO_START(mtd)  container_of(mtd, struct bm_part_info, part.mtd_dev_info)->start
#define MTD_DEV_INFO_TO_ID(mtd)     container_of(mtd, struct bm_part_info, part.mtd_dev_info)->id
#define MTD_OFFSET_TO_EB_INDEX(mtd, off)   ((off)/mtd->eb_size)
// #define MTD_BOUNDARY_IS_ALIGNED(mtd, off)  (off%mtd->eb_size == 0)
#define MTD_BOUNDARY_IS_ALIGNED(mtd, off)  (((off)&(~mtd->eb_size + 1)) == (off))
#define MTD_BLOCK_ALIGN(mtd, off)   ((off)&(~mtd->eb_size + 1))

#endif