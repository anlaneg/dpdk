/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2021 Broadcom
 * All rights reserved.
 */

#include <rte_common.h>

#include "cfa_resource_types.h"
#include "tf_device.h"
#include "tf_identifier.h"
#include "tf_tbl.h"
#include "tf_tcam.h"
#include "tf_em.h"
#include "tf_if_tbl.h"
#include "tfp.h"
#include "tf_msg_common.h"

#define TF_DEV_P58_PARIF_MAX 16
#define TF_DEV_P58_PF_MASK 0xfUL

/* For print alignment, make all entries 8 chars in this table */
const char *tf_resource_str_p58[CFA_RESOURCE_TYPE_P58_LAST + 1] = {
	[CFA_RESOURCE_TYPE_P58_METER]              = "meter   ",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_0]        = "sram_bk0",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_1]        = "sram_bk1",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_2]        = "sram_bk2",
	[CFA_RESOURCE_TYPE_P58_SRAM_BANK_3]        = "sram_bk3",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_HIGH]  = "l2ctx_hi",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_TCAM_LOW]   = "l2ctx_lo",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_HIGH] = "l2ctr_hi",
	[CFA_RESOURCE_TYPE_P58_L2_CTXT_REMAP_LOW]  = "l2ctr_lo",
	[CFA_RESOURCE_TYPE_P58_PROF_FUNC]          = "prf_func",
	[CFA_RESOURCE_TYPE_P58_PROF_TCAM]          = "prf_tcam",
	[CFA_RESOURCE_TYPE_P58_EM_PROF_ID]         = "em_prof ",
	[CFA_RESOURCE_TYPE_P58_WC_TCAM_PROF_ID]    = "wc_prof ",
	[CFA_RESOURCE_TYPE_P58_EM_REC]             = "em_rec  ",
	[CFA_RESOURCE_TYPE_P58_WC_TCAM]            = "wc_tcam ",
	[CFA_RESOURCE_TYPE_P58_METER_PROF]         = "mtr_prof",
	[CFA_RESOURCE_TYPE_P58_MIRROR]             = "mirror  ",
	[CFA_RESOURCE_TYPE_P58_EM_FKB]             = "em_fkb  ",
	[CFA_RESOURCE_TYPE_P58_WC_FKB]             = "wc_fkb  ",
	[CFA_RESOURCE_TYPE_P58_VEB_TCAM]           = "veb     ",
};

/**
 * Device specific function that retrieves the MAX number of HCAPI
 * types the device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] max_types
 *   Pointer to the MAX number of HCAPI types supported
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_max_types(struct tf *tfp,
			 uint16_t *max_types)
{
	if (max_types == NULL || tfp == NULL)
		return -EINVAL;

	*max_types = CFA_RESOURCE_TYPE_P58_LAST + 1;

	return 0;
}
/**
 * Device specific function that retrieves a human readable
 * string to identify a CFA resource type.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [in] resource_id
 *   HCAPI CFA resource id
 *
 * [out] resource_str
 *   Resource string
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_resource_str(struct tf *tfp __rte_unused,
			    uint16_t resource_id,
			    const char **resource_str)
{
	if (resource_str == NULL)
		return -EINVAL;

	if (resource_id > CFA_RESOURCE_TYPE_P58_LAST)
		return -EINVAL;

	*resource_str = tf_resource_str_p58[resource_id];

	return 0;
}

/**
 * Device specific function that retrieves the WC TCAM slices the
 * device supports.
 *
 * [in] tfp
 *   Pointer to TF handle
 *
 * [out] slice_size
 *   Pointer to the WC TCAM slice size
 *
 * [out] num_slices_per_row
 *   Pointer to the WC TCAM row slice configuration
 *
 * Returns
 *   - (0) if successful.
 *   - (-EINVAL) on failure.
 */
static int
tf_dev_p58_get_tcam_slice_info(struct tf *tfp __rte_unused,
			      enum tf_tcam_tbl_type type,
			      uint16_t key_sz,
			      uint16_t *num_slices_per_row)
{
#define CFA_P58_WC_TCAM_SLICES_PER_ROW 2
#define CFA_P58_WC_TCAM_SLICE_SIZE     12

	if (type == TF_TCAM_TBL_TYPE_WC_TCAM) {
		*num_slices_per_row = CFA_P58_WC_TCAM_SLICES_PER_ROW;
		if (key_sz > *num_slices_per_row * CFA_P58_WC_TCAM_SLICE_SIZE)
			return -ENOTSUP;

		*num_slices_per_row = 1;
	} else { /* for other type of tcam */
		*num_slices_per_row = 1;
	}

	return 0;
}

