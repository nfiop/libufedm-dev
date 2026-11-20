/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "emulated-proxy/bitmap.h"

int bitmap_init(bitmap_t *bitmap, bit_index_t size)
{
	if (!bitmap || size <= 0)
		return -1;

	bitmap->bits = bit_array_create(size);
	if (!bitmap->bits)
		return -1;

	bitmap->size = size;

	if (pthread_mutex_init(&bitmap->mutex, NULL) != 0) {
		bit_array_free(bitmap->bits);
		bitmap->bits = NULL;
		bitmap->size = 0;
		return -1;
	}

	return 0;
}

bit_index_t bitmap_alloc(bitmap_t *bitmap)
{
	bit_index_t index;

	if (!bitmap || !bitmap->bits)
		return -1;

	pthread_mutex_lock(&bitmap->mutex);

	if (!bit_array_find_first_clear_bit(bitmap->bits, &index)) {
		pthread_mutex_unlock(&bitmap->mutex);
		return -1;
	}

	bit_array_set_bit(bitmap->bits, index);

	pthread_mutex_unlock(&bitmap->mutex);

	return index;
}

int bitmap_free(bitmap_t *bitmap, bit_index_t index)
{
	if (!bitmap || !bitmap->bits || index < 0 || index >= bitmap->size)
		return -1;

	pthread_mutex_lock(&bitmap->mutex);

	if (!bit_array_get_bit(bitmap->bits, index)) {
		pthread_mutex_unlock(&bitmap->mutex);
		return -1;
	}

	bit_array_clear_bit(bitmap->bits, index);

	pthread_mutex_unlock(&bitmap->mutex);

	return 0;
}

void bitmap_deinit(bitmap_t *bitmap)
{
	if (!bitmap || !bitmap->bits)
		return;

	pthread_mutex_destroy(&bitmap->mutex);
	bit_array_free(bitmap->bits);

	bitmap->bits = NULL;
	bitmap->size = 0;
}
