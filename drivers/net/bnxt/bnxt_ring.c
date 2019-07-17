/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <rte_bitmap.h>
#include <rte_memzone.h>
#include <unistd.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"

#include "hsi_struct_def_dpdk.h"

/*
 * Generic ring handling
 */

void bnxt_free_ring(struct bnxt_ring *ring)
{
	if (!ring)
		return;

	if (ring->vmem_size && *ring->vmem) {
		memset((char *)*ring->vmem, 0, ring->vmem_size);
		*ring->vmem = NULL;
	}
	ring->mem_zone = NULL;
}

/*
 * Ring groups
 */

int bnxt_init_ring_grps(struct bnxt *bp)
{
	unsigned int i;

	for (i = 0; i < bp->max_ring_grps; i++)
		memset(&bp->grp_info[i], (uint8_t)HWRM_NA_SIGNATURE,
		       sizeof(struct bnxt_ring_grp_info));

	return 0;
}

/*
 * Allocates a completion ring with vmem and stats optionally also allocating
 * a TX and/or RX ring.  Passing NULL as tx_ring_info and/or rx_ring_info
 * to not allocate them.
 *
 * Order in the allocation is:
 * stats - Always non-zero length
 * cp vmem - Always zero-length, supported for the bnxt_ring abstraction
 * tx vmem - Only non-zero length if tx_ring_info is not NULL
 * rx vmem - Only non-zero length if rx_ring_info is not NULL
 * cp bd ring - Always non-zero length
 * tx bd ring - Only non-zero length if tx_ring_info is not NULL
 * rx bd ring - Only non-zero length if rx_ring_info is not NULL
 */
