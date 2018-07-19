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
#include <rte_malloc.h>

#include "iotlb.h"
#include "vhost.h"

#define MAX_PKT_BURST 32

#define MAX_BATCH_LEN 256

static  __rte_always_inline bool
rxvq_is_mergeable(struct virtio_net *dev)
{
	return dev->features & (1ULL << VIRTIO_NET_F_MRG_RXBUF);
}

static bool
is_valid_virt_queue_idx(uint32_t idx, int is_tx, uint32_t nr_vring)
{
	//首先队列的idx必须是小于nr_vring的，其次，如果is_tx是发队列，则idx &1 必须为1(即必须为奇数）
	return (is_tx ^ (idx & 1)) == 0 && idx < nr_vring;
}

static __rte_always_inline void *
alloc_copy_ind_table(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t desc_addr, uint64_t desc_len)
{
	void *idesc;
	uint64_t src, dst;
	uint64_t len, remain = desc_len;

	idesc = rte_malloc(__func__, desc_len, 0);
	if (unlikely(!idesc))
		return 0;

	dst = (uint64_t)(uintptr_t)idesc;

	while (remain) {
		len = remain;
		src = vhost_iova_to_vva(dev, vq, desc_addr, &len,
				VHOST_ACCESS_RO);
		if (unlikely(!src || !len)) {
			rte_free(idesc);
			return 0;
		}

		rte_memcpy((void *)(uintptr_t)dst, (void *)(uintptr_t)src, len);

		remain -= len;
		dst += len;
		desc_addr += len;
	}

	return idesc;
}

static __rte_always_inline void
free_ind_table(void *idesc)
{
	rte_free(idesc);
}

static __rte_always_inline void
do_flush_shadow_used_ring_split(struct virtio_net *dev,
			struct vhost_virtqueue *vq,
			uint16_t to, uint16_t from, uint16_t size)
{
	//将shadow_used_split中的信息更新到used中
	rte_memcpy(&vq->used->ring[to],
			&vq->shadow_used_split[from],
			size * sizeof(struct vring_used_elem));
	vhost_log_cache_used_vring(dev, vq,
			offsetof(struct vring_used, ring[to]),
			size * sizeof(struct vring_used_elem));
}

static __rte_always_inline void
flush_shadow_used_ring_split(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint16_t used_idx = vq->last_used_idx & (vq->size - 1);

	if (used_idx + vq->shadow_used_idx <= vq->size) {
		do_flush_shadow_used_ring_split(dev, vq, used_idx, 0,
					  vq->shadow_used_idx);
	} else {
		uint16_t size;

		/* update used ring interval [used_idx, vq->size] */
		size = vq->size - used_idx;
		do_flush_shadow_used_ring_split(dev, vq, used_idx, 0, size);

		/* update the left half used ring interval [0, left_size] */
		do_flush_shadow_used_ring_split(dev, vq, 0, size,
					  vq->shadow_used_idx - size);
	}
	vq->last_used_idx += vq->shadow_used_idx;

	rte_smp_wmb();

	vhost_log_cache_sync(dev, vq);

	*(volatile uint16_t *)&vq->used->idx += vq->shadow_used_idx;
	vq->shadow_used_idx = 0;
	vhost_log_used_vring(dev, vq, offsetof(struct vring_used, idx),
		sizeof(vq->used->idx));
}

//记录哪些描述符已被使用完成
static __rte_always_inline void
update_shadow_used_ring_split(struct vhost_virtqueue *vq,
			 uint16_t desc_idx, uint16_t len)
{
	uint16_t i = vq->shadow_used_idx++;

	vq->shadow_used_split[i].id  = desc_idx;
	vq->shadow_used_split[i].len = len;
}

static __rte_always_inline void
flush_shadow_used_ring_packed(struct virtio_net *dev,
			struct vhost_virtqueue *vq)
{
	int i;
	uint16_t used_idx = vq->last_used_idx;

	/* Split loop in two to save memory barriers */
	for (i = 0; i < vq->shadow_used_idx; i++) {
		vq->desc_packed[used_idx].id = vq->shadow_used_packed[i].id;
		vq->desc_packed[used_idx].len = vq->shadow_used_packed[i].len;

		used_idx += vq->shadow_used_packed[i].count;
		if (used_idx >= vq->size)
			used_idx -= vq->size;
	}

	rte_smp_wmb();

	for (i = 0; i < vq->shadow_used_idx; i++) {
		uint16_t flags;

		if (vq->shadow_used_packed[i].len)
			flags = VRING_DESC_F_WRITE;
		else
			flags = 0;

		if (vq->used_wrap_counter) {
			flags |= VRING_DESC_F_USED;
			flags |= VRING_DESC_F_AVAIL;
		} else {
			flags &= ~VRING_DESC_F_USED;
			flags &= ~VRING_DESC_F_AVAIL;
		}

		vq->desc_packed[vq->last_used_idx].flags = flags;

		vhost_log_cache_used_vring(dev, vq,
					vq->last_used_idx *
					sizeof(struct vring_packed_desc),
					sizeof(struct vring_packed_desc));

		vq->last_used_idx += vq->shadow_used_packed[i].count;
		if (vq->last_used_idx >= vq->size) {
			vq->used_wrap_counter ^= 1;
			vq->last_used_idx -= vq->size;
		}
	}

	rte_smp_wmb();
	vq->shadow_used_idx = 0;
	vhost_log_cache_sync(dev, vq);
}

static __rte_always_inline void
update_shadow_used_ring_packed(struct vhost_virtqueue *vq,
			 uint16_t desc_idx, uint16_t len, uint16_t count)
{
	uint16_t i = vq->shadow_used_idx++;

	vq->shadow_used_packed[i].id  = desc_idx;
	vq->shadow_used_packed[i].len = len;
	vq->shadow_used_packed[i].count = count;
}

