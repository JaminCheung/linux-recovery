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

#ifndef LIB_CRC32_H
#define LIB_CRC32_H

#include <stdint.h>

/* Return a 32-bit CRC of the contents of the buffer */
extern uint32_t crc32(uint32_t val, const void *ss, int len);

#endif /* LIB_CRC32_H */
