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

#include "mtree_entry.h"
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

void
mtree_entry_prepend_child(mtree_entry *entry, mtree_entry *child)
{

	assert(entry != NULL);
	assert(child != NULL);

	if (entry->children != NULL) {
		entry->children->prev = child;
		child->next = entry->children;
	} else {
		child->next = NULL;
	}
	entry->children = child;
	child->parent = entry;
}

void
mtree_entry_append_child(mtree_entry *entry, mtree_entry *child)
{

	assert(entry != NULL);
	assert(child != NULL);

	if (entry->children != NULL) {
		mtree_entry *last;

		last = entry->children;
		while (last->next != NULL)
			last = last->next;

		last->next = child;
		child->prev = last;
	} else {
		entry->children = child;
	}
	child->parent = entry;
}

void
mtree_entry_unlink(mtree_entry *entry)
{

	assert(entry != NULL);

	if (entry->prev != NULL)
		entry->prev->next = entry->next;
	else if (entry->parent != NULL)
		entry->parent->children = entry->next;

	if (entry->next != NULL) {
		entry->next->prev = entry->prev;
		entry->next = NULL;
	}
	entry->prev = NULL;
	entry->parent = NULL;
}
