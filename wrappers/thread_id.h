/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PORTABLE_THREAD_ID_H_
#define __PORTABLE_THREAD_ID_H_

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>

static inline uint64_t get_thread_id(void)
{
	return (uint64_t)GetCurrentThreadId();
}

#elif defined(__APPLE__)
#include <pthread.h>

static inline uint64_t get_thread_id(void)
{
	uint64_t tid;
	pthread_threadid_np(NULL, &tid);
	return tid;
}

#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>

static inline uint64_t get_thread_id(void)
{
	return (uint64_t)syscall(SYS_gettid);
}

#endif

#endif