static inline void
do_data_copy_enqueue(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++) {
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);
		vhost_log_cache_write(dev, vq, elem[i].log_addr, elem[i].len);
		PRINT_PACKET(dev, (uintptr_t)elem[i].dst, elem[i].len, 0);
	}

	vq->batch_copy_nb_elems = 0;
}

//实现batch信息的copy
static inline void
do_data_copy_dequeue(struct vhost_virtqueue *vq)
{
	struct batch_copy_elem *elem = vq->batch_copy_elems;
	uint16_t count = vq->batch_copy_nb_elems;
	int i;

	for (i = 0; i < count; i++)
		rte_memcpy(elem[i].dst, elem[i].src, elem[i].len);

	vq->batch_copy_nb_elems = 0;
}

/* avoid write operation when necessary, to lessen cache issues */
#define ASSIGN_UNLESS_EQUAL(var, val) do {	\
	if ((var) != (val))			\
		(var) = (val);			\
} while (0)

//offload参数转换
static __rte_always_inline void
virtio_enqueue_offload(struct rte_mbuf *m_buf, struct virtio_net_hdr *net_hdr)
{
	//入队时，将mbuf上的标记，转到net_hdr中
	uint64_t csum_l4 = m_buf->ol_flags & PKT_TX_L4_MASK;

	//如果支持tso,则需要计算tcp checksum，准备checksum参数（4层checksum计算起始位置）
	if (m_buf->ol_flags & PKT_TX_TCP_SEG)
		csum_l4 |= PKT_TX_TCP_CKSUM;

	if (csum_l4) {
		net_hdr->flags = VIRTIO_NET_HDR_F_NEEDS_CSUM;
		net_hdr->csum_start = m_buf->l2_len + m_buf->l3_len;

		//记录到checksum字段的offset
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
		//硬件计处算checksum被disable,清0
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_start, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->csum_offset, 0);
		ASSIGN_UNLESS_EQUAL(net_hdr->flags, 0);
	}

	/* IP cksum verification cannot be bypassed, then calculate here */
	if (m_buf->ol_flags & PKT_TX_IP_CKSUM) {
		//如果mbuf中指明需要做checksum offload,则此处需要计算ip层checksum
		struct ipv4_hdr *ipv4_hdr;

		ipv4_hdr = rte_pktmbuf_mtod_offset(m_buf, struct ipv4_hdr *,
						   m_buf->l2_len);
		ipv4_hdr->hdr_checksum = rte_ipv4_cksum(ipv4_hdr);//计算ipv4头部checksum
	}

	//设置gso类型及其相关参数，例如gso大小，头部长度
	if (m_buf->ol_flags & PKT_TX_TCP_SEG) {
		if (m_buf->ol_flags & PKT_TX_IPV4)
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV4;//tso
		else
			net_hdr->gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
		net_hdr->gso_size = m_buf->tso_segsz;//设置分的块大小
		net_hdr->hdr_len = m_buf->l2_len + m_buf->l3_len
					+ m_buf->l4_len;//指定4层头
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

//将数据指针填充到到buf_vec中，vec_idx用于记录有多少个vector被占用
//由于数据在物理内存上可能是不连续的，故会占用多个buf_vec
static __rte_always_inline int
map_one_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
		struct buf_vector *buf_vec, uint16_t *vec_idx,
		uint64_t desc_iova, uint64_t desc_len, uint8_t perm)
{
	uint16_t vec_id = *vec_idx;

	//desc_iova指向的内存物理上可能是不连续的
	while (desc_len) {
		uint64_t desc_addr;
		uint64_t desc_chunck_len = desc_len;

		if (unlikely(vec_id >= BUF_VECTOR_MAX))
			return -1;

		//取描述符中buffer的虚拟地址
		desc_addr = vhost_iova_to_vva(dev, vq,
				desc_iova,
				&desc_chunck_len,
				perm);
		if (unlikely(!desc_addr))
			return -1;

		buf_vec[vec_id].buf_iova = desc_iova;
		buf_vec[vec_id].buf_addr = desc_addr;
		buf_vec[vec_id].buf_len  = desc_chunck_len;//buffer长度

		desc_len -= desc_chunck_len;
		desc_iova += desc_chunck_len;
		vec_id++;
	}
	*vec_idx = vec_id;

	return 0;
}

