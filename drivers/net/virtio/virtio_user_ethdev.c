/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/major.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>

#include <rte_malloc.h>
#include <rte_kvargs.h>
#include <ethdev_vdev.h>
#include <rte_bus_vdev.h>
#include <rte_alarm.h>
#include <rte_cycles.h>

#include "virtio_ethdev.h"
#include "virtio_logs.h"
#include "virtio.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_user/virtio_user_dev.h"
#include "virtio_user/vhost.h"

/*由hw获得virtio-user设备*/
#define virtio_user_get_dev(hwp) container_of(hwp, struct virtio_user_dev, hw)

//virtio user配置读取
static void
virtio_user_read_dev_config(struct virtio_hw *hw, size_t offset,
		     void *dst, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	//检查是否在读取mac地址
	if (offset == offsetof(struct virtio_net_config, mac) &&
	    length == RTE_ETHER_ADDR_LEN) {
		for (i = 0; i < RTE_ETHER_ADDR_LEN; ++i)
			((uint8_t *)dst)[i] = dev->mac_addr[i];
		return;
	}

	//检查是否读取status
	if (offset == offsetof(struct virtio_net_config, status)) {
		virtio_user_dev_update_link_state(dev);
		*(uint16_t *)dst = dev->net_status;
	}

	//读取max_virtqueue_pairs配置
	if (offset == offsetof(struct virtio_net_config, max_virtqueue_pairs))
		*(uint16_t *)dst = dev->max_queue_pairs;
}

//写mac地址
static void
virtio_user_write_dev_config(struct virtio_hw *hw, size_t offset,
		      const void *src, int length)
{
	int i;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	//写配置mac地址
	if ((offset == offsetof(struct virtio_net_config, mac)) &&
	    (length == RTE_ETHER_ADDR_LEN))
		for (i = 0; i < RTE_ETHER_ADDR_LEN; ++i)
			dev->mac_addr[i] = ((const uint8_t *)src)[i];
	else
		PMD_DRV_LOG(ERR, "not supported offset=%zu, len=%d",
			    offset, length);
}

static void
virtio_user_reset(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/*如果driver ok,则需要先停止设备*/
	if (dev->status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		virtio_user_stop_device(dev);
}

static void
virtio_user_set_status(struct virtio_hw *hw, uint8_t status)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);
	uint8_t old_status = dev->status;

	if (status & VIRTIO_CONFIG_STATUS_FEATURES_OK &&
			~old_status & VIRTIO_CONFIG_STATUS_FEATURES_OK)
		virtio_user_dev_set_features(dev);
	if (status & VIRTIO_CONFIG_STATUS_DRIVER_OK)
		virtio_user_start_device(dev);
	else if (status == VIRTIO_CONFIG_STATUS_RESET)
	    /*执行设备的reset*/
		virtio_user_reset(hw);

	/*设置设备状态*/
	virtio_user_dev_set_status(dev, status);
}

/*更新virtio-user设备的状态，并返回*/
static uint8_t
virtio_user_get_status(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	virtio_user_dev_update_status(dev);

	return dev->status;
}

static uint64_t
virtio_user_get_features(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/* unmask feature bits defined in vhost user protocol */
	return dev->device_features & VIRTIO_PMD_SUPPORTED_GUEST_FEATURES;
}

static void
virtio_user_set_features(struct virtio_hw *hw, uint64_t features)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	dev->features = features & dev->device_features;
}

static int
virtio_user_features_ok(struct virtio_hw *hw __rte_unused)
{
	return 0;
}

static uint8_t
virtio_user_get_isr(struct virtio_hw *hw __rte_unused)
{
	/* rxq interrupts and config interrupt are separated in virtio-user,
	 * here we only report config change.
	 */
	return VIRTIO_ISR_CONFIG;
}

static uint16_t
virtio_user_set_config_irq(struct virtio_hw *hw __rte_unused,
		    uint16_t vec __rte_unused)
{
	return 0;
}

static uint16_t
virtio_user_set_queue_irq(struct virtio_hw *hw __rte_unused,
			  struct virtqueue *vq __rte_unused,
			  uint16_t vec)
{
	/* pretend we have done that */
	return vec;
}

