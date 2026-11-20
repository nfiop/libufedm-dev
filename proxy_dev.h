/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __USER_PROXY_DEV_H_
#define __USER_PROXY_DEV_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

/* We include these header files, because the emulated device uses
 * these structs too.
 */
#include "linux-mtd/defs.h"
#include "linux-mtd/proxy_queue.h"

#if defined(__linux__)
#include "linux-mtd/dev.h"
#endif

#include "proxy_dev_types.h"

/**
 * @brief Write data to the proxy device.
 *
 * @param state a valid and initialized proxy device state.
 * @param buf the data buffer to write.
 * @param len number of bytes to write from the buffer.
 * @return number of bytes successfully written on success, or a negative
 *         value on error.
 */
ssize_t proxy_device_write(
    struct proxy_device_state *state, const uint8_t *buf, size_t len);

/**
 * @brief Read data from the proxy device.
 *
 * @param state a valid and initialized proxy device state.
 * @param buf the buffer where the read data will be stored.
 * @param len maximum number of bytes to read.
 * @return number of bytes successfully read on success, or a negative
 *         value on error.
 */
ssize_t proxy_device_read(
    struct proxy_device_state *state, uint8_t *buf, size_t len);

/**
 * @brief Reposition read/write proxy device I/O offset
 *
 * NOTE: There's no support for traditional lseek modes (known as whence)
 * in this function.
 *
 * @param state a valid and initialized proxy device state.
 * @param seek the new offset
 * @return 0 on success, or a negative value on error.
 */
int proxy_device_seek(struct proxy_device_state *state, off_t seek_offset);

/**
 * @brief Write data to the proxy device at a specified offset.
 *
 * @param state a valid and initialized proxy device state.
 * @param offset byte offset within the proxy device at which to write.
 * @param buf the data buffer to write.
 * @param len number of bytes to write from the buffer.
 * @return number of bytes successfully written on success, or a negative
 *         value on error.
 */
ssize_t proxy_device_write_with_offset(struct proxy_device_state *state,
    off_t offset, const uint8_t *buf, size_t len);

/**
 * @brief Read data from the proxy device at a specified offset.
 *
 * @param state a valid and initialized proxy device state.
 * @param offset byte offset within the proxy device from which to read.
 * @param buf the buffer where the read data will be stored.
 * @param len maximum number of bytes to read.
 * @return number of bytes successfully read on success, or a negative
 *         value on error.
 */
ssize_t proxy_device_read_with_offset(
    struct proxy_device_state *state, off_t offset, uint8_t *buf, size_t len);

/**
 * @brief Create an I/O queue state
 *
 * @param dev_state a complete and operable proxy device state
 * @param state an empty `struct proxy_device_queue_state`
 * @param queue_idx a queue index (in order of appearance in the shm buffer)
 * @param process_callback a I/O processing callback
 * @param fatal_stop_callback a fatal stop callback
 * @return 0 if successful, other negative number for error
 */
int proxy_device_create_queue_state(struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    struct proxy_queue_func_ops *func_ops, int (*thread_priv_create)(void **),
    void (*thread_priv_teardown)(void *));

/**
 * @brief Create a set of states of I/O queues
 *
 * This function uses `create_proxy_dev_queue_state` internally,
 * and simplifies "finding out" of the max allowed queues being
 * initialized.
 * This function might initialize less queues than specified in
 * `queues_count`, if the proxy device does not support that
 * number of queues.
 *
 * @param state a complete and operable proxy device state
 * @param queues an array of `struct proxy_device_queue_state`
 * @param queues_count a count of objects in that array
 * @param ops a read, write and fatal-stop processing callbacks
 * @return Number of queues actually being initialized if successful,
 *		   other negative number for error.
 */
int proxy_device_initialize_queues(struct proxy_device_state *state,
    struct proxy_device_queue_state *queues, size_t queues_count,
    struct proxy_queue_func_ops *ops, int (*thread_priv_create)(void **),
    void (*thread_priv_teardown)(void *));

/**
 * @brief Destroy an I/O queue state
 *
 * Destroy an I/O queue - free resources, destroy threads and close
 * eventfds.
 *
 * @param state a complete and operable proxy device queue state
 * @return 0 if successful, other negative number for error
 */
void proxy_device_destroy_queue_state(struct proxy_device_queue_state *state);

/**
 * @brief Close a proxy device
 *
 * This function closes a ufedm proxy device that was opened with the
 * `open_proxy_device_state` function.
 * It unmap the shared memory buffer and finally closes the file descriptor.
 *
 * IMPORTANT: We don't free the `struct proxy_device_state` - it's up to the
 * user application to do this afterwards.
 *
 * @param state to-be-filled pointer of a struct containting the new state
 * @return 0 if successful, other negative number for error
 */
void proxy_device_close(struct proxy_device_state *state);

#endif