int bnxt_alloc_rings(struct bnxt *bp, uint16_t qidx,
			    struct bnxt_tx_queue *txq,
			    struct bnxt_rx_queue *rxq,
			    struct bnxt_cp_ring_info *cp_ring_info,
			    struct bnxt_cp_ring_info *nq_ring_info,
			    const char *suffix)
{
	struct bnxt_ring *cp_ring = cp_ring_info->cp_ring_struct;
	struct bnxt_rx_ring_info *rx_ring_info = rxq ? rxq->rx_ring : NULL;
	struct bnxt_tx_ring_info *tx_ring_info = txq ? txq->tx_ring : NULL;
	struct bnxt_ring *tx_ring;
	struct bnxt_ring *rx_ring;
	struct rte_pci_device *pdev = bp->pdev;
	uint64_t rx_offloads = bp->eth_dev->data->dev_conf.rxmode.offloads;
	const struct rte_memzone *mz = NULL;
	char mz_name[RTE_MEMZONE_NAMESIZE];
	rte_iova_t mz_phys_addr_base;
	rte_iova_t mz_phys_addr;
	int sz;

	int stats_len = (tx_ring_info || rx_ring_info) ?
	    RTE_CACHE_LINE_ROUNDUP(sizeof(struct hwrm_stat_ctx_query_output) -
				   sizeof (struct hwrm_resp_hdr)) : 0;
	stats_len = RTE_ALIGN(stats_len, 128);

	int cp_vmem_start = stats_len;
	int cp_vmem_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size);
	cp_vmem_len = RTE_ALIGN(cp_vmem_len, 128);

	int nq_vmem_len = BNXT_CHIP_THOR(bp) ?
		RTE_CACHE_LINE_ROUNDUP(cp_ring->vmem_size) : 0;
	nq_vmem_len = RTE_ALIGN(nq_vmem_len, 128);

	int nq_vmem_start = cp_vmem_start + cp_vmem_len;

	int tx_vmem_start = nq_vmem_start + nq_vmem_len;
	int tx_vmem_len =
	    tx_ring_info ? RTE_CACHE_LINE_ROUNDUP(tx_ring_info->
						tx_ring_struct->vmem_size) : 0;
	tx_vmem_len = RTE_ALIGN(tx_vmem_len, 128);

	int rx_vmem_start = tx_vmem_start + tx_vmem_len;
	int rx_vmem_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->
						rx_ring_struct->vmem_size) : 0;
	rx_vmem_len = RTE_ALIGN(rx_vmem_len, 128);
	int ag_vmem_start = 0;
	int ag_vmem_len = 0;
	int cp_ring_start =  0;
	int nq_ring_start = 0;

	ag_vmem_start = rx_vmem_start + rx_vmem_len;
	ag_vmem_len = rx_ring_info ? RTE_CACHE_LINE_ROUNDUP(
				rx_ring_info->ag_ring_struct->vmem_size) : 0;
	cp_ring_start = ag_vmem_start + ag_vmem_len;
	cp_ring_start = RTE_ALIGN(cp_ring_start, 4096);

	int cp_ring_len = RTE_CACHE_LINE_ROUNDUP(cp_ring->ring_size *
						 sizeof(struct cmpl_base));
	cp_ring_len = RTE_ALIGN(cp_ring_len, 128);
	nq_ring_start = cp_ring_start + cp_ring_len;
	nq_ring_start = RTE_ALIGN(nq_ring_start, 4096);

	int nq_ring_len = BNXT_CHIP_THOR(bp) ? cp_ring_len : 0;

	int tx_ring_start = nq_ring_start + nq_ring_len;
	int tx_ring_len = tx_ring_info ?
	    RTE_CACHE_LINE_ROUNDUP(tx_ring_info->tx_ring_struct->ring_size *
				   sizeof(struct tx_bd_long)) : 0;
	tx_ring_len = RTE_ALIGN(tx_ring_len, 4096);

	int rx_ring_start = tx_ring_start + tx_ring_len;
	int rx_ring_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rx_ring_info->rx_ring_struct->ring_size *
		sizeof(struct rx_prod_pkt_bd)) : 0;
	rx_ring_len = RTE_ALIGN(rx_ring_len, 4096);

	int ag_ring_start = rx_ring_start + rx_ring_len;
	int ag_ring_len = rx_ring_len * AGG_RING_SIZE_FACTOR;
	ag_ring_len = RTE_ALIGN(ag_ring_len, 4096);

	int ag_bitmap_start = ag_ring_start + ag_ring_len;
	int ag_bitmap_len =  rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(rte_bitmap_get_memory_footprint(
			rx_ring_info->rx_ring_struct->ring_size *
			AGG_RING_SIZE_FACTOR)) : 0;

	int tpa_info_start = ag_bitmap_start + ag_bitmap_len;
	int tpa_info_len = rx_ring_info ?
		RTE_CACHE_LINE_ROUNDUP(BNXT_TPA_MAX *
				       sizeof(struct bnxt_tpa_info)) : 0;

	int total_alloc_len = tpa_info_start;
	if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
		total_alloc_len += tpa_info_len;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE,
		 "bnxt_%04x:%02x:%02x:%02x-%04x_%s", pdev->addr.domain,
		 pdev->addr.bus, pdev->addr.devid, pdev->addr.function, qidx,
		 suffix);
	mz_name[RTE_MEMZONE_NAMESIZE - 1] = 0;
	mz = rte_memzone_lookup(mz_name);
	if (!mz) {
		mz = rte_memzone_reserve_aligned(mz_name, total_alloc_len,
				SOCKET_ID_ANY,
				RTE_MEMZONE_2MB |
				RTE_MEMZONE_SIZE_HINT_ONLY |
				RTE_MEMZONE_IOVA_CONTIG,
				getpagesize());
		if (mz == NULL)
			return -ENOMEM;
	}
	memset(mz->addr, 0, mz->len);
	mz_phys_addr_base = mz->iova;
	mz_phys_addr = mz->iova;
	if ((unsigned long)mz->addr == mz_phys_addr_base) {
		PMD_DRV_LOG(WARNING,
			"Memzone physical address same as virtual.\n");
		PMD_DRV_LOG(WARNING,
			"Using rte_mem_virt2iova()\n");
		for (sz = 0; sz < total_alloc_len; sz += getpagesize())
			rte_mem_lock_page(((char *)mz->addr) + sz);
		mz_phys_addr_base = rte_mem_virt2iova(mz->addr);
		mz_phys_addr = rte_mem_virt2iova(mz->addr);
		if (mz_phys_addr == 0) {
			PMD_DRV_LOG(ERR,
			"unable to map ring address to physical memory\n");
			return -ENOMEM;
		}
	}

	if (tx_ring_info) {
		txq->mz = mz;
		tx_ring = tx_ring_info->tx_ring_struct;

		tx_ring->bd = ((char *)mz->addr + tx_ring_start);
		tx_ring_info->tx_desc_ring = (struct tx_bd_long *)tx_ring->bd;
		tx_ring->bd_dma = mz_phys_addr + tx_ring_start;
		tx_ring_info->tx_desc_mapping = tx_ring->bd_dma;
		tx_ring->mem_zone = (const void *)mz;

		if (!tx_ring->bd)
			return -ENOMEM;
		if (tx_ring->vmem_size) {
			tx_ring->vmem =
			    (void **)((char *)mz->addr + tx_vmem_start);
			tx_ring_info->tx_buf_ring =
			    (struct bnxt_sw_tx_bd *)tx_ring->vmem;
		}
	}

	if (rx_ring_info) {
		rxq->mz = mz;
		rx_ring = rx_ring_info->rx_ring_struct;

		rx_ring->bd = ((char *)mz->addr + rx_ring_start);
		rx_ring_info->rx_desc_ring =
		    (struct rx_prod_pkt_bd *)rx_ring->bd;
		rx_ring->bd_dma = mz_phys_addr + rx_ring_start;
		rx_ring_info->rx_desc_mapping = rx_ring->bd_dma;
		rx_ring->mem_zone = (const void *)mz;

		if (!rx_ring->bd)
			return -ENOMEM;
		if (rx_ring->vmem_size) {
			rx_ring->vmem =
			    (void **)((char *)mz->addr + rx_vmem_start);
			rx_ring_info->rx_buf_ring =
			    (struct bnxt_sw_rx_bd *)rx_ring->vmem;
		}

		rx_ring = rx_ring_info->ag_ring_struct;

		rx_ring->bd = ((char *)mz->addr + ag_ring_start);
		rx_ring_info->ag_desc_ring =
		    (struct rx_prod_pkt_bd *)rx_ring->bd;
		rx_ring->bd_dma = mz->iova + ag_ring_start;
		rx_ring_info->ag_desc_mapping = rx_ring->bd_dma;
		rx_ring->mem_zone = (const void *)mz;

		if (!rx_ring->bd)
			return -ENOMEM;
		if (rx_ring->vmem_size) {
			rx_ring->vmem =
			    (void **)((char *)mz->addr + ag_vmem_start);
			rx_ring_info->ag_buf_ring =
			    (struct bnxt_sw_rx_bd *)rx_ring->vmem;
		}

		rx_ring_info->ag_bitmap =
		    rte_bitmap_init(rx_ring_info->rx_ring_struct->ring_size *
				    AGG_RING_SIZE_FACTOR, (uint8_t *)mz->addr +
				    ag_bitmap_start, ag_bitmap_len);

		/* TPA info */
		if (rx_offloads & DEV_RX_OFFLOAD_TCP_LRO)
			rx_ring_info->tpa_info =
				((struct bnxt_tpa_info *)((char *)mz->addr +
							  tpa_info_start));
	}

	cp_ring->bd = ((char *)mz->addr + cp_ring_start);
	cp_ring->bd_dma = mz_phys_addr + cp_ring_start;
	cp_ring_info->cp_desc_ring = cp_ring->bd;
	cp_ring_info->cp_desc_mapping = cp_ring->bd_dma;
	cp_ring->mem_zone = (const void *)mz;

	if (!cp_ring->bd)
		return -ENOMEM;
	if (cp_ring->vmem_size)
		*cp_ring->vmem = ((char *)mz->addr + stats_len);
	if (stats_len) {
		cp_ring_info->hw_stats = mz->addr;
		cp_ring_info->hw_stats_map = mz_phys_addr;
	}
	cp_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;

	if (BNXT_HAS_NQ(bp)) {
		struct bnxt_ring *nq_ring = nq_ring_info->cp_ring_struct;

		nq_ring->bd = (char *)mz->addr + nq_ring_start;
		nq_ring->bd_dma = mz_phys_addr + nq_ring_start;
		nq_ring_info->cp_desc_ring = nq_ring->bd;
		nq_ring_info->cp_desc_mapping = nq_ring->bd_dma;
		nq_ring->mem_zone = (const void *)mz;

		if (!nq_ring->bd)
			return -ENOMEM;
		if (nq_ring->vmem_size)
			*nq_ring->vmem = (char *)mz->addr + nq_vmem_start;

		nq_ring_info->hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	}

	return 0;
}

