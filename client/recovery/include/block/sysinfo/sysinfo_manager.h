#ifndef SYSINFO_MANAGER_H
#define SYSINFO_MANAGER_H

#include <block/sysinfo/flag.h>

enum sysinfo_id {
    SYSINFO_FLASHINFO_PARTINFO,   //0x3c00: flash parameters and partition infomation is stored in
    // SYSINFO_UPDATE_FLAG,                //0x6000: ota update flag is stored in
};

struct sysinfo_layout {
    int64_t offset;
    int64_t length;
    char *value;
};

enum sysinfo_operation {
    SYSINFO_OPERATION_RAM,
    SYSINFO_OPERATION_DEV,
};

#define SYSINFO_FLASHINFO_PARTINFO_OFFSET  0x3c00
#define SYSINFO_FLASHINFO_PARTINFO_SIZE       0x400

#define SYSINFO_FLAG_OFFSET     0x6000
#define SYSINFO_FLAG_SIZE          0x400

struct sysinfo_manager {
    int64_t (*get_offset)(struct sysinfo_manager *this, int id);
    int64_t (*get_length)(struct sysinfo_manager *this, int id);
    int (*get_value)(struct sysinfo_manager *this, int id, char **buf, char flag);
    int (*traversal_save)(struct sysinfo_manager *this, int64_t offset, int64_t length);
    int (*traversal_merge)(struct sysinfo_manager *this, char *buf, int64_t offset, int64_t length);
    int (*init)(struct sysinfo_manager *this);
    int (*exit)(struct sysinfo_manager *this);
    int64_t (*get_flag_size)(int id);
    int (*read_flag)(int id, void *flag);
    int (*write_flag)(int id, void *flag);
    void *binder;
};

void sysinfo_manager_bind(struct sysinfo_manager *this, void *target);

extern struct sysinfo_manager sysinfo;

#define GET_SYSINFO_BINDER(t)   (t->binder)
#define GET_SYSINFO_ID(t)  (SYSINFO_##t)
#define GET_SYSINFO_MANAGER()  (&(sysinfo))

#endif