/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __EMULATED_PROXY_DEV__H
#define __EMULATED_PROXY_DEV__H

#include "proxy_dev_types.h"

/**
 * @brief Open an emulated proxy device based on a given parameters
 *
 * This function DOES NOT open an actual ufedm proxy device.
 * It fills a `struct proxy_device_state` based on given parameters
 * and create a memory mapping to shared memory buffer [with malloc]
 *
 * This function is especially useful for demo environments and non-Linux
 * platforms that want to create a device to incorporate an existing NAND
 * proxy functionality into their application.
 *
 * Upon success, the given `struct proxy_device_state *state` should be filled
 * with all the needed details about current state of the proxy device.
 *
 * @param flash_total_size a total size of a flash to be eumlated
 * @param flash_erase_sector_size an erase sector size
 * @param page_data_size page DATA section size
 * @param page_oob_size page OOB section size
 * @param state to-be-filled pointer of a struct containting the new state
 * @return 0 if successful, other negative number for error
 */
int proxy_device_open_emulated(size_t flash_total_size,
    size_t flash_erase_sector_size, size_t page_data_size, size_t page_oob_size,
    struct proxy_device_state *state);

int emulated_proxy_device_create_queue_state(
    struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    void *(*worker_thread_func)(void *), struct proxy_queue_func_ops *func_ops,
    int (*thread_priv_create)(void **), void (*thread_priv_teardown)(void *));

void emulated_proxy_device_destroy_queue_state(
    struct proxy_device_queue_state *state);

#endif