static void bnxt_init_dflt_coal(struct bnxt_coal *coal)
{
	/* Tick values in micro seconds.
	 * 1 coal_buf x bufs_per_record = 1 completion record.
	 */
	coal->num_cmpl_aggr_int = BNXT_NUM_CMPL_AGGR_INT;
	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	coal->num_cmpl_dma_aggr = BNXT_NUM_CMPL_DMA_AGGR;
	/* This is a 6-bit value and must not be 0, or we'll get non stop IRQ */
	coal->num_cmpl_dma_aggr_during_int = BNXT_NUM_CMPL_DMA_AGGR_DURING_INT;
	coal->int_lat_tmr_max = BNXT_INT_LAT_TMR_MAX;
	/* min timer set to 1/2 of interrupt timer */
	coal->int_lat_tmr_min = BNXT_INT_LAT_TMR_MIN;
	/* buf timer set to 1/4 of interrupt timer */
	coal->cmpl_aggr_dma_tmr = BNXT_CMPL_AGGR_DMA_TMR;
	coal->cmpl_aggr_dma_tmr_during_int = BNXT_CMPL_AGGR_DMA_TMR_DURING_INT;
}

static void bnxt_set_db(struct bnxt *bp,
			struct bnxt_db_info *db,
			uint32_t ring_type,
			uint32_t map_idx,
			uint32_t fid)
{
	if (BNXT_CHIP_THOR(bp)) {
		if (BNXT_PF(bp))
			db->doorbell = (char *)bp->doorbell_base + 0x10000;
		else
			db->doorbell = (char *)bp->doorbell_base + 0x4000;
		switch (ring_type) {
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_SQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_SRQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_CQ;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ:
			db->db_key64 = DBR_PATH_L2 | DBR_TYPE_NQ;
			break;
		}
		db->db_key64 |= (uint64_t)fid << DBR_XID_SFT;
		db->db_64 = true;
	} else {
		db->doorbell = (char *)bp->doorbell_base + map_idx * 0x80;
		switch (ring_type) {
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_TX:
			db->db_key32 = DB_KEY_TX;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_RX:
			db->db_key32 = DB_KEY_RX;
			break;
		case HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL:
			db->db_key32 = DB_KEY_CP;
			break;
		}
		db->db_64 = false;
	}
}

