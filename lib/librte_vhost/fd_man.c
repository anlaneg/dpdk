/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include <rte_common.h>
#include <rte_log.h>

#include "fd_man.h"


#define RTE_LOGTYPE_VHOST_FDMAN RTE_LOGTYPE_USER1

#define FDPOLLERR (POLLERR | POLLHUP | POLLNVAL)

//自last_valid_idx开始，向前（回退到0）找一个有效的fd
//即找到最后一个有效的fd
static int
get_last_valid_idx(struct fdset *pfdset, int last_valid_idx)
{
	int i;

	for (i = last_valid_idx; i >= 0 && pfdset->fd[i].fd == -1; i--)
		;

	return i;//如果此位置>=0,则此位置拥有有效的fd
}

//移动src位置的fdset到dst位置
static void
fdset_move(struct fdset *pfdset, int dst, int src)
{
	pfdset->fd[dst]    = pfdset->fd[src];
	pfdset->rwfds[dst] = pfdset->rwfds[src];
}

//将无效的fd移除，收紧pfdset
static void
fdset_shrink_nolock(struct fdset *pfdset)
{
	int i;
	int last_valid_idx = get_last_valid_idx(pfdset, pfdset->num - 1);

	//最多需要从0,检查到last_valid_idx位置
	for (i = 0; i < last_valid_idx; i++) {
		//如果是有效的，则继续
		if (pfdset->fd[i].fd != -1)
			continue;

		//i位置fd是无效的，我们将last_valid_idx位置的fd移动到此位置（防止整个数组移动）
		fdset_move(pfdset, i, last_valid_idx);

		//刚刚我们把最后一个向前移动了，所以我们需要从last_valid_idx-1位置开始向前再找一个有效的fd
		last_valid_idx = get_last_valid_idx(pfdset, last_valid_idx - 1);
	}

	pfdset->num = last_valid_idx + 1;
}

/*
 * Find deleted fd entries and remove them
 */
//移除掉无效的fd,保持fd数组连续
static void
fdset_shrink(struct fdset *pfdset)
{
	pthread_mutex_lock(&pfdset->fd_mutex);
	fdset_shrink_nolock(pfdset);
	pthread_mutex_unlock(&pfdset->fd_mutex);
}

/**
 * Returns the index in the fdset for a given fd.
 * @return
 *   index for the fd, or -1 if fd isn't in the fdset.
 */
//用fd找下标
static int
fdset_find_fd(struct fdset *pfdset, int fd)
{
	int i;

	for (i = 0; i < pfdset->num && pfdset->fd[i].fd != fd; i++)
		;

	return i == pfdset->num ? -1 : i;
}

//注册fd及其回调（使用idx位置处的空间，其读回调rcb,写回计wcb,dat为回调时参数）
static void
fdset_add_fd(struct fdset *pfdset, int idx, int fd,
	fd_cb rcb, fd_cb wcb, void *dat)
{
	struct fdentry *pfdentry = &pfdset->fd[idx];
	struct pollfd *pfd = &pfdset->rwfds[idx];

	//设置pfdentry
	pfdentry->fd  = fd;
	pfdentry->rcb = rcb;
	pfdentry->wcb = wcb;
	pfdentry->dat = dat;

	//poll参数初始化
	pfd->fd = fd;
	pfd->events  = rcb ? POLLIN : 0;//有读回调，关注in事件
	pfd->events |= wcb ? POLLOUT : 0;//有写回调，关注out事件
	pfd->revents = 0;
}

//fdset初始化
void
fdset_init(struct fdset *pfdset)
{
	int i;

	if (pfdset == NULL)
		return;

	for (i = 0; i < MAX_FDS; i++) {
		pfdset->fd[i].fd = -1;
		pfdset->fd[i].dat = NULL;
	}
	pfdset->num = 0;
}

/**
 * Register the fd in the fdset with read/write handler and context.
 */
//向fdset中注册一个fd及其回调
int
fdset_add(struct fdset *pfdset, int fd, fd_cb rcb, fd_cb wcb, void *dat)
{
	int i;

	if (pfdset == NULL || fd == -1)
		return -1;

	pthread_mutex_lock(&pfdset->fd_mutex);
	//找一个空闲的位置，默认按序分配，如果达到最后一个，则检查是否可以检测是否可以释放
	//尽量给分配一个空闲的位置
	i = pfdset->num < MAX_FDS ? pfdset->num++ : -1;
	if (i == -1) {
		fdset_shrink_nolock(pfdset);
		i = pfdset->num < MAX_FDS ? pfdset->num++ : -1;
		if (i == -1) {
			//没有空闲的位置了，失败返回
			pthread_mutex_unlock(&pfdset->fd_mutex);
			return -2;
		}
	}

	//加入到i位置
	fdset_add_fd(pfdset, i, fd, rcb, wcb, dat);
	pthread_mutex_unlock(&pfdset->fd_mutex);

	return 0;
}

/**
 *  Unregister the fd from the fdset.
 *  Returns context of a given fd or NULL.
 */
