/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_ether.h>

#include "cmdline_parse.h"
#include "cmdline_parse_etheraddr.h"

struct cmdline_token_ops cmdline_token_etheraddr_ops = {
	.parse = cmdline_parse_etheraddr,/*解析以太地址*/
	.complete_get_nb = NULL,
	.complete_get_elt = NULL,
	.get_help = cmdline_get_help_etheraddr,/*显示帮助信息*/
};

int
cmdline_parse_etheraddr(__rte_unused cmdline_parse_token_hdr_t *tk,
	const char *buf/*待解析内容*/, void *res/*出参，解析内容*/, unsigned ressize/*res内存长度*/)
{
	unsigned int token_len = 0;
	/*mac一共是6个字节，每个字节大约占用3个字符，故计18个字符,例如'AA[:|\0]'*/
	char ether_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr tmp;

	if (res && ressize < sizeof(tmp))
		return -1;

	if (!buf || ! *buf)
		return -1;

	/*跳出，直到遇到token分隔符*/
	while (!cmdline_isendoftoken(buf[token_len]))
		token_len++;

	/* if token doesn't match possible string lengths... */
	if (token_len >= RTE_ETHER_ADDR_FMT_SIZE)
	    /*token长度过长，报错*/
		return -1;

	/*token内容为ether*/
	strlcpy(ether_str, buf, token_len + 1);

	/*解析mac地址*/
	if (rte_ether_unformat_addr(ether_str, &tmp) < 0)
		return -1;

	/*将解析内容填充到res中*/
	if (res)
		memcpy(res, &tmp, sizeof(tmp));

	/*返回匹配的token长度*/
	return token_len;
}

int
cmdline_get_help_etheraddr(__rte_unused cmdline_parse_token_hdr_t *tk,
			       char *dstbuf, unsigned int size)
{
	int ret;

	/*格式化输出帮助信息*/
	ret = snprintf(dstbuf, size, "Ethernet address");
	if (ret < 0)
		return -1;
	return 0;
}
