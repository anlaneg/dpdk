/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include "mlx5_flow_os.h"

#include <rte_thread.h>

/* Key of thread specific flow workspace data. */
static rte_thread_key key_workspace;

/*tls初始化*/
int
mlx5_flow_os_init_workspace_once(void)
{
	if (rte_thread_key_create(&key_workspace, flow_release_workspace)) {
		DRV_LOG(ERR, "Can't create flow workspace data thread key.");
		return -ENOMEM;
	}
	return 0;
}

/*提取tls*/
void *
mlx5_flow_os_get_specific_workspace(void)
{
	return rte_thread_value_get(key_workspace);
}

/*设置私有数据*/
int
mlx5_flow_os_set_specific_workspace(struct mlx5_flow_workspace *data)
{
	return rte_thread_value_set(key_workspace, data);
}

/*移除tls*/
void
mlx5_flow_os_release_workspace(void)
{
	rte_thread_key_delete(key_workspace);
}
