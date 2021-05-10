/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Gaëtan Rivet
 */

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_class.h>
#include <rte_debug.h>

//记录rte_class的链表
static struct rte_class_list rte_class_list =
	TAILQ_HEAD_INITIALIZER(rte_class_list);

//rte_class注册（必须指定name)
void
rte_class_register(struct rte_class *class)
{
	RTE_VERIFY(class);
	RTE_VERIFY(class->name && strlen(class->name));

	//注册rte_class
	TAILQ_INSERT_TAIL(&rte_class_list, class, next);
	RTE_LOG(DEBUG, EAL, "Registered [%s] device class.\n", class->name);
}

//rte_class解注册
void
rte_class_unregister(struct rte_class *class)
{
	TAILQ_REMOVE(&rte_class_list, class, next);
	RTE_LOG(DEBUG, EAL, "Unregistered [%s] device class.\n", class->name);
}

/*自start位置开始执行class比对，如果命中返回class,否则返回NULL*/
struct rte_class *
rte_class_find(const struct rte_class *start, rte_class_cmp_t cmp/*class匹配*/,
	       const void *data)
{
	struct rte_class *cls;

	if (start != NULL)
		cls = TAILQ_NEXT(start, next);
	else
		cls = TAILQ_FIRST(&rte_class_list);
	while (cls != NULL) {
		if (cmp(cls, data) == 0)
			break;
		cls = TAILQ_NEXT(cls, next);
	}
	return cls;
}

/*class名称匹配*/
static int
cmp_class_name(const struct rte_class *class, const void *_name)
{
	const char *name = _name;

	return strcmp(class->name, name);
}

/*通过name查找指定rte_class*/
struct rte_class *
rte_class_find_by_name(const char *name)
{
	return rte_class_find(NULL, cmp_class_name, (const void *)name);
}
