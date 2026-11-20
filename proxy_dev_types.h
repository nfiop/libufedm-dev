/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PROXY_DEV_TYPES_H
#define __PROXY_DEV_TYPES_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Data structures that neutral to Linux ioctl commands' definitions */
#include "linux-mtd/proxy_ioctl_types.h"

#include "emulated-proxy/bitmap.h"
#include "emulated-proxy/sync_queue.h"

typedef enum {
	ANSWER_ACK,
	ANSWER_NACK,
} transform_answer_t;

struct transform_context_results {
	int errno_rc;
	bool had_fatal_stop;
	size_t data_retlen;
	size_t oob_retlen;
};
typedef transform_answer_t (*transform_data_func_t)(
    const struct shm_slot_hdr *hdr, u8 *raw_page_buf,
    struct transform_context_results *context_res, void *private_data);
typedef void (*notify_fatal_stop_thread_func_t)(uint64_t tid, int rc);

struct proxy_device_queue_state;
struct proxy_device_state {
	struct proxy_mtd_info mtd_info;
	struct proxy_shm_info shm_info;

	void *shared_mem_buf;
	size_t shared_mem_buf_mmap_size;

	/* Emulated device is set to -1 */
	int fd;

	/* Current Offset - this SHOULD have only being used when the device
	 * is emulated.
	 * The idea is to simulate some sort of lseek functionality, so
	 * consecutive I/O append to this counter, until EOF.
	 * However, since we don't use the ordinary file-descriptor syscalls
	 * on the upper MTD (so we use special ioctls on the proxy device),
	 * this is useful on Linux as well.
	 */
	off_t emu_cur_off;
	pthread_mutex_t emu_cur_off_lock;

	ssize_t (*write)(
	    struct proxy_device_state *state, const uint8_t *buf, size_t len);
	ssize_t (*read)(
	    struct proxy_device_state *state, uint8_t *buf, size_t len);
	ssize_t (*pwrite)(struct proxy_device_state *state, off_t offset,
	    const uint8_t *buf, size_t len);
	ssize_t (*pread)(struct proxy_device_state *state, off_t offset,
	    uint8_t *buf, size_t len);
	int (*seek)(struct proxy_device_state *state, off_t seek_offset);

	/* Keep a reference to queues, actual storage is the responsibility
	 * of the user program (which determines if uses malloc or not).
	 */
	struct proxy_device_queue_state *queues[PROXY_MAX_QUEUES_COUNT];

	/* Determined in runtime - so we don't hardcode an index value for
	 * a specific type.
	 */
	struct proxy_device_queue_state *read_queue;
	struct proxy_device_queue_state *write_queue;

	/* Used by proxy_device_initialize_queues to mark a good-working
	 * device state.
	 */
	atomic_bool queues_initialized;
};

struct proxy_queue_func_ops {
	transform_data_func_t write;
	transform_data_func_t read;
	notify_fatal_stop_thread_func_t fatal_stop;
};
struct proxy_device_thread_state {
	pthread_t tid;
	atomic_bool _stop;

	int proxy_device_fd;
	size_t queue_idx;
	size_t slot_idx;
	struct shared_mem_slot *slot;
	size_t slot_buf_len;

	transform_data_func_t transform_func;
	notify_fatal_stop_thread_func_t notify_fatal_stop_func;

	int (*proxy_ack)(
	    struct proxy_device_thread_state *, struct proxy_ack *ack);
	int (*proxy_nack)(
	    struct proxy_device_thread_state *, struct proxy_nack *nack);

	// Can be NULL, for non-emulated devices (when using ioctls on Linux)
	// The actual queue is per slot, and given by `struct
	// emulated_nand_proxy_io_slots`.
	nand_proxy_sync_queue_t *completion_queue;

	/* Request submission sync semaphore */
#if defined(__linux__)
	int efd;
#endif
	// Can be NULL, for non-emulated devices (when using eventfds on Linux)
	// The actual queue is per slot, and given by `struct
	// emulated_nand_proxy_io_slots`.
	nand_proxy_sync_queue_t *submission_queue;
	int (*block_until_request_submitted)(
	    struct proxy_device_thread_state *);

	/* Temporary scratch buffer for callback to do its "stuff"
	 * during its execution. When done, it's copied back as-is
	 * to the shared memory slot page buffer.
	 */
	u8 *raw_page_buf;
	size_t raw_page_len;

	/* Private data that can be used during execution of a thread callback
	 */
	void *private_data;
	void (*priv_teardown)(void *);
};

struct emulated_nand_proxy_io_slots {
	/* Bitmap for allocating slots */
	bitmap_t allocated_slots;

	/* Size of arrays is set by state->info.slots_count */
	nand_proxy_sync_queue_t *submission_queues;
	nand_proxy_sync_queue_t *completion_queues;
};

struct proxy_device_queue_state {
	/* Emulated device fd is set to -1 */
	int proxy_device_fd;

	struct proxy_shm_queue_info info;

	/* Size of eventfds array is set by state->info.slots_count
	 * for non-emulated devices.
	 *
	 * Emulated devices don't use eventfds, so another generic structure
	 * is used.
	 */
#if defined(__linux__)
	int *eventfds;
#endif
	struct emulated_nand_proxy_io_slots emu_io_slots;

	struct proxy_device_thread_state *thread_array;
};

#endif
