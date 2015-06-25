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

const mtree_keyword_map mtree_keywords[] = {
	{ "cksum", 		MTREE_KEYWORD_CKSUM },
	{ "contents", 		MTREE_KEYWORD_CONTENTS },
	{ "flags", 		MTREE_KEYWORD_FLAGS },
	{ "gid", 		MTREE_KEYWORD_GID },
	{ "gname", 		MTREE_KEYWORD_GNAME },
	{ "ignore", 		MTREE_KEYWORD_IGNORE },
	{ "inode", 		MTREE_KEYWORD_INODE },
	{ "link", 		MTREE_KEYWORD_LINK },
	{ "md5", 		MTREE_KEYWORD_MD5 },
	{ "md5digest", 		MTREE_KEYWORD_MD5DIGEST },
	{ "mode", 		MTREE_KEYWORD_MODE },
	{ "nlink", 		MTREE_KEYWORD_NLINK },
	{ "nochange", 		MTREE_KEYWORD_NOCHANGE },
	{ "optional", 		MTREE_KEYWORD_OPTIONAL },
	{ "ripemd160digest", 	MTREE_KEYWORD_RIPEMD160DIGEST },
	{ "rmd160", 		MTREE_KEYWORD_RMD160 },
	{ "rmd160digest", 	MTREE_KEYWORD_RMD160DIGEST },
	{ "sha1", 		MTREE_KEYWORD_SHA1 },
	{ "sha1digest", 	MTREE_KEYWORD_SHA1DIGEST },
	{ "sha256", 		MTREE_KEYWORD_SHA256 },
	{ "sha256digest", 	MTREE_KEYWORD_SHA256DIGEST },
	{ "sha384", 		MTREE_KEYWORD_SHA384 },
	{ "sha384digest", 	MTREE_KEYWORD_SHA384DIGEST },
	{ "sha512", 		MTREE_KEYWORD_SHA512 },
	{ "sha512digest", 	MTREE_KEYWORD_SHA512DIGEST },
	{ "size", 		MTREE_KEYWORD_SIZE },
	{ "time", 		MTREE_KEYWORD_TIME },
	{ "type", 		MTREE_KEYWORD_TYPE },
	{ "uid", 		MTREE_KEYWORD_UID },
	{ "uname", 		MTREE_KEYWORD_UNAME },
	{ NULL, -1 }
};

long
mtree_str_to_keyword(const char *s)
{
	int i;

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if (strcmp(mtree_keywords[i].name, s))
			continue;
		return mtree_keywords[i].keyword;
	}
	return (-1);
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

	return (-1);
}

const char *
mtree_keyword_to_str(long keyword)
{
	int i;

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if (mtree_keywords[i].keyword != keyword)
			continue;
		return mtree_keywords[i].name;
	}
	return (NULL);
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
