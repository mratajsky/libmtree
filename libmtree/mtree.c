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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"
#include "mtree.h"
#include "mtree_private.h"

/*
 * List of keyword names and constants. Keep it lexically sorted.
 */
const struct mtree_keyword_map mtree_keywords[] = {
	{ "cksum",		MTREE_KEYWORD_CKSUM },
	{ "contents",		MTREE_KEYWORD_CONTENTS },
	{ "device",		MTREE_KEYWORD_DEVICE },
	{ "flags",		MTREE_KEYWORD_FLAGS },
	{ "gid",		MTREE_KEYWORD_GID },
	{ "gname",		MTREE_KEYWORD_GNAME },
	{ "ignore",		MTREE_KEYWORD_IGNORE },
	{ "inode", 		MTREE_KEYWORD_INODE },
	{ "link",		MTREE_KEYWORD_LINK },
	{ "md5",		MTREE_KEYWORD_MD5 },
	{ "md5digest",		MTREE_KEYWORD_MD5DIGEST },
	{ "mode",		MTREE_KEYWORD_MODE },
	{ "nlink",		MTREE_KEYWORD_NLINK },
	{ "nochange",		MTREE_KEYWORD_NOCHANGE },
	{ "optional",		MTREE_KEYWORD_OPTIONAL },
	{ "ripemd160digest",	MTREE_KEYWORD_RIPEMD160DIGEST },
	{ "rmd160",		MTREE_KEYWORD_RMD160 },
	{ "rmd160digest",	MTREE_KEYWORD_RMD160DIGEST },
	{ "sha1",		MTREE_KEYWORD_SHA1 },
	{ "sha1digest",		MTREE_KEYWORD_SHA1DIGEST },
	{ "sha256",		MTREE_KEYWORD_SHA256 },
	{ "sha256digest",	MTREE_KEYWORD_SHA256DIGEST },
	{ "sha384",		MTREE_KEYWORD_SHA384 },
	{ "sha384digest",	MTREE_KEYWORD_SHA384DIGEST },
	{ "sha512",		MTREE_KEYWORD_SHA512 },
	{ "sha512digest",	MTREE_KEYWORD_SHA512DIGEST },
	{ "size",		MTREE_KEYWORD_SIZE },
	{ "tags",		MTREE_KEYWORD_TAGS },
	{ "time",		MTREE_KEYWORD_TIME },
	{ "type",		MTREE_KEYWORD_TYPE },
	{ "uid",		MTREE_KEYWORD_UID },
	{ "uname",		MTREE_KEYWORD_UNAME },
	{ NULL,			0 }
};
#define N_KEYWORDS		(__arraycount(mtree_keywords) - 1)

/*
 * List of mtree_entry_type names and constants. Keep it lexically sorted.
 */
static const struct entry_type_map {
	const char 	*name;
	mtree_entry_type type;
	int		 mode;
} entry_types[] = {
	{ "block",		MTREE_ENTRY_BLOCK,	S_IFBLK },
	{ "char",		MTREE_ENTRY_CHAR,	S_IFCHR },
	{ "dir",		MTREE_ENTRY_DIR,	S_IFDIR },
	{ "fifo",		MTREE_ENTRY_FIFO,	S_IFIFO },
	{ "file",		MTREE_ENTRY_FILE,	S_IFREG },
	{ "link",		MTREE_ENTRY_LINK,	S_IFLNK },
	{ "socket",		MTREE_ENTRY_SOCKET,	S_IFSOCK },
	{ NULL,			MTREE_ENTRY_UNKNOWN,	0 }
};
#define N_ENTRY_TYPES		(__arraycount(entry_types) - 1)

static int
compare_entry_name(const void *key, const void *map)
{

	return (strcmp((const char *)key, ((struct entry_type_map *)map)->name));
}

/*
 * Convert entry type string to an mtree_entry_type.
 */
mtree_entry_type
mtree_entry_type_parse(const char *name)
{
	struct entry_type_map *item;

	assert(name != NULL);

	item = bsearch(name, entry_types,
	    N_ENTRY_TYPES,
	    sizeof(entry_types[0]), compare_entry_name);
	if (item != NULL)
		return (item->type);
	return (MTREE_ENTRY_UNKNOWN);
}

/*
 * Convert mode to an mtree_entry_type.
 */
mtree_entry_type
mtree_entry_type_from_mode(int mode)
{
	size_t i;

	mode &= S_IFMT;
	for (i = 0; i < N_ENTRY_TYPES; i++)
		if (entry_types[i].mode == mode)
			return (entry_types[i].type);
	return (MTREE_ENTRY_UNKNOWN);
}

static int
compare_entry_type(const void *key, const void *map)
{

	return (*((int *)key) - ((struct entry_type_map *)map)->type);
}

/*
 * Convert mtree_entry_type to a string.
 */
const char *
mtree_entry_type_string(mtree_entry_type type)
{
	struct entry_type_map *item;

	item = bsearch(&type, entry_types,
	    N_ENTRY_TYPES,
	    sizeof(entry_types[0]), compare_entry_type);
	if (item != NULL)
		return (item->name);
	return (NULL);
}

static int
compare_keyword_name(const void *key, const void *map)
{

	return (strcmp((const char *)key, ((struct mtree_keyword_map *)map)->name));
}

/*
 * Convert keyword string to the appropriate numeric constant.
 */
uint64_t
mtree_keyword_parse(const char *keyword)
{
	struct mtree_keyword_map *item;

	assert(keyword != NULL);

	item = bsearch(keyword, mtree_keywords,
	    N_KEYWORDS,
	    sizeof(mtree_keywords[0]), compare_keyword_name);
	if (item != NULL)
		return (item->keyword);
	return (0);
}

static int
compare_keyword(const void *key, const void *map)
{

	return (*((uint64_t *)key) - ((struct mtree_keyword_map *)map)->keyword);
}

/*
 * Convert keyword to a string.
 */
const char *
mtree_keyword_string(uint64_t keyword)
{
	struct mtree_keyword_map *item;

	item = bsearch(&keyword, mtree_keywords,
	    N_KEYWORDS,
	    sizeof(mtree_keywords[0]), compare_keyword);
	if (item != NULL)
		return (item->name);
	return (NULL);
}