static int bnxt_alloc_cmpl_ring(struct bnxt *bp, int queue_index,
				struct bnxt_cp_ring_info *cpr,
				struct bnxt_cp_ring_info *nqr)
{
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	uint32_t nq_ring_id = HWRM_NA_SIGNATURE;
	uint8_t ring_type;
	int rc = 0;

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_L2_CMPL;

	if (BNXT_HAS_NQ(bp)) {
		if (nqr) {
			nq_ring_id = nqr->cp_ring_struct->fw_ring_id;
		} else {
			PMD_DRV_LOG(ERR, "NQ ring is NULL\n");
			return -EINVAL;
		}
	}

	rc = bnxt_hwrm_ring_alloc(bp, cp_ring, ring_type, queue_index,
				  HWRM_NA_SIGNATURE, nq_ring_id);
	if (rc)
		return rc;

	cpr->cp_cons = 0;
	bnxt_set_db(bp, &cpr->cp_db, ring_type, queue_index,
		    cp_ring->fw_ring_id);
	bnxt_db_cq(cpr);

	return 0;
}

static int bnxt_alloc_nq_ring(struct bnxt *bp, int queue_index,
			      struct bnxt_cp_ring_info *nqr,
			      bool rx)
{
	struct bnxt_ring *nq_ring = nqr->cp_ring_struct;
	uint8_t ring_type;
	int rc = 0;

	if (!BNXT_HAS_NQ(bp))
		return -EINVAL;

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_NQ;

	rc = bnxt_hwrm_ring_alloc(bp, nq_ring, ring_type, queue_index,
				  HWRM_NA_SIGNATURE, HWRM_NA_SIGNATURE);
	if (rc)
		return rc;

	if (rx)
		bp->grp_info[queue_index].cp_fw_ring_id = nq_ring->fw_ring_id;

	bnxt_set_db(bp, &nqr->cp_db, ring_type, queue_index,
		    nq_ring->fw_ring_id);
	bnxt_db_nq(nqr);

	return 0;
}

