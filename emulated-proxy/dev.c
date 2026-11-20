/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "emulated-proxy/dev.h"
#include "sync_queue.h"

/* Copied from proxy_device_fill_shm_info ufedm kernel function */
static void proxy_device_fill_shm_info(
    size_t page_data_size, size_t page_oob_size, struct proxy_shm_info *p)
{
	p->proto_ver = 0;
	p->slot_size =
	    sizeof(struct shared_mem_slot) + page_data_size + page_oob_size;
	p->queues_count = PROXY_MAX_QUEUES_COUNT;

	/* Start with full size */
	p->total_buf_size =
	    p->slot_size * PROXY_SLOTS_COUNT_PER_QUEUE * PROXY_MAX_QUEUES_COUNT;

	memset(p->reserved, 0, sizeof(u32) * 6);
}

static int proxy_device_fill_mtd_info(size_t flash_total_size,
    size_t flash_erase_sector_size, size_t page_data_size, size_t page_oob_size,
    struct proxy_mtd_info *p)
{
	if ((flash_total_size % flash_erase_sector_size) != 0)
		return -EINVAL;

	/* We don't impose any limit on the actual pages per sector */
	if ((flash_erase_sector_size % page_data_size) != 0)
		return -EINVAL;

	if ((page_data_size / 512) < 1)
		return -EINVAL;

	/* We could technically check that oob size is aligned to 16 bytes,
	 * but there's a flash with 576 bytes of OOB according to Linux.
	 */
	if ((page_oob_size < 16))
		return -EINVAL;

	/* Emulated device has no "backing" MTD, set to -1 (0xFFFFFFFF) */
	p->backend_mtd_index = 0xFFFFFFFF;

	p->flash_page_size = page_data_size + page_oob_size;
	p->flash_oob_size = page_oob_size;
	p->flash_sectors_cnt = flash_total_size / flash_erase_sector_size;
	p->flash_pages_per_sector_cnt =
	    flash_erase_sector_size / page_data_size;
	p->flash_erase_sector_size = flash_erase_sector_size;
	p->flash_size = flash_total_size;
	memset(p->reserved, 0, sizeof(u32) * 3);
	return 0;
}

static int submit_request(
    struct proxy_device_queue_state *queue, size_t slot_idx)
{
	nand_proxy_sync_event_t queued;
	nand_proxy_sync_queue_t *sync_queue;

	sync_queue = &queue->emu_io_slots.submission_queues[slot_idx];

	queued.type = NAND_PROXY_SYNC_REQUEST_SUBMISSION;

	// FIXME: Is this actually needed? we have a completion queue per slot
	queued.slot_idx = slot_idx;

	if (!nand_proxy_sync_queue_push(sync_queue, &queued))
		return -ENOMEM;

	return 0;
}

static void completion_queue_block_until_request_done(
    struct proxy_device_queue_state *queue, size_t slot_idx,
    nand_proxy_sync_event_t *event)
{
	nand_proxy_sync_queue_pop_blocking(
	    &queue->emu_io_slots.completion_queues[slot_idx], event);
}

/* This function is copied from the ufedm kernel module, and is adapted
 * for this environment.
 */
static void fill_shm_slot_packet_buffer(struct shared_mem_slot *shm_slot,
    const uint8_t *data_buf, size_t datalen, uint64_t eraseblock_idx,
    uint64_t page_idx, size_t page_data_size)
{
	/* Emulated devices don't use sequence numbers like in the kernel */
	shm_slot->header.seq_num = 0;

	/* FIXME: We don't support sending OOB currently within the current API
	 */
	shm_slot->header.ooblen = 0;

	u8 *shm_slot_buf = (u8 *)shm_slot->buf;
	if (data_buf) {
		memcpy(shm_slot_buf, data_buf, datalen);
		shm_slot->header.datalen = datalen;

		if (datalen < page_data_size)
			memset(shm_slot_buf + datalen, 0,
			    page_data_size - datalen);
	}

	memset(&shm_slot->header.pos_params, 0,
	    sizeof(struct nand_io_position_params));
	/* We don't support multiple targets, LUNs or planes */
	shm_slot->header.pos_params.eraseblock = eraseblock_idx;
	shm_slot->header.pos_params.page = page_idx;
}