//将avail_idex描述符指出的数据地址填充到出参buf_vec中（需要考虑地址转换，需要考虑物理地址不连续）
//出参nr_vec用于指出buf_vec中占用的数量
//出参desc_chain_head为首个描述符索引
//出参desc_chain_len为描述符链的buffer长度
//perm为io地址转换时的权限位
static __rte_always_inline int
fill_vec_buf_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
			 uint32_t avail_idx, uint16_t *vec_idx,
			 struct buf_vector *buf_vec, uint16_t *desc_chain_head,
			 uint16_t *desc_chain_len, uint8_t perm)
{
	//取出有效索引位下记录的有效描述符索引
	uint16_t idx = vq->avail->ring[avail_idx & (vq->size - 1)];
	uint16_t vec_id = *vec_idx;
	uint32_t len    = 0;
	uint64_t dlen;
	struct vring_desc *descs = vq->desc;
	struct vring_desc *idesc = NULL;

	*desc_chain_head = idx;

	//如果此描述符是the buffer contains a list of buffer descriptors
	if (vq->desc[idx].flags & VRING_DESC_F_INDIRECT) {
		dlen = vq->desc[idx].len;
		descs = (struct vring_desc *)(uintptr_t)
			vhost_iova_to_vva(dev, vq, vq->desc[idx].addr,
						&dlen,
						VHOST_ACCESS_RO);
		if (unlikely(!descs))
			return -1;//描述地址为空，失败

		if (unlikely(dlen < vq->desc[idx].len)) {
			/*
			 * The indirect desc table is not contiguous
			 * in process VA space, we have to copy it.
			 */
			//dlen小于vq->desc[idx].len,说明存放描述符的内存物理上不连续
			//申请一段连续内存把描述符 copy出来
			idesc = alloc_copy_ind_table(dev, vq,
					vq->desc[idx].addr, vq->desc[idx].len);
			if (unlikely(!idesc))
				return -1;

			descs = idesc;
		}

		idx = 0;
	}

	while (1) {
		if (unlikely(idx >= vq->size)) {
			free_ind_table(idesc);
			return -1;
		}

		len += descs[idx].len;

		//转换descs[idx]中指向的内存到buf_vec中（转换为本端虚拟地址）
		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[idx].addr, descs[idx].len,
						perm))) {
			free_ind_table(idesc);
			return -1;
		}

		//检查此描述符是否有next
		if ((descs[idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		//有next，取下一个描述符索引，继续转换进buf_vec中
		idx = descs[idx].next;
	}

	*desc_chain_len = len;//buffer的长度
	*vec_idx = vec_id;//占用了多少个数据段

	if (unlikely(!!idesc))
		free_ind_table(idesc);//如果有申请idesc，则释放它

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint32_t size, struct buf_vector *buf_vec,
				uint16_t *num_buffers, uint16_t avail_head,
				uint16_t *nr_vec)
{
	uint16_t cur_idx;
	uint16_t vec_idx = 0;
	uint16_t max_tries, tries = 0;

	uint16_t head_idx = 0;
	uint16_t len = 0;

	*num_buffers = 0;
	cur_idx  = vq->last_avail_idx;

	if (rxvq_is_mergeable(dev))
		max_tries = vq->size;
	else
		max_tries = 1;

	while (size > 0) {
		if (unlikely(cur_idx == avail_head))
			return -1;

		if (unlikely(fill_vec_buf_split(dev, vq, cur_idx,
						&vec_idx, buf_vec,
						&head_idx, &len,
						VHOST_ACCESS_RW) < 0))
			return -1;
		len = RTE_MIN(len, size);
		update_shadow_used_ring_split(vq, head_idx, len);
		size -= len;

		cur_idx++;
		tries++;
		*num_buffers += 1;

		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(tries > max_tries))
			return -1;
	}

	*nr_vec = vec_idx;

	return 0;
}

static __rte_always_inline int
fill_vec_buf_packed_indirect(struct virtio_net *dev,
			struct vhost_virtqueue *vq,
			struct vring_packed_desc *desc, uint16_t *vec_idx,
			struct buf_vector *buf_vec, uint16_t *len, uint8_t perm)
{
	uint16_t i;
	uint32_t nr_descs;
	uint16_t vec_id = *vec_idx;
	uint64_t dlen;
	struct vring_packed_desc *descs, *idescs = NULL;

	dlen = desc->len;
	descs = (struct vring_packed_desc *)(uintptr_t)
		vhost_iova_to_vva(dev, vq, desc->addr, &dlen, VHOST_ACCESS_RO);
	if (unlikely(!descs))
		return -1;

	if (unlikely(dlen < desc->len)) {
		//不连续，需要copy
		/*
		 * The indirect desc table is not contiguous
		 * in process VA space, we have to copy it.
		 */
		idescs = alloc_copy_ind_table(dev, vq, desc->addr, desc->len);
		if (unlikely(!idescs))
			return -1;

		descs = idescs;
	}

	//取出对应的一组描述符
	nr_descs =  desc->len / sizeof(struct vring_packed_desc);
	if (unlikely(nr_descs >= vq->size)) {
		free_ind_table(idescs);
		return -1;
	}

	//与split不同，这组描述符是数组形式，遍历并直接copy
	for (i = 0; i < nr_descs; i++) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX)) {
			free_ind_table(idescs);
			return -1;
		}

		*len += descs[i].len;
		if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
						descs[i].addr, descs[i].len,
						perm)))
			return -1;
	}
	*vec_idx = vec_id;

	if (unlikely(!!idescs))
		free_ind_table(idescs);

	return 0;
}

//处理avail_idx指定的描述符，将其转换为buf_vec进行保存（实现qemu地址向dpdk虚地址的转换）
//desc_count用于表示有多少描述符被使用
static __rte_always_inline int
fill_vec_buf_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint16_t avail_idx, uint16_t *desc_count,
				struct buf_vector *buf_vec, uint16_t *vec_idx,
				uint16_t *buf_id, uint16_t *len, uint8_t perm)
{
	bool wrap_counter = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t vec_id = *vec_idx;

	if (avail_idx < vq->last_avail_idx)
		wrap_counter ^= 1;

	if (unlikely(!desc_is_avail(&descs[avail_idx], wrap_counter)))
		return -1;

	*desc_count = 0;

	while (1) {
		if (unlikely(vec_id >= BUF_VECTOR_MAX))
			return -1;//超出vector限制，报错

		*desc_count += 1;
		*buf_id = descs[avail_idx].id;

		//处理packed形式的indirect描述符形式（报文过大时占用多个描述符）
		if (descs[avail_idx].flags & VRING_DESC_F_INDIRECT) {
			if (unlikely(fill_vec_buf_packed_indirect(dev, vq,
							&descs[avail_idx],
							&vec_id, buf_vec,
							len, perm) < 0))
				return -1;
		} else {
			*len += descs[avail_idx].len;

			//填充buf_vec
			if (unlikely(map_one_desc(dev, vq, buf_vec, &vec_id,
							descs[avail_idx].addr,
							descs[avail_idx].len,
							perm)))
				return -1;
		}

		//此描述符对应的报文结束
		if ((descs[avail_idx].flags & VRING_DESC_F_NEXT) == 0)
			break;

		//切换到下一个idx
		if (++avail_idx >= vq->size) {
			avail_idx -= vq->size;
			wrap_counter ^= 1;
		}
	}

	*vec_idx = vec_id;

	return 0;
}

