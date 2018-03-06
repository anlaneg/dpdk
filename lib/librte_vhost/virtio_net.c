/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_vhost.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_sctp.h>
#include <rte_arp.h>
#include <rte_spinlock.h>

#include "iotlb.h"
#include "vhost.h"

#define MAX_PKT_BURST 32

#define MAX_BATCH_LEN 256

static bool
is_valid_virt_queue_idx(uint32_t idx, int is_tx, uint32_t nr_vring)
{
	//首先队列的idx必须是小于nr_vring的，其次，如果is_tx是发队列，则idx &1 必须为1(即必须为奇数）
	return (is_tx ^ (idx & 1)) == 0 && idx < nr_vring;
}

static __rte_always_inline void
do_flush_shadow_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq,
			  uint16_t to, uint16_t from, uint16_t size)
{
	rte_memcpy(&vq->used->ring[to],
			&vq->shadow_used_ring[from],
			size * sizeof(struct vring_used_elem));
	vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[to]),
			size * sizeof(struct vring_used_elem));
}

static __rte_always_inline void
flush_shadow_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t used_idx = vq->last_used_idx & (vq->size - 1);

	if (used_idx + vq->shadow_used_idx <= vq->size) {
		do_flush_shadow_used_ring(dev, vq, used_idx, 0,
					  vq->shadow_used_idx);
	} else {
		uint16_t size;

		/* update used ring interval [used_idx, vq->size] */
		size = vq->size - used_idx;
		do_flush_shadow_used_ring(dev, vq, used_idx, 0, size);

		/* update the left half used ring interval [0, left_size] */
		do_flush_shadow_used_ring(dev, vq, 0, size,
					  vq->shadow_used_idx - size);
	}
	vq->last_used_idx += vq->shadow_used_idx;

	rte_smp_wmb();

	*(volatile uint16_t *)&vq->used->idx += vq->shadow_used_idx;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));
}

static __rte_always_inline void
update_shadow_used_ring(struct vhost_virtqueue *vq,
			 uint16_t desc_idx, uint16_t len)
{
	uint16_t i = vq->shadow_used_idx++;

	vq->shadow_used_ring[i].id  = desc_idx;
	vq->shadow_used_ring[i].len = len;
}

static inline void
do_data_copy_enqueue(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++) {
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);
		vhost_log_write(dev, elem[i].log_addr, elem[i].len);
		PRINT_PACKET(dev, (uintptr_t)elem[i].dst, elem[i].len, 0);
	}
}

static inline void
do_data_copy_dequeue(struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++)
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);
}

/* avoid write operation when necessary, to lessen cache issues */
#define ASSIGN_UNLESS_EQUAL(var, val) do {	\
	if ((var) != (val))			\
		(var) = (val);			\
} while (0)

static void
virtio_enqueue_offload(struct rte_mbuf *m_buf, struct virtio_net_hdr *net_hdr)
{
	//入队时，将mbuf上的标记，转到net_hdr中
	uint64_t csum_l4 = m_buf->ol_flags & PKT_TX_L4_MASK;

	if (m_buf->ol_flags & PKT_TX_TCP_SEG)
		csum_l4 |= PKT_TX_TCP_CKSUM;

	if (csum_l4) {
		net_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		net_hdr->csum_start = m_buf->l2_len + m_buf->l3_len;

		switch (csum_l4) {
		case PKT_TX_TCP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct tcp_hdr,
						cksum));
			break;
		case PKT_TX_UDP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct udp_hdr,
						dgram_cksum));
			break;
		case PKT_TX_SCTP_CKSUM:
			net_hdr->csum_offset = (offsetof(struct sctp_hdr,
						cksum));
			break;
		}
	} else {
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_start, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_offset, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->flags, 0);
	}

	/* IP cksum verification cannot be bypassed, then calculate here */
	if (m_buf->ol_flags & PKT_TX_IP_CKSUM) {
		struct ipv4_hdr *ipv4_hdr;

		ipv4_hdr = rte_pktmbuf_mtod_offset(m_buf, struct ipv4_hdr *,
						   m_buf->l2_len);
		ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);
	}

	if (m_buf->ol_flags & PKT_TX_TCP_SEG) {
		if (m_buf->ol_flags & PKT_TX_IPV4)
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
		else
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len
					+ m_buf->l4_len;
	} else if (m_buf->ol_flags & PKT_TX_UDP_SEG) {
		net_hdr->gso_type = VIRTIO_NET_HDR_GSO_UDP;
		net_hdr->gso_size = m_buf->tso_segsz;
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len +
			m_buf->l4_len;
	} else {
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_type, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->gso_size, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->hdr_len, 0);
	}
}

