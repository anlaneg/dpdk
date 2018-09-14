/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "private.h"


extern struct rte_pci_bus rte_pci_bus;

#define SYSFS_PCI_DEVICES "/sys/bus/pci/devices"

//pci扫描路径（例如/sys/bus/pci/devices)
const char *rte_pci_get_sysfs_path(void)
{
	const char *path = NULL;

	path = getenv("SYSFS_PCI_DEVICES");
	if (path == NULL)
		return SYSFS_PCI_DEVICES;

	return path;
}

static struct rte_devargs *pci_devargs_lookup(struct rte_pci_device *dev)
{
	struct rte_devargs *devargs;
	struct rte_pci_addr addr;

	RTE_EAL_DEVARGS_FOREACH("pci", devargs) {
		devargs->bus->parse(devargs->name, &addr);
		if (!rte_pci_addr_cmp(&dev->addr, &addr))
			return devargs;
	}
	return NULL;
}

void
pci_name_set(struct rte_pci_device *dev)
{
	struct rte_devargs *devargs;

	/* Each device has its internal, canonical name set. */
	rte_pci_device_name(&dev->addr,
			dev->name, sizeof(dev->name));
	devargs = pci_devargs_lookup(dev);
	dev->device.devargs = devargs;
	/* In blacklist mode, if the device is not blacklisted, no
	 * rte_devargs exists for it.
	 */
	if (devargs != NULL)
		/* If an rte_devargs exists, the generic rte_device uses the
		 * given name as its name.
		 */
		dev->device.name = dev->device.devargs->name;
	else
		/* Otherwise, it uses the internal, canonical form. */
		dev->device.name = dev->name;
}

/*
 * Match the PCI Driver and Device using the ID Table
 */
int
//检查pci-drv是否可匹配此设备
rte_pci_match(const struct rte_pci_driver *pci_drv,
	      const struct rte_pci_device *pci_dev)
{
	const struct rte_pci_id *id_table;

	//检查驱动支持的设备列表中是否包含本设备
	for (id_table = pci_drv->id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != pci_dev->id.vendor_id &&
				id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != pci_dev->id.device_id &&
				id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    pci_dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    pci_dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != PCI_ANY_ID)
			continue;
		if (id_table->class_id != pci_dev->id.class_id &&
				id_table->class_id != RTE_CLASS_ANY_ID)
			continue;

		return 1;//驱动匹配此设备
	}

	return 0;
}

/*
 * If vendor/device ID match, call the probe() function of the
 * driver.
 */
static int
rte_pci_probe_one_driver(struct rte_pci_driver *dr,
			 struct rte_pci_device *dev)
{
	int ret;
	struct rte_pci_addr *loc;

	if ((dr == NULL) || (dev == NULL))
		return -EINVAL;

	loc = &dev->addr;

	/* The device is not blacklisted; Check if driver supports it */
	if (!rte_pci_match(dr, dev))
		/* Match of device and driver failed */
		//此driver不匹配此device
		return 1;

	//设备可以被此驱动使用
	RTE_LOG(INFO, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
			loc->domain, loc->bus, loc->devid, loc->function,
			dev->device.numa_node);

	/* no initialization when blacklisted, return without error */
	//设备被列在黑名单里，不初始化
	if (dev->device.devargs != NULL &&
		dev->device.devargs->policy ==
			RTE_DEV_BLACKLISTED) {
		RTE_LOG(INFO, EAL, "  Device is blacklisted, not"
			" initializing\n");
		return 1;
	}

	//设备所属numa不合法，更新为0
	if (dev->device.numa_node < 0) {
		RTE_LOG(WARNING, EAL, "  Invalid NUMA socket, default to 0\n");
		dev->device.numa_node = 0;
	}

	//指名驱动适配成功
	RTE_LOG(INFO, EAL, "  probe driver: %x:%x %s\n", dev->id.vendor_id,
		dev->id.device_id, dr->driver.name);

	/*
	 * reference driver structure
	 * This needs to be before rte_pci_map_device(), as it enables to use
	 * driver flags for adjusting configuration.
	 */
	//为设备暂时赋上此驱动，进行probe(这段代码与kernel极其相似）
	dev->driver = dr;
	dev->device.driver = &dr->driver;