ssize_t __emulated_proxy_device_write_page(struct proxy_device_state *state,
    size_t queue_slot_idx, const uint8_t *buf, uint64_t eraseblock_idx,
    uint64_t page_idx, size_t len)
{
	int ret;
	struct shared_mem_slot *slot;
	nand_proxy_sync_event_t event;
	struct proxy_device_queue_state *queue;
	size_t page_data_size =
	    state->mtd_info.flash_page_size - state->mtd_info.flash_oob_size;

	queue = state->write_queue;
	slot = (struct shared_mem_slot *)(uint8_t *)state->shared_mem_buf +
	       queue->info.mem_offset;
	fill_shm_slot_packet_buffer(
	    slot, buf, len, eraseblock_idx, page_idx, page_data_size);

	ret = submit_request(queue, queue_slot_idx);
	if (ret < 0) {
		return ret;
	}

	completion_queue_block_until_request_done(
	    queue, queue_slot_idx, &event);

	switch (event.type) {
	case NAND_PROXY_SYNC_REQUEST_COMPLETION_ACK:
		return len;
	case NAND_PROXY_SYNC_REQUEST_COMPLETION_NACK:
		return -event.response.nack.positive_errno;
	default:
		assert(false);
	}

	/* Should not be reacheable */
	assert(false);
	return -1;
}

ssize_t __emulated_proxy_device_read_page(struct proxy_device_state *state,
    size_t queue_slot_idx, uint8_t *buf, uint64_t eraseblock_idx,
    uint64_t page_idx, size_t len)
{
	int ret;
	struct shared_mem_slot *slot;
	nand_proxy_sync_event_t event;
	struct proxy_device_queue_state *queue;
	size_t page_data_size =
	    state->mtd_info.flash_page_size - state->mtd_info.flash_oob_size;

	queue = state->write_queue;
	slot = (struct shared_mem_slot *)(uint8_t *)state->shared_mem_buf +
	       queue->info.mem_offset;
	fill_shm_slot_packet_buffer(
	    slot, NULL, len, eraseblock_idx, page_idx, page_data_size);

	ret = submit_request(queue, queue_slot_idx);
	if (ret < 0) {
		return ret;
	}

	completion_queue_block_until_request_done(
	    queue, queue_slot_idx, &event);

	switch (event.type) {
	case NAND_PROXY_SYNC_REQUEST_COMPLETION_ACK:
		memcpy(buf, (u8 *)slot->buf, len);
		return len;
	case NAND_PROXY_SYNC_REQUEST_COMPLETION_NACK:
		return -event.response.nack.positive_errno;
	default:
		assert(false);
	}

	/* Should not be reacheable */
	assert(false);
	return -1;
}

#if !defined(MIN)
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#endif

static bool calculate_eraseblock_and_page_indices(
    const struct proxy_device_state *state, off_t offset,
    uint64_t *eraseblock_idx, uint64_t *page_idx)
{
	size_t page_data_size;
	uint64_t flash_size = state->mtd_info.flash_size;

	if (offset >= flash_size)
		return false;

	page_data_size =
	    state->mtd_info.flash_page_size - state->mtd_info.flash_oob_size;

	*eraseblock_idx = offset / state->mtd_info.flash_erase_sector_size;
	*page_idx = offset / page_data_size;
	return true;
}

static ssize_t emulated_proxy_device_write_with_offset(
    struct proxy_device_state *state, off_t offset, const uint8_t *buf,
    size_t len)
{
	ssize_t tmp;
	ssize_t ret = 0;
	bit_index_t slot_idx;
	struct proxy_device_queue_state *queue = state->write_queue;
	bitmap_t *allocated_slots_bitmap = &queue->emu_io_slots.allocated_slots;
	size_t page_data_size;
	uint64_t eraseblock_idx;
	uint64_t page_idx;

	/* Device is not fully initialized, reject further I/O */
	if (!atomic_load(&state->queues_initialized))
		return -EIO;

	if (offset >= state->mtd_info.flash_size)
		return 0;

	slot_idx = bitmap_alloc(allocated_slots_bitmap);
	if (slot_idx < 0) {
		return -EAGAIN;
	}

	while (len > 0) {
		if (!calculate_eraseblock_and_page_indices(
			state, offset, &eraseblock_idx, &page_idx)) {
			/* No more space left, exit now */
			goto exit;
		}
		tmp = __emulated_proxy_device_write_page(state, slot_idx,
		    buf + ret, eraseblock_idx, page_idx,
		    MIN(len, page_data_size));
		if (tmp < 0) {
			ret = tmp;
			goto exit;
		}

		ret += tmp;
		len -= tmp;
	}

exit:
	bitmap_free(allocated_slots_bitmap, slot_idx);
	return ret;
}