static __rte_always_inline int
copy_mbuf_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		  struct vring_desc *descs, struct rte_mbuf *m,
		  uint16_t desc_idx, uint32_t size)
{
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct vring_desc *desc;
	uint64_t desc_addr;
	/* A counter to avoid desc dead loop chain */
	uint16_t nr_desc = 1;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	uint16_t copy_nb = vq->batch_copy_nb_elems;
	int error = 0;

	desc = &descs[desc_idx];
	desc_addr = vhost_iova_to_vva(dev, vq, desc->addr,
					desc->len, VHOST_ACCESS_RW);
	/*
	 * Checking of 'desc_addr' placed outside of 'unlikely' macro to avoid
	 * performance issue with some versions of gcc (4.8.4 and 5.3.0) which
	 * otherwise stores offset on the stack instead of in a register.
	 */
	if (unlikely(desc->len < dev->vhost_hlen) || !desc_addr) {
		error = -1;
		goto out;
	}

	rte_prefetch0((void *)(uintptr_t)desc_addr);

	virtio_enqueue_offload(m, (struct virtio_net_hdr *)(uintptr_t)desc_addr);
	vhost_log_write(dev, desc->addr, dev->vhost_hlen);
	PRINT_PACKET(dev, (uintptr_t)desc_addr, dev->vhost_hlen, 0);

	desc_offset = dev->vhost_hlen;
	desc_avail  = desc->len - dev->vhost_hlen;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current mbuf, fetch next */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		/* done with current desc buf, fetch next */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0) {
				/* Room in vring buffer is not enough */
				error = -1;
				goto out;
			}
			if (unlikely(desc->next >= size || ++nr_desc > size)) {
				error = -1;
				goto out;
			}

			desc = &descs[desc->next];
			desc_addr = vhost_iova_to_vva(dev, vq, desc->addr,
							desc->len,
							VHOST_ACCESS_RW);
			if (unlikely(!desc_addr)) {
				error = -1;
				goto out;
			}

			desc_offset = 0;
			desc_avail  = desc->len;
		}

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);
		if (likely(cpy_len > MAX_BATCH_LEN || copy_nb >= vq->size)) {
			rte_memcpy((void *)((uintptr_t)(desc_addr +
							desc_offset)),
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
				cpy_len);
			vhost_log_write(dev, desc->addr + desc_offset, cpy_len);
			PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset),
				     cpy_len, 0);
		} else {
			batch_copy[copy_nb].dst =
				(void *)((uintptr_t)(desc_addr + desc_offset));
			batch_copy[copy_nb].src =
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset);
			batch_copy[copy_nb].log_addr = desc->addr + desc_offset;
			batch_copy[copy_nb].len = cpy_len;
			copy_nb++;
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
	}

out:
	vq->batch_copy_nb_elems = copy_nb;

	return error;
}

/**
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtio device. A packet
 * count is returned to indicate the number of packets that are successfully
 * added to the RX queue. This function works when the mbuf is scattered, but
 * it doesn't support the mergeable feature.
 */
static __rte_always_inline uint32_t
virtio_dev_rx(struct virtio_net *dev, uint16_t queue_id,
	      struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint16_t avail_idx, free_entries, start_idx;
	uint16_t desc_indexes[MAX_PKT_BURST];
	struct vring_desc *descs;
	uint16_t used_idx;
	uint32_t i, sz;

	//队列合法性检查
	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	//取出队列
	vq = dev->virtqueue[queue_id];

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(vq->access_ok == 0)) {
		if (unlikely(vring_translate(dev, vq) < 0)) {
			count = 0;
			goto out;
		}
	}

	//选择本次可以发送多少个报文（受用户要发送的包数，buffer中可用的空间,
	//本函数数组最大长度）
	avail_idx = *((volatile uint16_t *)&vq->avail->idx);
	start_idx = vq->last_used_idx;
	free_entries = avail_idx - start_idx;
	count = RTE_MIN(count, free_entries);
	count = RTE_MIN(count, (uint32_t)MAX_PKT_BURST);
	if (count == 0)
		//只能发送0个，退出
		goto out;

	LOG_DEBUG(VHOST_DATA, "(%d) start_idx %d | end_idx %d\n",
		dev->vid, start_idx, start_idx + count);

	vq->batch_copy_nb_elems = 0;

	/* Retrieve all of the desc indexes first to avoid caching issues. */
	//预取描述符idx(可预取32个）
	rte_prefetch0(&vq->avail->ring[start_idx & (vq->size - 1)]);

	//填充used->ring
	for (i = 0; i < count; i++) {
		used_idx = (start_idx + i) & (vq->size - 1);
		desc_indexes[i] = vq->avail->ring[used_idx];
		vq->used->ring[used_idx].id = desc_indexes[i];
		vq->used->ring[used_idx].len = pkts[i]->pkt_len +
					       dev->vhost_hlen;//报文长度+vhost_hlen
		vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[used_idx]),
			sizeof(vq->used->ring[used_idx]));
	}

	//预取描述符
	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		uint16_t desc_idx = desc_indexes[i];
		int err;

		//描述符要求按list写
		if (vq->desc[desc_idx].flags & VRING_DESC_F_INDIRECT) {
			descs = (struct vring_desc *)(uintptr_t)
				vhost_iova_to_vva(dev,
						vq, vq->desc[desc_idx].addr,
						vq->desc[desc_idx].len,
						VHOST_ACCESS_RO);
			if (unlikely(!descs)) {
				count = i;
				break;
			}

			desc_idx = 0;
			sz = vq->desc[desc_idx].len / sizeof(*descs);
		} else {
			descs = vq->desc;
			sz = vq->size;
		}

		err = copy_mbuf_to_desc(dev, vq, descs, pkts[i], desc_idx, sz);
		if (unlikely(err)) {
			count = i;
			break;
		}

		if (i + 1 < count)
			rte_prefetch0(&vq->desc[desc_indexes[i+1]]);
	}

	do_data_copy_enqueue(dev, vq);

	rte_smp_wmb();

	*(volatile uint16_t *)&vq->used->idx += count;
	vq->last_used_idx += count;
	vhost_log_used_vring(dev, vq,
		offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));

	vhost_vring_call(dev, vq);
