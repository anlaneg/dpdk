/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef EAL_HUGEPAGES_H
#define EAL_HUGEPAGES_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#define MAX_HUGEPAGE_PATH PATH_MAX

/**
 * Structure used to store information about hugepages that we mapped
 * through the files in hugetlbfs.
 */
struct hugepage_file {
	/*此页虚拟地址*/
	void *orig_va;      /**< virtual addr of first mmap() */
	void *final_va;     /**< virtual addr of 2nd mmap() */
	/*此页对应的物理地址*/
	uint64_t physaddr;  /**< physical addr */
	/*页大小*/
	size_t size;        /**< the page size */
	int socket_id;      /**< NUMA socket ID */
	int file_id;        /**< the '%d' in HUGEFILE_FMT */
	/*此大页对应的文件名称*/
	char filepath[MAX_HUGEPAGE_PATH]; /**< path to backing file on filesystem */
};

/**
 * Read the information on what hugepages are available for the EAL to use,
 * clearing out any unused ones.
 */
int eal_hugepage_info_init(void);

/**
 * Read whatever information primary process has shared about hugepages into
 * secondary process.
 */
int eal_hugepage_info_read(void);

#endif /* EAL_HUGEPAGES_H */