static ssize_t emulated_proxy_device_read_with_offset(
    struct proxy_device_state *state, off_t offset, uint8_t *buf, size_t len)
{
	ssize_t tmp;
	ssize_t ret = 0;
	bit_index_t slot_idx;
	struct proxy_device_queue_state *queue = state->read_queue;
	bitmap_t *allocated_slots_bitmap = &queue->emu_io_slots.allocated_slots;
	size_t page_data_size;
	uint64_t eraseblock_idx;
	uint64_t page_idx;

	/* Device is not fully initialized, reject further I/O */
	if (!atomic_load(&state->queues_initialized))
		return -EIO;

	if (offset >= state->mtd_info.flash_size)
		return 0;

	slot_idx = bitmap_alloc(allocated_slots_bitmap);
	if (slot_idx < 0) {
		return -EAGAIN;
	}

	while (len > 0) {
		if (!calculate_eraseblock_and_page_indices(
			state, offset, &eraseblock_idx, &page_idx)) {
			/* No more space left, exit now */
			goto exit;
		}
		tmp = __emulated_proxy_device_read_page(state, slot_idx,
		    buf + ret, eraseblock_idx, page_idx,
		    MIN(len, page_data_size));
		if (tmp < 0) {
			ret = tmp;
			goto exit;
		}

		ret += tmp;
		len -= tmp;
	}

exit:
	bitmap_free(allocated_slots_bitmap, slot_idx);
	return ret;
}

static ssize_t emulated_proxy_device_write(
    struct proxy_device_state *state, const uint8_t *buf, size_t len)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);

	ssize_t n = emulated_proxy_device_write_with_offset(
	    state, state->emu_cur_off, buf, len);

	if (n > 0)
		state->emu_cur_off += n;

	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return n;
}

static ssize_t emulated_proxy_device_read(
    struct proxy_device_state *state, uint8_t *buf, size_t len)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);

	ssize_t n = emulated_proxy_device_read_with_offset(
	    state, state->emu_cur_off, buf, len);

	if (n > 0)
		state->emu_cur_off += n;

	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return n;
}

static int emulated_proxy_device_seek(
    struct proxy_device_state *state, off_t offset)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);
	state->emu_cur_off = offset;
	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return 0;
}

int proxy_device_open_emulated(size_t flash_total_size,
    size_t flash_erase_sector_size, size_t page_data_size, size_t page_oob_size,
    struct proxy_device_state *state)
{
	int ret;
	ret = proxy_device_fill_mtd_info(flash_total_size,
	    flash_erase_sector_size, page_data_size, page_oob_size,
	    &state->mtd_info);
	if (ret < 0)
		goto exit;

	ret = pthread_mutex_init(&state->emu_cur_off_lock, NULL);
	if (ret < 0) {
		goto exit;
	}

	proxy_device_fill_shm_info(
	    page_data_size, page_oob_size, &state->shm_info);

	state->shared_mem_buf_mmap_size = state->shm_info.total_buf_size;
	state->shared_mem_buf =
	    calloc(sizeof(uint8_t), state->shared_mem_buf_mmap_size);
	if (!state->shared_mem_buf) {
		ret = -ENOMEM;
		goto exit;
	}

	state->write = emulated_proxy_device_write;
	state->read = emulated_proxy_device_read;
	state->pwrite = emulated_proxy_device_write_with_offset;
	state->pread = emulated_proxy_device_read_with_offset;
	state->seek = emulated_proxy_device_seek;
	state->fd = -1;

	ret = 0;
exit:
	return ret;
}

