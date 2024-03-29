/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <rte_string_fns.h>

#include "cmdline_private.h"

/*校验输入的buffer,并执行其对应的命令*/
static void
cmdline_valid_buffer(struct rdline *rdl, const char *buf,
		     __rte_unused unsigned int size)
{
	struct cmdline *cl = rdl->opaque;/*取对应的命令行*/
	int ret;
	ret = cmdline_parse(cl, buf);
	/*针对出错，显示响应*/
	if (ret == CMDLINE_PARSE_AMBIGUOUS)
		cmdline_printf(cl, "Ambiguous command\n");
	else if (ret == CMDLINE_PARSE_NOMATCH)
		cmdline_printf(cl, "Command not found\n");
	else if (ret == CMDLINE_PARSE_BAD_ARGS)
		cmdline_printf(cl, "Bad arguments\n");
}

static int
cmdline_complete_buffer(struct rdline *rdl, const char *buf,
			char *dstbuf, unsigned int dstsize,
			int *state)
{
	struct cmdline *cl = rdl->opaque;
	return cmdline_complete(cl, buf, state, dstbuf, dstsize);
}

/*如果s_out存在，则向其输出单个字符*/
int
cmdline_write_char(struct rdline *rdl, char c)
{
	int ret = -1;
	struct cmdline *cl;

	if (!rdl)
		return -1;

	cl = rdl->opaque;

	if (cl->s_out >= 0)
		ret = write(cl->s_out, &c, 1);

	return ret;
}

/*设置命令提示符*/
void
cmdline_set_prompt(struct cmdline *cl, const char *prompt)
{
	if (!cl || !prompt)
		return;
	strlcpy(cl->prompt, prompt, sizeof(cl->prompt));
}

struct cmdline *
cmdline_new(cmdline_parse_ctx_t *ctx, const char *prompt/*提示信息*/, int s_in, int s_out)
{
	struct cmdline *cl;
	int ret;

	if (!ctx || !prompt)
		return NULL;

	cl = malloc(sizeof(struct cmdline));
	if (cl == NULL)
		return NULL;
	memset(cl, 0, sizeof(struct cmdline));
	cl->s_in = s_in;
	cl->s_out = s_out;
	cl->ctx = ctx;/*设置解析指令*/

	/*初始化readline*/
	ret = rdline_init(&cl->rdl, cmdline_write_char, cmdline_valid_buffer,
			cmdline_complete_buffer, cl);
	if (ret != 0) {
		free(cl);
		return NULL;
	}

	/*设置命令提示符，输出命令提示符*/
	cmdline_set_prompt(cl, prompt);
	rdline_newline(&cl->rdl, cl->prompt);

	return cl;
}

struct rdline*
cmdline_get_rdline(struct cmdline *cl)
{
	return &cl->rdl;
}

void
cmdline_free(struct cmdline *cl)
{
	dprintf("called\n");

	if (!cl)
		return;

	if (cl->s_in > 2)
		close(cl->s_in);
	if (cl->s_out != cl->s_in && cl->s_out > 2)
		close(cl->s_out);
	free(cl);
}

void
cmdline_printf(const struct cmdline *cl, const char *fmt, ...)
{
	va_list ap;

	if (!cl || !fmt)
		return;

	if (cl->s_out < 0)
		return;
	va_start(ap, fmt);
	cmdline_vdprintf(cl->s_out, fmt, ap);
	va_end(ap);
}

int
cmdline_in(struct cmdline *cl, const char *buf, int size)
{
	const char *history, *buffer;
	size_t histlen, buflen;
	int ret = 0;
	int i, same;

	if (!cl || !buf)
		return -1;

	for (i=0; i<size; i++) {
		/*提供命令给rdl,如果发现回车后，触发解析命令过程*/
		ret = rdline_char_in(&cl->rdl, buf[i]);

		if (ret == RDLINE_RES_VALIDATED) {
			/*有效命令，命令加入历史记录，显示新行*/
			buffer = rdline_get_buffer(&cl->rdl);
			history = rdline_get_history_item(&cl->rdl, 0);
			if (history) {
				histlen = strnlen(history, RDLINE_BUF_SIZE);
				same = !memcmp(buffer, history, histlen) &&
					buffer[histlen] == '\n';
			}
			else
				same = 0;
			buflen = strnlen(buffer, RDLINE_BUF_SIZE);
			if (buflen > 1 && !same)
				rdline_add_history(&cl->rdl, buffer);
			rdline_newline(&cl->rdl, cl->prompt);/*执行新行输出*/
		}
		else if (ret == RDLINE_RES_EOF)
			return -1;
		else if (ret == RDLINE_RES_EXITED)
			return -1;
	}
	return i;
}

void
cmdline_quit(struct cmdline *cl)
{
	if (!cl)
		return;
	cmdline_cancel(cl);
	rdline_quit(&cl->rdl);
}

int
cmdline_poll(struct cmdline *cl)
{
	int status;
	ssize_t read_status;
	char c;

	if (!cl)
		return -EINVAL;
	else if (cl->rdl.status == RDLINE_EXITED)
		return RDLINE_EXITED;

	status = cmdline_poll_char(cl);
	if (status < 0)
		return status;
	else if (status > 0) {
		c = -1;
		read_status = cmdline_read_char(cl, &c);
		if (read_status < 0)
			return read_status;

		if (read_status == 0) {
			/* end of file is implicit quit */
			cmdline_quit(cl);
		} else {
			status = cmdline_in(cl, &c, 1);
			if (status < 0 && cl->rdl.status != RDLINE_EXITED)
				return status;
		}
	}

	return cl->rdl.status;
}

void
cmdline_interact(struct cmdline *cl)
{
	char c;

	if (!cl)
		return;

	c = -1;
	while (1) {
		/*读取一个字符*/
		if (cmdline_read_char(cl, &c) <= 0)
			break;
		/*将字符提供给cl,检查是否为vt100控制符/执行输出/执行命令*/
		if (cmdline_in(cl, &c, 1) < 0)
			break;
	}
}
