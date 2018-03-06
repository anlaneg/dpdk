/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "cmdline_cirbuf.h"


int
cirbuf_init(struct cirbuf *cbuf, char *buf, unsigned int start, unsigned int maxlen)
{
	if (!cbuf || !buf)
		return -EINVAL;
	cbuf->maxlen = maxlen;
	cbuf->len = 0;
	cbuf->start = start;
	cbuf->end = start;
	cbuf->buf = buf;
	return 0;
}

/* multiple add */

int
cirbuf_add_buf_head(struct cirbuf *cbuf, const char *c, unsigned int n)
{
	unsigned int e;

	//要放入的字符，必须有足够的空间
	if (!cbuf || !c || !n || n > CIRBUF_GET_FREELEN(cbuf))
		return -EINVAL;

	e = CIRBUF_IS_EMPTY(cbuf) ? 1 : 0;

	if (n < cbuf->start + e) {
		dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->start - n + e, n);
		memcpy(cbuf->buf + cbuf->start - n + e, c, n);
	}
	else {
		dprintf("s[%d] -> d[%d] (%d)\n", + n - (cbuf->start + e), 0,
			cbuf->start + e);
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->maxlen - n +
			(cbuf->start + e), 0, n - (cbuf->start + e));
		memcpy(cbuf->buf, c  + n - (cbuf->start + e) , cbuf->start + e);
		memcpy(cbuf->buf + cbuf->maxlen - n + (cbuf->start + e), c,
		       n - (cbuf->start + e));
	}
	cbuf->len += n;//有效字符数增加
	cbuf->start += (cbuf->maxlen - n + e);//start反着向前走
	cbuf->start %= cbuf->maxlen;
	return n;
}

/* multiple add */

int
cirbuf_add_buf_tail(struct cirbuf *cbuf, const char *c, unsigned int n)
{
	unsigned int e;

	if (!cbuf || !c || !n || n > CIRBUF_GET_FREELEN(cbuf))
		return -EINVAL;

	e = CIRBUF_IS_EMPTY(cbuf) ? 1 : 0;

	if (n < cbuf->maxlen - cbuf->end - 1 + e) {
		dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->end + !e, n);
		memcpy(cbuf->buf + cbuf->end + !e, c, n);
	}
	else {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->end + !e, 0,
			cbuf->maxlen - cbuf->end - 1 + e);
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->maxlen - cbuf->end - 1 +
			e, 0, n - cbuf->maxlen + cbuf->end + 1 - e);
		memcpy(cbuf->buf + cbuf->end + !e, c, cbuf->maxlen -
		       cbuf->end - 1 + e);
		memcpy(cbuf->buf, c + cbuf->maxlen - cbuf->end - 1 + e,
		       n - cbuf->maxlen + cbuf->end + 1 - e);
	}
	cbuf->len += n;
	cbuf->end += n - e;
	cbuf->end %= cbuf->maxlen;
	return n;
}

/* add at head */
//在cbuf->start指定的位置前加一个'c'
static inline void
__cirbuf_add_head(struct cirbuf * cbuf, char c)
{
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		//当不为空时，cbuf->start指向已用的空间
		//start向前减
		cbuf->start += (cbuf->maxlen - 1);
		cbuf->start %= cbuf->maxlen;
		//
		//cbuf->start = ((cbuf->start + 1) % cbuf->maxlen)
	}
	cbuf->buf[cbuf->start] = c;
	cbuf->len ++;
}

int
cirbuf_add_head_safe(struct cirbuf * cbuf, char c)
{
	//会检查是否为满
	if (cbuf && !CIRBUF_IS_FULL(cbuf)) {
		__cirbuf_add_head(cbuf, c);
		return 0;
	}
	return -EINVAL;
}

//不检查是否为满，直接加入
void
cirbuf_add_head(struct cirbuf * cbuf, char c)
{
	__cirbuf_add_head(cbuf, c);
}

/* add at tail */

static inline void
__cirbuf_add_tail(struct cirbuf * cbuf, char c)
{
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		//当buf不为空时，end向后移
		cbuf->end ++;
		cbuf->end %= cbuf->maxlen;
	}
	cbuf->buf[cbuf->end] = c;//存入数据
	cbuf->len ++;
}

