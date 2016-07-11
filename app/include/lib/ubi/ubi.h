#ifndef __UBI_H__
#define __UBI_H__

#include <flash/flash_manager.h>

#define CONFIG_UBI_WEAR_LEVEL  100    // 0.01

#define CONFIG_UBI_INI_PARSER_LOAD_IN_MEM
#define CONFIG_UBI_VOLUME_WRITE_MTD

int ubi_partition_update(struct flash_manager* this, char *mtd_part, char *imgname,
        char *volname);
#endif