/* This function is to get the queue size, aka, number of descs, of a specified
 * queue. Different with the VHOST_USER_GET_QUEUE_NUM, which is used to get the
 * max supported queues.
 */
static uint16_t
virtio_user_get_queue_num(struct virtio_hw *hw, uint16_t queue_id __rte_unused)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	/* Currently, each queue has same queue size */
	return dev->queue_size;
}

static void
virtio_user_setup_queue_packed(struct virtqueue *vq,
			       struct virtio_user_dev *dev)
{
	uint16_t queue_idx = vq->vq_queue_index;
	struct vring_packed *vring;
	uint64_t desc_addr;
	uint64_t avail_addr;
	uint64_t used_addr;
	uint16_t i;

	vring  = &dev->packed_vrings[queue_idx];
	desc_addr = (uintptr_t)vq->vq_ring_virt_mem;
	avail_addr = desc_addr + vq->vq_nentries *
		sizeof(struct vring_packed_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr +
			   sizeof(struct vring_packed_desc_event),
			   VIRTIO_VRING_ALIGN);
	vring->num = vq->vq_nentries;
	vring->desc = (void *)(uintptr_t)desc_addr;
	vring->driver = (void *)(uintptr_t)avail_addr;
	vring->device = (void *)(uintptr_t)used_addr;
	dev->packed_queues[queue_idx].avail_wrap_counter = true;
	dev->packed_queues[queue_idx].used_wrap_counter = true;

	for (i = 0; i < vring->num; i++)
		vring->desc[i].flags = 0;
}

static void
virtio_user_setup_queue_split(struct virtqueue *vq, struct virtio_user_dev *dev)
{
	uint16_t queue_idx = vq->vq_queue_index;
	uint64_t desc_addr, avail_addr, used_addr;

	desc_addr = (uintptr_t)vq->vq_ring_virt_mem;
	avail_addr = desc_addr + vq->vq_nentries * sizeof(struct vring_desc);
	used_addr = RTE_ALIGN_CEIL(avail_addr + offsetof(struct vring_avail,
							 ring[vq->vq_nentries]),
				   VIRTIO_VRING_ALIGN);

	dev->vrings[queue_idx].num = vq->vq_nentries;
	dev->vrings[queue_idx].desc = (void *)(uintptr_t)desc_addr;
	dev->vrings[queue_idx].avail = (void *)(uintptr_t)avail_addr;
	dev->vrings[queue_idx].used = (void *)(uintptr_t)used_addr;
}

static int
virtio_user_setup_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	if (virtio_with_packed_queue(hw))
		virtio_user_setup_queue_packed(vq, dev);
	else
		virtio_user_setup_queue_split(vq, dev);

	return 0;
}

static void
virtio_user_del_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	/* For legacy devices, write 0 to VIRTIO_PCI_QUEUE_PFN port, QEMU
	 * correspondingly stops the ioeventfds, and reset the status of
	 * the device.
	 * For modern devices, set queue desc, avail, used in PCI bar to 0,
	 * not see any more behavior in QEMU.
	 *
	 * Here we just care about what information to deliver to vhost-user
	 * or vhost-kernel. So we just close ioeventfd for now.
	 */
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	close(dev->callfds[vq->vq_queue_index]);
	close(dev->kickfds[vq->vq_queue_index]);
}

static void
virtio_user_notify_queue(struct virtio_hw *hw, struct virtqueue *vq)
{
	uint64_t buf = 1;
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	//收到通知，且被通知的队列为控制队列，处理控制消息
	if (hw->cvq && (virtnet_cq_to_vq(hw->cvq) == vq)) {
		if (virtio_with_packed_queue(vq->hw))
			virtio_user_handle_cq_packed(dev, vq->vq_queue_index);
		else
			virtio_user_handle_cq(dev, vq->vq_queue_index);
		return;
	}

	//其它队列，则通过kickfds再次触发事件
	if (write(dev->kickfds[vq->vq_queue_index], &buf, sizeof(buf)) < 0)
		PMD_DRV_LOG(ERR, "failed to kick backend: %s",
			    strerror(errno));
}

static int
virtio_user_dev_close(struct virtio_hw *hw)
{
	struct virtio_user_dev *dev = virtio_user_get_dev(hw);

	virtio_user_dev_uninit(dev);

	return 0;
}