	//如果driver要求mapping，则进行资源map(例如bnxt驱动就要求进行mapping)
	//按理解，所有采用igb_uio方式绑定的均要走此函数
	if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) {
		/* map resources for devices that use igb_uio */
		ret = rte_pci_map_device(dev);
		if (ret != 0) {
			dev->driver = NULL;
			dev->device.driver = NULL;
			return ret;
		}
	}

	/* call the driver probe() function */
	//用此driver探测此设备
	ret = dr->probe(dr, dev);
	if (ret) {
		dev->driver = NULL;
		dev->device.driver = NULL;
		//适配失败，检查是否需要unmap资源
		if ((dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) &&
			/* Don't unmap if device is unsupported and
			 * driver needs mapped resources.
			 */
			!(ret > 0 &&
				(dr->drv_flags & RTE_PCI_DRV_KEEP_MAPPED_RES)))
			rte_pci_unmap_device(dev);
	}

	return ret;
}

/*
 * If vendor/device ID match, call the remove() function of the
 * driver.
 */
static int
rte_pci_detach_dev(struct rte_pci_device *dev)
{
	struct rte_pci_addr *loc;
	struct rte_pci_driver *dr;
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	dr = dev->driver;
	loc = &dev->addr;

	RTE_LOG(DEBUG, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
			loc->domain, loc->bus, loc->devid,
			loc->function, dev->device.numa_node);

	RTE_LOG(DEBUG, EAL, "  remove driver: %x:%x %s\n", dev->id.vendor_id,
			dev->id.device_id, dr->driver.name);

	if (dr->remove) {
		ret = dr->remove(dev);
		if (ret < 0)
			return ret;
	}

	/* clear driver structure */
	dev->driver = NULL;

	if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING)
		/* unmap resources for devices that use igb_uio */
		rte_pci_unmap_device(dev);

	return 0;
}

/*
 * If vendor/device ID match, call the probe() function of all
 * registered driver for the given device. Return -1 if initialization
 * failed, return 1 if no driver is found for this device.
 */