/*
 * Returns -1 on fail, 0 on success
 */
static inline int
reserve_avail_buf_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint32_t size, struct buf_vector *buf_vec,
				uint16_t *nr_vec, uint16_t *num_buffers,
				uint16_t *nr_descs)
{
	uint16_t avail_idx;
	uint16_t vec_idx = 0;
	uint16_t max_tries, tries = 0;

	uint16_t buf_id = 0;
	uint16_t len = 0;
	uint16_t desc_count;

	*num_buffers = 0;
	avail_idx = vq->last_avail_idx;

	if (rxvq_is_mergeable(dev))
		max_tries = vq->size;
	else
		max_tries = 1;

	while (size > 0) {
		if (unlikely(fill_vec_buf_packed(dev, vq,
						avail_idx, &desc_count,
						buf_vec, &vec_idx,
						&buf_id, &len,
						VHOST_ACCESS_RO) < 0))
			return -1;

		len = RTE_MIN(len, size);
		update_shadow_used_ring_packed(vq, buf_id, len, desc_count);
		size -= len;

		avail_idx += desc_count;
		if (avail_idx >= vq->size)
			avail_idx -= vq->size;

		*nr_descs += desc_count;
		tries++;
		*num_buffers += 1;

		/*
		 * if we tried all available ring items, and still
		 * can't get enough buf, it means something abnormal
		 * happened.
		 */
		if (unlikely(tries > max_tries))
			return -1;
	}

	*nr_vec = vec_idx;

	return 0;
}

static __rte_always_inline int
copy_mbuf_to_desc(struct virtio_net *dev, struct vhost_virtqueue *vq,
			    struct rte_mbuf *m, struct buf_vector *buf_vec,
			    uint16_t nr_vec, uint16_t num_buffers)
{
	uint32_t vec_idx = 0;
	uint32_t mbuf_offset, mbuf_avail;
	uint32_t buf_offset, buf_avail;
	uint64_t buf_addr, buf_iova, buf_len;
	uint32_t cpy_len;
	uint64_t hdr_addr;
	struct rte_mbuf *hdr_mbuf;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	struct virtio_net_hdr_mrg_rxbuf tmp_hdr, *hdr = NULL;
	int error = 0;

	if (unlikely(m == NULL)) {
		//mbuf为空，报错
		error = -1;
		goto out;
	}

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_iova = buf_vec[vec_idx].buf_iova;
	buf_len = buf_vec[vec_idx].buf_len;

	if (nr_vec > 1)
		//vector数量为1时，没办法预取
		rte_prefetch0((void *)(uintptr_t)buf_vec[1].buf_addr);

	if (unlikely(buf_len < dev->vhost_hlen && nr_vec <= 1)) {
		//空间不足以存在dev->vhost_hlen,报错
		error = -1;
		goto out;
	}

	hdr_mbuf = m;
	hdr_addr = buf_addr;
	if (unlikely(buf_len < dev->vhost_hlen))
		//首个desc空间不足以存放dev->vhost_hlen时，使hdr指向临时空间
		hdr = &tmp_hdr;
	else
		hdr = (struct virtio_net_hdr_mrg_rxbuf *)(uintptr_t)hdr_addr;

	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) RX: num merge buffers %d\n",
		dev->vid, num_buffers);

	if (unlikely(buf_len < dev->vhost_hlen)) {
		//描述符的长度不足vhost_len,先跳过buf_vec[vec_idx](第一个描述符）
		//后面再处理它
		buf_offset = dev->vhost_hlen - buf_len;
		vec_idx++;
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;
		buf_avail = buf_len - buf_offset;
	} else {
		buf_offset = dev->vhost_hlen;
		buf_avail = buf_len - dev->vhost_hlen;
	}

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current buf, get the next one */
		if (buf_avail == 0) {
			//此时当前描述符空间已使用完，切换到下一个空间
			vec_idx++;
			if (unlikely(vec_idx >= nr_vec)) {
				error = -1;
				goto out;//超限，报错
			}

			buf_addr = buf_vec[vec_idx].buf_addr;
			buf_iova = buf_vec[vec_idx].buf_iova;
			buf_len = buf_vec[vec_idx].buf_len;

			/* Prefetch next buffer address. */
			if (vec_idx + 1 < nr_vec)
				rte_prefetch0((void *)(uintptr_t)
						buf_vec[vec_idx + 1].buf_addr);
			buf_offset = 0;
			buf_avail  = buf_len;
		}

		/* done with current mbuf, get the next one */
		if (mbuf_avail == 0) {
			//此时mbuf中的空间已拷贝完成，切换下一个mbuf
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		if (hdr_addr) {
			virtio_enqueue_offload(hdr_mbuf, &hdr->hdr);
			if (rxvq_is_mergeable(dev))
				//如果启用mergeable,则设置num_buffers
				ASSIGN_UNLESS_EQUAL(hdr->num_buffers,
						num_buffers);

			if (unlikely(hdr == &tmp_hdr)) {
				//首段desc空间过小，不足以存放virtio_net_hdr_mrg_rxbuf
				uint64_t len;
				uint64_t remain = dev->vhost_hlen;//XXX 这块是个bug(如果remain使用dev->vhost_hlen,则必然copy超界）
				uint64_t src = (uint64_t)(uintptr_t)hdr, dst;
				uint64_t iova = buf_vec[0].buf_iova;
				uint16_t hdr_vec_idx = 0;

				while (remain) {
					len = remain;
					dst = buf_vec[hdr_vec_idx].buf_addr;
					rte_memcpy((void *)(uintptr_t)dst,
							(void *)(uintptr_t)src,
							len);

					PRINT_PACKET(dev, (uintptr_t)dst,
							(uint32_t)len, 0);
					vhost_log_cache_write(dev, vq,
							iova, len);

					remain -= len;
					iova += len;
					src += len;
					hdr_vec_idx++;
				}
			} else {
				PRINT_PACKET(dev, (uintptr_t)hdr_addr,
						dev->vhost_hlen, 0);
				vhost_log_cache_write(dev, vq,
						buf_vec[0].buf_iova,
						dev->vhost_hlen);
			}

			hdr_addr = 0;
		}

		cpy_len = RTE_MIN(buf_len, mbuf_avail);

		if (likely(cpy_len > MAX_BATCH_LEN ||
					vq->batch_copy_nb_elems >= vq->size)) {
			//直接将mbuf内容copy到描述符指向的空间
			rte_memcpy((void *)((uintptr_t)(buf_addr + buf_offset)),
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
				cpy_len);

			vhost_log_cache_write(dev, vq, buf_iova + buf_offset,
					cpy_len);
			PRINT_PACKET(dev, (uintptr_t)(buf_addr + buf_offset),
				cpy_len, 0);
		} else {
			//记录此次copy到batch_copy中
			batch_copy[vq->batch_copy_nb_elems].dst =
				(void *)((uintptr_t)(buf_addr + buf_offset));
			batch_copy[vq->batch_copy_nb_elems].src =
				rte_pktmbuf_mtod_offset(m, void *, mbuf_offset);
			batch_copy[vq->batch_copy_nb_elems].log_addr =
				buf_iova + buf_offset;
			batch_copy[vq->batch_copy_nb_elems].len = cpy_len;
			vq->batch_copy_nb_elems++;
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		buf_avail  -= cpy_len;
		buf_offset += cpy_len;
	}

out:

	return error;
}

