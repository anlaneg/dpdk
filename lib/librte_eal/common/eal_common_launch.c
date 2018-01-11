/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_launch.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_pause.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>

/*
 * Wait until a lcore finished its job.
 */
int
rte_eal_wait_lcore(unsigned slave_id)
{
	if (lcore_config[slave_id].state == WAIT)
		return 0;

	while (lcore_config[slave_id].state != WAIT &&
	       lcore_config[slave_id].state != FINISHED)
		rte_pause();

	rte_rmb();

	/* we are in finished state, go to wait state */
	lcore_config[slave_id].state = WAIT;
	return lcore_config[slave_id].ret;
}

/*
 * Check that every SLAVE lcores are in WAIT state, then call
 * rte_eal_remote_launch() for all of them. If call_master is true
 * (set to CALL_MASTER), also call the function on the master lcore.
 */
int
rte_eal_mp_remote_launch(int (*f)(void *), void *arg,
			 enum rte_rmt_call_master_t call_master)
{
	int lcore_id;
	int master = rte_get_master_lcore();

	/* check state of lcores */
	//各slave必须处于wait状态
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (lcore_config[lcore_id].state != WAIT)
			return -EBUSY;
	}

	/* send messages to cores */
	//逐个向slave安排任务，并等待答复
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(f, arg, lcore_id);
	}

	//作为master，理论下也是要做事情的，这里做做样子，让master
	//也处理处理任务。由上层控制，是否做
	if (call_master == CALL_MASTER) {
		lcore_config[master].ret = f(arg);
		lcore_config[master].state = FINISHED;
	}

	return 0;
}

/*
 * Return the state of the lcore identified by slave_id.
 */
enum rte_lcore_state_t
rte_eal_get_lcore_state(unsigned lcore_id)
{
	return lcore_config[lcore_id].state;
}

/*
 * Do a rte_eal_wait_lcore() for every lcore. The return values are
 * ignored.
 */
//等待各slave完成工作，并在确认完成后，将状态直为wait状态（下一步做什么，等待指令）
void
rte_eal_mp_wait_lcore(void)
{
	unsigned lcore_id;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_wait_lcore(lcore_id);
	}
}