//针对当前device，检查其是否可以匹配已知的driver
static int
pci_probe_all_drivers(struct rte_pci_device *dev)
{
	struct rte_pci_driver *dr = NULL;
	int rc = 0;

	if (dev == NULL)
		return -1;

	/* Check if a driver is already loaded */
	//如果已有驱动，则不必再探测了
	if (dev->driver != NULL)
		return 0;

	//遍历已注册的所有pci驱动，检查是否有驱动可适配dev
	FOREACH_DRIVER_ON_PCIBUS(dr) {
		rc = rte_pci_probe_one_driver(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/*
 * Scan the content of the PCI bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
int
rte_pci_probe(void)
{
	struct rte_pci_device *dev = NULL;
	size_t probed = 0, failed = 0;
	struct rte_devargs *devargs;
	int probe_all = 0;
	int ret = 0;

	if (rte_pci_bus.bus.conf.scan_mode != RTE_BUS_SCAN_WHITELIST)
		probe_all = 1;

	//遍历扫描出来的所有设备
	FOREACH_DEVICE_ON_PCIBUS(dev) {
		probed++;

		devargs = dev->device.devargs;
		/* probe all or only whitelisted devices */
		if (probe_all)
			ret = pci_probe_all_drivers(dev);
		else if (devargs != NULL &&
			devargs->policy == RTE_DEV_WHITELISTED)
			ret = pci_probe_all_drivers(dev);

		//黑名单或者虚拟设备时，将被跳过
		if (ret < 0) {
			//设备无法被使用，报错，继续尝试
			RTE_LOG(ERR, EAL, "Requested device " PCI_PRI_FMT
				 " cannot be used\n", dev->addr.domain, dev->addr.bus,
				 dev->addr.devid, dev->addr.function);
			rte_errno = errno;
			failed++;
			ret = 0;
		}
	}

	//探测的与失败的一样多时，返回-1
	return (probed && probed == failed) ? -1 : 0;
}

/* dump one device */
static int
pci_dump_one_device(FILE *f, struct rte_pci_device *dev)
{
	int i;

	fprintf(f, PCI_PRI_FMT, dev->addr.domain, dev->addr.bus,
	       dev->addr.devid, dev->addr.function);
	fprintf(f, " - vendor:%x device:%x\n", dev->id.vendor_id,
	       dev->id.device_id);

	for (i = 0; i != sizeof(dev->mem_resource) /
		sizeof(dev->mem_resource[0]); i++) {
		fprintf(f, "   %16.16"PRIx64" %16.16"PRIx64"\n",
			dev->mem_resource[i].phys_addr,
			dev->mem_resource[i].len);
	}
	return 0;
}

/* dump devices on the bus */
void
rte_pci_dump(FILE *f)
{
	struct rte_pci_device *dev = NULL;

	FOREACH_DEVICE_ON_PCIBUS(dev) {
		pci_dump_one_device(f, dev);
	}
}

static int
pci_parse(const char *name, void *addr)
{
	struct rte_pci_addr *out = addr;
	struct rte_pci_addr pci_addr;
	bool parse;

	parse = (rte_pci_addr_parse(name, &pci_addr) == 0);
	if (parse && addr != NULL)
		*out = pci_addr;
	return parse == false;
}

/* register a driver */
//pci驱动注册
void
rte_pci_register(struct rte_pci_driver *driver)
{
	TAILQ_INSERT_TAIL(&rte_pci_bus.driver_list, driver, next);
	driver->bus = &rte_pci_bus;
}

/* unregister a driver */
void
rte_pci_unregister(struct rte_pci_driver *driver)
{
	TAILQ_REMOVE(&rte_pci_bus.driver_list, driver, next);
	driver->bus = NULL;
}

/* Add a device to PCI bus */
void
rte_pci_add_device(struct rte_pci_device *pci_dev)
{
	TAILQ_INSERT_TAIL(&rte_pci_bus.device_list, pci_dev, next);
}

/* Insert a device into a predefined position in PCI bus */
void
rte_pci_insert_device(struct rte_pci_device *exist_pci_dev,
		      struct rte_pci_device *new_pci_dev)
{
	TAILQ_INSERT_BEFORE(exist_pci_dev, new_pci_dev, next);
}

/* Remove a device from PCI bus */
static void
rte_pci_remove_device(struct rte_pci_device *pci_dev)
{
	TAILQ_REMOVE(&rte_pci_bus.device_list, pci_dev, next);
}

static struct rte_device *
pci_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		const void *data)
{
	const struct rte_pci_device *pstart;
	struct rte_pci_device *pdev;

	if (start != NULL) {
		pstart = RTE_DEV_TO_PCI_CONST(start);
		pdev = TAILQ_NEXT(pstart, next);
	} else {
		pdev = TAILQ_FIRST(&rte_pci_bus.device_list);
	}
	while (pdev != NULL) {
		if (cmp(&pdev->device, data) == 0)
			return &pdev->device;
		pdev = TAILQ_NEXT(pdev, next);
	}
	return NULL;
}

static int
pci_plug(struct rte_device *dev)
{
	return pci_probe_all_drivers(RTE_DEV_TO_PCI(dev));
}

static int
pci_unplug(struct rte_device *dev)
{
	struct rte_pci_device *pdev;
	int ret;

	pdev = RTE_DEV_TO_PCI(dev);
	ret = rte_pci_detach_dev(pdev);
	if (ret == 0) {
		rte_pci_remove_device(pdev);
		free(pdev);
	}
	return ret;
}

struct rte_pci_bus rte_pci_bus = {
	.bus = {
		.scan = rte_pci_scan,//扫描函数,找到系统中所有pci设备
		.probe = rte_pci_probe,//探测
		.find_device = pci_find_device,
		.plug = pci_plug,
		.unplug = pci_unplug,
		.parse = pci_parse,
		.get_iommu_class = rte_pci_get_iommu_class,
	},
	//保存读到的pci设备
	.device_list = TAILQ_HEAD_INITIALIZER(rte_pci_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_pci_bus.driver_list),
};

//加载pci bus
RTE_REGISTER_BUS(pci, rte_pci_bus.bus);
