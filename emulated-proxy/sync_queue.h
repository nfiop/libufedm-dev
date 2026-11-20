/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 *
 * Originally written for the sipbell project.
 * See https://github.com/supercomputer7/sipbell for more details.
 */

#ifndef NAND_PROXY_EVENT_QUEUE_H__
#define NAND_PROXY_EVENT_QUEUE_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define NAND_PROXY_QUEUE_SIZE 256

#include "linux-mtd/proxy_ioctl_types.h"

typedef enum {
	NAND_PROXY_SYNC_WAKE_UP, // Used for exit condition
	NAND_PROXY_SYNC_REQUEST_SUBMISSION,
	NAND_PROXY_SYNC_REQUEST_COMPLETION_ACK,
	NAND_PROXY_SYNC_REQUEST_COMPLETION_NACK,
} nand_proxy_sync_type_t;

typedef struct {
	nand_proxy_sync_type_t type;

	size_t slot_idx;

	/* Used for completion queues only */
	union {
		struct proxy_nack nack;
		struct proxy_ack ack;
	} response;
} nand_proxy_sync_event_t;

typedef struct {
	nand_proxy_sync_event_t items[NAND_PROXY_QUEUE_SIZE];
	int head;
	int tail;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} nand_proxy_sync_queue_t;

void nand_proxy_sync_queue_init(nand_proxy_sync_queue_t *q);
bool nand_proxy_sync_queue_push(
    nand_proxy_sync_queue_t *q, nand_proxy_sync_event_t *queued_event);
void nand_proxy_sync_queue_pop_blocking(
    nand_proxy_sync_queue_t *q, nand_proxy_sync_event_t *recv_event);

#endif
