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
#include <configure/update_file.h>
#include <lib/mxml/mxml.h>

#define LOG_TAG "update_file"

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

static int parse_update_xml(struct update_file* this, const char* path) {
    assert_die_if(path == NULL, "path is NULL");

    if (file_exist(path) < 0)
        return -1;

    FILE* fp = NULL;
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;
    mxml_node_t *sub_node = NULL;

    INIT_LIST_HEAD(&this->update_info.list);

    fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s", path, strerror(errno));
        goto error;
    }

    tree = mxmlLoadFile(NULL, fp, mxml_type_callback);
    fclose(fp);

    if (tree == NULL) {
        LOGE("Failed to load %s", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "update", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"update\" element in %s", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "devctl", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"devctl\" element in %s", path);
        goto error;
    }
    this->update_info.devctl = mxmlGetInteger(node);

    node = mxmlFindElement(tree, tree, "imagelist", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"imagelist\" element in %s", path);
        goto error;
    }

    const char* count_str =  mxmlElementGetAttr(node, "count");
    if (count_str == NULL) {
        LOGE("Failed to find \"count\" attribute in %s", path);
        goto error;
    }
    this->update_info.image_count = strtoul(count_str, NULL, 0);

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
            LOGE("Failed to find \"name\" element in %s", path);
            free(image);
            break;
        }
        const char* name = mxmlGetOpaque(sub_node);
        memcpy(image->name, name, strlen(name));

        /*
         * get fs type node
         */
        sub_node =  mxmlFindElement(node, node, "type", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"type\" element in %s", path);
            free(image);
            break;
        }
        const char* fs_type = mxmlGetOpaque(sub_node);
        memcpy(image->fs_type, fs_type, strlen(fs_type));

        /*
         * get offset node
         */
        sub_node =  mxmlFindElement(node, node, "offset", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"offset\" element in %s", path);
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
            LOGE("Failed to find \"size\" element in %s", path);
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
            LOGE("Failed to find \"updatemode\" element in %s", path);
            free(image);
            break;
        }

        sub_node = mxmlFindElement(sub_node, sub_node, "type", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"type\" element in %s", path);
            free(image);
            break;
        }
        image->update_mode = mxmlGetInteger(sub_node);

        if (image->update_mode == UPDATE_MODE_CHUNK) {
            sub_node = mxmlGetParent(sub_node);
            sub_node = mxmlFindElement(sub_node, sub_node, "chunksize", NULL, NULL,
                    MXML_DESCEND);
            if (sub_node == NULL) {
                LOGE("Failed to find \"chunksize\" element in %s", path);
                free(image);
                break;
            }
            image->chunksize = mxmlGetInteger(sub_node);

            sub_node = mxmlGetParent(sub_node);
            sub_node = mxmlFindElement(sub_node, sub_node, "chunkcount", NULL, NULL,
                    MXML_DESCEND);
            if (sub_node == NULL) {
                LOGE("Failed to find \"chunkcount\" element in %s", path);
                free(image);
                break;
            }
            image->chunkcount = mxmlGetInteger(sub_node);
        }

        list_add_tail(&image->head, &this->update_info.list);
    }

    mxmlDelete(tree);

    return 0;

error:
    if (tree)
        mxmlDelete(tree);

    return -1;
}