out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	return count;
}

static __rte_always_inline int
fill_vec_buf(struct virtio_net *dev, struct vhost_virtqueue *vq,
			 uint32_t avail_idx, uint32_t *vec_idx,
			 struct buf_vector *buf_vec, uint16_t *desc_chain_head,
			 uint16_t *desc_chain_len)
{
	uint16_t idx = vq->avail->ring[avail_idx & (vq->size - 1)];
	uint32_t vec_id = *vec_idx;
	uint32_t len    = 0;
	struct vring_desc *descs = vq->desc;

	*desc_chain_head = idx;

	if (vq->desc[idx].flags & VRING_DESC_F_INDIRECT) {
		descs = (struct vring_desc *)(uintptr_t)
			vhost_iova_to_vva(dev, vq, vq->desc[idx].addr,
						vq->desc[idx].len,
						VHOST_ACCESS_RO);
		if (unlikely(!descs))
			return -1;

		idx = 0;
	}

	while (1) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX || idx >= vq->size))
			return -1;

		len += descs[idx].len;
		buf_vec[vec_id].buf_addr = descs[idx].addr;
		buf_vec[vec_id].buf_len  = descs[idx].len;
		buf_vec[vec_id].desc_idx = idx;
		vec_id++;

		if ((descs[idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		idx = descs[idx].next;
	}

	*desc_chain_len = len;
	*vec_idx = vec_id;

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_mergeable(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint32_t size, struct buf_vector *buf_vec,
				uint16_t *num_buffers, uint16_t avail_head)
{
	uint16_t cur_idx;
	uint32_t vec_idx = 0;
	uint16_t tries = 0;

	uint16_t head_idx = 0;
	uint16_t len = 0;

	*num_buffers = 0;
	cur_idx  = vq->last_avail_idx;

	while (size > 0) {
		if (unlikely(cur_idx == avail_head))
			return -1;

		if (unlikely(fill_vec_buf(dev, vq, cur_idx, &vec_idx, buf_vec,
						&head_idx, &len) < 0))
			return -1;
		len = RTE_MIN(len, size);
		update_shadow_used_ring(vq, head_idx, len);
		size -= len;

		cur_idx++;
		tries++;
		*num_buffers += 1;

		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(tries >= vq->size))
			return -1;
	}

	return 0;
}

static __rte_always_inline int
copy_mbuf_to_desc_mergeable(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    struct rte_mbuf *m, struct buf_vector *buf_vec,
			    uint16_t num_buffers)
{
	uint32_t vec_idx = 0;
	uint64_t desc_addr;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t desc_offset, desc_avail;
	uint32_t cpy_len;
	uint64_t hdr_addr, hdr_phys_addr;
	struct rte_mbuf *hdr_mbuf;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	uint16_t copy_nb = vq->batch_copy_nb_elems;
	int error = 0;

	if (unlikely(m == NULL)) {
		error = -1;
		goto out;
	}

	desc_addr = vhost_iova_to_vva(dev, vq, buf_vec[vec_idx].buf_addr,
						buf_vec[vec_idx].buf_len,
						VHOST_ACCESS_RW);
	if (buf_vec[vec_idx].buf_len < dev->vhost_hlen || !desc_addr) {
		error = -1;
		goto out;
	}

	hdr_mbuf = m;
	hdr_addr = desc_addr;
	hdr_phys_addr = buf_vec[vec_idx].buf_addr;
	rte_prefetch0((void *)(uintptr_t)hdr_addr);

	LOG_DEBUG(VHOST_DATA, "(%d) RX: num merge buffers %d\n",
		dev->vid, num_buffers);

	desc_avail  = buf_vec[vec_idx].buf_len - dev->vhost_hlen;
	desc_offset = dev->vhost_hlen;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current desc buf, get the next one */
		if (desc_avail == 0) {
			vec_idx++;
			desc_addr =
				vhost_iova_to_vva(dev, vq,
					buf_vec[vec_idx].buf_addr,
					buf_vec[vec_idx].buf_len,
					VHOST_ACCESS_RW);
			if (unlikely(!desc_addr)) {
				error = -1;
				goto out;
			}

			/* Prefetch buffer address. */
			rte_prefetch0((void *)(uintptr_t)desc_addr);
			desc_offset = 0;
			desc_avail  = buf_vec[vec_idx].buf_len;
		}

		/* done with current mbuf, get the next one */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		if (hdr_addr) {
			struct virtio_net_hdr_mrg_rxbuf *hdr;

			hdr = (struct virtio_net_hdr_mrg_rxbuf *)(uintptr_t)
				hdr_addr;
			virtio_enqueue_offload(hdr_mbuf, &hdr->hdr);
			ASSIGN_UNLESS_EQUAL(hdr->num_buffers, num_buffers);

			vhost_log_write(dev, hdr_phys_addr, dev->vhost_hlen);
			PRINT_PACKET(dev, (uintptr_t)hdr_addr,
				     dev->vhost_hlen, 0);

			hdr_addr = 0;
		}

		cpy_len = RTE_MIN(desc_avail, mbuf_avail);

		if (likely(cpy_len > MAX_BATCH_LEN || copy_nb >= vq->size)) {
			rte_memcpy((void *)((uintptr_t)(desc_addr +
							desc_offset)),
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
				cpy_len);
			vhost_log_write(dev,
				buf_vec[vec_idx].buf_addr + desc_offset,
				cpy_len);
			PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset),
				cpy_len, 0);
		} else {
			batch_copy[copy_nb].dst =
				(void *)((uintptr_t)(desc_addr + desc_offset));
			batch_copy[copy_nb].src =
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset);
			batch_copy[copy_nb].log_addr =
				buf_vec[vec_idx].buf_addr + desc_offset;
			batch_copy[copy_nb].len = cpy_len;
			copy_nb++;
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
	}

