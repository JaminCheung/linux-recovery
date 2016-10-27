/*
 *  Copyright (C) 2016, Zhang YanMing <jamincheung@126.com>
 *
 *  Linux recovery updater
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <utils/log.h>
#include <utils/assert.h>
#include <utils/file_ops.h>
#include <utils/common.h>
#include <configure/update_file.h>
#include <lib/mxml/mxml.h>

#define LOG_TAG "update_file"

const char* supported_device_type[] = {
        "nor",
        "nand",
        "mmc",
        NULL
};

static const char *device_type_list[ARRAY_SIZE(supported_device_type) - 1];

static void free_update_info_list(struct update_file* this,
        struct update_info* update_info) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &update_info->list) {
        struct image_info* info = list_entry(pos, struct image_info, head);

        list_del(&info->head);
        free(info);
    }
}

static void free_device_info_list(struct update_file* this,
        struct device_info* device_info) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &device_info->list) {
        struct part_info* info = list_entry(pos, struct part_info, head);

        list_del(&info->head);
        free(info);
    }
}

static void free_global_list(struct update_file* this) {
    struct list_head* pos;
    struct list_head* next_pos;

    list_for_each_safe(pos, next_pos, &this->device_list) {
        struct device_info* info = list_entry(pos, struct device_info, head);

        list_del(&info->head);
        free_device_info_list(this, info);
    }

    list_for_each_safe(pos, next_pos, &this->update_list) {
        struct update_info* info = list_entry(pos, struct update_info, head);

        list_del(&info->head);
        free_update_info_list(this, info);
    }

    memset(device_type_list, 0, sizeof(device_type_list));
}

static mxml_type_t mxml_type_callback(mxml_node_t* node) {
    const char *type;

    if ((type = mxmlElementGetAttr(node, "type")) == NULL)
      type = node->value.element.name;

    if (!strcmp(type, "integer"))
      return (MXML_INTEGER);
    else if (!strcmp(type, "opaque") || !strcmp(type, "pre"))
      return (MXML_OPAQUE);
    else if (!strcmp(type, "real"))
      return (MXML_REAL);
    else
      return (MXML_TEXT);
}

static int parse_global_xml(struct update_file* this, const char* path) {
    assert_die_if(path == NULL, "path is NULL\n");

    memset(device_type_list, 0, sizeof(device_type_list));

    if (file_exist(path) < 0) {
        LOGE("Failed to foud %s: %s\n", path, strerror(errno));
        return -1;
    }

    FILE* fp = NULL;
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;

    fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s\n", path, strerror(errno));
        goto error;
    }

    tree = mxmlLoadFile(NULL, fp, mxml_type_callback);
    fclose(fp);

    if (tree == NULL) {
        LOGE("Failed to load %s\n", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "global", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"global\" element in %s\n", path);
        goto error;
    }

    int i = 0;
    for (node = mxmlFindElement(node, tree, "device", NULL, NULL, MXML_DESCEND);
            node != NULL;
            node = mxmlFindElement(node, tree, "device", NULL, NULL,MXML_DESCEND)) {
        const char* devtype = mxmlGetOpaque(node);
        if (devtype == NULL) {
            LOGE("Failed to find device type\n");
            goto error;
        }

        int j;
        for (j = 0; supported_device_type[j]; j++) {
            if (!strcmp(devtype, supported_device_type[j]))
                break;
        }

        if (supported_device_type[j] == NULL) {
            LOGE("Unsupport device type: %s\n", devtype);
            goto error;
        }

        device_type_list[i++] = supported_device_type[j];

        struct device_info* device_info = calloc(1, sizeof(struct device_info));
        strcpy(device_info->type, devtype);
        INIT_LIST_HEAD(&device_info->list);

        list_add_tail(&device_info->head, &this->device_list);

        struct update_info* update_info = calloc(1, sizeof(struct update_info));
        strcpy(update_info->devtype, devtype);
        INIT_LIST_HEAD(&update_info->list);

        list_add_tail(&update_info->head, &this->update_list);
    }

    return 0;

error:
    if (tree)
        mxmlDelete(tree);

    return -1;
}

static int parse_update_xml(struct update_file* this, const char* path,
        struct update_info* update_info) {
    assert_die_if(path == NULL, "path is NULL\n");
    assert_die_if(update_info == NULL, "update_info is NULL\n");

    if (file_exist(path) < 0) {
        LOGE("Failed to foud %s: %s\n", path, strerror(errno));
        return -1;
    }

    FILE* fp = NULL;
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;
    mxml_node_t *sub_node = NULL;

    fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s\n", path, strerror(errno));
        goto error;
    }

    tree = mxmlLoadFile(NULL, fp, mxml_type_callback);
    fclose(fp);

    if (tree == NULL) {
        LOGE("Failed to load %s\n", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "update", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"update\" element in %s\n", path);
        goto error;
    }

    const char* devctl_str = mxmlElementGetAttr(node, "devcontrol");
    if (devctl_str == NULL) {
        LOGE("Failed to find \"devcontrol\" attribute\n");
        goto error;
    }
    update_info->devctl = strtoll(devctl_str, NULL, 0);

    const char* devtype = mxmlElementGetAttr(node, "devtype");
    if (devtype == NULL) {
        LOGE("Failed to find \"devtype\" attribute\n");
        goto error;
    }

    if(strcmp(devtype, update_info->devtype)) {
        LOGE("Device type mismatch\n");
        goto error;
    }

    node = mxmlFindElement(tree, tree, "imagelist", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"imagelist\" element in %s\n", path);
        goto error;
    }

    const char* count_str =  mxmlElementGetAttr(node, "count");
    if (count_str == NULL)
        LOGW("Failed to find \"count\" attribute in %s\n", path);
    else
        update_info->image_count = strtoul(count_str, NULL, 0);

    int count = 0;
    for (node = mxmlFindElement(node, tree, "image", NULL, NULL, MXML_DESCEND);
            node != NULL;
            node = mxmlFindElement(node, tree, "image", NULL, NULL,MXML_DESCEND)) {

        struct image_info* image = calloc(1, sizeof(struct image_info));

        /*
         * get name node
         */
        sub_node =  mxmlFindElement(node, node, "name", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"name\" element in %s\n", path);
            free(image);
            break;
        }
        const char* name = mxmlGetOpaque(sub_node);
        if (name == NULL) {
            name = mxmlGetText(sub_node, 0);
            if (name == NULL) {
                LOGE("Failed to find \"name\" value\n");
                free(image);
                break;
            }
        }
        memcpy(image->name, name, strlen(name));

        /*
         * get fs type node
         */
        sub_node =  mxmlFindElement(node, node, "type", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"type\" element in %s\n", path);
            free(image);
            break;
        }
        const char* fs_type = mxmlGetOpaque(sub_node);
        if (fs_type == NULL) {
            fs_type = mxmlGetText(sub_node, 0);
            if (fs_type == NULL) {
                LOGE("Failed to find \"name\" value\n");
                free(image);
                break;
            }
        }
        memcpy(image->fs_type, fs_type, strlen(fs_type));

        /*
         * get offset node
         */
        sub_node =  mxmlFindElement(node, node, "offset", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"offset\" element in %s\n", path);
            free(image);
            break;
        }
        const char* offset_str = mxmlGetText(sub_node, 0);
        image->offset = strtoull(offset_str, NULL, 0);

        /*
         * get size node
         */
        sub_node =  mxmlFindElement(node, node, "size", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"size\" element in %s\n", path);
            free(image);
            break;
        }
        const char* size_str = mxmlGetText(sub_node, 0);
        image->size = strtoull(size_str, NULL, 0);

        /*
         * get update_mode node
         */
        sub_node =  mxmlFindElement(node, node, "updatemode", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"updatemode\" element in %s\n", path);
            free(image);
            break;
        }

        sub_node = mxmlFindElement(sub_node, sub_node, "type", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"type\" element in %s\n", path);
            free(image);
            break;
        }
        if (mxmlElementGetAttr(sub_node, "type") == NULL) {
            const char* update_mode_str = mxmlGetText(sub_node, 0);
            image->update_mode = strtoll(update_mode_str, NULL, 0);
        } else {
            image->update_mode = mxmlGetInteger(sub_node);
        }

        if (image->update_mode == UPDATE_MODE_CHUNK) {
            sub_node = mxmlGetParent(sub_node);
            sub_node = mxmlFindElement(sub_node, sub_node, "chunksize", NULL, NULL,
                    MXML_DESCEND);
            if (sub_node == NULL) {
                LOGE("Failed to find \"chunksize\" element in %s\n", path);
                free(image);
                break;
            }

            if (mxmlElementGetAttr(sub_node, "type") == NULL) {
                const char* chunksize_str = mxmlGetText(sub_node, 0);
                image->chunksize = strtoll(chunksize_str, NULL, 0);
            } else {
                image->chunksize = mxmlGetInteger(sub_node);
            }

            sub_node = mxmlGetParent(sub_node);
            sub_node = mxmlFindElement(sub_node, sub_node, "chunkcount", NULL, NULL,
                    MXML_DESCEND);
            if (sub_node == NULL) {
                LOGE("Failed to find \"chunkcount\" element in %s\n", path);
                free(image);
                break;
            }

            if (mxmlElementGetAttr(sub_node, "type") == NULL) {
                const char* chunkcount_str = mxmlGetText(sub_node, 0);
                image->chunkcount = strtoll(chunkcount_str, NULL, 0);
            } else {
                image->chunkcount = mxmlGetInteger(sub_node);
            }
        } else {
            image->chunkcount = 1;
        }

        count++;
        list_add_tail(&image->head, &update_info->list);
    }

    if (update_info->image_count != count) {
        LOGE("Update image count error\n");
        free_update_info_list(this, update_info);
        goto error;
    }

    mxmlDelete(tree);

    return 0;

