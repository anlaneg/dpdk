/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2013 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <string.h>
#include <stdlib.h>

#include <rte_log.h>
#include <rte_string_fns.h>

#include "rte_kvargs.h"

/*
 * Receive a string with a list of arguments following the pattern
 * key=value,key=value,... and insert them into the list.
 * strtok() is used so the params string will be copied to be modified.
 */
//将abc=33,def=vaue,dafdas=ab解析成一个数组，存放在kvlist中
static int
rte_kvargs_tokenize(struct rte_kvargs *kvlist, const char *params)
{
	unsigned i;
	char *str;
	char *ctx1 = NULL;
	char *ctx2 = NULL;

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	kvlist->str = strdup(params);
	if (kvlist->str == NULL) {
		RTE_LOG(ERR, PMD, "Cannot parse arguments: not enough memory\n");
		return -1;
	}

	/* browse each key/value pair and add it in kvlist */
	//类似这样的格式abc=1,dfer=45,dds=87
	str = kvlist->str;
	while ((str = strtok_r(str, RTE_KVARGS_PAIRS_DELIM, &ctx1)) != NULL) {

		i = kvlist->count;
		if (i >= RTE_KVARGS_MAX) {
			RTE_LOG(ERR, PMD, "Cannot parse arguments: list full\n");
			return -1;
		}

		kvlist->pairs[i].key = strtok_r(str, RTE_KVARGS_KV_DELIM, &ctx2);
		kvlist->pairs[i].value = strtok_r(NULL, RTE_KVARGS_KV_DELIM, &ctx2);
		if (kvlist->pairs[i].key == NULL || kvlist->pairs[i].value == NULL) {
			RTE_LOG(ERR, PMD,
				"Cannot parse arguments: wrong key or value\n"
				"params=<%s>\n", params);
			return -1;
		}

		kvlist->count++;
		str = NULL;
	}

	return 0;
}

/*
 * Determine whether a key is valid or not by looking
 * into a list of valid keys.
 */
//检查key_match是否在valid数组之内
static int
is_valid_key(const char * const valid[], const char *key_match)
{
	const char * const *valid_ptr;

	for (valid_ptr = valid; *valid_ptr != NULL; valid_ptr++) {
		if (strcmp(key_match, *valid_ptr) == 0)
			return 1;
	}
	return 0;
}

/*
 * Determine whether all keys are valid or not by looking
 * into a list of valid keys.
 */
//检查kvlist是否包含有非法的keys,如果有，返回-1,否则近回0
static int
check_for_valid_keys(struct rte_kvargs *kvlist,
		const char * const valid[])
{
	unsigned i, ret;
	struct rte_kvargs_pair *pair;

	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		ret = is_valid_key(valid, pair->key);
		if (!ret) {
			//我们看到了一个未知的参数，报错
			RTE_LOG(ERR, PMD,
				"Error parsing device, invalid key <%s>\n",
				pair->key);
			return -1;
		}
	}
	return 0;
}

/*
 * Return the number of times a given arg_name exists in the key/value list.
 * E.g. given a list = { rx = 0, rx = 1, tx = 2 } the number of args for
 * arg "rx" will be 2.
 */
//计算key-match在kvlist中出现的次数
unsigned
rte_kvargs_count(const struct rte_kvargs *kvlist, const char *key_match)
{
	const struct rte_kvargs_pair *pair;
	unsigned i, ret;

	ret = 0;
	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (key_match == NULL || strcmp(pair->key, key_match) == 0)
			ret++;
	}

	return ret;
}

/*
 * For each matching key, call the given handler function.
 */
//查找参数key-match,取其对应的值，并执行handler
//如果key-match为NULL，则kvlist中所有参数需要挨个执行
int
rte_kvargs_process(const struct rte_kvargs *kvlist,
		const char *key_match,
		arg_handler_t handler,
		void *opaque_arg)
{
	const struct rte_kvargs_pair *pair;
	unsigned i;

	//key_match与kvlist发生匹配，则执行对应回调
	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		if (key_match == NULL || strcmp(pair->key, key_match) == 0) {
			if ((*handler)(pair->key, pair->value, opaque_arg) < 0)
				return -1;
		}
	}
	return 0;
}

/* free the rte_kvargs structure */
void
rte_kvargs_free(struct rte_kvargs *kvlist)
{
	if (!kvlist)
		return;

	free(kvlist->str);
	free(kvlist);
}

/*
 * Parse the arguments "key=value,key=value,..." string and return
 * an allocated structure that contains a key/value list. Also
 * check if only valid keys were used.
 */
//解析参数，检查是否为有效参数，有效时返回kvlist,否则为NULL
struct rte_kvargs *
rte_kvargs_parse(const char *args, const char * const valid_keys[])
{
	struct rte_kvargs *kvlist;

	kvlist = malloc(sizeof(*kvlist));
	if (kvlist == NULL)
		return NULL;
	memset(kvlist, 0, sizeof(*kvlist));

	//将args解析为key,values（逗号划分）
	if (rte_kvargs_tokenize(kvlist, args) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	//如果指定了valid_keys，则检查kvlist中的key是否包含于valid_keys
	//如果不包含在valid_keys中，则释放kvlist,返回NULL
	if (valid_keys != NULL && check_for_valid_keys(kvlist, valid_keys) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	return kvlist;
}
