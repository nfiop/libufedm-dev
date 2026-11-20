/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __BITMAP_H
#define __BITMAP_H

#include <pthread.h>
#include <stdint.h>

#include "emulated-proxy/bits_array.h"

typedef struct {
	bits_array_t *bits;
	bit_index_t size;
	pthread_mutex_t mutex;
} bitmap_t;

int bitmap_init(bitmap_t *bitmap, bit_index_t size);

bit_index_t bitmap_alloc(bitmap_t *bitmap);

int bitmap_free(bitmap_t *bitmap, bit_index_t index);

void bitmap_deinit(bitmap_t *bitmap);

#endif