//向virtio_net收包处理（vhost设备的发包处理）
//pkts要发送的报文
//count要发送的报文数
static __rte_always_inline uint32_t
virtio_dev_rx_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mbuf **pkts, uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];
	uint16_t avail_head;

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);
	avail_head = *((volatile uint16_t *)&vq->avail->idx);

	//要发送的报文
	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		//加头后的pkt_len
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;
		uint16_t nr_vec = 0;

		//预留足够的描述符，使用的描述符来源于avail,要存放pkt_len,出参num_buffers
		//标明我们将占用多少个avail描述符（可能会是链式的），nr_vec标明我们使用多少个描述符
		if (unlikely(reserve_avail_buf_split(dev, vq,
						pkt_len, buf_vec, &num_buffers,
						avail_head, &nr_vec) < 0)) {
			VHOST_LOG_DEBUG(VHOST_DATA,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		rte_prefetch0((void *)(uintptr_t)buf_vec[0].buf_addr);

		VHOST_LOG_DEBUG(VHOST_DATA, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + num_buffers);

		//将pkts[pkt_idx] mbuf的内容copy到描述符
		if (copy_mbuf_to_desc(dev, vq, pkts[pkt_idx],
						buf_vec, nr_vec,
						num_buffers) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		vq->last_avail_idx += num_buffers;
	}

	//完成batch copy
	do_data_copy_enqueue(dev, vq);

	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring_split(dev, vq);
		vhost_vring_call_split(dev, vq);
	}

	return pkt_idx;
}

static __rte_always_inline uint32_t
virtio_dev_rx_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mbuf **pkts, uint32_t count)
{
	uint32_t pkt_idx = 0;
	uint16_t num_buffers;
	struct buf_vector buf_vec[BUF_VECTOR_MAX];

	for (pkt_idx = 0; pkt_idx < count; pkt_idx++) {
		uint32_t pkt_len = pkts[pkt_idx]->pkt_len + dev->vhost_hlen;
		uint16_t nr_vec = 0;
		uint16_t nr_descs = 0;

		if (unlikely(reserve_avail_buf_packed(dev, vq,
						pkt_len, buf_vec, &nr_vec,
						&num_buffers, &nr_descs) < 0)) {
			VHOST_LOG_DEBUG(VHOST_DATA,
				"(%d) failed to get enough desc from vring\n",
				dev->vid);
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		rte_prefetch0((void *)(uintptr_t)buf_vec[0].buf_addr);

		VHOST_LOG_DEBUG(VHOST_DATA, "(%d) current index %d | end index %d\n",
			dev->vid, vq->last_avail_idx,
			vq->last_avail_idx + num_buffers);

		if (copy_mbuf_to_desc(dev, vq, pkts[pkt_idx],
						buf_vec, nr_vec,
						num_buffers) < 0) {
			vq->shadow_used_idx -= num_buffers;
			break;
		}

		vq->last_avail_idx += nr_descs;
		if (vq->last_avail_idx >= vq->size) {
			vq->last_avail_idx -= vq->size;
			vq->avail_wrap_counter ^= 1;
		}
	}

	do_data_copy_enqueue(dev, vq);

	if (likely(vq->shadow_used_idx)) {
		flush_shadow_used_ring_packed(dev, vq);
		vhost_vring_call_packed(dev, vq);
	}

	return pkt_idx;
}

//vhost报文入队处理（发送）
//向接口vid发送数量为count个的报文pkts，要求报文存入queue_id队列
static __rte_always_inline uint32_t
virtio_dev_rx(struct virtio_net *dev, uint16_t queue_id,
	struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_virtqueue *vq;

	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);
	//队列合法性检查，不能是收队列，不能超过合法队列数目
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 0, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	rte_spinlock_lock(&vq->access_lock);

	//队列未使能不处理
	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	//队列目前还不能访问，尝试对队列进行地址转换
	if (unlikely(vq->access_ok == 0))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	count = RTE_MIN((uint32_t)MAX_PKT_BURST, count);
	if (count == 0)
		goto out;

	//报文发送(virtio设备收包处理）
	if (vq_is_packed(dev))
		count = virtio_dev_rx_packed(dev, vq, pkts, count);
	else
		count = virtio_dev_rx_split(dev, vq, pkts, count);

out:
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_unlock(vq);

out_access_unlock:
	rte_spinlock_unlock(&vq->access_lock);

	return count;
}

