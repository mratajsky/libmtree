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
#include <sys/time.h>

#include <assert.h>
#include <fts.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mtree.h"
#include "mtree_private.h"

/* Mask of all keywords read by stat(2) */
#define MTREE_KEYWORD_MASK_STAT	(MTREE_KEYWORD_FLAGS |	\
				 MTREE_KEYWORD_GID |	\
				 MTREE_KEYWORD_GNAME |	\
				 MTREE_KEYWORD_INODE |	\
				 MTREE_KEYWORD_LINK |	\
				 MTREE_KEYWORD_MODE |	\
				 MTREE_KEYWORD_NLINK |	\
				 MTREE_KEYWORD_SIZE |	\
				 MTREE_KEYWORD_TIME |	\
				 MTREE_KEYWORD_TYPE |	\
				 MTREE_KEYWORD_UID |	\
				 MTREE_KEYWORD_UNAME)

mtree_entry *
mtree_entry_create(void)
{
	mtree_entry *entry;

	entry = calloc(1, sizeof(mtree_entry));

	return (entry);
}

mtree_entry *
mtree_entry_create_from_file(const char *path)
{
	mtree_entry *entry;
	struct stat  st;

	entry = mtree_entry_create();
	if (entry == NULL)
		return (NULL);

	if (stat(path, &st) == -1) {
		free(entry);
		return (NULL);
	}
	return (entry);
}

mtree_entry *
mtree_entry_create_from_ftsent(FTSENT *ftsent)
{
	mtree_entry *entry;

	assert (ftsent != NULL);
	assert (ftsent->fts_statp != NULL);

	entry = mtree_entry_create();
	if (entry == NULL)
		return (NULL);

	entry->path = strdup(ftsent->fts_path);
	entry->name = strdup(ftsent->fts_name);

	/* Copy stat parts */
	entry->data.stat.st_gid   = ftsent->fts_statp->st_gid;
	entry->data.stat.st_ino   = ftsent->fts_statp->st_ino;
	entry->data.stat.st_mode  = ftsent->fts_statp->st_mode;
	entry->data.stat.st_mtim  = ftsent->fts_statp->st_mtim;
	entry->data.stat.st_nlink = ftsent->fts_statp->st_nlink;
	entry->data.stat.st_size  = ftsent->fts_statp->st_size;
	entry->data.stat.st_uid	  = ftsent->fts_statp->st_uid;
	entry->data.keywords   	  = MTREE_KEYWORD_MASK_STAT;
	return (entry);
}

void
mtree_entry_free(mtree_entry *entry)
{

	mtree_entry_free_data_items(&entry->data);
	free(entry->path);
	free(entry->name);
	free(entry);
}

void
mtree_entry_free_all(mtree_entry *entries)
{
	mtree_entry *next;

	while (entries != NULL) {
		next = entries->next;
		mtree_entry_free(entries);
		entries = next;
	}
}

void
mtree_entry_free_data_items(mtree_entry_data *data)
{

	free(data->contents);
	free(data->flags);
	free(data->gname);
	free(data->link);
	free(data->uname);
	free(data->md5digest);
	free(data->rmd160digest);
	free(data->sha1digest);
	free(data->sha256digest);
	free(data->sha384digest);
	free(data->sha512digest);
}

