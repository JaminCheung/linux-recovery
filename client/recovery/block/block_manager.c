#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/log.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <types.h>
#include <lib/mtd/jffs2-user.h>
#include <lib/libcommon.h>
#include <lib/mtd/mtd-user.h>
#include <autoconf.h>
#include <lib/ubi/ubi.h>
#include <lib/crc/libcrc.h>
#include <utils/list.h>
#include <block/block_manager.h>

#define    LOG_TAG     "block_manager"

extern int mtd_manager_init(void);
extern int mtd_manager_destroy(void);
extern int mmc_manager_init(void);

static LIST_HEAD(block_manager_list);
// static char * strdup(const char *s)
// {
//     char * copy = (char*) malloc(strlen(s));
//     strcpy(copy, s);
//     return copy ;
// }

int register_block_manager(struct block_manager* this) {
    struct block_manager *m;
    struct list_head *cell;
    list_for_each(cell, &block_manager_list) {
        m = list_entry(cell, struct block_manager, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGE("Block manager \''%s\' is already registered\n", m->name);
            return false;
        }
    }
    list_add_tail(&block_manager_list, &this->list_cell);
    return true;
}

int unregister_block_manager(struct block_manager* this) {
    struct block_manager *m;
    struct list_head *cell;
    list_for_each(cell, &block_manager_list) {
        m = list_entry(cell, struct block_manager, list_cell);
        if (!strcmp(m->name,  this->name)) {
            LOGI("Block manager \''%s\' is removed successfully\n", m->name);
            list_del(cell);
            return true;
        }
    }
    return false;
}

struct block_manager* get_block_manager(struct block_manager* this, char *name) {
    struct block_manager *m;
    struct list_head *cell;
    list_for_each(cell, &block_manager_list) {
        m = list_entry(cell, struct block_manager, list_cell);
        if (!strcmp(m->name,  name))
            return m;
    }
    return NULL;
}

static void get_supported_block_managers(struct block_manager* this, char *buf) {
    struct block_manager *m;
    struct list_head *cell;
    char list[128];
    int i = 0;
    list_for_each(cell, &block_manager_list) {
        m = list_entry(cell, struct block_manager, list_cell);
        if (i == 0)
            strcpy(list, "[");
        strcat(list, ",");
        strcat(list, m->name);
    }
    strcat(list, "]");
    strcpy(buf, list);
    LOGI("Block manager supporting list \''%s\'\n",  buf);
    return;
}

static void get_supported_filetype(struct block_manager* this, char *buf) {
    BM_FILE_TYPE_INIT(supported_array);
    char list[128];
    int i;
    for (i = 0; i < sizeof(supported_array)/sizeof(supported_array[0]); i++){
        if (i == 0)
            strcpy(list, "[");
        strcat(list, ",");
        strcat(list,  supported_array[i]);
    }
    strcat(list, "]");
    strcpy(buf, list);
    LOGI("Block manager supporting filesystem list  \''%s\'\n",  buf);
    return;
}

// static int is_filetype_supported(struct block_manager* this, char *filetype) {
//     BM_FILE_TYPE_INIT(supported_array);
//     int i;
//     for (i = 0; i < sizeof(supported_array)/sizeof(supported_array[0]); i++){
//         if (!strcmp(supported_array[i], filetype))
//             return true;
//     }
//     return false;    
// }
static struct bm_operation_option* set_operation_option(struct block_manager* this,
            int method, char *filetype) {
    static struct bm_operation_option option;

    memset(&option, 0, sizeof(option));
    option.method = method;
    strcpy(option.filetype, filetype);
    return &option;
}

void construct_block_manager(struct block_manager* this, char *blockname,
                             bm_event_listener_t listener, void* param) {

    mtd_manager_init();
    // mmc_manager_init();
    struct block_manager* bm = get_block_manager(this, blockname);
    if (bm == NULL) {
        LOGI("Block manager \''%s\' is not exist\n", blockname);
        return;
    }
    bm->param = param;
    BM_GET_LISTENER(bm) = listener;
    memcpy(this, bm, sizeof(*this));
    this->get_supported = get_supported_block_managers;
    this->get_supported_filetype = get_supported_filetype;
    this->set_operation_option = set_operation_option;
    return;
}

void destruct_block_manager(struct block_manager* this) {
    // mmc_manager_destroy();
    mtd_manager_destroy();
    memset(this, 0, sizeof(* this));
    return;
}