out:
	vq->batch_copy_nb_elems = copy_nb;

	return error;
}

static __rte_always_inline uint32_t
virtio_dev_merge_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t avail_head;

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	//取出指定queue
	vq = dev->virtqueue[queue_id];

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(vq->access_ok == 0))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	//最多一次收32个报文
	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);
	if (count == 0)
		goto out;

	vq->batch_copy_nb_elems = 0;

	//预取一个cache line
	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	vq->shadow_used_idx = 0;
	avail_head = *((volatile uint16_t *)&vq->avail->idx);
	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;

		if (unlikely(reserve_avail_buf_mergeable(dev, vq,
						pkt_len, buf_vec, &num_buffers,
						avail_head) < 0)) {
			LOG_DEBUG(VHOST_DATA,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		LOG_DEBUG(VHOST_DATA, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + num_buffers);

		if (copy_mbuf_to_desc_mergeable(dev, vq, pkts[pkt_idx],
						buf_vec, num_buffers) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		vq->last_avail_idx += num_buffers;
	}

	do_data_copy_enqueue(dev, vq);

	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring(dev, vq);
		vhost_vring_call(dev, vq);
	}

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	return pkt_idx;
}

//vhost报文入队处理（发送）
uint16_t
rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return 0;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	if (dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF))
		return virtio_dev_merge_rx(dev, queue_id, pkts, count);
	else
		return virtio_dev_rx(dev, queue_id, pkts, count);
}

static inline bool
virtio_net_with_host_offload(struct virtio_net *dev)
{
	if (dev->features &
			((1ULL << VIRTIO_NET_F_CSUM) |
			 (1ULL << VIRTIO_NET_F_HOST_ECN) |
			 (1ULL << VIRTIO_NET_F_HOST_TSO4) |
			 (1ULL << VIRTIO_NET_F_HOST_TSO6) |
			 (1ULL << VIRTIO_NET_F_HOST_UFO)))
		return true;

	return false;
}

