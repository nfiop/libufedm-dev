/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "linux-mtd/dev.h"
#include "linux-mtd/proxy_ioctl.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

static int do_io_on_proxy_mtd_with_timeout(
    struct proxy_device_state *state, u32 ioctl_cmd, void *opaque)
{
	int ret;
	size_t attempts;
	attempts = 0;
	/* FIXME: We wait up to 10 seconds for driver to be fully initialized
	 * but there might be a better wait to synchronize this in the kernel
	 * module.
	 */
	while (attempts < 10) {
		ret = ioctl(state->fd, ioctl_cmd, opaque);
		if (ret < 0) {
			if (errno == EAGAIN) {
				attempts++;
				sleep(1);
				continue;
			}

			ret = -errno;
		}
		break;
	}

	return ret;
}

static ssize_t linux_proxy_device_write_with_offset(
    struct proxy_device_state *state, off_t offset, const uint8_t *buf,
    size_t len)
{
	int ret;
	struct proxy_upper_mtd_write_req req;

	/* Device is not fully initialized, reject further I/O */
	if (!atomic_load(&state->queues_initialized))
		return -EIO;

	req.offset = (u64)offset;
	req.buffer = (u64)buf;
	req.length = (u64)len;
	req.retlen = 0;

	ret = do_io_on_proxy_mtd_with_timeout(
	    state, PROXY_IOC_UPPER_MTD_WRITE, &req);
	if (ret < 0)
		return ret;

	return (ssize_t)req.retlen;
}

static ssize_t linux_proxy_device_read_with_offset(
    struct proxy_device_state *state, off_t offset, uint8_t *buf, size_t len)
{
	int ret;
	struct proxy_upper_mtd_write_req req;

	/* Device is not fully initialized, reject further I/O */
	if (!atomic_load(&state->queues_initialized))
		return -EIO;

	req.offset = (u64)offset;
	req.buffer = (u64)buf;
	req.length = (u64)len;
	req.retlen = 0;

	ret = do_io_on_proxy_mtd_with_timeout(
	    state, PROXY_IOC_UPPER_MTD_READ, &req);
	if (ret < 0)
		return ret;

	return (ssize_t)req.retlen;
}

static ssize_t linux_proxy_device_write(
    struct proxy_device_state *state, const uint8_t *buf, size_t len)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);

	ssize_t n = linux_proxy_device_write_with_offset(
	    state, state->emu_cur_off, buf, len);

	if (n > 0)
		state->emu_cur_off += n;

	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return n;
}

static ssize_t linux_proxy_device_read(
    struct proxy_device_state *state, uint8_t *buf, size_t len)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);

	ssize_t n = linux_proxy_device_read_with_offset(
	    state, state->emu_cur_off, buf, len);

	if (n > 0)
		state->emu_cur_off += n;

	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return n;
}

static int linux_proxy_device_seek(
    struct proxy_device_state *state, off_t offset)
{
	pthread_mutex_lock(&state->emu_cur_off_lock);
	state->emu_cur_off = offset;
	pthread_mutex_unlock(&state->emu_cur_off_lock);

	return 0;
}

int proxy_device_erase(
    struct proxy_device_state *state, off_t offset, size_t len)
{
	int ret;
	struct proxy_mtd_erase_req req;
	if ((offset % state->mtd_info.flash_erase_sector_size) != 0)
		return -EINVAL;
	if ((len % state->mtd_info.flash_erase_sector_size) != 0)
		return -EINVAL;

	req.offset = offset;
	req.length = len;

	ret = do_io_on_proxy_mtd_with_timeout(state, PROXY_IOC_MTD_ERASE, &req);
	if (ret < 0)
		return ret;

	return 0;
}

int proxy_device_erase_blocks(struct proxy_device_state *state,
    uint64_t eraseblock_idx, size_t blocks_count)
{
	int ret;
	struct proxy_mtd_erase_req req;
	req.offset = eraseblock_idx * state->mtd_info.flash_erase_sector_size;
	req.length = blocks_count * state->mtd_info.flash_erase_sector_size;

	ret = do_io_on_proxy_mtd_with_timeout(state, PROXY_IOC_MTD_ERASE, &req);
	if (ret < 0)
		return ret;

	return 0;
}

