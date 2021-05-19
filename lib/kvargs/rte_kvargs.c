/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2013 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <rte_os_shim.h>
#include <rte_string_fns.h>

#include "rte_kvargs.h"

/*
 * Receive a string with a list of arguments following the pattern
 * key=value,key=value,... and insert them into the list.
 * Params string will be copied to be modified.
 * list "[]" and list element splitter ",", "-" is treated as value.
 * Supported examples:
 *   k1=v1,k2=v2
 *   k1
 *   k1=x[0-1]y[1,3-5,9]z
 */
static int
rte_kvargs_tokenize(struct rte_kvargs *kvlist, const char *params)
{
	char *str, *start;
	bool in_list = false/*标记在list中*/, end_key = false/*key结束标记*/, end_value = false;
	bool save = false, end_pair = false;

	/* Copy the const char *params to a modifiable string
	 * to pass to rte_strsplit
	 */
	kvlist->str = strdup(params);
	if (kvlist->str == NULL)
		return -1;

	/* browse each key/value pair and add it in kvlist */
	/*这段代码可以考虑定义一个状态机并按状态进行解析，可读性会好一些。*/
	str = kvlist->str;
	start = str; /* start of current key or value */
	while (1) {
	    /*根据当前str内容，搞清楚当前解析状态*/
		switch (*str) {
		case '=': /* End of key. */
			end_key = true;
			save = true;
			break;
		case ',':
			/* End of value, skip comma in middle of range */
			if (!in_list) {
				if (end_key)
					end_value = true;
				else
					end_key = true;
				save = true;
				end_pair = true;
			}
			/*忽略掉在list中遇到,号的情况*/
			break;
		case '[': /* Start of list. */
			in_list = true;
			break;
		case ']': /* End of list.  */
			if (in_list)
				in_list = false;
			break;
		case '\0': /* End of string */
			if (end_key)
				end_value = true;
			else
				end_key = true;
			save = true;
			end_pair = true;
			break;
		default:
			break;
		}

		/*非解析关键节点，忽略掉继续分析*/
		if (!save) {
			/* Continue if not end of key or value. */
			str++;
			continue;
		}

		/*kvlist数目超限*/
		if (kvlist->count >= RTE_KVARGS_MAX)
			return -1;

		/*按状态记录key,value,未设置字符串结尾*/
		if (end_value)
			/* Value parsed */
			kvlist->pairs[kvlist->count].value = start;
		else if (end_key)
			/* Key parsed. */
			kvlist->pairs[kvlist->count].key = start;

		if (end_pair) {
			if (end_value || str != start)
				/* Ignore empty pair. */
				kvlist->count++;
			end_key = false;
			end_value = false;
			end_pair = false;
		}

		if (*str == '\0') /* End of string. */
			break;
		/*设置字符串结尾，开始下一轮*/
		*str = '\0';
		str++;
		start = str;
		save = false;
	}

	return 0;
}

/*
 * Determine whether a key is valid or not by looking
 * into a list of valid keys.
 */
static int
is_valid_key(const char * const valid[], const char *key_match)
{
	const char * const *valid_ptr;

	/*检查key_match是否属于valid数组的一个成员*/
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
static int
check_for_valid_keys(struct rte_kvargs *kvlist,
		const char * const valid[])
{
	unsigned i, ret;
	struct rte_kvargs_pair *pair;

	/*遍历kvlist的pairs,针对每个pair进行key有效性检查*/
	for (i = 0; i < kvlist->count; i++) {
		pair = &kvlist->pairs[i];
		ret = is_valid_key(valid, pair->key);
		if (!ret)
			return -1;
	}
	return 0;
}

/*
 * Return the number of times a given arg_name exists in the key/value list.
 * E.g. given a list = { rx = 0, rx = 1, tx = 2 } the number of args for
 * arg "rx" will be 2.
 */
unsigned
rte_kvargs_count(const struct rte_kvargs *kvlist, const char *key_match)
{
    /*返回key_match在kvlist中出现的次数*/
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
int
rte_kvargs_process(const struct rte_kvargs *kvlist,
		const char *key_match,
		arg_handler_t handler,
		void *opaque_arg)
{
    /*通过handler函数处理kvlist中的每个与key_match匹配的key*/
	const struct rte_kvargs_pair *pair;
	unsigned i;

	if (kvlist == NULL)
		return 0;

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

/* Lookup a value in an rte_kvargs list by its key. */
const char *
rte_kvargs_get(const struct rte_kvargs *kvlist, const char *key)
{
	unsigned int i;

	if (kvlist == NULL || key == NULL)
		return NULL;
	for (i = 0; i < kvlist->count; ++i) {
		if (strcmp(kvlist->pairs[i].key, key) == 0)
			return kvlist->pairs[i].value;
	}
	return NULL;
}

/*
 * Parse the arguments "key=value,key=value,..." string and return
 * an allocated structure that contains a key/value list. Also
 * check if only valid keys were used.
 */
struct rte_kvargs *
rte_kvargs_parse(const char *args, const char * const valid_keys[])
{
	struct rte_kvargs *kvlist;

	kvlist = malloc(sizeof(*kvlist));
	if (kvlist == NULL)
		return NULL;
	memset(kvlist, 0, sizeof(*kvlist));

	/*将args打散成key,value对儿*/
	if (rte_kvargs_tokenize(kvlist, args) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	/*指定了valid_keys,进行kvlist校验*/
	if (valid_keys != NULL && check_for_valid_keys(kvlist, valid_keys) < 0) {
		rte_kvargs_free(kvlist);
		return NULL;
	}

	return kvlist;
}

struct rte_kvargs *
rte_kvargs_parse_delim(const char *args, const char * const valid_keys[],
		       const char *valid_ends)
{
	struct rte_kvargs *kvlist = NULL;
	char *copy;
	size_t len;

	if (valid_ends == NULL)
		return rte_kvargs_parse(args, valid_keys);

	copy = strdup(args);
	if (copy == NULL)
		return NULL;

	len = strcspn(copy, valid_ends);
	copy[len] = '\0';

	kvlist = rte_kvargs_parse(copy, valid_keys);

	free(copy);
	return kvlist;
}

int
rte_kvargs_strcmp(const char *key __rte_unused,
		  const char *value, void *opaque)
{
	const char *str = opaque;

	return -abs(strcmp(str, value));
}