/*virtio-user设备对应的函数操作集*/
const struct virtio_ops virtio_user_ops = {
	//读配置
	.read_dev_cfg	= virtio_user_read_dev_config,
	//写配置
	.write_dev_cfg	= virtio_user_write_dev_config,
	/*调用底层设备，获取设备状态信息*/
	.get_status	= virtio_user_get_status,
	/*设置设备状态*/
	.set_status	= virtio_user_set_status,
	.get_features	= virtio_user_get_features,
	.set_features	= virtio_user_set_features,
	.features_ok	= virtio_user_features_ok,
	.get_isr	= virtio_user_get_isr,
	.set_config_irq	= virtio_user_set_config_irq,
	.set_queue_irq	= virtio_user_set_queue_irq,
	.get_queue_num	= virtio_user_get_queue_num,
	.setup_queue	= virtio_user_setup_queue,
	.del_queue	= virtio_user_del_queue,
	//通知指定队列（如果是控制队列，则会触发控制队列处理消息）
	.notify_queue	= virtio_user_notify_queue,
	.dev_close	= virtio_user_dev_close,
};

//valid_支持的参数
static const char *valid_args[] = {
#define VIRTIO_USER_ARG_QUEUES_NUM     "queues"
	VIRTIO_USER_ARG_QUEUES_NUM,
#define VIRTIO_USER_ARG_CQ_NUM         "cq"
	VIRTIO_USER_ARG_CQ_NUM,
#define VIRTIO_USER_ARG_MAC            "mac"
	VIRTIO_USER_ARG_MAC,
#define VIRTIO_USER_ARG_PATH           "path"
	VIRTIO_USER_ARG_PATH,
#define VIRTIO_USER_ARG_QUEUE_SIZE     "queue_size"
	VIRTIO_USER_ARG_QUEUE_SIZE,
#define VIRTIO_USER_ARG_INTERFACE_NAME "iface"
	VIRTIO_USER_ARG_INTERFACE_NAME,
#define VIRTIO_USER_ARG_SERVER_MODE    "server"
	VIRTIO_USER_ARG_SERVER_MODE,
#define VIRTIO_USER_ARG_MRG_RXBUF      "mrg_rxbuf"
	VIRTIO_USER_ARG_MRG_RXBUF,
#define VIRTIO_USER_ARG_IN_ORDER       "in_order"
	VIRTIO_USER_ARG_IN_ORDER,
#define VIRTIO_USER_ARG_PACKED_VQ      "packed_vq"
	VIRTIO_USER_ARG_PACKED_VQ,
#define VIRTIO_USER_ARG_SPEED          "speed"
	VIRTIO_USER_ARG_SPEED,
#define VIRTIO_USER_ARG_VECTORIZED     "vectorized"
	VIRTIO_USER_ARG_VECTORIZED,
	NULL
};

#define VIRTIO_USER_DEF_CQ_EN	0
#define VIRTIO_USER_DEF_Q_NUM	1
#define VIRTIO_USER_DEF_Q_SZ	256
#define VIRTIO_USER_DEF_SERVER_MODE	0

static int
get_string_arg(const char *key __rte_unused,
	       const char *value, void *extra_args)
{
	if (!value || !extra_args)
		return -EINVAL;

	*(char **)extra_args = strdup(value);

	if (!*(char **)extra_args)
		return -ENOMEM;

	return 0;
}

static int
get_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	uint64_t integer = 0;
	if (!value || !extra_args)
		return -EINVAL;
	errno = 0;
	integer = strtoull(value, NULL, 0);
	/* extra_args keeps default value, it should be replaced
	 * only in case of successful parsing of the 'value' arg
	 */
	if (errno == 0)
		*(uint64_t *)extra_args = integer;
	return -errno;
}

static uint32_t
vdpa_dynamic_major_num(void)
{
	FILE *fp;
	char *line = NULL;
	size_t size = 0;
	char name[11];
	bool found = false;
	uint32_t num;

	fp = fopen("/proc/devices", "r");
	if (fp == NULL) {
		PMD_INIT_LOG(ERR, "Cannot open /proc/devices: %s",
			     strerror(errno));
		return UNNAMED_MAJOR;
	}

	while (getline(&line, &size, fp) > 0) {
		char *stripped = line + strspn(line, " ");
		if ((sscanf(stripped, "%u %10s", &num, name) == 2) &&
		    (strncmp(name, "vhost-vdpa", 10) == 0)) {
			found = true;
			break;
		}
	}
	free(line);
	fclose(fp);
	return found ? num : UNNAMED_MAJOR;
}

