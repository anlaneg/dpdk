/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <assert.h>
#include <string.h>

#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_log.h>

#include "eal_private.h"
#include "eal_thread.h"

RTE_DECLARE_PER_LCORE(unsigned , _socket_id);

unsigned rte_socket_id(void)
{
	return RTE_PER_LCORE(_socket_id);
}

int
rte_lcore_has_role(unsigned int lcore_id, enum rte_lcore_role_t role)
{
	struct rte_config *cfg = rte_eal_get_configuration();

	if (lcore_id >= RTE_MAX_LCORE)
		return -EINVAL;

	return cfg->lcore_role[lcore_id] == role;
}

//检查cpusetp中cpu对应的numa,如果cpusetp中包含的cpu有多个
//且分属于不同的numa,则返回的numa为ANY,否则返回cpu对应的numa
int eal_cpuset_socket_id(rte_cpuset_t *cpusetp)
{
	unsigned cpu = 0;
	int socket_id = SOCKET_ID_ANY;
	int sid;

	if (cpusetp == NULL)
		return SOCKET_ID_ANY;

	do {
		//遍历检查cpusetp中包含的是那个cpu
		if (!CPU_ISSET(cpu, cpusetp))
			continue;

		//如果未设置值，则使用此cpu对应的numa
		if (socket_id == SOCKET_ID_ANY)
			socket_id = eal_cpu_socket_id(cpu);

		//取当前cpu对应的numa
		sid = eal_cpu_socket_id(cpu);

		//如果cpusetp中包含有多个cpu，且numa不相等，则将socket_id置为any
		//且不再尝试
		if (socket_id != sid) {
			socket_id = SOCKET_ID_ANY;
			break;
		}

	} while (++cpu < RTE_MAX_LCORE);

	return socket_id;
}

int
rte_thread_set_affinity(rte_cpuset_t *cpusetp)
{
	int s;
	unsigned lcore_id;
	pthread_t tid;

	tid = pthread_self();

	//使线程绑定cpu
	s = pthread_setaffinity_np(tid, sizeof(rte_cpuset_t), cpusetp);
	if (s != 0) {
		RTE_LOG(ERR, EAL, "pthread_setaffinity_np failed\n");
		return -1;
	}

	/* store socket_id in TLS for quick access */
	//设置当前线程默认使用的socket_id
	RTE_PER_LCORE(_socket_id) =
		eal_cpuset_socket_id(cpusetp);

	/* store cpuset in TLS for quick access */
	//存储本线程占用的cpuset
	memmove(&RTE_PER_LCORE(_cpuset), cpusetp,
		sizeof(rte_cpuset_t));

	lcore_id = rte_lcore_id();
	if (lcore_id != (unsigned)LCORE_ID_ANY) {
		/* EAL thread will update lcore_config */
		lcore_config[lcore_id].socket_id = RTE_PER_LCORE(_socket_id);
		memmove(&lcore_config[lcore_id].cpuset, cpusetp,
			sizeof(rte_cpuset_t));
	}

	return 0;
}

void
rte_thread_get_affinity(rte_cpuset_t *cpusetp)
{
	assert(cpusetp);
	memmove(cpusetp, &RTE_PER_LCORE(_cpuset),
		sizeof(rte_cpuset_t));
}

int
eal_thread_dump_affinity(char *str, unsigned size)
{
	rte_cpuset_t cpuset;
	unsigned cpu;
	int ret;
	unsigned int out = 0;

	//获取当前线程的cpuset
	rte_thread_get_affinity(&cpuset);

	//收集cpuset中的cpu列表
	for (cpu = 0; cpu < RTE_MAX_LCORE; cpu++) {
		if (!CPU_ISSET(cpu, &cpuset))
			continue;

		ret = snprintf(str + out,
			       size - out, "%u,", cpu);
		if (ret < 0 || (unsigned)ret >= size - out) {
			/* string will be truncated */
			ret = -1;
			goto exit;
		}

		out += ret;
	}

	ret = 0;
exit:
	/* remove the last separator */
	if (out > 0)
		str[out - 1] = '\0';

	return ret;
}


struct rte_thread_ctrl_params {
	void *(*start_routine)(void *);
	void *arg;
	pthread_barrier_t configured;
};

static void *rte_thread_init(void *arg)
{
	int ret;
	struct rte_thread_ctrl_params *params = arg;
	void *(*start_routine)(void *) = params->start_routine;
	void *routine_arg = params->arg;

	//等待barrier
	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	//执行回调
	return start_routine(routine_arg);
}

//创建线程，并配置cpu亲昵性
__rte_experimental int
rte_ctrl_thread_create(pthread_t *thread, const char *name,
		const pthread_attr_t *attr,
		void *(*start_routine)(void *), void *arg)
{
	struct rte_thread_ctrl_params *params;
	unsigned int lcore_id;
	rte_cpuset_t cpuset;
	int cpu_found, ret;

	params = malloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	params->start_routine = start_routine;
	params->arg = arg;

	//初始化thread barrier,等待数为1+1
	pthread_barrier_init(&params->configured, NULL, 2);

	ret = pthread_create(thread, attr, rte_thread_init, (void *)params);
	if (ret != 0) {
		free(params);
		return -ret;
	}

	//修改线程名称
	if (name != NULL) {
		ret = rte_thread_setname(*thread, name);
		if (ret < 0)
			RTE_LOG(DEBUG, EAL,
				"Cannot set name for ctrl thread\n");
	}

	//优先用所有unuse core
	cpu_found = 0;
	CPU_ZERO(&cpuset);
	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (eal_cpu_detected(lcore_id) &&
				rte_lcore_has_role(lcore_id, ROLE_OFF)) {
			CPU_SET(lcore_id, &cpuset);
			cpu_found = 1;
		}
	}
	/* if no detected cpu is off, use master core */
	if (!cpu_found)
		//无off的cpu,用master core
		CPU_SET(rte_get_master_lcore(), &cpuset);

	//设置线程亲昵性
	ret = pthread_setaffinity_np(*thread, sizeof(cpuset), &cpuset);
	if (ret < 0)
		goto fail;

	//知会线程开始跑
	ret = pthread_barrier_wait(&params->configured);
	if (ret == PTHREAD_BARRIER_SERIAL_THREAD) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}

	return 0;

fail:
	if (PTHREAD_BARRIER_SERIAL_THREAD ==
	    pthread_barrier_wait(&params->configured)) {
		pthread_barrier_destroy(&params->configured);
		free(params);
	}
	pthread_cancel(*thread);
	pthread_join(*thread, NULL);
	return -ret;
}