static void dump_update_xml(struct update_file* this) {
    LOGI("===================================\n");
    LOGI("Dump update xml\n");
    LOGI("devctl:      %d\n", this->update_info.devctl);
    LOGI("image count: %d\n", this->update_info.image_count);

    struct list_head* pos;
    struct image_info* image;
    list_for_each(pos, &this->update_info.list) {
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

static int parse_device_xml(struct update_file* this, const char *path) {
    assert_die_if(path == NULL, "path is NULL");

    if (file_exist(path) < 0)
        return -1;

    FILE* fp = NULL;
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;
    mxml_node_t *sub_node = NULL;

    INIT_LIST_HEAD(&this->device_info.list);

    fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("Failed to open %s: %s", path, strerror(errno));
        goto error;
    }

    tree = mxmlLoadFile(NULL, fp, mxml_type_callback);
    fclose(fp);

    if (tree == NULL) {
        LOGE("Failed to load %s", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"device\" element in %s", path);
        goto error;
    }

    node = mxmlFindElement(tree, tree, "devinfo", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"devinfo\" element in %s", path);
        goto error;
    }

    sub_node = mxmlFindElement(node, node, "type", NULL, NULL, MXML_DESCEND);
    const char* type = mxmlGetOpaque(sub_node);
    memcpy(this->device_info.type, type, strlen(type));

    sub_node = mxmlFindElement(node, node, "capacity", NULL, NULL, MXML_DESCEND);
    const char* capacity = mxmlGetText(sub_node, 0);
    this->device_info.capacity = strtoull(capacity, NULL, 0);

    node = mxmlFindElement(tree, tree, "partition", NULL, NULL, MXML_DESCEND);
    if (node == NULL) {
        LOGE("Failed to find \"partition\" element in %s", path);
        goto error;
    }

    const char* count_str =  mxmlElementGetAttr(node, "count");
    if (count_str == NULL) {
        LOGE("Failed to find \"count\" attribute in %s", path);
        goto error;
    }
    this->device_info.part_count = strtoul(count_str, NULL, 0);

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
            LOGE("Failed to find \"name\" element in %s", path);
            free(partition);
            break;
        }
        const char* name = mxmlGetOpaque(sub_node);
        memcpy(partition->name, name, strlen(name));

        /*
         * get offset node
         */
        sub_node =  mxmlFindElement(node, node, "offset", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"offset\" element in %s", path);
            free(partition);
            break;
        }
        const char* offset_str = mxmlGetText(sub_node, 0);
        partition->offset = strtoull(offset_str, NULL, 0);

        /*
         * get size node
         */
        sub_node =  mxmlFindElement(node, node, "size", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"size\" element in %s", path);
            free(partition);
            break;
        }
        const char* size_str = mxmlGetText(sub_node, 0);
        partition->size = strtoull(size_str, NULL, 0);

        /*
         * get fs type node
         */
        sub_node =  mxmlFindElement(node, node, "blockname", NULL, NULL,
                MXML_DESCEND);
        if (sub_node == NULL) {
            LOGE("Failed to find \"blockname\" element in %s", path);
            free(partition);
            break;
        }
        const char* block_name = mxmlGetOpaque(sub_node);
        memcpy(partition->block_name, block_name, strlen(block_name));

        list_add_tail(&partition->head, &this->device_info.list);
    }

    mxmlDelete(tree);

    return 0;

error:
    if (tree)
        mxmlDelete(tree);

    return -1;
}

static void dump_device_xml(struct update_file* this) {
    LOGI("===================================\n");
    LOGI("Dump device xml\n");
    LOGI("device type:     %s\n", this->device_info.type);
    LOGI("device capacity: 0x%x\n", (uint32_t) this->device_info.capacity);

    struct list_head* pos;
    struct part_info* partition;
    list_for_each(pos, &this->device_info.list) {
        partition = list_entry(pos, struct part_info, head);
        LOGI("-----------------------------------\n");
        LOGI("part name:       %s\n", partition->name);
        LOGI("part offset:     0x%x\n", (uint32_t) partition->offset);
        LOGI("part size:       0x%x\n", (uint32_t) partition->size);
        LOGI("part block name: %s\n", partition->block_name);
    }
    LOGI("===================================\n");
}

void construct_update_file(struct update_file* this) {
    this->parse_device_xml = parse_device_xml;
    this->parse_update_xml = parse_update_xml;
    this->dump_device_xml = dump_device_xml;
    this->dump_update_xml = dump_update_xml;
}

void destruct_update_file(struct update_file* this) {
    this->parse_device_xml = NULL;
    this->parse_update_xml = NULL;
    this->dump_device_xml = NULL;
    this->dump_update_xml = NULL;
}