/*依据path检查virtio-user的后端*/
static enum virtio_user_backend_type
virtio_user_backend_type(const char *path)
{
	struct stat sb;

	/*文件不存，返回backend_unknown*/
	if (stat(path, &sb) == -1) {
		if (errno == ENOENT)
			return VIRTIO_USER_BACKEND_VHOST_USER;

		PMD_INIT_LOG(ERR, "Stat fails: %s (%s)\n", path,
			     strerror(errno));
		return VIRTIO_USER_BACKEND_UNKNOWN;
	}

	if (S_ISSOCK(sb.st_mode)) {
	    /*如果其为socket,则定义为vhost_user*/
		return VIRTIO_USER_BACKEND_VHOST_USER;
	} else if (S_ISCHR(sb.st_mode)) {
	    /*如为字符设备，则定义为vhost-kernel或vhost-vdpa*/
		if (major(sb.st_rdev) == MISC_MAJOR)
			return VIRTIO_USER_BACKEND_VHOST_KERNEL;
		if (major(sb.st_rdev) == vdpa_dynamic_major_num())
			return VIRTIO_USER_BACKEND_VHOST_VDPA;
	}
	return VIRTIO_USER_BACKEND_UNKNOWN;
}

static struct rte_eth_dev *
virtio_user_eth_dev_alloc(struct rte_vdev_device *vdev)
{
	struct rte_eth_dev *eth_dev;
	struct rte_eth_dev_data *data;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;

	//创建vdev对应的以太网设备，其私有数据为virtio_user_dev
	eth_dev = rte_eth_vdev_allocate(vdev, sizeof(*dev));
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "cannot alloc rte_eth_dev");
		return NULL;
	}

	data = eth_dev->data;
	dev = eth_dev->data->dev_private;
	hw = &dev->hw;

	/*初始化virtio_hw*/
	hw->port_id = data->port_id;
	VIRTIO_OPS(hw) = &virtio_user_ops;

	hw->intr_lsc = 1;
	hw->use_vec_rx = 0;
	hw->use_vec_tx = 0;
	hw->use_inorder_rx = 0;
	hw->use_inorder_tx = 0;

	return eth_dev;
}

static void
virtio_user_eth_dev_free(struct rte_eth_dev *eth_dev)
{
	rte_eth_dev_release_port(eth_dev);
}

/* Dev initialization routine. Invoked once for each virtio vdev at
 * EAL init time, see rte_bus_probe().
 * Returns 0 on success.
 */
