/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <rte_string_fns.h>

#include "cmdline_parse.h"
#include "cmdline_parse_string.h"

struct cmdline_token_ops cmdline_token_string_ops = {
	.parse = cmdline_parse_string,
	.complete_get_nb = cmdline_complete_get_nb_string,
	.complete_get_elt = cmdline_complete_get_elt_string,
	.get_help = cmdline_get_help_string,
};

#define CHOICESTRING_HELP "Mul-choice STRING"
#define ANYSTRING_HELP    "Any STRING"
#define ANYSTRINGS_HELP   "Any STRINGS"
#define FIXEDSTRING_HELP  "Fixed STRING"

static unsigned int
get_token_len(const char *s)
{
	char c;
	unsigned int i=0;

	c = s[i];
	while (c!='#' && c!='\0') {
		i++;
		c = s[i];
	}
	return i;
}

static const char *
get_next_token(const char *s)
{
	unsigned int i;
	i = get_token_len(s);
	if (s[i] == '#')
		return s+i+1;
	return NULL;
}

int
cmdline_parse_string(cmdline_parse_token_hdr_t *tk, const char *buf, void *res,
	unsigned ressize)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	unsigned int token_len;
	const char *str;

	if (res && ressize < STR_TOKEN_SIZE)
		/*提供的res不满足要求，退出*/
		return -1;

	if (!tk || !buf || ! *buf)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;

	sd = &tk2->string_data;

	/* fixed string (known single token) */
	if ((sd->str != NULL) && (strcmp(sd->str, TOKEN_STRING_MULTI) != 0)) {
		/*sd->str不为空，也不为空串*/
		str = sd->str;
		do {
			token_len = get_token_len(str);/*取token长度*/

			/* if token is too big... */
			if (token_len >= STR_TOKEN_SIZE - 1) {
				/*token过长*/
				continue;
			}

			if ( strncmp(buf, str, token_len) ) {
				/*buf开始，token_len长度的token与str不匹配，尝试下一个token*/
				continue;
			}

			if ( !cmdline_isendoftoken(*(buf+token_len)) ) {
				/*未遇到token结尾*/
				continue;
			}

			break;/*成功匹配*/
		} while ( (str = get_next_token(str)) != NULL );

		if (!str)
			return -1;/*遍历结束，未匹配*/
	}
	/* multi string */
	else if (sd->str != NULL) {
		/*此时sd->str取值""*/
		if (ressize < STR_MULTI_TOKEN_SIZE)
			return -1;

		/*取当前命令中token指定的长度*/
		token_len = 0;
		while (!cmdline_isendofcommand(buf[token_len]) &&
		      token_len < (STR_MULTI_TOKEN_SIZE - 1))
			token_len++;

		/* return if token too long */
		if (token_len >= (STR_MULTI_TOKEN_SIZE - 1))
			/*token长度过长，报错*/
			return -1;
	}
	/* unspecified string (unknown single token) */
	else {
		/*此时sd->str取值为NULL*/

		/*取当前命令中的token指定长度*/
		token_len = 0;
		while(!cmdline_isendoftoken(buf[token_len]) &&
		      token_len < (STR_TOKEN_SIZE-1))
			token_len++;

		/* return if token too long */
		if (token_len >= STR_TOKEN_SIZE - 1) {
			/*token长度过长，报错*/
			return -1;
		}
	}

	if (res) {
		/*提供了出参res,针对NULL，空串情况，设置res*/
		if ((sd->str != NULL) && (strcmp(sd->str, TOKEN_STRING_MULTI) == 0))
			/* we are sure that token_len is < STR_MULTI_TOKEN_SIZE-1 */
			strlcpy(res, buf, STR_MULTI_TOKEN_SIZE);
		else
			/* we are sure that token_len is < STR_TOKEN_SIZE-1 */
			strlcpy(res, buf, STR_TOKEN_SIZE);

		*((char *)res + token_len) = 0;
	}

	return token_len;/*返回匹配的命令字符串长度*/
}

int cmdline_complete_get_nb_string(cmdline_parse_token_hdr_t *tk)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *str;
	int ret = 1;

	if (!tk)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	if (!sd->str)
		return 0;

	str = sd->str;
	while( (str = get_next_token(str)) != NULL ) {
		ret++;
	}
	return ret;
}

int cmdline_complete_get_elt_string(cmdline_parse_token_hdr_t *tk, int idx,
				    char *dstbuf, unsigned int size)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *s;
	unsigned int len;

	if (!tk || !dstbuf || idx < 0)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	s = sd->str;

	while (idx-- && s)
		s = get_next_token(s);

	if (!s)
		return -1;

	len = get_token_len(s);
	if (len > size - 1)
		return -1;

	memcpy(dstbuf, s, len);
	dstbuf[len] = '\0';
	return 0;
}


int cmdline_get_help_string(cmdline_parse_token_hdr_t *tk, char *dstbuf,
			    unsigned int size)
{
	struct cmdline_token_string *tk2;
	struct cmdline_token_string_data *sd;
	const char *s;

	if (!tk || !dstbuf)
		return -1;

	tk2 = (struct cmdline_token_string *)tk;
	sd = &tk2->string_data;

	s = sd->str;

	if (s) {
		if (strcmp(s, TOKEN_STRING_MULTI) == 0)
			snprintf(dstbuf, size, ANYSTRINGS_HELP);
		else if (get_next_token(s))
			snprintf(dstbuf, size, CHOICESTRING_HELP);
		else
			snprintf(dstbuf, size, FIXEDSTRING_HELP);
	} else
		snprintf(dstbuf, size, ANYSTRING_HELP);

	return 0;
}
