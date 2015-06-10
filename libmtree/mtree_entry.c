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

#include "mtree.h"
#include "mtree_private.h"

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

	entry = mtree_entry_create();
	if (entry == NULL)
		return (NULL);

	if (stat(path, &entry->stat) == -1) {
		free(entry);
		return (NULL);
	}
	return (entry);
}

mtree_entry *
mtree_entry_create_from_ftsent(FTSENT *ftsent)
{
	mtree_entry *entry;

	entry = mtree_entry_create();
	if (entry == NULL)
		return (NULL);

	/* XXX might not be available, use a flag */
	entry->stat = *(ftsent->fts_statp);

	return (entry);
}

void
mtree_entry_free(mtree_entry *entry)
{

	free(entry);
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