//vritio-user驱动探测设备
static int
virtio_user_pmd_probe(struct rte_vdev_device *vdev)
{
	struct rte_kvargs *kvlist = NULL;
	struct rte_eth_dev *eth_dev;
	struct virtio_hw *hw;
	struct virtio_user_dev *dev;
	enum virtio_user_backend_type backend_type = VIRTIO_USER_BACKEND_UNKNOWN;
	uint64_t queues = VIRTIO_USER_DEF_Q_NUM;
	uint64_t cq = VIRTIO_USER_DEF_CQ_EN;
	uint64_t queue_size = VIRTIO_USER_DEF_Q_SZ;
	uint64_t server_mode = VIRTIO_USER_DEF_SERVER_MODE;
	uint64_t mrg_rxbuf = 1;
	uint64_t in_order = 1;
	uint64_t packed_vq = 0;
	uint64_t vectorized = 0;
	char *path = NULL;
	char *ifname = NULL;
	char *mac_addr = NULL;
	int ret = -1;

	RTE_BUILD_BUG_ON(offsetof(struct virtio_user_dev, hw) != 0);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		const char *name = rte_vdev_device_name(vdev);
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev) {
			PMD_INIT_LOG(ERR, "Failed to probe %s", name);
			return -1;
		}

		dev = eth_dev->data->dev_private;
		hw = &dev->hw;
		VIRTIO_OPS(hw) = &virtio_user_ops;

		if (eth_virtio_dev_init(eth_dev) < 0) {
			PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
			rte_eth_dev_release_port(eth_dev);
			return -1;
		}

		eth_dev->dev_ops = &virtio_user_secondary_eth_dev_ops;
		eth_dev->device = &vdev->device;
		rte_eth_dev_probing_finish(eth_dev);
		return 0;
	}

	/*解析kvlist,并校验其是否合法*/
	kvlist = rte_kvargs_parse(rte_vdev_device_args(vdev), valid_args);
	if (!kvlist) {
		PMD_INIT_LOG(ERR, "error when parsing param");
		goto end;
	}

	//自kvlist中提供key为‘path’的参数，将其填充到path变量。
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_PATH) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_PATH,
				       &get_string_arg, &path) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_PATH);
			goto end;
		}
	} else {
		PMD_INIT_LOG(ERR, "arg %s is mandatory for virtio_user",
			     VIRTIO_USER_ARG_PATH);
		goto end;
	}

	/*backend类型检查*/
	backend_type = virtio_user_backend_type(path);
	if (backend_type == VIRTIO_USER_BACKEND_UNKNOWN) {
		PMD_INIT_LOG(ERR,
			     "unable to determine backend type for path %s",
			path);
		goto end;
	}
	PMD_INIT_LOG(INFO, "Backend type detected: %s",
		     virtio_user_backend_strings[backend_type]);

	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_INTERFACE_NAME) == 1) {
		//如果配置了iface,则path不能是socket
		if (backend_type != VIRTIO_USER_BACKEND_VHOST_KERNEL) {
			PMD_INIT_LOG(ERR,
				"arg %s applies only to vhost-kernel backend",
				VIRTIO_USER_ARG_INTERFACE_NAME);
			goto end;
		}

		//自kvlist中提供key为‘iface’的参数,填充ifname
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_INTERFACE_NAME,
				       &get_string_arg, &ifname) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_INTERFACE_NAME);
			goto end;
		}
	}

	//自kvlist中提供key为‘mac’的参数,填充mac_addr
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_MAC) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_MAC,
				       &get_string_arg, &mac_addr) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_MAC);
			goto end;
		}
	}

	//填充queue_size
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUE_SIZE,
				       &get_integer_arg, &queue_size) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUE_SIZE);
			goto end;
		}
	}

	//填充queues
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_QUEUES_NUM) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_QUEUES_NUM,
				       &get_integer_arg, &queues) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_QUEUES_NUM);
			goto end;
		}
	}

	//填充server mode
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_SERVER_MODE) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_SERVER_MODE,
				       &get_integer_arg, &server_mode) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_SERVER_MODE);
			goto end;
		}
	}

	//填充cq
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_CQ_NUM) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_CQ_NUM,
				       &get_integer_arg, &cq) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_CQ_NUM);
			goto end;
		}
	} else if (queues > 1) {
		cq = 1;
	}

	//填充packed_vq
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_PACKED_VQ) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_PACKED_VQ,
				       &get_integer_arg, &packed_vq) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_PACKED_VQ);
			goto end;
		}
	}

	//填充vectorized
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_VECTORIZED) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_VECTORIZED,
				       &get_integer_arg, &vectorized) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_VECTORIZED);
			goto end;
		}
	}

	/*如果queues>1时，则cq必须不能为0*/
	if (queues > 1 && cq == 0) {
		PMD_INIT_LOG(ERR, "multi-q requires ctrl-q");
		goto end;
	}

	//配置的队列数量过大
	if (queues > VIRTIO_MAX_VIRTQUEUE_PAIRS) {
		PMD_INIT_LOG(ERR, "arg %s %" PRIu64 " exceeds the limit %u",
			VIRTIO_USER_ARG_QUEUES_NUM, queues,
			VIRTIO_MAX_VIRTQUEUE_PAIRS);
		goto end;
	}

	//填充merge_rxbuf
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_MRG_RXBUF) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_MRG_RXBUF,
				       &get_integer_arg, &mrg_rxbuf) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_MRG_RXBUF);
			goto end;
		}
	}

	//填充in_order
	if (rte_kvargs_count(kvlist, VIRTIO_USER_ARG_IN_ORDER) == 1) {
		if (rte_kvargs_process(kvlist, VIRTIO_USER_ARG_IN_ORDER,
				       &get_integer_arg, &in_order) < 0) {
			PMD_INIT_LOG(ERR, "error to parse %s",
				     VIRTIO_USER_ARG_IN_ORDER);
			goto end;
		}
	}

	/*申请并创建并vdev对应的以太网络设备*/
	eth_dev = virtio_user_eth_dev_alloc(vdev);
	if (!eth_dev) {
		PMD_INIT_LOG(ERR, "virtio_user fails to alloc device");
		goto end;
	}

	dev = eth_dev->data->dev_private;
	hw = &dev->hw;
	if (virtio_user_dev_init(dev/*设备私有数据*/, path/*设备地址*/, queues/*队列数*/, cq,
			 queue_size/*队列长度*/, mac_addr/*mac地址*/, &ifname, server_mode,
			 mrg_rxbuf, in_order, packed_vq, backend_type) < 0) {
		PMD_INIT_LOG(ERR, "virtio_user_dev_init fails");
		virtio_user_eth_dev_free(eth_dev);
		goto end;
	}

	/* previously called by pci probing for physical dev */
	/*virtio设备初始化*/
	if (eth_virtio_dev_init(eth_dev) < 0) {
		PMD_INIT_LOG(ERR, "eth_virtio_dev_init fails");
		virtio_user_eth_dev_free(eth_dev);
		goto end;
	}

	if (vectorized) {
		if (packed_vq) {
#if defined(CC_AVX512_SUPPORT) || defined(RTE_ARCH_ARM)
			hw->use_vec_rx = 1;
			hw->use_vec_tx = 1;
#else
			PMD_INIT_LOG(INFO,
				"building environment do not support packed ring vectorized");
#endif
		} else {
			hw->use_vec_rx = 1;
		}
	}

	rte_eth_dev_probing_finish(eth_dev);
	ret = 0;

