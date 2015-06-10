/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mtree.h"
#include "mtree_private.h"

typedef struct {
	char   *str;
	int32_t field;
} mtree_field_map;

static mtree_field_map fm[] = {
	{ "cksum", 		MTREE_F_CKSUM },
	{ "contents", 		MTREE_F_CONTENTS },
	{ "flags", 		MTREE_F_FLAGS },
	{ "gid", 		MTREE_F_GID },
	{ "gname", 		MTREE_F_GNAME },
	{ "ignore", 		MTREE_F_IGNORE },
	{ "inode", 		MTREE_F_INODE },
	{ "link", 		MTREE_F_LINK },
	{ "md5", 		MTREE_F_MD5 },
	{ "md5digest", 		MTREE_F_MD5DIGEST },
	{ "mode", 		MTREE_F_MODE },
	{ "nlink", 		MTREE_F_NLINK },
	{ "nochange", 		MTREE_F_NOCHANGE },
	{ "optional", 		MTREE_F_OPTIONAL },
	{ "ripemd160digest", 	MTREE_F_RIPEMD160DIGEST },
	{ "rmd160", 		MTREE_F_RMD160 },
	{ "rmd160digest", 	MTREE_F_RMD160DIGEST },
	{ "sha1", 		MTREE_F_SHA1 },
	{ "sha1digest", 	MTREE_F_SHA1DIGEST },
	{ "sha256", 		MTREE_F_SHA256 },
	{ "sha256digest", 	MTREE_F_SHA256DIGEST },
	{ "sha384", 		MTREE_F_SHA384 },
	{ "sha384digest", 	MTREE_F_SHA384DIGEST },
	{ "sha512", 		MTREE_F_SHA512 },
	{ "sha512digest", 	MTREE_F_SHA512DIGEST },
	{ "size", 		MTREE_F_SIZE },
	{ "time", 		MTREE_F_TIME },
	{ "type", 		MTREE_F_TYPE },
	{ "uid", 		MTREE_F_UID },
	{ "uname", 		MTREE_F_UNAME },
	{ NULL, -1 }
};

int32_t
mtree_str_to_field(const char *s)
{
	mtree_field_map *item;

	for (item = fm; item->str != NULL; item++) {
		if (strcmp(item->str, s) == 0)
			return item->field;
	}
	return -1;
}

int32_t
mtree_str_to_type(const char *s)
{
	if (strcmp(s, "file") == 0)
		return S_IFREG;
	if (strcmp(s, "dir") == 0)
		return S_IFDIR;
	if (strcmp(s, "link") == 0)
		return S_IFLNK;
	if (strcmp(s, "block") == 0)
		return S_IFBLK;
	if (strcmp(s, "char") == 0)
		return S_IFCHR;
	if (strcmp(s, "fifo") == 0)
		return S_IFIFO;
	if (strcmp(s, "socket") == 0)
		return S_IFSOCK;

	return -1;
}

const char *
mtree_field_to_str(int32_t field)
{
	mtree_field_map *item;

	for (item = fm; item->str != NULL; item++) {
		if (item->field == field)
			return item->str;
	}
	return NULL;
}

int
mtree_copy_string(char **dst, const char *src)
{

	assert(dst != NULL);
	assert(src != NULL);

	free(*dst);
	if ((*dst = strdup(src)) == NULL)
		return (-1);

	return (0);
}
