/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include "rte_ethdev.h"
#include "rte_ethdev_driver.h"
#include "ethdev_private.h"

uint16_t
eth_dev_to_id(const struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return RTE_MAX_ETHPORTS;
	return dev - rte_eth_devices;
}

struct rte_eth_dev *
eth_find_device(const struct rte_eth_dev *start, rte_eth_cmp_t cmp,
		const void *data)
{
	struct rte_eth_dev *edev;
	ptrdiff_t idx;

	/* Avoid Undefined Behaviour */
	if (start != NULL &&
	    (start < &rte_eth_devices[0] ||
	     start > &rte_eth_devices[RTE_MAX_ETHPORTS]))
		return NULL;/*遍历范围不正确时，返回NULL*/

	if (start != NULL)
	    /*取start对应的id*/
		idx = eth_dev_to_id(start) + 1;
	else
		idx = 0;

	/*遍历所有设备，检查能匹配的edev*/
	for (; idx < RTE_MAX_ETHPORTS; idx++) {
		edev = &rte_eth_devices[idx];
		if (cmp(edev, data) == 0)
			return edev;
	}
	return NULL;
}

//callback完成列表内容解析，每个列表元素被callback调用一次
int
rte_eth_devargs_parse_list(char *str, rte_eth_devargs_callback_t callback,
	void *data)
{
	char *str_start;
	int state;
	int result;

	//非list情况，直接通过回调完成解析
	if (*str != '[')
		/* Single element, not a list */
		return callback(str, data);

	/* Sanity check, then strip the brackets */
	str_start = &str[strlen(str) - 1];
	if (*str_start != ']') {
	    /*最后一个字符必须为']'*/
		RTE_LOG(ERR, EAL, "(%s): List does not end with ']'\n", str);
		return -EINVAL;
	}
	str++;
	*str_start = '\0';

	/* Process list elements */
	state = 0;
	while (1) {
		if (state == 0) {
			if (*str == '\0')
			    /*解析内容结束*/
				break;
			if (*str != ',') {
				str_start = str;
				state = 1;
			}
		} else if (state == 1) {
			if (*str == ',' || *str == '\0') {
				if (str > str_start) {
					/* Non-empty string fragment */
					*str = '\0';
					/*完成数据解析*/
					result = callback(str_start, data);
					if (result < 0)
						return result;
				}
				state = 0;
			}
		}
		str++;
	}
	return 0;
}

static int
rte_eth_devargs_process_range(char *str, uint16_t *list/*出参，收集到的list项*/, uint16_t *len_list/*出参，list长度*/,
	const uint16_t max_list)
{
	uint16_t lo, hi, val;
	int result;

	//取范围的最小数及最大数
	result = sscanf(str, "%hu-%hu", &lo, &hi);
	if (result == 1) {
		if (*len_list >= max_list)
			return -ENOMEM;
		/*只有单个数的情况*/
		list[(*len_list)++] = lo;
	} else if (result == 2) {
	    /*范围情况，从lo到hi所有数目依次入列表*/
		if (lo >= hi || lo > RTE_MAX_ETHPORTS || hi > RTE_MAX_ETHPORTS)
			return -EINVAL;
		for (val = lo; val <= hi; val++) {
			if (*len_list >= max_list)
				return -ENOMEM;
			list[(*len_list)++] = val;
		}
	} else
		return -EINVAL;
	return 0;
}

//解析reprentor接口配置
int
rte_eth_devargs_parse_representor_ports(char *str, void *data)
{
	struct rte_eth_devargs *eth_da = data;

	return rte_eth_devargs_process_range(str, eth_da->representor_ports,
		&eth_da->nb_representor_ports, RTE_MAX_ETHPORTS);
}
