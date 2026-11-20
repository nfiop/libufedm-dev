/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Liav A.
 *
 * Originally written for the sipbell project.
 * See https://github.com/supercomputer7/sipbell for more details.
 */

#include "emulated-proxy/sync_queue.h"
#include <errno.h>
#include <string.h>

void nand_proxy_sync_queue_init(nand_proxy_sync_queue_t *q)
{
	q->head = q->tail = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

bool nand_proxy_sync_queue_push(
    nand_proxy_sync_queue_t *q, nand_proxy_sync_event_t *event)
{
	pthread_mutex_lock(&q->mutex);

	int next = (q->tail + 1) % NAND_PROXY_QUEUE_SIZE;
	if (next == q->head) {
		// queue full
		pthread_mutex_unlock(&q->mutex);
		return -ENOMEM;
	}

	memcpy(&q->items[q->tail], event, sizeof(nand_proxy_sync_event_t));
	q->tail = next;

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
	return 0;
}

void nand_proxy_sync_queue_pop_blocking(
    nand_proxy_sync_queue_t *q, nand_proxy_sync_event_t *recv_event)
{
	pthread_mutex_lock(&q->mutex);

	while (q->head == q->tail) {
		pthread_cond_wait(&q->cond, &q->mutex);
	}

	memcpy(recv_event, &q->items[q->head], sizeof(nand_proxy_sync_event_t));
	q->head = (q->head + 1) % NAND_PROXY_QUEUE_SIZE;

	pthread_mutex_unlock(&q->mutex);
}