static int bnxt_alloc_rx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct bnxt_ring *ring = rxr->rx_ring_struct;
	uint8_t ring_type;
	int rc = 0;

	ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX;

	rc = bnxt_hwrm_ring_alloc(bp, ring, ring_type,
				  queue_index, cpr->hw_stats_ctx_id,
				  cp_ring->fw_ring_id);
	if (rc)
		return rc;

	rxr->rx_prod = 0;
	bp->grp_info[queue_index].rx_fw_ring_id = ring->fw_ring_id;
	bnxt_set_db(bp, &rxr->rx_db, ring_type, queue_index, ring->fw_ring_id);
	bnxt_db_write(&rxr->rx_db, rxr->rx_prod);

	return 0;
}

static int bnxt_alloc_rx_agg_ring(struct bnxt *bp, int queue_index)
{
	unsigned int map_idx = queue_index + bp->rx_cp_nr_rings;
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct bnxt_ring *ring = rxr->ag_ring_struct;
	uint32_t hw_stats_ctx_id = HWRM_NA_SIGNATURE;
	uint8_t ring_type;
	int rc = 0;

	ring->fw_rx_ring_id = rxr->rx_ring_struct->fw_ring_id;

	if (BNXT_CHIP_THOR(bp)) {
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX_AGG;
		hw_stats_ctx_id = cpr->hw_stats_ctx_id;
	} else {
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_RX;
	}

	rc = bnxt_hwrm_ring_alloc(bp, ring, ring_type, map_idx,
				  hw_stats_ctx_id, cp_ring->fw_ring_id);

	if (rc)
		return rc;

	rxr->ag_prod = 0;
	bp->grp_info[queue_index].ag_fw_ring_id = ring->fw_ring_id;
	bnxt_set_db(bp, &rxr->ag_db, ring_type, map_idx, ring->fw_ring_id);
	bnxt_db_write(&rxr->ag_db, rxr->ag_prod);

	return 0;
}

int bnxt_alloc_hwrm_rx_ring(struct bnxt *bp, int queue_index)
{
	struct bnxt_rx_queue *rxq = bp->rx_queues[queue_index];
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
	struct bnxt_cp_ring_info *nqr = rxq->nq_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	int rc = 0;

	if (BNXT_HAS_NQ(bp)) {
		if (bnxt_alloc_nq_ring(bp, queue_index, nqr, true))
			goto err_out;
	}

	if (bnxt_alloc_cmpl_ring(bp, queue_index, cpr, nqr))
		goto err_out;

	bp->grp_info[queue_index].fw_stats_ctx = cpr->hw_stats_ctx_id;
	bp->grp_info[queue_index].cp_fw_ring_id = cp_ring->fw_ring_id;

	if (!queue_index) {
		/*
		 * In order to save completion resources, use the first
		 * completion ring from PF or VF as the default completion ring
		 * for async event and HWRM forward response handling.
		 */
		bp->def_cp_ring = cpr;
		rc = bnxt_hwrm_set_async_event_cr(bp);
		if (rc)
			goto err_out;
	}

	if (bnxt_alloc_rx_ring(bp, queue_index))
		goto err_out;

	if (bnxt_alloc_rx_agg_ring(bp, queue_index))
		goto err_out;

	rxq->rx_buf_use_size = BNXT_MAX_MTU + RTE_ETHER_HDR_LEN +
		RTE_ETHER_CRC_LEN + (2 * VLAN_TAG_SIZE);

	if (bp->eth_dev->data->rx_queue_state[queue_index] ==
	    RTE_ETH_QUEUE_STATE_STARTED) {
		if (bnxt_init_one_rx_ring(rxq)) {
			RTE_LOG(ERR, PMD,
				"bnxt_init_one_rx_ring failed!\n");
			bnxt_rx_queue_release_op(rxq);
			rc = -ENOMEM;
			goto err_out;
		}
		bnxt_db_write(&rxr->rx_db, rxr->rx_prod);
		bnxt_db_write(&rxr->ag_db, rxr->ag_prod);
	}
	rxq->index = queue_index;
	PMD_DRV_LOG(INFO,
		    "queue %d, rx_deferred_start %d, state %d!\n",
		    queue_index, rxq->rx_deferred_start,
		    bp->eth_dev->data->rx_queue_state[queue_index]);

err_out:
	return rc;
}