//vhost实现报文发送
uint16_t
rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count)
{
	//获取对应的virtio设备
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return 0;

	//如果禁用了内建的virtio_net，则直接返回
	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	//向virtio_net设备的queue_id队列发送count个报文
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

//解析报文，填充mbuf的
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
		//checksum flags处理，打上相关标记位，交给？？？处理
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
		  struct buf_vector *buf_vec, uint16_t nr_vec,
		  struct rte_mbuf *m, struct rte_mempool *mbuf_pool)
{
	uint32_t buf_avail, buf_offset;
	uint64_t buf_addr, buf_iova, buf_len;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	struct virtio_net_hdr tmp_hdr;
	struct virtio_net_hdr *hdr = NULL;
	/* A counter to avoid desc dead loop chain */
	uint16_t vec_idx = 0;
	struct batch_copy_elem *batch_copy = vq->batch_copy_elems;
	int error = 0;

	buf_addr = buf_vec[vec_idx].buf_addr;
	buf_iova = buf_vec[vec_idx].buf_iova;
	buf_len = buf_vec[vec_idx].buf_len;

	//描述长度明显小于vhost_hlen是无效报文
	if (unlikely(buf_len < dev->vhost_hlen && nr_vec <= 1)) {
		error = -1;
		goto out;
	}

	if (likely(nr_vec > 1))
		rte_prefetch0((void *)(uintptr_t)buf_vec[1].buf_addr);

	if (virtio_net_with_host_offload(dev)) {
		//需要在virtio_net这一层做offload
		if (unlikely(buf_len < sizeof(struct virtio_net_hdr))) {
			//首个desc长度不足，读取virtio_net_hdr到tmp_hdr
			//并使hdr指向它，这里的copy目的仅仅用于分析。virtio_net_hdr头部仍然会copy进mbuf
			uint64_t len;
			uint64_t remain = sizeof(struct virtio_net_hdr);
			uint64_t src;
			uint64_t dst = (uint64_t)(uintptr_t)&tmp_hdr;
			uint16_t hdr_vec_idx = 0;

			/*
			 * No luck, the virtio-net header doesn't fit
			 * in a contiguous virtual area.
			 */
			while (remain) {
				len = remain;
				src = buf_vec[hdr_vec_idx].buf_addr;
				rte_memcpy((void *)(uintptr_t)dst,
						   (void *)(uintptr_t)src, len);

				remain -= len;
				dst += len;
				hdr_vec_idx++;
			}

			hdr = &tmp_hdr;
		} else {
			//长度足够，使hdr直接指向它
			hdr = (struct virtio_net_hdr *)((uintptr_t)buf_addr);
			rte_prefetch0(hdr);
		}
	}

	/*
	 * A virtio driver normally uses at least 2 desc buffers
	 * for Tx: the first for storing the header, and others
	 * for storing the data.
	 */
	//如上面所言，通常virtio驱动会至少使用2个desc buffer来做传输
	//1个存储描述信息头部，一个存储报文内容
	if (unlikely(buf_len < dev->vhost_hlen)) {
		//当前描述符中buffer的长度不足dev的描述信息头部长度
		buf_offset = dev->vhost_hlen - buf_len;
		vec_idx++;//切到下一个描述符
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;
		buf_avail  = buf_len - buf_offset;//更新有效长度（减少）
	} else if (buf_len == dev->vhost_hlen) {
		//当前描述符中的buffer长度恰等于dev的描述信息头部长度
		if (unlikely(++vec_idx >= nr_vec))
			goto out;//错误情况
		buf_addr = buf_vec[vec_idx].buf_addr;
		buf_iova = buf_vec[vec_idx].buf_iova;
		buf_len = buf_vec[vec_idx].buf_len;

		buf_offset = 0;
		buf_avail = buf_len;
	} else {
		buf_offset = dev->vhost_hlen;
		buf_avail = buf_vec[vec_idx].buf_len - dev->vhost_hlen;
	}

	//预取报文内容
	rte_prefetch0((void *)(uintptr_t)
			(buf_addr + buf_offset));

	//print报文内容
	PRINT_PACKET(dev,
			(uintptr_t)(buf_addr + buf_offset),
			(uint32_t)buf_avail, 0);

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;//mbuf可存储的空间