error:
    if (tree)
        mxmlDelete(tree);

    return -1;
}

static void dump_update_info(struct update_file* this,
        struct update_info* update_info) {
    LOGI("===================================\n");
    LOGI("Dump update info\n");
    LOGI("devtype:     %s\n", update_info->devtype);
    LOGI("devctl:      %d\n", update_info->devctl);
    LOGI("image count: %d\n", update_info->image_count);

    struct list_head* pos;
    struct image_info* image;
    list_for_each(pos, &update_info->list) {
        image = list_entry(pos, struct image_info, head);
        LOGI("-----------------------------------\n");
        LOGI("image name:        %s\n", image->name);
        LOGI("image fs type:     %s\n", image->fs_type);
        LOGI("image offset:      0x%x\n", (uint32_t) image->offset);
        LOGI("image size:        %llu\n", image->size);
        LOGI("image update mode: 0x%x\n", image->update_mode);
        LOGI("image chunksize:   %u\n", image->chunksize);
        LOGI("image chunkcount:  %u\n", image->chunkcount);
    }

    LOGI("===================================\n");
}

static int parse_device_xml(struct update_file* this, const char *path,
        struct device_info* device_info) {
    assert_die_if(path == NULL, "path is NULL");
    assert_die_if(device_info == NULL, "device_info is NULL");

    if (file_exist(path) < 0) {
        LOGE("Failed to foud %s: %s\n", path, strerror(errno));
        return -1;
    }

    FILE* fp = NULL;
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;
    mxml_node_t *sub_node = NULL;

    fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s\n", path, strerror(errno));
        goto error;
    }

    tree = mxmlLoadFile(NULL, fp, mxml_type_callback);
    fclose(fp);

    if (tree == NULL) {
        LOGE("Failed to load %s\n", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"device\" element in %s\n", path);
        goto error;
    }

    const char* type = mxmlElementGetAttr(node, "type");
    if (type == NULL) {
        LOGE("Failed to find device type\n");
        goto error;
    }

    if(strcmp(type, device_info->type)) {
        LOGE("Device type mismatch\n");
        goto error;
    }

    const char* capacity = mxmlElementGetAttr(node, "capacity");
    if (capacity == NULL) {
        LOGE("Failed to find device capacity\n");
        goto error;
    }
    device_info->capacity = strtoull(capacity, NULL, 0);

    node = mxmlFindElement(tree, tree, "partition", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"partition\" element in %s\n", path);
        goto error;
    }

    const char* count_str =  mxmlElementGetAttr(node, "count");
    if (count_str == NULL)
        LOGW("Failed to find \"count\" attribute in %s\n", path);
    else
        device_info->part_count = strtoul(count_str, NULL, 0);

    int count = 0;
    for (node = mxmlFindElement(node, tree, "item", NULL, NULL, MXML_DESCEND);
            node != NULL;
            node = mxmlFindElement(node, tree, "item", NULL, NULL,MXML_DESCEND)) {

        struct part_info* partition = calloc(1, sizeof(struct part_info));

        /*
         * get name node
         */
        sub_node =  mxmlFindElement(node, node, "name", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"name\" element in %s\n", path);
            free(partition);
            break;
        }
        const char* name = mxmlGetOpaque(sub_node);
        if (name == NULL) {
            name = mxmlGetText(sub_node, 0);
            if (name == NULL) {
                LOGE("Failed to find \"name\" value\n");
                free(partition);
                break;
            }
        }
        memcpy(partition->name, name, strlen(name));

        /*
         * get offset node
         */
        sub_node =  mxmlFindElement(node, node, "offset", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"offset\" element in %s\n", path);
            free(partition);
            break;
        }
        const char* offset_str = mxmlGetText(sub_node, 0);
        if (offset_str == NULL) {
            LOGE("Failed find \"offset\" value\n");
            free(partition);
            break;
        }
        partition->offset = strtoull(offset_str, NULL, 0);

        /*
         * get size node
         */
        sub_node =  mxmlFindElement(node, node, "size", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"size\" element in %s\n", path);
            free(partition);
            break;
        }
        const char* size_str = mxmlGetText(sub_node, 0);
        if (size_str == NULL) {
            LOGE("Failed find \"size\" value\n");
            free(partition);
            break;
        }
        partition->size = strtoull(size_str, NULL, 0);

        /*
         * get fs type node
         */
        sub_node =  mxmlFindElement(node, node, "blockname", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"blockname\" element in %s\n", path);
            free(partition);
            break;
        }
        const char* block_name = mxmlGetOpaque(sub_node);
        if (block_name == NULL) {
            block_name = mxmlGetText(sub_node, 0);
            if (block_name == NULL) {
                LOGE("Failed to find \"blockname\" value\n");
                free(partition);
                break;
            }
        }
        memcpy(partition->block_name, block_name, strlen(block_name));

        count++;

        list_add_tail(&partition->head, &device_info->list);
    }

    if (device_info->part_count != count) {
        LOGE("Device partition count error\n");
        free_device_info_list(this, device_info);
        goto error;
    }

    mxmlDelete(tree);

    return 0;