/* ring_grp usage:
 * [0] = default completion ring
 * [1 -> +rx_cp_nr_rings] = rx_cp, rx rings
 * [1+rx_cp_nr_rings + 1 -> +tx_cp_nr_rings] = tx_cp, tx rings
 */
int bnxt_alloc_hwrm_rings(struct bnxt *bp)
{
	struct bnxt_coal coal;
	unsigned int i;
	uint8_t ring_type;
	int rc = 0;

	bnxt_init_dflt_coal(&coal);

	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];
		struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
		struct bnxt_cp_ring_info *nqr = rxq->nq_ring;
		struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
		struct bnxt_rx_ring_info *rxr = rxq->rx_ring;

		if (BNXT_HAS_NQ(bp)) {
			if (bnxt_alloc_nq_ring(bp, i, nqr, true))
				goto err_out;
		}

		if (bnxt_alloc_cmpl_ring(bp, i, cpr, nqr))
			goto err_out;

		bp->grp_info[i].fw_stats_ctx = cpr->hw_stats_ctx_id;
		bp->grp_info[i].cp_fw_ring_id = cp_ring->fw_ring_id;

		bnxt_hwrm_set_ring_coal(bp, &coal, cp_ring->fw_ring_id);

		if (!i) {
			/*
			 * In order to save completion resource, use the first
			 * completion ring from PF or VF as the default
			 * completion ring for async event & HWRM
			 * forward response handling.
			 */
			bp->def_cp_ring = cpr;
			rc = bnxt_hwrm_set_async_event_cr(bp);
			if (rc)
				goto err_out;
		}

		if (bnxt_alloc_rx_ring(bp, i))
			goto err_out;

		if (bnxt_alloc_rx_agg_ring(bp, i))
			goto err_out;

		rxq->rx_buf_use_size = BNXT_MAX_MTU + RTE_ETHER_HDR_LEN +
					RTE_ETHER_CRC_LEN + (2 * VLAN_TAG_SIZE);
		if (bnxt_init_one_rx_ring(rxq)) {
			PMD_DRV_LOG(ERR, "bnxt_init_one_rx_ring failed!\n");
			bnxt_rx_queue_release_op(rxq);
			return -ENOMEM;
		}
		bnxt_db_write(&rxr->rx_db, rxr->rx_prod);
		bnxt_db_write(&rxr->ag_db, rxr->ag_prod);
		rxq->index = i;
	}

	for (i = 0; i < bp->tx_cp_nr_rings; i++) {
		struct bnxt_tx_queue *txq = bp->tx_queues[i];
		struct bnxt_cp_ring_info *cpr = txq->cp_ring;
		struct bnxt_ring *cp_ring = cpr->cp_ring_struct;
		struct bnxt_cp_ring_info *nqr = txq->nq_ring;
		struct bnxt_tx_ring_info *txr = txq->tx_ring;
		struct bnxt_ring *ring = txr->tx_ring_struct;
		unsigned int idx = i + bp->rx_cp_nr_rings;

		if (BNXT_HAS_NQ(bp)) {
			if (bnxt_alloc_nq_ring(bp, idx, nqr, false))
				goto err_out;
		}

		if (bnxt_alloc_cmpl_ring(bp, idx, cpr, nqr))
			goto err_out;

		/* Tx ring */
		ring_type = HWRM_RING_ALLOC_INPUT_RING_TYPE_TX;
		rc = bnxt_hwrm_ring_alloc(bp, ring,
					  ring_type,
					  idx, cpr->hw_stats_ctx_id,
					  cp_ring->fw_ring_id);
		if (rc)
			goto err_out;

		bnxt_set_db(bp, &txr->tx_db, ring_type, idx, ring->fw_ring_id);
		txq->index = idx;
		bnxt_hwrm_set_ring_coal(bp, &coal, cp_ring->fw_ring_id);
	}

err_out:
	return rc;
}