int
cirbuf_add_tail_safe(struct cirbuf * cbuf, char c)
{
	//会检查是否为满
	if (cbuf && !CIRBUF_IS_FULL(cbuf)) {
		__cirbuf_add_tail(cbuf, c);
		return 0;
	}
	return -EINVAL;
}

//不检查是否为满
void
cirbuf_add_tail(struct cirbuf * cbuf, char c)
{
	__cirbuf_add_tail(cbuf, c);
}


//字符向左移
static inline void
__cirbuf_shift_left(struct cirbuf *cbuf)
{
	unsigned int i;
	char tmp = cbuf->buf[cbuf->start];

	//将cbuf->start指向的字符向左移一个索引位
	for (i=0 ; i<cbuf->len ; i++) {
		cbuf->buf[(cbuf->start+i)%cbuf->maxlen] =
			cbuf->buf[(cbuf->start+i+1)%cbuf->maxlen];
	}
	//start左移一位，end左移一位
	cbuf->buf[(cbuf->start-1+cbuf->maxlen)%cbuf->maxlen] = tmp;
	cbuf->start += (cbuf->maxlen - 1);
	cbuf->start %= cbuf->maxlen;
	cbuf->end += (cbuf->maxlen - 1);
	cbuf->end %= cbuf->maxlen;
}

//字符向右移
static inline void
__cirbuf_shift_right(struct cirbuf *cbuf)
{
	unsigned int i;
	char tmp = cbuf->buf[cbuf->end];

	for (i=0 ; i<cbuf->len ; i++) {
		cbuf->buf[(cbuf->end+cbuf->maxlen-i)%cbuf->maxlen] =
			cbuf->buf[(cbuf->end+cbuf->maxlen-i-1)%cbuf->maxlen];
	}
	cbuf->buf[(cbuf->end+1)%cbuf->maxlen] = tmp;
	cbuf->start += 1;
	cbuf->start %= cbuf->maxlen;
	cbuf->end += 1;
	cbuf->end %= cbuf->maxlen;
}

/* XXX we could do a better algorithm here... */
//使start移动到0位置（通过左移或者右移来实现）
int
cirbuf_align_left(struct cirbuf * cbuf)
{
	if (!cbuf)
		return -EINVAL;

	if (cbuf->start < cbuf->maxlen/2) {
		while (cbuf->start != 0) {
			__cirbuf_shift_left(cbuf);
		}
	}
	else {
		while (cbuf->start != 0) {
			__cirbuf_shift_right(cbuf);
		}
	}

	return 0;
}

/* XXX we could do a better algorithm here... */
//使end移动到cbuf->maxlen-1位置
int
cirbuf_align_right(struct cirbuf * cbuf)
{
	if (!cbuf)
		return -EINVAL;

	if (cbuf->start >= cbuf->maxlen/2) {
		while (cbuf->end != cbuf->maxlen-1) {
			__cirbuf_shift_left(cbuf);
		}
	}
	else {
		while (cbuf->start != cbuf->maxlen-1) {
			__cirbuf_shift_right(cbuf);
		}
	}

	return 0;
}

/* buffer del */

//移除掉从start位置开始size字节的数据
int
cirbuf_del_buf_head(struct cirbuf *cbuf, unsigned int size)
{
	if (!cbuf || !size || size > CIRBUF_GET_LEN(cbuf))
		return -EINVAL;

	cbuf->len -= size;
	if (CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->start += size - 1;
		cbuf->start %= cbuf->maxlen;
	}
	else {
		cbuf->start += size;
		cbuf->start %= cbuf->maxlen;
	}
	return 0;
}

/* buffer del */

//移除掉从end结尾向前size字节的数据
int
cirbuf_del_buf_tail(struct cirbuf *cbuf, unsigned int size)
{
	if (!cbuf || !size || size > CIRBUF_GET_LEN(cbuf))
		return -EINVAL;

	cbuf->len -= size;
	if (CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->end  += (cbuf->maxlen - size + 1);
		cbuf->end %= cbuf->maxlen;
	}
	else {
		cbuf->end  += (cbuf->maxlen - size);
		cbuf->end %= cbuf->maxlen;
	}
	return 0;
}

/* del at head */
//移掉一个字符
static inline void
__cirbuf_del_head(struct cirbuf * cbuf)
{
	cbuf->len --;
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->start ++;
		cbuf->start %= cbuf->maxlen;
	}
}

