/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "proxy_dev.h"
#include "wrappers/thread_id.h"

#if defined(__linux__)
#include "linux-mtd/dev.h"
/* Defined for the close function */
#include <fcntl.h>
#include <sys/mman.h>
#endif

#include "emulated-proxy/dev.h"

ssize_t proxy_device_write(
    struct proxy_device_state *state, const uint8_t *buf, size_t len)
{
	return state->write(state, buf, len);
}

ssize_t proxy_device_read(
    struct proxy_device_state *state, uint8_t *buf, size_t len)
{
	return state->read(state, buf, len);
}

ssize_t proxy_device_write_with_offset(struct proxy_device_state *state,
    off_t offset, const uint8_t *buf, size_t len)
{
	return state->pwrite(state, offset, buf, len);
}

ssize_t proxy_device_read_with_offset(
    struct proxy_device_state *state, off_t offset, uint8_t *buf, size_t len)
{
	return state->pread(state, offset, buf, len);
}

int proxy_device_seek(struct proxy_device_state *state, off_t seek_offset)
{
	return state->seek(state, seek_offset);
}

void proxy_device_close(struct proxy_device_state *state)
{
#if defined(__linux__)
	/* fd is -1 when it's emulated */
	if (state->fd != -1) {
		munmap(state->shared_mem_buf, state->shared_mem_buf_mmap_size);
		close(state->fd);
	} else {
		pthread_mutex_destroy(&state->emu_cur_off_lock);
		free(state->shared_mem_buf);
	}
#else
	assert(state->fd == -1);
	pthread_mutex_destroy(&state->emu_cur_off_lock);
	free(state->shared_mem_buf);
#endif
}

static void zero_transform_context_results(
    struct transform_context_results *context_res)
{
	context_res->errno_rc = 0;
	context_res->had_fatal_stop = false;
	context_res->data_retlen = 0;
	context_res->oob_retlen = 0;
}

static void *worker_thread_func(void *arg)
{
	int rc = 0;
	transform_answer_t answer;
	struct transform_context_results context_res;
	u64 counter = 0;
	bool had_fatal_stop = false;
	struct proxy_device_thread_state *state = arg;
	struct shm_slot_hdr *hdr;
	struct proxy_nack nack;
	struct proxy_ack ack;

	/* Callbacks */
	int (*proxy_ack)(
	    struct proxy_device_thread_state *, struct proxy_ack *ack);
	int (*proxy_nack)(
	    struct proxy_device_thread_state *, struct proxy_nack *nack);
	int (*block_until_request_submitted)(
	    struct proxy_device_thread_state *);
	transform_data_func_t callback;

	hdr = (struct shm_slot_hdr *)&state->slot->header;

	assert(state != NULL);
	assert(state->proxy_ack != NULL);
	assert(state->proxy_nack != NULL);
	assert(state->block_until_request_submitted != NULL);

#if defined(__linux__)
	assert(state->efd >= 0);
#endif

	callback = state->transform_func;
	proxy_ack = state->proxy_ack;
	proxy_nack = state->proxy_nack;
	block_until_request_submitted = state->block_until_request_submitted;

	while (!state->_stop) {
		// This call should block until there's a new
		// data in the slot to process, or because we
		// wrote to the file descriptor to stop the
		// thread.
		//
		// It is expected that upon destroying of thread workers
		// that the eventfd will be unregistered from the kernel
		// to ensure no interference from stopping.
		rc = block_until_request_submitted(state);
		if (rc < 0)
			break;
		// We might wake just for exiting right now.
		if (state->_stop)
			break;

		zero_transform_context_results(&context_res);

		/* Copy OOB and data bytes to a temporary buffer so to disallow
		 * direct manipulation of the shared memory buffer within
		 * the callback function.
		 */
		memcpy(
		    state->raw_page_buf, state->slot->buf, state->raw_page_len);

		/* Some basic rules and guidelines:
		 *  - A callback should be ideally a non-blocking function
		 *  - If there's a need for blocking, it MUST NOT block forever
		 *	  (i.e. use a timeout)
		 *  - A callback SHOULD NOT touch the header structure. That
		 *    header is considered constant during callback execution.
		 *  - The answer (retval) is either ANSWER_NACK or ANSWER_ACK
		 * only.
		 */
		answer = callback(hdr, state->raw_page_buf, &context_res,
		    state->private_data);
		if (had_fatal_stop)
			break;

		assert(answer == ANSWER_NACK || answer == ANSWER_ACK);
		if (answer == ANSWER_NACK) {
			nack.base.queue_idx = state->queue_idx;
			nack.base.slot_num = state->slot_idx;
			nack.base.seq_num = hdr->seq_num;
			nack.positive_errno = rc;
			rc = proxy_nack(state, &nack);
			if (rc < 0) {
				rc = errno;
				had_fatal_stop = true;
				break;
			}
		} else {
			/* Copy data back now to the shared memory slot. */
			memcpy(state->slot->buf, state->raw_page_buf,
			    state->raw_page_len);
			ack.base.queue_idx = state->queue_idx;
			ack.base.slot_num = state->slot_idx;
			ack.base.seq_num = hdr->seq_num;
			ack.retlen = context_res.data_retlen;
			ack.oob_retlen = context_res.oob_retlen;
			rc = proxy_ack(state, &ack);
			if (rc < 0) {
				rc = errno;
				had_fatal_stop = true;
				break;
			}
		}
	}

	if (had_fatal_stop && state->notify_fatal_stop_func)
		state->notify_fatal_stop_func(get_thread_id(), rc);

	return NULL;
}

