/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include <string.h>

#include <rte_class.h>
#include <rte_compat.h>
#include <rte_errno.h>
#include <rte_kvargs.h>
#include <rte_log.h>

#include "rte_ethdev.h"
#include "rte_ethdev_core.h"
#include "rte_ethdev_driver.h"
#include "ethdev_private.h"

enum eth_params {
	RTE_ETH_PARAM_MAC,
	RTE_ETH_PARAM_REPRESENTOR,
	RTE_ETH_PARAM_MAX,
};

static const char * const eth_params_keys[] = {
	[RTE_ETH_PARAM_MAC] = "mac",
	[RTE_ETH_PARAM_REPRESENTOR] = "representor",
	[RTE_ETH_PARAM_MAX] = NULL,
};

struct eth_dev_match_arg {
	struct rte_device *device;
	struct rte_kvargs *kvlist;
};

//构造匹配参数
#define eth_dev_match_arg(d, k) \
	(&(const struct eth_dev_match_arg) { \
		.device = (d), \
		.kvlist = (k), \
	})

//通过mac查找rte_eth_dev_data
static int
eth_mac_cmp(const char *key __rte_unused,
		const char *value/*待解析的mac配置情况*/, void *opaque)
{
	struct rte_ether_addr mac;
	const struct rte_eth_dev_data *data = opaque;
	struct rte_eth_dev_info dev_info;
	uint32_t index;

	/* Parse devargs MAC address. */
	//将value转为mac地址
	if (rte_ether_unformat_addr(value, &mac) < 0)
		return -1; /* invalid devargs value */

	/* Return 0 if devargs MAC is matching one of the device MACs. */
	//检查设备是否具体此mac地址
	rte_eth_dev_info_get(data->port_id, &dev_info);
	for (index = 0; index < dev_info.max_mac_addrs; index++)
		if (rte_is_same_ether_addr(&mac, &data->mac_addrs[index]))
			return 0;
	return -1; /* no match */
}

/*检查opaque对应的eth_dev_data是否包含在value指定的representor配置中，如果在返回0，否则返回-1*/
static int
eth_representor_cmp(const char *key __rte_unused,
		const char *value/*待解析的representor配置*/, void *opaque/*待比较的eth_dev_data*/)
{
	int ret;
	char *values;
	const struct rte_eth_dev_data *data = opaque;
	struct rte_eth_devargs representors;
	uint16_t index;

	/*如果设备非representor,则直接返回*/
	if ((data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0)
		return -1; /* not a representor port */

	/* Parse devargs representor values. */
	values = strdup(value);
	if (values == NULL)
		return -1;
	memset(&representors, 0, sizeof(representors));
	ret = rte_eth_devargs_parse_list(values,
			rte_eth_devargs_parse_representor_ports,
			&representors);
	free(values);
	if (ret != 0)
		return -1; /* invalid devargs value */

	//遍历data->representor_id，查找是否已存在对应的representor_ports
	/* Return 0 if representor id is matching one of the values. */
	for (index = 0; index < representors.nb_representor_ports; index++)
		if (data->representor_id ==
				representors.representor_ports[index])
			return 0;
	return -1; /* no match */
}

static int
eth_dev_match(const struct rte_eth_dev *edev,
	      const void *_arg)
{
	int ret;
	const struct eth_dev_match_arg *arg = _arg;
	const struct rte_kvargs *kvlist = arg->kvlist;
	unsigned int pair;

	//设备未使用，则不能匹配
	if (edev->state == RTE_ETH_DEV_UNUSED)
		return -1;

	//如果后端设备不一致，则不能匹配
	if (arg->device != NULL && arg->device != edev->device)
		return -1;

	//在mac中查找edev，检查能否匹配
	ret = rte_kvargs_process(kvlist,
			eth_params_keys[RTE_ETH_PARAM_MAC],
			eth_mac_cmp, edev->data);
	if (ret != 0)
		return -1;

	//在representor中查找edev
	ret = rte_kvargs_process(kvlist,
			eth_params_keys[RTE_ETH_PARAM_REPRESENTOR],
			eth_representor_cmp, edev->data);
	if (ret != 0)
		return -1;

	/* search for representor key */
	for (pair = 0; pair < kvlist->count; pair++) {
		ret = strcmp(kvlist->pairs[pair].key,
				eth_params_keys[RTE_ETH_PARAM_REPRESENTOR]);
		if (ret == 0)
		    /*如果配置了reprensentor,则匹配*/
			break; /* there is a representor key */
	}
	/* if no representor key, default is to not match representor ports */
	if (ret != 0)
	    /*没有配置representor,仅edev为repreensentor时不能匹配*/
		if ((edev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) != 0)
			return -1; /* do not match any representor */

	return 0;
}

//执行设备枚举
static void *
eth_dev_iterate(const void *start/*起始*/,
		const char *str/*kv格式串*/,
		const struct rte_dev_iterator *it)
{
	struct rte_kvargs *kvargs = NULL;
	struct rte_eth_dev *edev = NULL;
	const char * const *valid_keys = NULL;

	//自str中解析valid_keys对应的kvargs
	if (str != NULL) {
		if (str[0] == '+') /* no validation of keys */
			str++;
		else
			valid_keys = eth_params_keys;
		kvargs = rte_kvargs_parse(str, valid_keys);
		if (kvargs == NULL) {
			RTE_LOG(ERR, EAL, "cannot parse argument list\n");
			rte_errno = EINVAL;
			return NULL;
		}
	}

	//从start设备开始执行eth_dev_match，查找与it->device,kvargs可匹配的dev
	edev = eth_find_device(start, eth_dev_match,
			       eth_dev_match_arg(it->device, kvargs));
	rte_kvargs_free(kvargs);
	return edev;
}

static struct rte_class rte_class_eth = {
	.dev_iterate = eth_dev_iterate,
};

RTE_REGISTER_CLASS(eth, rte_class_eth);