int
cirbuf_del_head_safe(struct cirbuf * cbuf)
{
	if (cbuf && !CIRBUF_IS_EMPTY(cbuf)) {
		__cirbuf_del_head(cbuf);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_del_head(struct cirbuf * cbuf)
{
	__cirbuf_del_head(cbuf);
}

/* del at tail */
//从end位置向前移掉一个字符
static inline void
__cirbuf_del_tail(struct cirbuf * cbuf)
{
	cbuf->len --;
	if (!CIRBUF_IS_EMPTY(cbuf)) {
		cbuf->end  += (cbuf->maxlen - 1);
		cbuf->end %= cbuf->maxlen;
	}
}

int
cirbuf_del_tail_safe(struct cirbuf * cbuf)
{
	if (cbuf && !CIRBUF_IS_EMPTY(cbuf)) {
		__cirbuf_del_tail(cbuf);
		return 0;
	}
	return -EINVAL;
}

void
cirbuf_del_tail(struct cirbuf * cbuf)
{
	__cirbuf_del_tail(cbuf);
}

/* convert to buffer */
//从start位置到end位置取最多size个字符
int
cirbuf_get_buf_head(struct cirbuf *cbuf, char *c, unsigned int size)
{
	unsigned int n;

	if (!cbuf || !c)
		return -EINVAL;

	n = (size < CIRBUF_GET_LEN(cbuf)) ? size : CIRBUF_GET_LEN(cbuf);

	if (!n)
		return 0;

	if (cbuf->start <= cbuf->end) {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->start, 0, n);
		memcpy(c, cbuf->buf + cbuf->start , n);
	}
	else {
		/* check if we need to go from end to the beginning */
		if (n <= cbuf->maxlen - cbuf->start) {
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->start, n);
			memcpy(c, cbuf->buf + cbuf->start , n);
		}
		else {
			dprintf("s[%d] -> d[%d] (%d)\n", cbuf->start, 0,
				cbuf->maxlen - cbuf->start);
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->maxlen - cbuf->start,
				n - cbuf->maxlen + cbuf->start);
			memcpy(c, cbuf->buf + cbuf->start , cbuf->maxlen - cbuf->start);
			memcpy(c + cbuf->maxlen - cbuf->start, cbuf->buf,
				   n - cbuf->maxlen + cbuf->start);
		}
	}
	return n;
}

/* convert to buffer */
//从end向前，最多取size个字符
int
cirbuf_get_buf_tail(struct cirbuf *cbuf, char *c, unsigned int size)
{
	unsigned int n;

	if (!cbuf || !c)
		return -EINVAL;

	n = (size < CIRBUF_GET_LEN(cbuf)) ? size : CIRBUF_GET_LEN(cbuf);

	if (!n)
		return 0;

	if (cbuf->start <= cbuf->end) {
		dprintf("s[%d] -> d[%d] (%d)\n", cbuf->end - n + 1, 0, n);
		memcpy(c, cbuf->buf + cbuf->end - n + 1, n);
	}
	else {
		/* check if we need to go from end to the beginning */
		if (n <= cbuf->end + 1) {
			dprintf("s[%d] -> d[%d] (%d)\n", 0, cbuf->end - n + 1, n);
			memcpy(c, cbuf->buf + cbuf->end - n + 1, n);
		}
		else {
			dprintf("s[%d] -> d[%d] (%d)\n", 0,
				cbuf->maxlen - cbuf->start, cbuf->end + 1);
			dprintf("s[%d] -> d[%d] (%d)\n",
				cbuf->maxlen - n + cbuf->end + 1, 0, n - cbuf->end - 1);
			memcpy(c + cbuf->maxlen - cbuf->start,
					       cbuf->buf, cbuf->end + 1);
			memcpy(c, cbuf->buf + cbuf->maxlen - n + cbuf->end +1,
				   n - cbuf->end - 1);
		}
	}
	return n;
}

/* get head or get tail */
//取start指向的字符
char
cirbuf_get_head(struct cirbuf * cbuf)
{
	return cbuf->buf[cbuf->start];
}

/* get head or get tail */
//取end指向的字符
char
cirbuf_get_tail(struct cirbuf * cbuf)
{
	return cbuf->buf[cbuf->end];
}