static void
parse_ethernet(struct rte_mbuf *m, uint16_t *l4_proto, void **l4_hdr)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	void *l3_hdr = NULL;
	struct ether_hdr *eth_hdr;
	uint16_t ethertype;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	m->l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);

		m->l2_len += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	l3_hdr = (char *)eth_hdr + m->l2_len;

	switch (ethertype) {
	case ETHER_TYPE_IPv4:
		ipv4_hdr = l3_hdr;
		*l4_proto = ipv4_hdr->next_proto_id;
		m->l3_len = (ipv4_hdr->version_ihl & 0x0f) * 4;
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV4;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = l3_hdr;
		*l4_proto = ipv6_hdr->proto;
		m->l3_len = sizeof(struct ipv6_hdr);
		*l4_hdr = (char *)l3_hdr + m->l3_len;
		m->ol_flags |= PKT_TX_IPV6;
		break;
	default:
		m->l3_len = 0;
		*l4_proto = 0;
		*l4_hdr = NULL;
		break;
	}
}

static __rte_always_inline void
vhost_dequeue_offload(struct virtio_net_hdr *hdr, struct rte_mbuf *m)
{
	uint16_t l4_proto = 0;
	void *l4_hdr = NULL;
	struct tcp_hdr *tcp_hdr = NULL;

	if (hdr->flags == 0 && hdr->gso_type == VIRTIO_NET_HDR_GSO_NONE)
		//无offload flag,退出
		return;

	//解析4层协议，解析4层头
	parse_ethernet(m, &l4_proto, &l4_hdr);
	if (hdr->flags == VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		//checksum flags处理
		if (hdr->csum_start == (m->l2_len + m->l3_len)) {
			switch (hdr->csum_offset) {
			case (offsetof(struct tcp_hdr, cksum)):
				if (l4_proto == IPPROTO_TCP)
					m->ol_flags |= PKT_TX_TCP_CKSUM;
				break;
			case (offsetof(struct udp_hdr, dgram_cksum)):
				if (l4_proto == IPPROTO_UDP)
					m->ol_flags |= PKT_TX_UDP_CKSUM;
				break;
			case (offsetof(struct sctp_hdr, cksum)):
				if (l4_proto == IPPROTO_SCTP)
					m->ol_flags |= PKT_TX_SCTP_CKSUM;
				break;
			default:
				break;
			}
		}
	}

	//gso功能处理
	if (l4_hdr && hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			tcp_hdr = l4_hdr;
			m->ol_flags |= PKT_TX_TCP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = (tcp_hdr->data_off & 0xf0) >> 2;
			break;
		case VIRTIO_NET_HDR_GSO_UDP:
			m->ol_flags |= PKT_TX_UDP_SEG;
			m->tso_segsz = hdr->gso_size;
			m->l4_len = sizeof(struct udp_hdr);
			break;
		default:
			RTE_LOG(WARNING, VHOST_DATA,
				"unsupported gso type %u.\n", hdr->gso_type);
			break;
		}
	}
}

static __rte_always_inline void
put_zmbuf(struct zcopy_mbuf *zmbuf)
{
	zmbuf->in_use = 0;
}

//将描述符中指明的报文内存放置在mbuf中，目前支持两种放置方式:1。内存copy；2。zero copy
static __rte_always_inline int
copy_desc_to_mbuf(struct virtio_net *dev, struct vhost_virtqueue *vq,
		  struct vring_desc *descs, uint16_t max_desc,
		  struct rte_mbuf *m, uint16_t desc_idx,
		  struct rte_mempool *mbuf_pool)
{
	struct vring_desc *desc;
	uint64_t desc_addr;
	uint32_t desc_avail, desc_offset;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr *hdr = NULL;
	/* A counter to avoid desc dead loop chain */
	uint32_t nr_desc = 1;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	uint16_t copy_nb = vq->batch_copy_nb_elems;
	int error = 0;

	//取出desc_idx位置的描述信息
	desc = &descs[desc_idx];
	//描述长度明显小于vhost_hlen是无效报文
	if (unlikely((desc->len < dev->vhost_hlen)) ||
			(desc->flags & VRING_DESC_F_INDIRECT)) {
		error = -1;
		goto out;
	}

	//将对端的地址转换为自已的地址
	desc_addr = vhost_iova_to_vva(dev,
					vq, desc->addr,
					desc->len,
					VHOST_ACCESS_RO);
	if (unlikely(!desc_addr)) {
		error = -1;
		goto out;
	}

	//如果有offload，则预取virtio_net_hdr
	if (virtio_net_with_host_offload(dev)) {
		hdr = (struct virtio_net_hdr *)((uintptr_t)desc_addr);
		rte_prefetch0(hdr);
	}

