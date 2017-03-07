#ifndef UPDATE_FLAG_H
#define UPDATE_FLAG_H

enum sysinfo_flag_id {
    SYSINFO_FLAG_ID_UPDATE_DONE,   //0x3c00: flash parameters and partition infomation is stored in
};

struct sysinfo_flag_layout {
    int64_t offset;
    int64_t length;
};

/* The following defination is relative to the start address of SYSINFO_FLAG_OFFSET*/
#define SYSINFO_FLAG_UPDATE_DONE_OFFSET        0
#define SYSINFO_FLAG_UPDATE_DONE_SIZE             4
#define SYSINFO_FLAG_VALUE_UPDATE_START         0x5A5A5A5A
#define SYSINFO_FLAG_VALUE_UPDATE_DONE          0xA5A5A5A5

struct sysinfo_flag {
    int64_t (*get_size)(int id);
    int (*read)(int id, void *flag);
    int (*write)(int id, void *flag);
};
extern struct sysinfo_flag sysinfo_flags;
#define GET_SYSINFO_FLAG()  ((struct sysinfo_flag*)&(sysinfo_flags))
#endif