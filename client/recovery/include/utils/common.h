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

#ifndef COMMON_H
#define COMMON_H

#include <types.h>

#define _new(T, P)                          \
({                                          \
    T * obj = (T *)calloc(1, sizeof(T));    \
    obj->construct = construct_##P;         \
    obj->destruct = destruct_##P;           \
    obj->construct(obj);                    \
    obj;                                    \
})

#define _delete(P)                          \
({                                          \
    P->destruct(P);                         \
    free((void *)(P));                      \
})

extern const char* public_key_path;

void msleep(uint64_t msec);

int get_multiplier(const char *str);
long long get_bytes(const char *str);
void print_bytes(long long bytes, int bracket);

#endif /* COMMON_H */