//移除指定fd
void *
fdset_del(struct fdset *pfdset, int fd)
{
	int i;
	void *dat = NULL;

	if (pfdset == NULL || fd == -1)
		return NULL;

	do {
		pthread_mutex_lock(&pfdset->fd_mutex);

		//找到这个fd,如果这个fd存在，并且不是busy状态，就将其标记为删除（不收缩，由dispatch线程负责，收缩）
		//如果处于busy状态（dispatch线程正在执行回调），则解锁并重试。（加锁是防止将置为0的fd再次置为1）
		i = fdset_find_fd(pfdset, fd);
		if (i != -1 && pfdset->fd[i].busy == 0) {
			/* busy indicates r/wcb is executing! */
			dat = pfdset->fd[i].dat;
			pfdset->fd[i].fd = -1;
			pfdset->fd[i].rcb = pfdset->fd[i].wcb = NULL;
			pfdset->fd[i].dat = NULL;
			i = -1;
		}
		pthread_mutex_unlock(&pfdset->fd_mutex);
	} while (i != -1);//如果没有移除，则再次尝试

	return dat;
}


/**
 * This functions runs in infinite blocking loop until there is no fd in
 * pfdset. It calls corresponding r/w handler if there is event on the fd.
 *
 * Before the callback is called, we set the flag to busy status; If other
 * thread(now rte_vhost_driver_unregister) calls fdset_del concurrently, it
 * will wait until the flag is reset to zero(which indicates the callback is
 * finished), then it could free the context after fdset_del.
 */
//poll相关的fd,并对依据事件调用不同的回调函数
void *
fdset_event_dispatch(void *arg)
{
	int i;
	struct pollfd *pfd;
	struct fdentry *pfdentry;
	fd_cb rcb, wcb;
	void *dat;
	int fd, numfds;
	int remove1, remove2;
	int need_shrink;
	struct fdset *pfdset = arg;
	int val;

	if (pfdset == NULL)
		return NULL;

	while (1) {

		/*
		 * When poll is blocked, other threads might unregister
		 * listenfds from and register new listenfds into fdset.
		 * When poll returns, the entries for listenfds in the fdset
		 * might have been updated. It is ok if there is unwanted call
		 * for new listenfds.
		 */
		pthread_mutex_lock(&pfdset->fd_mutex);
		numfds = pfdset->num;
		pthread_mutex_unlock(&pfdset->fd_mutex);

		val = poll(pfdset->rwfds, numfds, 1000 /* millisecs */);
		if (val < 0)
			continue;

		need_shrink = 0;
		for (i = 0; i < numfds; i++) {
			pthread_mutex_lock(&pfdset->fd_mutex);

			pfdentry = &pfdset->fd[i];
			fd = pfdentry->fd;
			pfd = &pfdset->rwfds[i];

			//fd无效，置可收缩
			if (fd < 0) {
				need_shrink = 1;
				pthread_mutex_unlock(&pfdset->fd_mutex);
				continue;
			}

			//此fd无事件
			if (!pfd->revents) {
				pthread_mutex_unlock(&pfdset->fd_mutex);
				continue;
			}

			remove1 = remove2 = 0;

			rcb = pfdentry->rcb;
			wcb = pfdentry->wcb;
			dat = pfdentry->dat;
			pfdentry->busy = 1;

			pthread_mutex_unlock(&pfdset->fd_mutex);

			//如果关心读回调，且有读事件或者出错
			if (rcb && pfd->revents & (POLLIN | FDPOLLERR))
				//调用读回调
				rcb(fd, dat, &remove1);
			//如果关心写事件，且有写事件或者出错，则调用写回调
			if (wcb && pfd->revents & (POLLOUT | FDPOLLERR))
				wcb(fd, dat, &remove2);
			pfdentry->busy = 0;
			/*
			 * fdset_del needs to check busy flag.
			 * We don't allow fdset_del to be called in callback
			 * directly.
			 */
			/*
			 * When we are to clean up the fd from fdset,
			 * because the fd is closed in the cb,
			 * the old fd val could be reused by when creates new
			 * listen fd in another thread, we couldn't call
			 * fd_set_del.
			 */
			//如果需要移除fd，标记需要收缩
			if (remove1 || remove2) {
				pfdentry->fd = -1;
				need_shrink = 1;
			}
		}

		//如果需要收缩，则将fd数组紧缩
		if (need_shrink)
			fdset_shrink(pfdset);
	}

	return NULL;
}

static void
fdset_pipe_read_cb(int readfd, void *dat __rte_unused,
		   int *remove __rte_unused)
{
	char charbuf[16];
	int r = read(readfd, charbuf, sizeof(charbuf));
	/*
	 * Just an optimization, we don't care if read() failed
	 * so ignore explicitly its return value to make the
	 * compiler happy
	 */
	RTE_SET_USED(r);
}

void
fdset_pipe_uninit(struct fdset *fdset)
{
	fdset_del(fdset, fdset->u.readfd);
	close(fdset->u.readfd);
	close(fdset->u.writefd);
}

int
fdset_pipe_init(struct fdset *fdset)
{
	int ret;

	if (pipe(fdset->u.pipefd) < 0) {
		RTE_LOG(ERR, VHOST_FDMAN,
			"failed to create pipe for vhost fdset\n");
		return -1;
	}

	ret = fdset_add(fdset, fdset->u.readfd,
			fdset_pipe_read_cb, NULL, NULL);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_FDMAN,
			"failed to add pipe readfd %d into vhost server fdset\n",
			fdset->u.readfd);

		fdset_pipe_uninit(fdset);
		return -1;
	}

	return 0;
}

void
fdset_pipe_notify(struct fdset *fdset)
{
	int r = write(fdset->u.writefd, "1", 1);
	/*
	 * Just an optimization, we don't care if write() failed
	 * so ignore explicitly its return value to make the
	 * compiler happy
	 */
	RTE_SET_USED(r);

}
