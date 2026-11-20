/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PROXY_IOCTL_TYPES_H
#define __PROXY_IOCTL_TYPES_H

#include "linux-mtd/defs.h"
#include "linux-mtd/proxy_queue.h"
#include "linux-mtd/shared_mem.h"

/* Struct definitions for ioctl ops */
struct proxy_mtd_info {
	u32 backend_mtd_index;
	u16 flash_page_size;   /* Should be the entire page size of a NAND flash
				* chip, including ECC bytes, bad block marker
				* and other misc bytes which are not intended
				* for data storage
				*/
	u16 flash_oob_size;    // OOB size includes ECC bytes, bad block marker
			       // and other misc bytes
	u16 flash_sectors_cnt; // Total count of flash device's sectors
			       // (eraseblocks)
	u32 flash_pages_per_sector_cnt; // Page count per erase sector (known
					// also as eraseblock)
	u32 flash_erase_sector_size;	// Size of erase sector
	u64 flash_size;			// Total size of flash device
	u32 reserved[3];		/* reserved for future expansion */
};

struct proxy_stats {
	u64 requests;
	u64 responses;
	u64 errors;
	u64 reserved[5]; /* reserved for future expansion */
};

struct proxy_answer_base {
	u8 queue_idx;

	/* A packet slot to ACK or NACK, currently used with a slot number
	 * to verify that userspace didn't do this by mistake.
	 * It **MIGHT** be removed if deemed unnecessary later on...
	 */
	u64 seq_num;

	u64 slot_num;
};

struct proxy_ack {
	struct proxy_answer_base base;

	/* Data region that is returned. For most cases should be equal
	 * to the actual page data region size on the flash chip.
	 */
	u32 retlen;
	/* OOB region that is returned. For most cases should be equal
	 * to the actual page OOB region size on the flash chip.
	 */
	u32 oob_retlen;
};

struct proxy_nack {
	struct proxy_answer_base base;

	u16 positive_errno;
};

struct proxy_upper_mtd_read_req {
	u64 offset;
	u64 buffer; /* Userspace buffer pointer */
	u32 length;
	u32 retlen; /* Updated after the request is completed - actual bytes
		       read */
};

struct proxy_upper_mtd_write_req {
	u64 offset;
	u64 buffer; /* Userspace buffer pointer */
	u32 length;
	u32 retlen; /* Updated after the request is completed - Actual bytes
		       written */
};

struct proxy_mtd_erase_req {
	u64 offset;
	u64 length;
};

struct proxy_register_eventfd {
	u8 type;
	u32 slot_idx;
	int fd;
};

struct proxy_unregister_eventfd {
	u8 type;
	u32 slot_idx;
};

#endif
