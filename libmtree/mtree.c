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

#include <assert.h>
#include <string.h>

#include "mtree.h"
#include "mtree_private.h"

/*
 * Keep this in the order of writing
 */
const mtree_keyword_map mtree_keywords[] = {
	{ "type", 		MTREE_KEYWORD_TYPE },
	{ "uname", 		MTREE_KEYWORD_UNAME },
	{ "uid", 		MTREE_KEYWORD_UID },
	{ "gname", 		MTREE_KEYWORD_GNAME },
	{ "gid", 		MTREE_KEYWORD_GID },
	{ "mode", 		MTREE_KEYWORD_MODE },
	{ "nlink", 		MTREE_KEYWORD_NLINK },
	{ "size", 		MTREE_KEYWORD_SIZE },
	{ "time", 		MTREE_KEYWORD_TIME },
	{ "link", 		MTREE_KEYWORD_LINK },
	{ "cksum", 		MTREE_KEYWORD_CKSUM },
	{ "md5", 		MTREE_KEYWORD_MD5 },
	{ "md5digest", 		MTREE_KEYWORD_MD5DIGEST },
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
	{ "flags", 		MTREE_KEYWORD_FLAGS },
	{ "contents", 		MTREE_KEYWORD_CONTENTS },
	{ "ignore", 		MTREE_KEYWORD_IGNORE },
	{ "nochange", 		MTREE_KEYWORD_NOCHANGE },
	{ "optional", 		MTREE_KEYWORD_OPTIONAL },
	{ "inode", 		MTREE_KEYWORD_INODE },
	{ NULL, -1 }
};

mtree_entry_type
mtree_parse_type(const char *type)
{

	assert(type != NULL);

	if (strcmp(type, "file") == 0)
		return MTREE_ENTRY_FILE;
	if (strcmp(type, "dir") == 0)
		return MTREE_ENTRY_DIR;
	if (strcmp(type, "link") == 0)
		return MTREE_ENTRY_LINK;
	if (strcmp(type, "block") == 0)
		return MTREE_ENTRY_BLOCK;
	if (strcmp(type, "char") == 0)
		return MTREE_ENTRY_CHAR;
	if (strcmp(type, "fifo") == 0)
		return MTREE_ENTRY_FIFO;
	if (strcmp(type, "socket") == 0)
		return MTREE_ENTRY_SOCKET;

	return MTREE_ENTRY_UNKNOWN;
}

const char *
mtree_type_string(mtree_entry_type type)
{

	switch (type) {
	case MTREE_ENTRY_FILE:
		return ("file");
	case MTREE_ENTRY_DIR:
		return ("dir");
	case MTREE_ENTRY_LINK:
		return ("link");
	case MTREE_ENTRY_BLOCK:
		return ("block");
	case MTREE_ENTRY_CHAR:
		return ("char");
	case MTREE_ENTRY_FIFO:
		return ("fifo");
	case MTREE_ENTRY_SOCKET:
		return ("socket");
	case MTREE_ENTRY_UNKNOWN:
		return ("");
	default:
		return (NULL);
	}
}

long
mtree_parse_keyword(const char *keyword)
{
	int i;

	assert(keyword != NULL);

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if (strcmp(mtree_keywords[i].name, keyword) != 0)
			continue;
		return mtree_keywords[i].keyword;
	}
	return (-1);
}