	//将描述符中指出的数据copy到mbuf中（如果支持zero copy,则直接指向数据即可）
	while (1) {
		uint64_t hpa;

		//mbuf有一个自已的长度，对方送过来的报文有一个自已的长度
		//如果要把对方的报文放入mbuf，那么copy的长度只能是两者中最小的一个。
		cpy_len = RTE_MIN(buf_avail, mbuf_avail);

		/*
		 * A desc buf might across two host physical pages that are
		 * not continuous. In such case (gpa_to_hpa returns 0), data
		 * will be copied even though zero copy is enabled.
		 */
		//如果开启了zero copy,直接让mbuf指向这段内存就可以了
		if (unlikely(dev->dequeue_zero_copy && (hpa = gpa_to_hpa(dev,
					buf_iova + buf_offset, cpy_len)))) {
			cur->data_len = cpy_len;
			cur->data_off = 0;
			//报文指向这一段内存
			cur->buf_addr =
				(void *)(uintptr_t)(buf_addr + buf_offset);
			cur->buf_iova = hpa;

			/*
			 * In zero copy mode, one mbuf can only reference data
			 * for one or partial of one desc buff.
			 */
			mbuf_avail = cpy_len;
		} else {
			if (likely(cpy_len > MAX_BATCH_LEN ||
				   vq->batch_copy_nb_elems >= vq->size ||
				   (hdr && cur == m))) {
				//直接将buf_addr指向的内存copy到mbuf中
				rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
								   mbuf_offset),
					   (void *)((uintptr_t)(buf_addr +
							   buf_offset)),
					   cpy_len);
			} else {
				batch_copy[vq->batch_copy_nb_elems].dst =
					rte_pktmbuf_mtod_offset(cur, void *,
								mbuf_offset);
				batch_copy[vq->batch_copy_nb_elems].src =
					(void *)((uintptr_t)(buf_addr +
								buf_offset));
				batch_copy[vq->batch_copy_nb_elems].len =
					cpy_len;
				vq->batch_copy_nb_elems++;
			}
		}

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		buf_avail -= cpy_len;//desc中还有多少字节没有放入到mbuf
		buf_offset += cpy_len;

		/* This buf reaches to its end, get the next one */
		//当desc_avail为0时，说明所有字节均已放入到mbuf中了，可以处理下一个desc了
		if (buf_avail == 0) {
			if (++vec_idx >= nr_vec)
				//这个desc没有指明有下一个，处理结束，退出循环
				break;

			//此时buf_vec下的buffer长度不如mbuf的长度大，故需要切换buf_vec,mbuf不用切换
			buf_addr = buf_vec[vec_idx].buf_addr;
			buf_iova = buf_vec[vec_idx].buf_iova;
			buf_len = buf_vec[vec_idx].buf_len;

			/*
			 * Prefecth desc n + 1 buffer while
			 * desc n buffer is processed.
			 */
			if (vec_idx + 1 < nr_vec)
				rte_prefetch0((void *)(uintptr_t)
						buf_vec[vec_idx + 1].buf_addr);

			buf_offset = 0;
			buf_avail  = buf_len;

			PRINT_PACKET(dev, (uintptr_t)buf_addr,
					(uint32_t)buf_avail, 0);
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
		//处理offload
		vhost_dequeue_offload(hdr, m);

out:

	return error;
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

//自队列vq中收包，首先检查队列中是否entries，vq->avail->ring队列里存放的是描述符索引
//通过vq->last_avail_idx指针，我们可以拿到首个描述符索引（desc_indexes)，然后通过
//vq->desc数组取出报文描述符（vq->desc数组中每个elem表示一个报文，如果报文过大时，elem会被组织
//成一个描述符list,并在链表头上标注VRING_DESC_F_INDIRECT）
//再由描述符定位到实际的报文，然后copy或者zero copy构建mbuf
//
static __rte_always_inline uint16_t
virtio_dev_tx_split(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	uint16_t i;
	uint16_t free_entries;

	if (unlikely(dev->dequeue_zero_copy)) {
		//zero copy代码未读
		struct zcopy_mbuf *zmbuf, *next;
		int nr_updated = 0;

		for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
		     zmbuf != NULL; zmbuf = next) {
			next = TAILQ_NEXT(zmbuf, next);

			if (mbuf_is_consumed(zmbuf->mbuf)) {
				update_shadow_used_ring_split(vq,
						zmbuf->desc_idx, 0);
				nr_updated += 1;

				TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
				restore_mbuf(zmbuf->mbuf);
				rte_pktmbuf_free(zmbuf->mbuf);
				put_zmbuf(zmbuf);
				vq->nr_zmbuf -= 1;
			}
		}

		flush_shadow_used_ring_split(dev, vq);
		vhost_vring_call_split(dev, vq);
	}

	rte_prefetch0(&vq->avail->ring[vq->last_avail_idx & (vq->size - 1)]);

	//获取有多少实体可收取
	free_entries = *((volatile uint16_t *)&vq->avail->idx) -
			vq->last_avail_idx;
	if (free_entries == 0)
		return 0;//队列中无报文可收取

	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);

	//将count更新为实际本次可收取的数目
	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);

	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	for (i = 0; i < count; i++) {
		struct buf_vector buf_vec[BUF_VECTOR_MAX];
		uint16_t head_idx, dummy_len;
		uint16_t nr_vec = 0;
		int err;

		//将vq->last_avail_idx+i描述符指向的buffer地址映射并填充到buf_vec中
		//注意此此填充仅对应一个报文（但可以是巨帧，此时会有多个mbuf通过next指针串连）
		//nr_vec用于指出buf_vec中占用的数量
		//head_idx为首个描述符索引
		if (unlikely(fill_vec_buf_split(dev, vq,
						vq->last_avail_idx + i,
						&nr_vec, buf_vec,
						&head_idx, &dummy_len,
						VHOST_ACCESS_RO) < 0))
			break;

		if (likely(dev->dequeue_zero_copy == 0))
			//记录head_idx已被使用（注意：还没有使用完，还需要使用其对应的buffer)
			update_shadow_used_ring_split(vq, head_idx, 0);

		rte_prefetch0((void *)(uintptr_t)buf_vec[0].buf_addr);

		//申请mbuf
		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}

		//自mbuf_pool中申请mbuf,并将描述符中指定的数据内容copy到mbuf
		err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, pkts[i],
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
			zmbuf->desc_idx = head_idx;

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

	//切换到下一个报文的描述符
	vq->last_avail_idx += i;

	if (likely(dev->dequeue_zero_copy == 0)) {
		//做未完成的功能（实现mbuf数据批量copy)
		do_data_copy_dequeue(vq);
		if (unlikely(i < count))
			vq->shadow_used_idx = i;
		flush_shadow_used_ring_split(dev, vq);
		vhost_vring_call_split(dev, vq);
	}

	return i;
}

