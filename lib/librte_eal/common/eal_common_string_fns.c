/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <rte_string_fns.h>

/* split string into tokens */
int
rte_strsplit(char *string, int stringlen,
	     char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if (string == NULL || tokens == NULL)
		goto einval_error;

	for (i = 0; i < stringlen; i++) {
		if (string[i] == '\0' || tok >= maxtokens)
			break;
		if (tokstart) {
			tokstart = 0;
			//将string中的内容全部给tokens,然后再检查分隔符
			tokens[tok++] = &string[i];
		}

		//发现delim符，置为'\0'，将上一个tokens中的string置为结束
		if (string[i] == delim) {
			string[i] = '\0';
			tokstart = 1;
		}
	}
	return tok;//分割为多少组

einval_error:
	errno = EINVAL;
	return -1;
}