	/*
	 * A virtio driver normally uses at least 2 desc buffers
	 * for Tx: the first for storing the header, and others
	 * for storing the data.
	 */
	//如上面所言，通常virtio驱动会至少使用2个desc buffer来做传输
	//检查其是否有next标记
	if (likely((desc->len == dev->vhost_hlen) &&
		   (desc->flags & VRING_DESC_F_NEXT) != 0)) {
		desc = &descs[desc->next];//按标记要求，使用下一个描述符
		if (unlikely(desc->flags & VRING_DESC_F_INDIRECT)) {
			error = -1;
			goto out;
		}

		//由于切换了desc，重新转换desc地址
		desc_addr = vhost_iova_to_vva(dev,
							vq, desc->addr,
							desc->len,
							VHOST_ACCESS_RO);
		if (unlikely(!desc_addr)) {
			error = -1;
			goto out;
		}

		desc_offset = 0;
		desc_avail  = desc->len;
		nr_desc    += 1;
	} else {
		desc_avail  = desc->len - dev->vhost_hlen;
		desc_offset = dev->vhost_hlen;
	}

	rte_prefetch0((void *)(uintptr_t)(desc_addr + desc_offset));

	PRINT_PACKET(dev, (uintptr_t)(desc_addr + desc_offset), desc_avail, 0);

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		uint64_t hpa;

		//mbuf有一个自已的长度，对方送过来的报文有一个自已的长度
		//如果要把对方的报文放入mbuf，那个copy的长度只能是两者中最小的一个。
		cpy_len = RTE_MIN(desc_avail, mbuf_avail);

		/*
		 * A desc buf might across two host physical pages that are
		 * not continuous. In such case (gpa_to_hpa returns 0), data
		 * will be copied even though zero copy is enabled.
		 */
		//如果开启了zero copy,直接让mbuf指向这段内存就可以了
		if (unlikely(dev->dequeue_zero_copy && (hpa = gpa_to_hpa(dev,
					desc->addr + desc_offset, cpy_len)))) {
			cur->data_len = cpy_len;
			cur->data_off = 0;
			//报文指向这一段内存
			cur->buf_addr = (void *)(uintptr_t)(desc_addr
				+ desc_offset);
			cur->buf_iova = hpa;

			/*
			 * In zero copy mode, one mbuf can only reference data
			 * for one or partial of one desc buff.
			 */
			mbuf_avail = cpy_len;
		} else {
			if (likely(cpy_len > MAX_BATCH_LEN ||
				   copy_nb >= vq->size ||
				   (hdr && cur == m))) {
				rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
								   mbuf_offset),
					   (void *)((uintptr_t)(desc_addr +
								desc_offset)),
					   cpy_len);
			} else {
				batch_copy[copy_nb].dst =
					rte_pktmbuf_mtod_offset(cur, void *,
								mbuf_offset);
				batch_copy[copy_nb].src =
					(void *)((uintptr_t)(desc_addr +
							     desc_offset));
				batch_copy[copy_nb].len = cpy_len;
				copy_nb++;
			}
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;//desc中还有多少字节没有放入到mbuf
		desc_offset += cpy_len;

		/* This desc reaches to its end, get the next one */
		//当desc_avail为0时，说明所有字节均已放入到mbuf中了，可以处理下一个desc了
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0)
				//这个desc没有指明有下一个，处理结束，退出循环
				break;

			//走到这里时，desc指明还有一个新的段需要收取，这时极有可能是遇着巨帧报文了？
			if (unlikely(desc->next >= max_desc ||
				     ++nr_desc > max_desc)) {
				//对方发送协议处理有误，出错处理
				error = -1;
				goto out;
			}
			//按要求取下一个描述符
			desc = &descs[desc->next];
			//不支持再有下一个描述符的情况
			if (unlikely(desc->flags & VRING_DESC_F_INDIRECT)) {
				error = -1;
				goto out;
			}

			desc_addr = vhost_iova_to_vva(dev,
							vq, desc->addr,
							desc->len,
							VHOST_ACCESS_RO);
			if (unlikely(!desc_addr)) {
				error = -1;
				goto out;
			}

			rte_prefetch0((void *)(uintptr_t)desc_addr);

			desc_offset = 0;
			desc_avail  = desc->len;

			PRINT_PACKET(dev, (uintptr_t)desc_addr, desc->len, 0);
		}

		/*
		 * This mbuf reaches to its end, get a new one
		 * to hold more data.
		 */
		//当这种情况发生时，mbuf的缓冲不够用了，为了缓存此报文需要申请mbuf
		if (mbuf_avail == 0) {
			cur = rte_pktmbuf_alloc(mbuf_pool);
			if (unlikely(cur == NULL)) {
				RTE_LOG(ERR, VHOST_DATA, "Failed to "
					"allocate memory for mbuf.\n");
				error = -1;
				goto out;
			}
			if (unlikely(dev->dequeue_zero_copy))
				rte_mbuf_refcnt_update(cur, 1);

			prev->next = cur;//串在mbuf的next链上
			prev->data_len = mbuf_offset;
			m->nb_segs += 1;//段数量增加
			m->pkt_len += mbuf_offset;
			prev = cur;

			mbuf_offset = 0;
			mbuf_avail  = cur->buf_len - RTE_PKTMBUF_HEADROOM;
		}
	}

	prev->data_len = mbuf_offset;
	m->pkt_len    += mbuf_offset;

	if (hdr)
		vhost_dequeue_offload(hdr, m);