int linux_proxy_device_open_by_argv_index(const char *index)
{
	unsigned long idx;
	char *end;
	int fd;
	char path[64];

	errno = 0;
	idx = strtoul(index, &end, 10);

	/* Validation */
	if (errno != 0) {
		perror("strtoul");
		return 1;
	}

	if (*end != '\0') {
		fprintf(
		    stderr, "Invalid input: trailing characters '%s'\n", end);
		return 1;
	}

	if (idx > 1000) {
		fprintf(stderr, "Index too large (max 1000)\n");
		return 1;
	}

	snprintf(path, sizeof(path), "/dev/ufedm_proxy%lu", idx);

	fprintf(stderr, "Opening %s\n", path);

	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	fprintf(stderr, "Device opened successfully\n");
	return fd;
}

int proxy_device_open(const char *path, struct proxy_device_state *state)
{
	int ret;
	size_t attempts;
	char mtd_path[64];

	state->fd = open(path, O_RDWR);
	if (state->fd < 0) {
		ret = -errno;
		goto exit;
	}

	ret = ioctl(state->fd, PROXY_IOC_GET_SHM_INFO, &state->shm_info);
	if (ret < 0) {
		ret = -errno;
		goto exit;
	}

	ret = ioctl(state->fd, PROXY_IOC_GET_MTD_INFO, &state->mtd_info);
	if (ret < 0) {
		ret = -errno;
		goto exit;
	}

	state->shared_mem_buf_mmap_size = state->shm_info.total_buf_size;
	state->shared_mem_buf = mmap(NULL, state->shared_mem_buf_mmap_size,
	    PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);

	if (state->shared_mem_buf == NULL) {
		ret = -EFAULT;
		goto exit;
	}

	state->write = linux_proxy_device_write;
	state->read = linux_proxy_device_read;
	state->pwrite = linux_proxy_device_write_with_offset;
	state->pread = linux_proxy_device_read_with_offset;
	state->seek = linux_proxy_device_seek;

	return 0;

exit:
	return ret;
}

static void close_eventfds(
    struct proxy_device_queue_state *state, size_t max_efd_idx)
{
	size_t idx;
	for (idx = 0; idx < max_efd_idx; idx++) {
		close(state->eventfds[idx]);
		state->eventfds[idx] = -1;
	}
}

static int create_eventfds(struct proxy_device_queue_state *state)
{
	int efd;
	size_t idx;

	for (idx = 0; idx < state->info.slots_count; idx++) {
		efd = eventfd(0, EFD_SEMAPHORE);
		if (efd < 0) {
			efd = -errno;
			goto cleanup;
		}

		state->eventfds[idx] = efd;
	}

	return 0;

cleanup:
	close_eventfds(state, idx);
	return efd;
}

int proxy_device_register_eventfd(
    struct proxy_device_queue_state *state, size_t slot_idx, int efd)
{
	int ret;
	struct proxy_register_eventfd register_req;
	assert(state->info.slots_count > slot_idx);
	assert(state->info.type == PROXY_QUEUE_TYPE_READ ||
	       state->info.type == PROXY_QUEUE_TYPE_WRITE);

	register_req.fd = efd;
	register_req.type = state->info.type;

	register_req.slot_idx = slot_idx;
	ret = ioctl(
	    state->proxy_device_fd, PROXY_IOC_REGISTER_EVENTFD, &register_req);
	if (ret < 0) {
		ret = errno;
		perror("ioctl");
		goto exit_ioctl_error;
	}

	return 0;

exit_ioctl_error:
	close(efd);
	return ret;
}

void proxy_device_unregister_eventfd(
    struct proxy_device_queue_state *state, size_t slot_idx)
{
	int ret;
	struct proxy_unregister_eventfd unregister_req;
	assert(state->info.slots_count > slot_idx);
	assert(state->info.type == PROXY_QUEUE_TYPE_READ ||
	       state->info.type == PROXY_QUEUE_TYPE_WRITE);

	unregister_req.type = state->info.type;
	unregister_req.slot_idx = slot_idx;
	ret = ioctl(state->proxy_device_fd, PROXY_IOC_UNREGISTER_EVENTFD,
	    &unregister_req);

