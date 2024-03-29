/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Dmitry Kozlyuk
 */

#include <poll.h>
#include <string.h>
#include <unistd.h>

#include "cmdline_private.h"

void
terminal_adjust(struct cmdline *cl)
{
	struct termios term;

	tcgetattr(0, &cl->oldterm);

	memcpy(&term, &cl->oldterm, sizeof(term));
	term.c_lflag &= ~(ICANON | ECHO | ISIG);
	tcsetattr(0, TCSANOW, &term);/*设置新的终端配置*/

	/*关闭stdin的buffer能力*/
	setbuf(stdin, NULL);
}

void
terminal_restore(const struct cmdline *cl)
{
    /*恢复终端属性*/
	tcsetattr(fileno(stdin), TCSANOW, &cl->oldterm);
}

/*poll stdin描述符的读事件*/
int
cmdline_poll_char(struct cmdline *cl)
{
	struct pollfd pfd;

	pfd.fd = cl->s_in;
	pfd.events = POLLIN;
	pfd.revents = 0;

	return poll(&pfd, 1, 0);
}

ssize_t
cmdline_read_char(struct cmdline *cl, char *c)
{
    /*自标准输入读取一个字符*/
	return read(cl->s_in, c, 1);
}

int
cmdline_vdprintf(int fd, const char *format, va_list op)
{
    /*格式化内容向fd输出*/
	return vdprintf(fd, format, op);
}