static void emulated_proxy_get_queue_info(size_t queue_idx,
    const struct proxy_shm_info *shm_info, struct proxy_shm_queue_info *info)
{
	size_t memlen;

	/* This function simulates the Linux PROXY_IOC_GET_QUEUE_INFO ioctl */
	info->idx = queue_idx;
	info->type = (queue_idx % 2 == 0) ? PROXY_QUEUE_TYPE_READ
					  : PROXY_QUEUE_TYPE_WRITE;
	info->slots_count = PROXY_SLOTS_COUNT_PER_QUEUE;

	memlen = info->slots_count * shm_info->slot_size;

	info->mem_offset = queue_idx * memlen;
	info->mem_len = memlen;
}

static int completion_queue_nack(
    struct proxy_device_thread_state *state, struct proxy_nack *nack)
{
	nand_proxy_sync_event_t queued;
	queued.type = NAND_PROXY_SYNC_REQUEST_COMPLETION_NACK;
	memcpy(&queued.response.nack, nack, sizeof(struct proxy_nack));

	// FIXME: Is this actually needed? we have a completion queue per slot
	queued.slot_idx = state->slot_idx;

	if (!nand_proxy_sync_queue_push(state->completion_queue, &queued))
		return -ENOMEM;

	return 0;
}

static int completion_queue_ack(
    struct proxy_device_thread_state *state, struct proxy_ack *ack)
{
	nand_proxy_sync_event_t queued;
	queued.type = NAND_PROXY_SYNC_REQUEST_COMPLETION_ACK;
	memcpy(&queued.response.ack, ack, sizeof(struct proxy_ack));

	// FIXME: Is this actually needed? we have a completion queue per slot
	queued.slot_idx = state->slot_idx;

	if (!nand_proxy_sync_queue_push(state->completion_queue, &queued))
		return -ENOMEM;

	return 0;
}

static int submission_queue_block_until_request_submitted(
    struct proxy_device_thread_state *state)
{
	nand_proxy_sync_event_t dummy;
	nand_proxy_sync_queue_pop_blocking(state->submission_queue, &dummy);
	return 0;
}

static int wake_queue(struct proxy_device_thread_state *state)
{
	nand_proxy_sync_event_t queued;
	queued.type = NAND_PROXY_SYNC_WAKE_UP;

	// FIXME: Is this actually needed? we have a completion queue per slot
	queued.slot_idx = state->slot_idx;

	if (!nand_proxy_sync_queue_push(state->completion_queue, &queued))
		return -ENOMEM;

	return 0;
}

static void destroy_threads(
    struct proxy_device_queue_state *state, size_t max_thread_index)
{
	int ret;
	size_t idx;
	const char *dummy_buf = "0\n";
	struct proxy_device_thread_state *thread;

	for (idx = 0; idx < max_thread_index; idx++) {
		thread = &state->thread_array[idx];
		ret = wake_queue(thread);
		if (ret < 0) {
			/* FIXME: Closest to write as we get... */
			perror("write");
			continue;
		}

		pthread_join(thread->tid, NULL);

		if (thread->priv_teardown && thread->private_data)
			thread->priv_teardown(thread->private_data);

		free(thread->raw_page_buf);
	}
}

static int create_threads(struct proxy_device_queue_state *state,
    u8 *shared_mem_buffer, size_t slot_size, size_t flash_page_size,
    void *transform_callback,
    notify_fatal_stop_thread_func_t fatal_stop_callback,
    void *(*worker_thread_func)(void *), int (*thread_priv_create)(void **),
    void (*thread_priv_teardown)(void *))
{
	int ret;
	size_t slot_idx;
	struct proxy_device_thread_state *thread_state;

	if ((thread_priv_create && !thread_priv_teardown) ||
	    (!thread_priv_create && thread_priv_teardown))
		return -EINVAL;

	for (slot_idx = 0; slot_idx < state->info.slots_count; slot_idx++) {
		thread_state = &state->thread_array[slot_idx];

#if defined(__linux__)
		thread_state->efd = -1;
#endif
		thread_state->_stop = false;
		thread_state->proxy_device_fd = state->proxy_device_fd;
		thread_state->queue_idx = state->info.idx;
		thread_state->slot_idx = slot_idx;
		thread_state->slot =
		    (struct shared_mem_slot *)shared_mem_buffer;
		thread_state->slot_buf_len = slot_size;
		thread_state->transform_func = transform_callback;
		thread_state->notify_fatal_stop_func = fatal_stop_callback;
		thread_state->priv_teardown = thread_priv_teardown;

		thread_state->proxy_ack = completion_queue_ack;
		thread_state->proxy_nack = completion_queue_nack;
		thread_state->block_until_request_submitted =
		    submission_queue_block_until_request_submitted;

		thread_state->completion_queue =
		    &state->emu_io_slots.completion_queues[slot_idx];
		thread_state->submission_queue =
		    &state->emu_io_slots.submission_queues[slot_idx];

		if (thread_priv_create) {
			ret = thread_priv_create(&thread_state->private_data);
			if (ret < 0)
				goto cleanup;
		}

		thread_state->raw_page_buf = malloc(flash_page_size);
		if (!thread_state->raw_page_buf) {
			ret = -ENOMEM;
			goto cleanup;
		}

		thread_state->raw_page_len = flash_page_size;

		ret = pthread_create(
		    &thread_state->tid, NULL, worker_thread_func, thread_state);
		if (ret < 0)
			ret = -errno;

		shared_mem_buffer += slot_size;
		if (ret < 0) {
			free(thread_state->raw_page_buf);
			goto cleanup;
		}
	}

	return 0;

cleanup:
	destroy_threads(state, slot_idx);
	return ret;
}