static __rte_always_inline uint16_t
virtio_dev_tx_packed(struct virtio_net *dev, struct vhost_virtqueue *vq,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	uint16_t i;

	rte_prefetch0(&vq->desc_packed[vq->last_avail_idx]);

	if (unlikely(dev->dequeue_zero_copy)) {
		struct zcopy_mbuf *zmbuf, *next;

		for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
		     zmbuf != NULL; zmbuf = next) {
			next = TAILQ_NEXT(zmbuf, next);

			if (mbuf_is_consumed(zmbuf->mbuf)) {
				update_shadow_used_ring_packed(vq,
						zmbuf->desc_idx,
						0,
						zmbuf->desc_count);

				TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
				restore_mbuf(zmbuf->mbuf);
				rte_pktmbuf_free(zmbuf->mbuf);
				put_zmbuf(zmbuf);
				vq->nr_zmbuf -= 1;
			}
		}

		flush_shadow_used_ring_packed(dev, vq);
		vhost_vring_call_packed(dev, vq);
	}

	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) %s\n", dev->vid, __func__);

	//更新收包数
	count = RTE_MIN(count, MAX_PKT_BURST);
	VHOST_LOG_DEBUG(VHOST_DATA, "(%d) about to dequeue %u buffers\n",
			dev->vid, count);

	for (i = 0; i < count; i++) {
		struct buf_vector buf_vec[BUF_VECTOR_MAX];
		uint16_t buf_id, dummy_len;
		uint16_t desc_count, nr_vec = 0;
		int err;

		//在内部会增加avail_idx,desc_count返回有多少描述符被使用
		if (unlikely(fill_vec_buf_packed(dev, vq,
						vq->last_avail_idx, &desc_count,
						buf_vec, &nr_vec,
						&buf_id, &dummy_len,
						VHOST_ACCESS_RW) < 0))
			break;

		if (likely(dev->dequeue_zero_copy == 0))
			update_shadow_used_ring_packed(vq, buf_id, 0,
					desc_count);

		rte_prefetch0((void *)(uintptr_t)buf_vec[0].buf_addr);

		//申请mbuf
		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}

		//copy描述信息到pkts
		err = copy_desc_to_mbuf(dev, vq, buf_vec, nr_vec, pkts[i],
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
			zmbuf->desc_idx = buf_id;
			zmbuf->desc_count = desc_count;

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

		vq->last_avail_idx += desc_count;
		if (vq->last_avail_idx >= vq->size) {
			vq->last_avail_idx -= vq->size;
			vq->avail_wrap_counter ^= 1;
		}
	}

	if (likely(dev->dequeue_zero_copy == 0)) {
		do_data_copy_dequeue(vq);
		if (unlikely(i < count))
			vq->shadow_used_idx = i;
		flush_shadow_used_ring_packed(dev, vq);
		vhost_vring_call_packed(dev, vq);
	}

	return i;
}

//自队列queue_id中出队count个报文，将出队的报文存储在pkts中，由于出队会导致ring中无mbuf可填充，故需要
//在出队后，自mbuf_pool中申请mbuf填充进队列
uint16_t
rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct virtio_net *dev;
	struct rte_mbuf *rarp_mbuf = NULL;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);//取对应的virtio_net设备
	if (!dev)
		return 0;

	//内建的virtio_net被禁用，此函数不能工作
	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return 0;
	}

	//队列id有效性检查，队列不能是tx队列
	if (unlikely(!is_valid_virt_queue_idx(queue_id, 1, dev->nr_vring))) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, queue_id);
		return 0;
	}

	vq = dev->virtqueue[queue_id];

	//如果拿不到访问锁，则退出
	if (unlikely(rte_spinlock_trylock(&vq->access_lock) == 0))
		return 0;

	//对列未使能，不处理
	if (unlikely(vq->enabled == 0))
		goto out_access_unlock;

	//iotlb读加锁
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_rd_lock(vq);

	//如果ring还未进行地址翻译，则尝试翻译，如果翻译失败，则退出
	if (unlikely(vq->access_ok == 0))
		if (unlikely(vring_translate(dev, vq) < 0))
			goto out;

	/*
	 * Construct a RARP broadcast packet, and inject it to the "pkts"
	 * array, to looks like that guest actually send such packet.
	 *
	 * 构造一个rarp广播报文，并将其注入到pkts数组中，使之看起来是guest实际发送出来的报文一样
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
	//如果需要广播rarp,则发送rarp(保证仅一个队列发送此rarp报文）
	if (unlikely(rte_atomic16_read(&dev->broadcast_rarp) &&
			rte_atomic16_cmpset((volatile uint16_t *)
				&dev->broadcast_rarp.cnt, 1, 0))) {

		//构造反向arp请求(已知此dev的mac地址，请求此dev的ip地址）
		rarp_mbuf = rte_net_make_rarp_packet(mbuf_pool, &dev->mac);
		if (rarp_mbuf == NULL) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to make RARP packet.\n");
			return 0;
		}
		count -= 1;//由于需要注入rarp,故count数必须减1（这里可以通过优先去掉memmove操作）
		//但这种操作并不常见。
	}

	//这里提供供了两种tx方式，一种名为split,一种名为packet
	//简单比对了两者的区别，发现仅仅在通知对端的方式上有差别
	//另外packet情况下indirect方式下desc的存放方式与split也不相同
	if (vq_is_packed(dev))
		count = virtio_dev_tx_packed(dev, vq, mbuf_pool, pkts, count);
	else
		count = virtio_dev_tx_split(dev, vq, mbuf_pool, pkts, count);

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
		memmove(&pkts[1], pkts, count * sizeof(struct rte_mbuf *));
		pkts[0] = rarp_mbuf;//注入构造的rarp_mbuf(注入到pkts 0)
		count += 1;
	}

	return count;
}
