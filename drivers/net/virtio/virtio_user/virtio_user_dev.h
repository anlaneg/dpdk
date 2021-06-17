/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_USER_DEV_H
#define _VIRTIO_USER_DEV_H

#include <limits.h>
#include <stdbool.h>

#include "../virtio.h"
#include "../virtio_ring.h"

enum virtio_user_backend_type {
	VIRTIO_USER_BACKEND_UNKNOWN,
	VIRTIO_USER_BACKEND_VHOST_USER,
	VIRTIO_USER_BACKEND_VHOST_KERNEL,
	VIRTIO_USER_BACKEND_VHOST_VDPA,
};

struct virtio_user_queue {
	uint16_t used_idx;
	bool avail_wrap_counter;
	bool used_wrap_counter;
};

struct virtio_user_dev {
	struct virtio_hw hw;
	/*virtio-user后端类型*/
	enum virtio_user_backend_type backend_type;
	//是否server模式
	bool		is_server;  /* server or client mode */

	int		callfds[VIRTIO_MAX_VIRTQUEUES];
	int		kickfds[VIRTIO_MAX_VIRTQUEUES];
	//是否指定了mac地址
	int		mac_specified;
	//最大队列数
	uint32_t	max_queue_pairs;
	//生效队列数
	uint32_t	queue_pairs;
	//队列大小
	uint32_t	queue_size;
	uint64_t	features; /* the negotiated features with driver,
				   * and will be sync with device
				   */
	uint64_t	device_features; /* supported features by device */
	uint64_t	frontend_features; /* enabled frontend features */
	uint64_t	unsupported_features; /* unsupported features mask */
	uint8_t		status;
	uint16_t	net_status;
	//设备的mac地址
	uint8_t		mac_addr[RTE_ETHER_ADDR_LEN];
	//unix socet位置
	char		path[PATH_MAX];
	char		*ifname;

	union {
		struct vring		vrings[VIRTIO_MAX_VIRTQUEUES];
		struct vring_packed	packed_vrings[VIRTIO_MAX_VIRTQUEUES];
	};
	struct virtio_user_queue packed_queues[VIRTIO_MAX_VIRTQUEUES];
	bool		qp_enabled[VIRTIO_MAX_VIRTQUEUE_PAIRS];

	/*virtio-user后端对应的ops,例如virtio_ops_user*/
	struct virtio_user_backend_ops *ops;
	pthread_mutex_t	mutex;
	bool		started;

	void *backend_data;
};

int virtio_user_dev_set_features(struct virtio_user_dev *dev);
int virtio_user_start_device(struct virtio_user_dev *dev);
int virtio_user_stop_device(struct virtio_user_dev *dev);
int virtio_user_dev_init(struct virtio_user_dev *dev, char *path, int queues,
			 int cq, int queue_size, const char *mac, char **ifname,
			 int server, int mrg_rxbuf, int in_order,
			 int packed_vq,
			 enum virtio_user_backend_type backend_type);
void virtio_user_dev_uninit(struct virtio_user_dev *dev);
void virtio_user_handle_cq(struct virtio_user_dev *dev, uint16_t queue_idx);
void virtio_user_handle_cq_packed(struct virtio_user_dev *dev,
				  uint16_t queue_idx);
uint8_t virtio_user_handle_mq(struct virtio_user_dev *dev, uint16_t q_pairs);
int virtio_user_dev_set_status(struct virtio_user_dev *dev, uint8_t status);
int virtio_user_dev_update_status(struct virtio_user_dev *dev);
int virtio_user_dev_update_link_state(struct virtio_user_dev *dev);
int virtio_user_dev_set_mac(struct virtio_user_dev *dev);
int virtio_user_dev_get_mac(struct virtio_user_dev *dev);
void virtio_user_dev_delayed_disconnect_handler(void *param);
int virtio_user_dev_server_reconnect(struct virtio_user_dev *dev);
extern const char * const virtio_user_backend_strings[];
#endif