int emulated_proxy_device_create_queue_state(
    struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    void *(*worker_thread_func)(void *), struct proxy_queue_func_ops *func_ops,
    int (*thread_priv_create)(void **), void (*thread_priv_teardown)(void *))
{
	int ret;
	u8 *shared_mem_buf;
	void *transform_callback;

	/* In a real Linux proxy device, we have a hard limit by the kernel
	 * so let's simulate this.
	 */
	if (queue_idx >= PROXY_MAX_QUEUES_COUNT)
		return -EINVAL;

	emulated_proxy_get_queue_info(
	    queue_idx, &dev_state->shm_info, &state->info);

	state->emu_io_slots.submission_queues =
	    calloc(state->info.slots_count, sizeof(nand_proxy_sync_queue_t));
	if (!state->emu_io_slots.submission_queues) {
		ret = -ENOMEM;
		goto exit;
	}

	state->emu_io_slots.completion_queues =
	    calloc(state->info.slots_count, sizeof(nand_proxy_sync_queue_t));
	if (!state->emu_io_slots.completion_queues) {
		ret = -ENOMEM;
		goto free_submission_queues;
	}

	ret = bitmap_init(
	    &state->emu_io_slots.allocated_slots, state->info.slots_count);
	if (ret < 0) {
		goto free_completion_queues;
	}

	state->thread_array = calloc(
	    state->info.slots_count, sizeof(struct proxy_device_thread_state));
	if (!state->thread_array) {
		ret = -ENOMEM;
		goto destroy_bitmap;
	}

	assert(state->info.type == PROXY_QUEUE_TYPE_READ ||
	       state->info.type == PROXY_QUEUE_TYPE_WRITE);
	if (state->info.type == PROXY_QUEUE_TYPE_READ) {
		transform_callback = func_ops->read;
	} else {
		transform_callback = func_ops->write;
	}

	shared_mem_buf =
	    ((u8 *)dev_state->shared_mem_buf) + state->info.mem_offset;
	ret = create_threads(state, shared_mem_buf,
	    dev_state->shm_info.slot_size, dev_state->mtd_info.flash_page_size,
	    transform_callback, func_ops->fatal_stop, worker_thread_func,
	    thread_priv_create, thread_priv_teardown);
	if (ret < 0)
		goto free_thread_array;

	return 0;

free_thread_array:
	free(state->thread_array);
destroy_bitmap:
	bitmap_deinit(&state->emu_io_slots.allocated_slots);
free_completion_queues:
	free(state->emu_io_slots.completion_queues);
free_submission_queues:
	free(state->emu_io_slots.submission_queues);
exit:
	return ret;
}

void emulated_proxy_device_destroy_queue_state(
    struct proxy_device_queue_state *state)
{
	destroy_threads(state, state->info.slots_count);
	free(state->thread_array);
	bitmap_deinit(&state->emu_io_slots.allocated_slots);
	free(state->emu_io_slots.completion_queues);
	free(state->emu_io_slots.submission_queues);
}