int proxy_device_create_queue_state(struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    struct proxy_queue_func_ops *func_ops, int (*thread_priv_create)(void **),
    void (*thread_priv_teardown)(void *))
{
	int ret;

	state->proxy_device_fd = dev_state->fd;
	state->info.idx = queue_idx;

	if (state->proxy_device_fd == -1) {
		ret = emulated_proxy_device_create_queue_state(dev_state, state,
		    queue_idx, worker_thread_func, func_ops, thread_priv_create,
		    thread_priv_teardown);
	} else {
#if !defined(__linux__)
		/* On non-Linux platforms, there's simply no real MTD or a proxy
		 * device
		 */
		ret = -EOPNOTSUPP;
#else
		ret = linux_proxy_device_create_queue_state(dev_state, state,
		    queue_idx, worker_thread_func, func_ops, thread_priv_create,
		    thread_priv_teardown);
#endif
	}

	if (ret < 0)
		goto exit;

	/* Keep a reference for the queue in the device state */
	dev_state->queues[queue_idx] = state;
	ret = 0;
exit:
	return ret;
}

static void __destroy_queues(
    struct proxy_device_queue_state *queues, size_t max_idx)
{
	size_t idx;

	for (idx = 0; idx < max_idx; idx++) {
		proxy_device_destroy_queue_state(&queues[idx]);
	}
}

int proxy_device_initialize_queues(struct proxy_device_state *state,
    struct proxy_device_queue_state *queues, size_t queues_count,
    struct proxy_queue_func_ops *ops, int (*thread_priv_create)(void **),
    void (*thread_priv_teardown)(void *))
{
	int ret;
	size_t idx;

	if (queues_count > state->shm_info.queues_count)
		queues_count = state->shm_info.queues_count;

	for (idx = 0; idx < queues_count; idx++) {
		ret = proxy_device_create_queue_state(state, &queues[idx], idx,
		    ops, thread_priv_create, thread_priv_teardown);
		if (ret < 0) {
			goto destroy_queues;
		}
	}

	/* Return the number of queues being initialized */
	ret = idx;
	atomic_store(&state->queues_initialized, true);
	goto exit;

destroy_queues:
	__destroy_queues(queues, idx);
exit:
	return ret;
}

void proxy_device_destroy_queue_state(struct proxy_device_queue_state *state)
{
#if defined(__linux__)
	/* fd is -1 when it's emulated */
	if (state->proxy_device_fd != -1) {
		linux_proxy_device_destroy_queue_state(state);
	} else {
		emulated_proxy_device_destroy_queue_state(state);
	}
#else
	assert(state->proxy_device_fd == -1);
	emulated_proxy_device_destroy_queue_state(state);
#endif
}