out:
	vq->batch_copy_nb_elems = copy_nb;

	return error;
}

static __rte_always_inline void
update_used_ring(struct virtio_net *dev, struct vhost_virtqueue *vq,
		 uint32_t used_idx, uint32_t desc_idx)
{
	vq->used->ring[used_idx].id  = desc_idx;
	vq->used->ring[used_idx].len = 0;
	vhost_log_used_vring(dev, vq,
			offsetof(struct vring_used, ring[used_idx]),
			sizeof(vq->used->ring[used_idx]));
}

static __rte_always_inline void
update_used_idx(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint32_t count)
{
	if (unlikely(count == 0))
		return;

	rte_smp_wmb();
	rte_smp_rmb();

	vq->used->idx += count;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
			sizeof(vq->used->idx));
	vhost_vring_call(dev, vq);
}

static __rte_always_inline struct zcopy_mbuf *
get_zmbuf(struct vhost_virtqueue *vq)
{
	uint16_t i;
	uint16_t last;
	int tries = 0;

	/* search [last_zmbuf_idx, zmbuf_size) */
	i = vq->last_zmbuf_idx;
	last = vq->zmbuf_size;

again:
	for (; i < last; i++) {
		if (vq->zmbufs[i].in_use == 0) {
			vq->last_zmbuf_idx = i + 1;
			vq->zmbufs[i].in_use = 1;
			return &vq->zmbufs[i];
		}
	}

	tries++;
	if (tries == 1) {
		/* search [0, last_zmbuf_idx) */
		i = 0;
		last = vq->last_zmbuf_idx;
		goto again;
	}

	return NULL;
}

static __rte_always_inline bool
mbuf_is_consumed(struct rte_mbuf *m)
{
	while (m) {
		if (rte_mbuf_refcnt_read(m) > 1)
			return false;
		m = m->next;
	}

	return true;
}

static __rte_always_inline void
restore_mbuf(struct rte_mbuf *m)
{
	uint32_t mbuf_size, priv_size;

	while (m) {
		priv_size = rte_pktmbuf_priv_size(m->pool);
		mbuf_size = sizeof(struct rte_mbuf) + priv_size;
		/* start of buffer is after mbuf structure and priv data */

		m->buf_addr = (char *)m + mbuf_size;
		m->buf_iova = rte_mempool_virt2iova(m) + mbuf_size;
		m = m->next;
	}
}

//给定队列id后，首先检查队列中是否有内容，队列里存放的是描述符索引
//通过出队，我们可以拿到描述符索引（desc_indexes)，然后取出描述符
//(描述符，指出报文存放的首地址，报文的长度，标记位，报文过长时会被分片的下一片）
//再由描述符定位到实际的报文