error:
    if (tree)
        mxmlDelete(tree);

    return -1;
}

static void dump_device_info(struct update_file* this,
        struct device_info* device_info) {
    LOGI("===================================\n");
    LOGI("Dump device xml\n");
    LOGI("device type:     %s\n", device_info->type);
    LOGI("device capacity: 0x%x\n", (uint32_t) device_info->capacity);
    LOGI("partition count: %d\n", device_info->part_count);

    struct list_head* pos;
    struct part_info* partition;
    list_for_each(pos, &device_info->list) {
        partition = list_entry(pos, struct part_info, head);
        LOGI("-----------------------------------\n");
        LOGI("part name:       %s\n", partition->name);
        LOGI("part offset:     0x%x\n", (uint32_t) partition->offset);
        LOGI("part size:       0x%x\n", (uint32_t) partition->size);
        LOGI("part block name: %s\n", partition->block_name);
    }
    LOGI("===================================\n");
}

static struct device_info* get_device_info_by_devtype(struct update_file* this,
        const char* devtype) {
    struct list_head* pos;

    list_for_each(pos, &this->device_list) {
        struct device_info* info = list_entry(pos, struct device_info, head);
        if (!strcmp(devtype, info->type))
            return info;
    }

    return NULL;
}

static struct update_info* get_update_info_by_devtype(struct update_file* this,
        const char* devtype) {
    struct list_head* pos;

    list_for_each(pos, &this->update_list) {
        struct update_info* info = list_entry(pos, struct update_info, head);
        if (!strcmp(devtype, info->devtype))
            return info;
    }

    return NULL;
}

