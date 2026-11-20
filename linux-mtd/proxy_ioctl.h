/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

// proxy_ioctl.h - shared UAPI header for kernel and userspace
#ifndef PROXY_IOCTL_H
#define PROXY_IOCTL_H

#include "linux-mtd/proxy_ioctl_types.h"

// Detect whether we are compiling in the kernel or userspace
#ifdef __KERNEL__
#include <linux/ioctl.h> /* _IO, _IOR, _IOW */
#else
#include <sys/ioctl.h>
#endif

/* Magic number for ioctl commands */
#define PROXY_IOC_MAGIC 'P'

/* ioctl commands */
#define PROXY_IOC_GET_SHM_INFO _IOR(PROXY_IOC_MAGIC, 0, struct proxy_shm_info)
#define PROXY_IOC_GET_STATS _IOR(PROXY_IOC_MAGIC, 1, struct proxy_stats)
#define PROXY_IOC_GET_MTD_INFO _IOR(PROXY_IOC_MAGIC, 2, struct proxy_mtd_info)
#define PROXY_IOC_GET_QUEUE_INFO                                               \
	_IOR(PROXY_IOC_MAGIC, 3, struct proxy_shm_queue_info)
#define PROXY_IOC_ACK _IOW(PROXY_IOC_MAGIC, 4, struct proxy_ack)
#define PROXY_IOC_NACK _IOW(PROXY_IOC_MAGIC, 5, struct proxy_nack)
#define PROXY_IOC_REGISTER_EVENTFD                                             \
	_IOW(PROXY_IOC_MAGIC, 7, struct proxy_register_eventfd)
#define PROXY_IOC_UNREGISTER_EVENTFD                                           \
	_IOW(PROXY_IOC_MAGIC, 8, struct proxy_unregister_eventfd)
#define PROXY_IOC_UPPER_MTD_READ                                               \
	_IOWR(PROXY_IOC_MAGIC, 9, struct proxy_upper_mtd_read_req)
#define PROXY_IOC_UPPER_MTD_WRITE                                              \
	_IOWR(PROXY_IOC_MAGIC, 10, struct proxy_upper_mtd_write_req)
#define PROXY_IOC_MTD_ERASE                                                    \
	_IOW(PROXY_IOC_MAGIC, 11, struct proxy_mtd_erase_req)

#endif