static int tf_dev_p58_get_mailbox(void)
{
	return TF_CHIMP_MB;
}

static int tf_dev_p58_word_align(uint16_t size)
{
	return ((((size) + 63) >> 6) * 8);
}

/**
 * Truflow P58 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p58_init = {
	.tf_dev_get_max_types = tf_dev_p58_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p58_get_resource_str,
	.tf_dev_get_tcam_slice_info = tf_dev_p58_get_tcam_slice_info,
	.tf_dev_alloc_ident = NULL,
	.tf_dev_free_ident = NULL,
	.tf_dev_search_ident = NULL,
	.tf_dev_alloc_ext_tbl = NULL,
	.tf_dev_alloc_tbl = NULL,
	.tf_dev_free_ext_tbl = NULL,
	.tf_dev_free_tbl = NULL,
	.tf_dev_alloc_search_tbl = NULL,
	.tf_dev_set_tbl = NULL,
	.tf_dev_set_ext_tbl = NULL,
	.tf_dev_get_tbl = NULL,
	.tf_dev_get_bulk_tbl = NULL,
	.tf_dev_alloc_tcam = NULL,
	.tf_dev_free_tcam = NULL,
	.tf_dev_alloc_search_tcam = NULL,
	.tf_dev_set_tcam = NULL,
	.tf_dev_get_tcam = NULL,
	.tf_dev_insert_int_em_entry = NULL,
	.tf_dev_delete_int_em_entry = NULL,
	.tf_dev_insert_ext_em_entry = NULL,
	.tf_dev_delete_ext_em_entry = NULL,
	.tf_dev_alloc_tbl_scope = NULL,
	.tf_dev_map_tbl_scope = NULL,
	.tf_dev_map_parif = NULL,
	.tf_dev_free_tbl_scope = NULL,
	.tf_dev_set_if_tbl = NULL,
	.tf_dev_get_if_tbl = NULL,
	.tf_dev_set_global_cfg = NULL,
	.tf_dev_get_global_cfg = NULL,
	.tf_dev_get_mailbox = tf_dev_p58_get_mailbox,
	.tf_dev_word_align = NULL,
};

/**
 * Truflow P58 device specific functions
 */
const struct tf_dev_ops tf_dev_ops_p58 = {
	.tf_dev_get_max_types = tf_dev_p58_get_max_types,
	.tf_dev_get_resource_str = tf_dev_p58_get_resource_str,
	.tf_dev_get_tcam_slice_info = tf_dev_p58_get_tcam_slice_info,
	.tf_dev_alloc_ident = tf_ident_alloc,
	.tf_dev_free_ident = tf_ident_free,
	.tf_dev_search_ident = tf_ident_search,
	.tf_dev_alloc_tbl = tf_tbl_alloc,
	.tf_dev_alloc_ext_tbl = tf_tbl_ext_alloc,
	.tf_dev_free_tbl = tf_tbl_free,
	.tf_dev_free_ext_tbl = tf_tbl_ext_free,
	.tf_dev_alloc_search_tbl = tf_tbl_alloc_search,
	.tf_dev_set_tbl = tf_tbl_set,
	.tf_dev_set_ext_tbl = tf_tbl_ext_common_set,
	.tf_dev_get_tbl = tf_tbl_get,
	.tf_dev_get_bulk_tbl = tf_tbl_bulk_get,
	.tf_dev_alloc_tcam = tf_tcam_alloc,
	.tf_dev_free_tcam = tf_tcam_free,
	.tf_dev_alloc_search_tcam = tf_tcam_alloc_search,
	.tf_dev_set_tcam = tf_tcam_set,
	.tf_dev_get_tcam = tf_tcam_get,
	.tf_dev_insert_int_em_entry = tf_em_hash_insert_int_entry,
	.tf_dev_delete_int_em_entry = tf_em_hash_delete_int_entry,
	.tf_dev_insert_ext_em_entry = NULL,
	.tf_dev_delete_ext_em_entry = NULL,
	.tf_dev_alloc_tbl_scope = NULL,
	.tf_dev_map_tbl_scope = NULL,
	.tf_dev_map_parif = NULL,
	.tf_dev_free_tbl_scope = NULL,
	.tf_dev_set_if_tbl = tf_if_tbl_set,
	.tf_dev_get_if_tbl = tf_if_tbl_get,
	.tf_dev_set_global_cfg = tf_global_cfg_set,
	.tf_dev_get_global_cfg = tf_global_cfg_get,
	.tf_dev_get_mailbox = tf_dev_p58_get_mailbox,
	.tf_dev_word_align = tf_dev_p58_word_align,
};