end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (path)
		free(path);
	if (mac_addr)
		free(mac_addr);
	if (ifname)
		free(ifname);
	return ret;
}

static int
virtio_user_pmd_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_eth_dev *eth_dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	PMD_DRV_LOG(INFO, "Un-Initializing %s", name);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return rte_eth_dev_release_port(eth_dev);

	/* make sure the device is stopped, queues freed */
	return rte_eth_dev_close(eth_dev->data->port_id);
}

static int virtio_user_pmd_dma_map(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct virtio_user_dev *dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	dev = eth_dev->data->dev_private;

	if (dev->ops->dma_map)
		return dev->ops->dma_map(dev, addr, iova, len);

	return 0;
}

static int virtio_user_pmd_dma_unmap(struct rte_vdev_device *vdev, void *addr,
		uint64_t iova, size_t len)
{
	const char *name;
	struct rte_eth_dev *eth_dev;
	struct virtio_user_dev *dev;

	if (!vdev)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	eth_dev = rte_eth_dev_allocated(name);
	/* Port has already been released by close. */
	if (!eth_dev)
		return 0;

	dev = eth_dev->data->dev_private;

	if (dev->ops->dma_unmap)
		return dev->ops->dma_unmap(dev, addr, iova, len);

	return 0;
}

//virtio用户driver
static struct rte_vdev_driver virtio_user_driver = {
    /*驱动找到了一个设备，进行适配*/
	.probe = virtio_user_pmd_probe,
	.remove = virtio_user_pmd_remove,
	.dma_map = virtio_user_pmd_dma_map,
	.dma_unmap = virtio_user_pmd_dma_unmap,
	.drv_flags = RTE_VDEV_DRV_NEED_IOVA_AS_VA,
};

//注册virtio_user驱动
RTE_PMD_REGISTER_VDEV(net_virtio_user, virtio_user_driver);
/*指明net_virtio_user驱动的别名为virtio_user*/
RTE_PMD_REGISTER_ALIAS(net_virtio_user, virtio_user);
RTE_PMD_REGISTER_PARAM_STRING(net_virtio_user,
	"path=<path> "
	"mac=<mac addr> "
	"cq=<int> "
	"queue_size=<int> "
	"queues=<int> "
	"iface=<string> "
	"server=<0|1> "
	"mrg_rxbuf=<0|1> "
	"in_order=<0|1> "
	"packed_vq=<0|1> "
	"speed=<int> "
	"vectorized=<0|1>");
