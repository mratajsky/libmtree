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

#define E_HAS_FIELD(e, f)	((e)->data.fields & (f))
#define E_SET_FIELD(e, f)	((e)->data.fields |= (f))
#define D_HAS_FIELD(d, f)	((d)->fields & (f))
#define D_SET_FIELD(d, f)	((d)->fields |= (f))

/* Mask of all fields read by stat(2) */
#define F_MASK_STAT		(MTREE_F_GID |	 \
				 MTREE_F_INODE | \
				 MTREE_F_MODE |	 \
				 MTREE_F_NLINK | \
				 MTREE_F_SIZE |	 \
				 MTREE_F_TIME |	 \
				 MTREE_F_TYPE |	 \
				 MTREE_F_UID);

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
	entry->data.fields   	  = F_MASK_STAT;
	return (entry);
}

void
mtree_entry_free(mtree_entry *entry)
{

	free(entry);
}

static void
copy_field(mtree_entry_data *data, mtree_entry_data *from, int32_t field)
{

	switch (field) {
	case MTREE_F_CKSUM:
		data->cksum = from->cksum;
		break;
	case MTREE_F_CONTENTS:
		mtree_copy_string(&data->contents, from->contents);
		break;
	case MTREE_F_FLAGS:
		mtree_copy_string(&data->flags, from->flags);
		break;
	case MTREE_F_GID:
		data->stat.st_gid = from->stat.st_gid;
		break;
	case MTREE_F_GNAME:
		mtree_copy_string(&data->gname, from->gname);
		break;
	case MTREE_F_IGNORE:
		/* No value */
		break;
	case MTREE_F_INODE:
		data->stat.st_ino = from->stat.st_ino;
		break;
	case MTREE_F_LINK:
		mtree_copy_string(&data->link, from->link);
		break;
	case MTREE_F_MD5:
	case MTREE_F_MD5DIGEST:
		mtree_copy_string(&data->md5digest, from->md5digest);
		break;
	case MTREE_F_MODE:
		data->stat.st_mode &= S_IFMT;
		data->stat.st_mode |= from->stat.st_mode & ~S_IFMT;
		break;
	case MTREE_F_NLINK:
		data->stat.st_nlink = from->stat.st_nlink;
		break;
	case MTREE_F_NOCHANGE:
		/* No value */
		break;
	case MTREE_F_OPTIONAL:
		/* No value */
		break;
	case MTREE_F_RIPEMD160DIGEST:
	case MTREE_F_RMD160:
	case MTREE_F_RMD160DIGEST:
		mtree_copy_string(&data->rmd160digest, from->rmd160digest);
		break;
	case MTREE_F_SHA1:
	case MTREE_F_SHA1DIGEST:
		mtree_copy_string(&data->sha1digest, from->sha1digest);
		break;
	case MTREE_F_SHA256:
	case MTREE_F_SHA256DIGEST:
		mtree_copy_string(&data->sha256digest, from->sha256digest);
		break;
	case MTREE_F_SHA384:
	case MTREE_F_SHA384DIGEST:
		mtree_copy_string(&data->sha384digest, from->sha384digest);
		break;
	case MTREE_F_SHA512:
	case MTREE_F_SHA512DIGEST:
		mtree_copy_string(&data->sha512digest, from->sha512digest);
		break;
	case MTREE_F_SIZE:
		data->stat.st_size = from->stat.st_size;
		break;
	case MTREE_F_TIME:
		data->stat.st_mtim = from->stat.st_mtim;
		break;
	case MTREE_F_TYPE:
		data->stat.st_mode &= ~S_IFMT;
		data->stat.st_mode |= (from->stat.st_mode & S_IFMT);
		break;
	case MTREE_F_UID:
		data->stat.st_uid = from->stat.st_uid;
		break;
	case MTREE_F_UNAME:
		mtree_copy_string(&data->uname, from->uname);
		break;
	default:
		/* Invalid field */
		return;
	}

	data->fields |= field;
}

void
mtree_entry_copy_missing_fields(mtree_entry *entry, mtree_entry_data *from)
{
	int i;

	assert(entry != NULL);
	assert(from != NULL);

	for (i = 0; mtree_fields[i].name != NULL; i++) {
		if (E_HAS_FIELD(entry, mtree_fields[i].field) ||
		    !D_HAS_FIELD(from, mtree_fields[i].field))
			continue;
		copy_field(&entry->data, from, mtree_fields[i].field);
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
