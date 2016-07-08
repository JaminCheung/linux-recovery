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

#include <autoconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <eeprom/eeprom_manager.h>
#include <lib/libsmbus.h>
#include <utils/compare_string.h>

#define LOG_TAG "recovery--->eeprom_manager"

#define EEPROM_TYPE_UNKNOWN     0
#define EEPROM_TYPE_8BIT_ADDR   1
#define EEPROM_TYPE_16BIT_ADDR  2
#define EEPROM_ACCESS_DELAY_US  5000

#define CHECK_I2C_FUNC(funcs, lable)                \
    do {                                            \
            assert_die_if(0 == (funcs & lable),     \
            #lable " i2c function is required.");   \
    } while (0);                                    \

int i2c_write_1b(struct eeprom_manager* this, unsigned char* buf) {
    int retval = 0;

    retval = i2c_smbus_write_byte(this->fd, buf[0]);
    if (retval < 0)
        LOGE("Failed to write 1 byte: %s", strerror(errno));

    usleep(50);

    return retval;
}

int i2c_write_2b(struct eeprom_manager* this, unsigned char* buf) {
    int retval = 0;

    retval = i2c_smbus_write_byte_data(this->fd, buf[0], buf[1]);
    if (retval)
        LOGE("Failed to write 2 bytes: %s", strerror(errno));

    usleep(50);

    return retval;
}

int i2c_write_3b(struct eeprom_manager* this, unsigned char *buf) {
    int retval = 0;

    retval = i2c_smbus_write_word_data(this->fd, buf[0], buf[2] << 8 | buf[1]);
    if (retval)
        LOGE("Failed to write 3 bytes: %s", strerror(errno));

    usleep(50);

    return retval;
}

static int eeprom_write_byte(struct eeprom_manager* this, int addr, unsigned char data) {
#if CONFIG_EEPROM_TYPE == EEPROM_TYPE_8BIT_ADDR
    unsigned char buf[2];

    buf[0] = addr & 0xff;
    buf[1] = data;

    return i2c_write_2b(this, buf);

#elif CONFIG_EEPROM_TYPE == EEPROM_TYPE_16BIT_ADDR
    unsigned char buf[3];

    buf[0] = (addr >> 8) & 0xff;
    buf[1] = addr & 0xff;
    buf[2] = data;

    return i2c_write_3b(this, buf);

#else
#error "Unknown eeprom type."
#endif
}

static int eeprom_read_byte(struct eeprom_manager* this, int addr) {
    int retval = 0;

    ioctl(this->fd, BLKFLSBUF);

#if CONFIG_EEPROM_TYPE == EEPROM_TYPE_8BIT_ADDR
    unsigned char buf;

    buf = addr & 0xff;

    retval = i2c_write_1b(this, buf);
    if (retval < 0)
    return retval;

    retval = i2c_smbus_read_byte(this->fd);

    return retval;

#elif CONFIG_EEPROM_TYPE == EEPROM_TYPE_16BIT_ADDR
    unsigned char buf[2];

    buf[0] = (addr >> 8) & 0xff;
    buf[1] = addr & 0xff;

    retval = i2c_write_2b(this, buf);
    if (retval < 0)
        return retval;

    retval = i2c_smbus_read_byte(this->fd);

    return retval;

#else
#error "Unknown eeprom type."
#endif
}

static int eeprom_write(struct eeprom_manager* this, unsigned char* buf, int addr,
        int count) {
    int i, retval;

    for (i = 0; i < count; i++) {
        usleep(EEPROM_ACCESS_DELAY_US);
        retval = eeprom_write_byte(this, addr + i, buf[i]);
        assert_die_if(retval < 0, "Failed to write eeprom: %s",
                strerror(errno));
    }

    return 0;
}

static int eeprom_read(struct eeprom_manager* this, unsigned char* buf, int addr,
        int count) {
    int i;

    for (i = 0; i < count; i++) {
        usleep(EEPROM_ACCESS_DELAY_US);
        buf[i] = eeprom_read_byte(this, addr + i);
        assert_die_if(buf[i] < 0, "Failed to read eeprom: %s", strerror(errno));
    }

    return 0;
}

static int lookup_bus_num_by_chip_addr(int addr) {
    const char* const i2cdev_path = "/sys/class/i2c-dev";

    char path[NAME_MAX] = { 0 };
    char name[128] = { 0 };
    struct dirent *de, *subde;
    DIR* dir, *subdir;

    int bus, retval;

    dir = opendir(i2cdev_path);
    if (!dir)
        return -1;

    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.')
            continue;

        retval = sscanf(de->d_name, "i2c-%d", &bus);
        if (retval != 1)
            continue;

        snprintf(name, sizeof(name), "%d-%04x", bus, addr);

        snprintf(path, NAME_MAX, "%s/%s/device", i2cdev_path, de->d_name);

        subdir = opendir(path);
        if (!subdir)
            continue;

        while ((subde = readdir(subdir)) != NULL) {
            if (subde->d_name[0] == '.')
                continue;

            if (!strcmp(name, subde->d_name) && (subde->d_type == DT_DIR))
                return bus;
        }
    }

    return -1;
}

static void eeprom_dev_init(struct eeprom_manager* this) {
    int retval = 0;
    unsigned char chip_addr = CONFIG_EEPROM_ADDR;
    int bus_num = -1;

    retval = lookup_bus_num_by_chip_addr(chip_addr);
    assert_die_if(retval < 0,
            "Failed to init i2c device, check kernel configure or config.mk.");
    bus_num = retval;

    retval = i2c_smbus_open(bus_num);
    assert_die_if(retval < 0, "Failed to open i2c bus %d: %s", bus_num,
            strerror(errno));
    this->fd = retval;

    retval = i2c_smbus_get_funcs_matrix(this->fd, &this->funcs);
    assert_die_if(retval < 0, "Failed to get i2c functions: %s",
            strerror(errno));

    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_READ_BYTE);
    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_WRITE_BYTE);
    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_READ_BYTE_DATA);
    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_WRITE_BYTE_DATA);
    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_READ_WORD_DATA);
    CHECK_I2C_FUNC(this->funcs, I2C_FUNC_SMBUS_WRITE_WORD_DATA);

    retval = i2c_smbus_set_slave_addr(this->fd, chip_addr, true);
    assert_die_if(retval < 0, "Failed to set i2c slave addr: %s",
            strerror(errno));
}

static void eeprom_dev_deinit(struct eeprom_manager* this) {
    close(this->fd);
    this->fd = -1;
    this->funcs = 0;
}

void construct_eeprom_manager(struct eeprom_manager* this) {
    this->read = eeprom_read;
    this->write = eeprom_write;
    this->fd = -1;
    this->funcs = 0;

    eeprom_dev_init(this);
}

void destruct_eeprom_manager(struct eeprom_manager* this) {
    eeprom_dev_deinit(this);
    this->read = NULL;
    this->write = NULL;
}
