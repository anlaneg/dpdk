/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_DEV_H
#define _VIRTIO_USER_DEV_H

#include <limits.h>
#include <stdbool.h>
#include "../virtio_pci.h"
#include "../virtio_ring.h"
#include "vhost.h"

struct virtio_user_dev {
	/* for vhost_user backend */
	//client端时连接的fd
	int		vhostfd;
	//server端时监听的fd
	int		listenfd;   /* listening fd */
	//是否server模式
	bool		is_server;  /* server or client mode */

	/* for vhost_kernel backend */
	char		*ifname;//设备名称
	int		*vhostfds;
	int		*tapfds;

	/* for both vhost_user and vhost_kernel */
	int		callfds[VIRTIO_MAX_VIRTQUEUES];
	int		kickfds[VIRTIO_MAX_VIRTQUEUES];
	int		mac_specified;//是否指定了mac地址
	uint32_t	max_queue_pairs;//最大队列数
	uint32_t	queue_pairs;//生效队列数
	uint32_t	queue_size;//队列大小
	uint64_t	features; /* the negotiated features with driver,
				   * and will be sync with device
				   */
	uint64_t	device_features; /* supported features by device */
	uint64_t	unsupported_features; /* unsupported features mask */
	uint8_t		status;
	uint16_t	port_id;
	uint8_t		mac_addr[ETHER_ADDR_LEN];//设备的mac地址
	char		path[PATH_MAX];//unix socet位置
	struct vring	vrings[VIRTIO_MAX_VIRTQUEUES];
	struct virtio_user_backend_ops *ops;
	pthread_mutex_t	mutex;
	bool		started;
};

int is_vhost_user_by_type(const char *path);
int virtio_user_start_device(struct virtio_user_dev *dev);
int virtio_user_stop_device(struct virtio_user_dev *dev);
int virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
			 int cq, int queue_size, const char *mac, char **ifname,
			 int mrg_rxbuf, int in_order);
void virtio_user_dev_uninit(struct virtio_user_dev *dev);
void virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx);
uint8_t virtio_user_handle_mq(struct virtio_user_dev *dev, uint16_t q_pairs);
#endif
