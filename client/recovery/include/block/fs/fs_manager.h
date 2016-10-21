#ifndef FS_MANAGER_H
#define FS_MANAGER_H

struct fs_operation_params {
    int operation;
    int operation_method;
    char* fstype;
    char *buf;
    unsigned long blksize;
    long long offset;
    long long length;
    void *priv;
};

struct filesystem {
    char *name;
    struct list_head  list_cell;
    int (*init)(struct filesystem *fs);
    int (*erase)(struct filesystem *fs);
    int (*read)(struct filesystem *fs);
    int (*write)(struct filesystem *fs);
    long long (*get_operate_start_address)(struct filesystem *fs);
    unsigned long (*get_leb_size)(struct filesystem *fs);
    long long (*get_max_mapped_size_in_partition)(struct filesystem *fs);
    struct fs_operation_params *params;
    void *priv;
};

int fs_register(struct list_head *head, struct filesystem* this);
int fs_unregister(struct list_head *head, struct filesystem* this);
struct filesystem* fs_get_registered_by_name(struct list_head *head,
        char *filetype);
struct filesystem* fs_get_suppoted_by_name(char *filetype);
void fs_set_parameter(struct filesystem* fs,
                      struct fs_operation_params *p);
#endif