uint16_t
rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev;
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;
	uint32_t desc_indexes[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i = 0;
	uint16_t free_entries;
	uint16_t avail_idx;

	dev = get_device(vid);//找到对应的virtio_net
	if (!dev)
		return 0;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	//检查队列是否有效，类型及索引
	//rx队列属于奇数
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	//找到虚拟队列，若未启用，则返回0
	vq = dev->virtqueue[queue_id];

	if (unlikely(rte_spinlock_trylock(&vq->access_lock) == 0))
		return 0;

	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	vq->batch_copy_nb_elems = 0;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	if (unlikely(vq->access_ok == 0))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	if (unlikely(dev->dequeue_zero_copy)) {
		struct zcopy_mbuf *zmbuf, *next;
		int nr_updated = 0;

		for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
		     zmbuf != NULL; zmbuf = next) {
			next = TAILQ_NEXT(zmbuf, next);

			if (mbuf_is_consumed(zmbuf->mbuf)) {
				used_idx = vq->last_used_idx++ & (vq->size - 1);
				update_used_ring(dev, vq, used_idx,
						 zmbuf->desc_idx);
				nr_updated += 1;

				TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
				restore_mbuf(zmbuf->mbuf);
				rte_pktmbuf_free(zmbuf->mbuf);
				put_zmbuf(zmbuf);
				vq->nr_zmbuf -= 1;
			}
		}

		update_used_idx(dev, vq, nr_updated);
	}

	/*
	 * Construct a RARP broadcast packet, and inject it to the "pkts"
	 * array, to looks like that guest actually send such packet.
	 *
	 * Check user_send_rarp() for more information.
	 *
	 * broadcast_rarp shares a cacheline in the virtio_net structure
	 * with some fields that are accessed during enqueue and
	 * rte_atomic16_cmpset() causes a write if using cmpxchg. This could
	 * result in false sharing between enqueue and dequeue.
	 *
	 * Prevent unnecessary false sharing by reading broadcast_rarp first
	 * and only performing cmpset if the read indicates it is likely to
	 * be set.
	 */

	if (unlikely(rte_atomic16_read(&dev->broadcast_rarp) &&
			rte_atomic16_cmpset((volatile uint16_t *)
				&dev->broadcast_rarp.cnt, 1, 0))) {

		rarp_mbuf = rte_net_make_rarp_packet(mbuf_pool, &dev->mac);
		if (rarp_mbuf == NULL) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to make RARP packet.\n");
			return 0;
		}
		count -= 1;
	}

	//获取有多少实体可收取
	free_entries = *((volatile uint16_t *)&vq->avail->idx) -
			vq->last_avail_idx;
	if (free_entries == 0)
		goto out;//没有报文

	LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);

	/* Prefetch available and used ring */
	avail_idx = vq->last_avail_idx & (vq->size - 1);//队列中的有效位置
	used_idx  = vq->last_used_idx  & (vq->size - 1);
	rte_prefetch0(&vq->avail->ring[avail_idx]);
	rte_prefetch0(&vq->used->ring[used_idx]);

	//最多只能收到三者的最小值，求出需要收取多少个
	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);
	LOG_DEBUG(VHOST_DATA, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	/* Retrieve all of the head indexes first to avoid caching issues. */
	for (i = 0; i < count; i++) {
		avail_idx = (vq->last_avail_idx + i) & (vq->size - 1);
		used_idx  = (vq->last_used_idx  + i) & (vq->size - 1);
		desc_indexes[i] = vq->avail->ring[avail_idx];//读取描述符索引

		//将读取到的索引存入vq->used->ring中
		if (likely(dev->dequeue_zero_copy == 0))
			update_used_ring(dev, vq, used_idx, desc_indexes[i]);
	}

	/* Prefetch descriptor index. */
	//预取描述符索引（一个cache line,即可以预取4个desc)
	rte_prefetch0(&vq->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		struct vring_desc *desc;
		uint16_t sz, idx;
		int err;

		if (likely(i + 1 < count))
			//提前预取后面的
			rte_prefetch0(&vq->desc[desc_indexes[i + 1]]);

		//如果此分支进入，则接下来是一个list段
		if (vq->desc[desc_indexes[i]].flags & VRING_DESC_F_INDIRECT) {
			//取出vhost虚拟地址指向的描述信息
			desc = (struct vring_desc *)(uintptr_t)
				vhost_iova_to_vva(dev, vq,
						vq->desc[desc_indexes[i]].addr,
						sizeof(*desc),
						VHOST_ACCESS_RO);
			if (unlikely(!desc))
				break;

			rte_prefetch0(desc);
			sz = vq->desc[desc_indexes[i]].len / sizeof(*desc);//段的长度
			idx = 0;
		} else {
			desc = vq->desc;
			sz = vq->size;
			idx = desc_indexes[i];
		}

		//申请mbuf
		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}

		//copy描述信息到pkts
		err = copy_desc_to_mbuf(dev, vq, desc, sz, pkts[i], idx,
					mbuf_pool);
		if (unlikely(err)) {
			rte_pktmbuf_free(pkts[i]);
			break;
		}

		if (unlikely(dev->dequeue_zero_copy)) {
			struct zcopy_mbuf *zmbuf;

			zmbuf = get_zmbuf(vq);
			if (!zmbuf) {
				rte_pktmbuf_free(pkts[i]);
				break;
			}
			zmbuf->mbuf = pkts[i];
			zmbuf->desc_idx = desc_indexes[i];

			/*
			 * Pin lock the mbuf; we will check later to see
			 * whether the mbuf is freed (when we are the last
			 * user) or not. If that's the case, we then could
			 * update the used ring safely.
			 */
			rte_mbuf_refcnt_update(pkts[i], 1);

			vq->nr_zmbuf += 1;
			TAILQ_INSERT_TAIL(&vq->zmbuf_list, zmbuf, next);
		}
	}
	vq->last_avail_idx += i;//记录本次收到那个位置了

	if (likely(dev->dequeue_zero_copy == 0)) {
		do_data_copy_dequeue(vq);
		vq->last_used_idx += i;
		update_used_idx(dev, vq, i);
	}

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	if (unlikely(rarp_mbuf != NULL)) {
		/*
		 * Inject it to the head of "pkts" array, so that switch's mac
		 * learning table will get updated first.
		 */
		memmove(&pkts[1], pkts, i * sizeof(struct rte_mbuf *));
		pkts[0] = rarp_mbuf;
		i += 1;
	}

	return i;
}