static const char** get_device_type_list(struct update_file* this) {
    return device_type_list;
}

static void dump_device_type_list(struct update_file* this) {
    LOGI("===================================\n");
    LOGI("Dump device type list\n");
    for (int i = 0; device_type_list[i]; i++)
        LOGI("type: %s\n", device_type_list[i]);
    LOGI("===================================\n");
}

void construct_update_file(struct update_file* this) {
    INIT_LIST_HEAD(&this->device_list);
    INIT_LIST_HEAD(&this->update_list);

    this->parse_global_xml = parse_global_xml;
    this->parse_device_xml = parse_device_xml;
    this->parse_update_xml = parse_update_xml;

    this->dump_device_info = dump_device_info;
    this->dump_update_info = dump_update_info;
    this->get_device_info_by_devtype = get_device_info_by_devtype;
    this->get_update_info_by_devtype = get_update_info_by_devtype;
    this->get_device_type_list = get_device_type_list;
    this->dump_device_type_list = dump_device_type_list;
}

void destruct_update_file(struct update_file* this) {
    free_global_list(this);

    this->parse_global_xml = NULL;
    this->parse_device_xml = NULL;
    this->parse_update_xml = NULL;
    this->dump_device_info = NULL;
    this->dump_update_info = NULL;
    this->get_device_info_by_devtype = NULL;
    this->get_update_info_by_devtype = NULL;
    this->get_device_type_list = NULL;
    this->dump_device_info = NULL;
}
