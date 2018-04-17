/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _FD_MAN_H_
#define _FD_MAN_H_
#include <stdint.h>
#include <pthread.h>
#include <poll.h>

#define MAX_FDS 1024

typedef void (*fd_cb)(int fd, void *dat, int *remove);

struct fdentry {
	int fd;		/* -1 indicates this entry is empty */ //fd
	fd_cb rcb;	/* callback when this fd is readable. */ //读回调
	fd_cb wcb;	/* callback when this fd is writeable.*/ //写回调
	void *dat;	/* fd context */ //回调参数
	int busy;	/* whether this entry is being used in cb. */ //指明entry是否正在被用
};

struct fdset {
	struct pollfd rwfds[MAX_FDS];//poll参数
	struct fdentry fd[MAX_FDS];//注册所有fd用于轮询
	pthread_mutex_t fd_mutex;//保护rwfds,fd数组
	int num;	/* current fd number of this fdset *///rwfds,fd数组的有效长度

	union pipefds {
		struct {
			int pipefd[2];
		};
		struct {
			int readfd;
			int writefd;
		};
	} u;
};


void fdset_init(struct fdset *pfdset);

int fdset_add(struct fdset *pfdset, int fd,
	fd_cb rcb, fd_cb wcb, void *dat);

void *fdset_del(struct fdset *pfdset, int fd);

void *fdset_event_dispatch(void *arg);

int fdset_pipe_init(struct fdset *fdset);

void fdset_pipe_uninit(struct fdset *fdset);

void fdset_pipe_notify(struct fdset *fdset);

#endif