static void
copy_keyword(mtree_entry_data *data, mtree_entry_data *from, long keyword)
{

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		data->cksum = from->cksum;
		break;
	case MTREE_KEYWORD_CONTENTS:
		mtree_copy_string(&data->contents, from->contents);
		break;
	case MTREE_KEYWORD_FLAGS:
		mtree_copy_string(&data->flags, from->flags);
		break;
	case MTREE_KEYWORD_GID:
		data->stat.st_gid = from->stat.st_gid;
		break;
	case MTREE_KEYWORD_GNAME:
		mtree_copy_string(&data->gname, from->gname);
		break;
	case MTREE_KEYWORD_IGNORE:
		/* No value */
		break;
	case MTREE_KEYWORD_INODE:
		data->stat.st_ino = from->stat.st_ino;
		break;
	case MTREE_KEYWORD_LINK:
		mtree_copy_string(&data->link, from->link);
		break;
	case MTREE_KEYWORD_MD5:
	case MTREE_KEYWORD_MD5DIGEST:
		mtree_copy_string(&data->md5digest, from->md5digest);
		break;
	case MTREE_KEYWORD_MODE:
		data->stat.st_mode &= S_IFMT;
		data->stat.st_mode |= from->stat.st_mode & ~S_IFMT;
		break;
	case MTREE_KEYWORD_NLINK:
		data->stat.st_nlink = from->stat.st_nlink;
		break;
	case MTREE_KEYWORD_NOCHANGE:
		/* No value */
		break;
	case MTREE_KEYWORD_OPTIONAL:
		/* No value */
		break;
	case MTREE_KEYWORD_RIPEMD160DIGEST:
	case MTREE_KEYWORD_RMD160:
	case MTREE_KEYWORD_RMD160DIGEST:
		mtree_copy_string(&data->rmd160digest, from->rmd160digest);
		break;
	case MTREE_KEYWORD_SHA1:
	case MTREE_KEYWORD_SHA1DIGEST:
		mtree_copy_string(&data->sha1digest, from->sha1digest);
		break;
	case MTREE_KEYWORD_SHA256:
	case MTREE_KEYWORD_SHA256DIGEST:
		mtree_copy_string(&data->sha256digest, from->sha256digest);
		break;
	case MTREE_KEYWORD_SHA384:
	case MTREE_KEYWORD_SHA384DIGEST:
		mtree_copy_string(&data->sha384digest, from->sha384digest);
		break;
	case MTREE_KEYWORD_SHA512:
	case MTREE_KEYWORD_SHA512DIGEST:
		mtree_copy_string(&data->sha512digest, from->sha512digest);
		break;
	case MTREE_KEYWORD_SIZE:
		data->stat.st_size = from->stat.st_size;
		break;
	case MTREE_KEYWORD_TIME:
		data->stat.st_mtim = from->stat.st_mtim;
		break;
	case MTREE_KEYWORD_TYPE:
		data->stat.st_mode &= ~S_IFMT;
		data->stat.st_mode |= (from->stat.st_mode & S_IFMT);
		break;
	case MTREE_KEYWORD_UID:
		data->stat.st_uid = from->stat.st_uid;
		break;
	case MTREE_KEYWORD_UNAME:
		mtree_copy_string(&data->uname, from->uname);
		break;
	default:
		/* Invalid keyword */
		return;
	}

	data->keywords |= keyword;
}

void
mtree_entry_copy_missing_keywords(mtree_entry *entry, mtree_entry_data *from)
{
	int i;

	assert(entry != NULL);
	assert(from != NULL);

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if (entry->data.keywords & mtree_keywords[i].keyword)
			continue;
		if ((from->keywords & mtree_keywords[i].keyword) == 0)
			continue;
		copy_keyword(&entry->data, from, mtree_keywords[i].keyword);
	}
}

mtree_entry *
mtree_entry_first(mtree_entry *entry)
{

	assert(entry != NULL);

	while (entry->prev != NULL)
		entry = entry->prev;

	return entry;
}

mtree_entry *
mtree_entry_last(mtree_entry *entry)
{

	assert(entry != NULL);

	while (entry->next != NULL)
		entry = entry->next;

	return entry;
}

mtree_entry *
mtree_entry_previous(mtree_entry *entry)
{

	assert(entry != NULL);

	return entry->prev;
}

mtree_entry *
mtree_entry_next(mtree_entry *entry)
{

	assert(entry != NULL);

	return entry->next;
}

mtree_entry *
mtree_entry_prepend(mtree_entry *entry, mtree_entry *child)
{

	child->next = entry;
	if (entry != NULL) {
		child->prev = entry->prev;
		if (entry->prev != NULL)
			entry->prev->next = child;
		entry->prev = child;
	} else
		child->prev = NULL;

	return (child);
}

mtree_entry *
mtree_entry_append(mtree_entry *entry, mtree_entry *child)
{

	if (entry != NULL) {
		mtree_entry *last;

		last = mtree_entry_last(entry);
		last->next  = child;
		child->prev = entry;
	} else {
		child->prev = NULL;
		entry = child;
	}
	return (entry);
}
