/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __LINUX_MTD__PROXY_DEV_H
#define __LINUX_MTD__PROXY_DEV_H

#include "proxy_dev_types.h"

/**
 * @brief Open a proxy device file descriptor based on a MTD index string
 *
 * This function is mainly used by utests - it's lean, and doesn't do much
 * and also quite noisy about what it does which is perfect for verbose apps.
 *
 * @param index a string containing an MTD index (e.g. "0" for /dev/mtd0)
 * @return non-negative file descriptor number if successful, other negative
 *         number for error
 */
int linux_proxy_device_open_by_argv_index(const char *index);

/**
 * @brief Erase sections of the underlying MTD.
 *
 * IMPORTANT: Offset and length parameters must be aligned to the erase block
 *            size of the backing MTD.
 *
 * @param state a valid and initialized proxy device state.
 * @param offset the offset that the device should be erased from.
 * @param len number of bytes to erase.
 * @return 0 on success, or a negative value on error.
 */
int proxy_device_erase(
    struct proxy_device_state *state, off_t offset, size_t len);

/**
 * @brief Erase sections of the underlying MTD.
 *
 * This is a "safer" version of proxy_device_erase, because it offloads
 * the checks of a correct offset and length.
 *
 * IMPORTANT: Eraseblock is 0-indexed.
 *
 * @param state a valid and initialized proxy device state.
 * @param eraseblock_idx the offset of the first eraseblock to be erased.
 * @param blocks_count number of blocks to erase.
 * @return 0 on success, or a negative value on error.
 */
int proxy_device_erase_blocks(struct proxy_device_state *state,
    uint64_t eraseblock_idx, size_t blocks_count);

/**
 * @brief Open a proxy device file descriptor based on a full path
 *
 * This function opens a proxy proxy device and fills a given
 * `struct proxy_device_state` - it issues several ioctls to fetch crucial
 * data on the device and its backing MTD device, create a memory mapping
 * to shared memory buffer [with mmap(2) call].
 *
 * Upon success, the given `struct proxy_device_state *state` should be filled
 * with all the needed details about current state of the proxy device.
 *
 * @param index a string containing an MTD index (e.g. "0" for /dev/mtd0)
 * @param state to-be-filled pointer of a struct containting the new state
 * @return 0 if successful, other negative number for error
 */
int proxy_device_open(const char *path, struct proxy_device_state *state);

/**
 * @brief Unregister an eventfd on a I/O read/write slot
 *
 * This function unregisters an eventfd for an I/O write/read slot.
 * It also closes the eventfd file descriptor.
 * NOTE: This function has many assert statements to ensure a callee
 * doesn't use this before actually calling `register_proxy_eventfd`
 * successfully.
 *
 * @param state A pointer to open proxy device state struct
 * @param type The type of the I/O request to listen to with eventfd
 * @param slot_idx The I/O write/read slot index
 * @return void
 */
void unregister_proxy_eventfd(
    struct proxy_device_queue_state *state, size_t slot_idx);

/**
 * @brief Register an eventfd on a I/O read/write slot
 *
 * This function tries to allocate an eventfd for a read/write slot
 * and then attempts to register it on the request slot index.
 *
 * @param state A pointer to open proxy device state struct
 * @param slot_idx The I/O write/read slot index
 * @return 0 if successful, other negative number for error
 */
int register_proxy_eventfd(
    struct proxy_device_queue_state *state, size_t slot_idx, int efd);

int linux_proxy_device_create_queue_state(struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    void *(*worker_thread_func)(void *), struct proxy_queue_func_ops *func_ops,
    int (*thread_priv_create)(void **), void (*thread_priv_teardown)(void *));

void linux_proxy_device_destroy_queue_state(
    struct proxy_device_queue_state *state);

#endif
