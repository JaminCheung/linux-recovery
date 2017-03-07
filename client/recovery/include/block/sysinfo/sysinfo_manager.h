#ifndef SYSINFO_MANAGER_H
#define SYSINFO_MANAGER_H

#include <block/sysinfo/flag.h>

enum sysinfo_id {
    SYSINFO_FLASHINFO_PARTINFO,   //0x3c00: flash parameters and partition infomation is stored in
    SYSINFO_FLAG,                             //0x6000: ota update flag is stored in
};

struct sysinfo_layout {
    int64_t offset;
    int64_t length;
    char *value;
    int  reserve;
};

enum sysinfo_operation_mode {
    SYSINFO_OPERATION_RAM,
    SYSINFO_OPERATION_DEV,
};

enum sysinfo_reserve {
    SYSINFO_RESERVED,           //SYSINFO_RESERVED: whenever erase step is run across the layout area, it will be saved in memory
    SYSINFO_NO_RESERVED,    //SYSINFO_RESERVED: whenever erase step is run across the layout area, it will not be saved in memory
};

#define SYSINFO_FLASHINFO_PARTINFO_OFFSET  0x3c00
#define SYSINFO_FLASHINFO_PARTINFO_SIZE       0x400

#define SYSINFO_FLAG_OFFSET     0x6000
#define SYSINFO_FLAG_SIZE          0x400

struct sysinfo_manager {
    int64_t (*get_offset)(struct sysinfo_manager *this, int id);
    int64_t (*get_length)(struct sysinfo_manager *this, int id);
    int (*get_value)(struct sysinfo_manager *this, int id, char **buf, char flag);
    int (*set_value)(struct sysinfo_manager *this, int id, char *buf, char flag);
    int (*get_reserve)(struct sysinfo_manager *this, int id);
    int (*set_reserve)(struct sysinfo_manager *this, int id, int reserve);
    int (*traversal_save)(struct sysinfo_manager *this, int64_t offset, int64_t length);
    int (*traversal_merge)(struct sysinfo_manager *this, char *buf, int64_t offset, int64_t length);
    int (*init)(struct sysinfo_manager *this);
    int (*exit)(struct sysinfo_manager *this);
    void *binder;
};

void sysinfo_manager_bind(struct sysinfo_manager *this, void *target);

extern struct sysinfo_manager sysinfo;

#define GET_SYSINFO_BINDER(t)   (t->binder)
#define GET_SYSINFO_ID(t)  (SYSINFO_##t)
#define GET_SYSINFO_MANAGER()  ((struct sysinfo_manager*)&sysinfo)

#endif