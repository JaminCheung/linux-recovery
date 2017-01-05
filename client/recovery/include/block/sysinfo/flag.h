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

int64_t sysinfo_get_flag_size(int id);
int sysinfo_read_flag(int id, void *flag);
int sysinfo_write_flag(int id, void *flag);
#endif