	// We are very unlikey to get error (unless we passed wrong slot idx,
	// etc), so assert on such condition.
	assert(ret == 0);

	// FIXME: The compiler on the Olimex A20 buildroot environment was
	// angry we didn't "use" ret anywhere in the function. Put this to calm
	// it down, although we have an assertion check with that variable.
	(void)ret;
}

static void unregister_eventfds(
    struct proxy_device_queue_state *state, size_t max_slot_idx)
{
	for (size_t slot_idx = 0; slot_idx < max_slot_idx; slot_idx++) {
		proxy_device_unregister_eventfd(state, slot_idx);
	}
}

static int register_eventfds(struct proxy_device_queue_state *state)
{
	int ret;
	size_t slot_idx;
	for (slot_idx = 0; slot_idx < state->info.slots_count; slot_idx++) {
		ret = proxy_device_register_eventfd(
		    state, slot_idx, state->eventfds[slot_idx]);
		if (ret < 0)
			goto cleanup;
	}

	return 0;

cleanup:
	unregister_eventfds(state, slot_idx);
	return ret;
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
		ret = write(thread->efd, dummy_buf, sizeof(*dummy_buf));
		if (ret < 0) {
			perror("write");
			continue;
		}

		pthread_join(thread->tid, NULL);

		if (thread->priv_teardown && thread->private_data)
			thread->priv_teardown(thread->private_data);

		free(thread->raw_page_buf);
	}
}

int proxy_ioctl_nack(
    struct proxy_device_thread_state *state, struct proxy_nack *nack)
{
	return ioctl(state->proxy_device_fd, PROXY_IOC_NACK, nack);
}

int proxy_ioctl_ack(
    struct proxy_device_thread_state *state, struct proxy_ack *ack)
{
	return ioctl(state->proxy_device_fd, PROXY_IOC_ACK, ack);
}

int eventfd_block_until_request_submitted(
    struct proxy_device_thread_state *state)
{
	u64 dummy;
	return read(state->efd, &dummy, sizeof(dummy));
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

		thread_state->efd = state->eventfds[slot_idx];
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

		/* Some needed callbacks */
		thread_state->proxy_ack = proxy_ioctl_ack;
		thread_state->proxy_nack = proxy_ioctl_nack;
		thread_state->block_until_request_submitted =
		    eventfd_block_until_request_submitted;

		/* Just in case */
		thread_state->completion_queue = NULL;
		thread_state->submission_queue = NULL;

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

int linux_proxy_device_create_queue_state(struct proxy_device_state *dev_state,
    struct proxy_device_queue_state *state, size_t queue_idx,
    void *(*worker_thread_func)(void *), struct proxy_queue_func_ops *func_ops,
    int (*thread_priv_create)(void **), void (*thread_priv_teardown)(void *))
{
	int ret;
	u8 *shared_mem_buf;
	void *transform_callback;

	ret = ioctl(
	    state->proxy_device_fd, PROXY_IOC_GET_QUEUE_INFO, &state->info);
	if (ret < 0) {
		ret = errno;
		goto exit;
	}

	state->eventfds = calloc(state->info.slots_count, sizeof(int));
	if (!state->eventfds) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = create_eventfds(state);
	if (ret < 0)
		goto free_eventfds_list;

	state->thread_array = calloc(
	    state->info.slots_count, sizeof(struct proxy_device_thread_state));
	if (!state->thread_array) {
		ret = -ENOMEM;
		goto remove_eventfds;
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

	ret = register_eventfds(state);
	if (ret < 0)
		goto remove_threads;

	return 0;

remove_threads:
	destroy_threads(state, state->info.slots_count);
free_thread_array:
	free(state->thread_array);
remove_eventfds:
	close_eventfds(state, state->info.slots_count);
free_eventfds_list:
	free(state->eventfds);
exit:
	return ret;
}

void linux_proxy_device_destroy_queue_state(
    struct proxy_device_queue_state *state)
{
	unregister_eventfds(state, state->info.slots_count);
	destroy_threads(state, state->info.slots_count);
	free(state->thread_array);
	close_eventfds(state, state->info.slots_count);
	free(state->eventfds);
}
