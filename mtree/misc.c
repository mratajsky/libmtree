/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
#include <libmtree/mtree.h>
#include <string.h>

#include "mtree.h"

long
parse_keyword(const char *name)
{
	long keyword;

	assert(name != NULL);

	if (strcmp(name, "all") == 0)
		return (MTREE_KEYWORD_MASK_ALL);

	keyword = mtree_parse_keyword(name);
	if (keyword == -1)
		mtree_err("unknown keyword `%s'", name);

	return (keyword);
}

const char *
file_type_string(mode_t mode)
{

	switch (mode & S_IFMT) {
	case S_IFBLK:
		return ("block");
	case S_IFCHR:
		return ("char");
	case S_IFDIR:
		return ("dir");
	case S_IFIFO:
		return ("fifo");
	case S_IFREG:
		return ("file");
	case S_IFLNK:
		return ("link");
	case S_IFSOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}
