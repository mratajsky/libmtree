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
#include <stdlib.h>
#include <string.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

/*
 * Compare spec1 and spec2 entries, producing lists of spec1-only, spec2-only,
 * matching and different entries.
 *
 * Only selected keywords are compared and options allow to specify whether
 * matching and/or different entries should be collected.
 */
struct mtree_spec_diff *
mtree_spec_diff_create(struct mtree_spec *spec1, struct mtree_spec *spec2,
    long keywords, int options)
{
	struct mtree_spec_diff	*sd;
	struct mtree_entry	*s1only;
	struct mtree_entry	*s2only;
	struct mtree_entry	*match;
	struct mtree_entry	*diff;
	struct mtree_entry	*e1, *e2;
	struct mtree_entry	*prev;
	struct mtree_trie	*s2trie;
	long			 differ;

	assert(spec1 != NULL);
	assert(spec2 != NULL);

	sd = malloc(sizeof(struct mtree_spec_diff));
	if (sd == NULL)
		return (NULL);

	match = NULL;
	diff  = NULL;
	if (spec1->entries != NULL) {
		s1only = mtree_entry_copy_all(spec1->entries);
		if (s1only == NULL) {
			free(sd);
			return (NULL);
		}
	} else
		s1only = NULL;
	/*
	 * If spec2 is empty, s1only already contains all spec1-only entries and
	 * the work is done.
	 */
	if (spec2->entries == NULL) {
		s2only = NULL;
		goto end;
	}

	s2only = mtree_entry_copy_all(spec2->entries);
	if (s2only == NULL) {
		mtree_entry_free_all(s1only);
		free(sd);
		return (NULL);
	}
	if (s1only == NULL)
		goto end;

	/*
	 * Convert the 2nd spec to a trie to avoid O^2 comparison.
	 *
	 * This has a side-effect of "merging" the list by replacing former
	 * entries with later ones with the same path.
	 *
	 * To make this function work correctly on non-merged lists, the 1st
	 * list should be converted as well, but there is unlikely any practical
	 * need for this.
	 */
	s2trie = mtree_trie_create();
	e2 = s2only;
	while (e2 != NULL) {
		if (mtree_trie_insert(s2trie, e2->path, e2) == -1) {
			mtree_entry_free_all(s1only);
			mtree_entry_free_all(s2only);
			free(sd);
			return (NULL);
		}
		e2 = e2->next;
	}

	/*
	 * Read the 1st list from the end back to the start and for each entry
	 * lookup the path in the 2nd list's trie.
	 *
	 * Reading from the end allows prepending matching entries to either the
	 * match or diff list without having to reverse the lists later.
	 */
	e1 = mtree_entry_get_last(s1only);
	while (e1 != NULL) {
		prev = e1->prev;
		e2 = mtree_trie_find(s2trie, e1->path);
		if (e2 != NULL) {
			/*
			 * A matching entry has been found in the 2nd list,
			 * remove both entries from their lists, compare them and
			 * add them to either match or diff list.
			 *
			 * The remaining entries in s1only will be the ones only
			 * present in the 1st list and the ones in s2only only
			 * in the 2nd list.
			 */
			s1only = mtree_entry_unlink(s1only, e1);
			s2only = mtree_entry_unlink(s2only, e2);
			if (options &
			    (MTREE_SPEC_DIFF_COLLECT_DIFFERENT |
			     MTREE_SPEC_DIFF_COLLECT_MATCHING)) {
				if (keywords == 0 ||
				    mtree_entry_compare_keywords(e1, e2, keywords,
				    &differ) == 0) {
					if (options & MTREE_SPEC_DIFF_COLLECT_MATCHING) {
						match = mtree_entry_prepend(match, e2);
						match = mtree_entry_prepend(match, e1);
					}
				} else {
					if (options & MTREE_SPEC_DIFF_COLLECT_DIFFERENT) {
						mtree_entry_set_keywords(e1, differ, 0);
						mtree_entry_set_keywords(e2, differ, 0);
						diff = mtree_entry_prepend(diff, e2);
						diff = mtree_entry_prepend(diff, e1);
					}
				}
			}
			if (match != e1 && diff != e1) {
				/* Not added to either list. */
				mtree_entry_free(e1);
				mtree_entry_free(e2);
			}
		}
		e1 = prev;
	}
	mtree_trie_free(s2trie);
end:
	if (options & MTREE_SPEC_DIFF_COLLECT_SPEC1_ONLY)
		sd->s1only = s1only;
	else {
		mtree_entry_free_all(s1only);
		sd->s1only = NULL;
	}
	if (options & MTREE_SPEC_DIFF_COLLECT_SPEC2_ONLY)
		sd->s2only = s2only;
	else {
		mtree_entry_free_all(s2only);
		sd->s2only = NULL;
	}
	sd->match = match;
	sd->diff  = diff;
	return (sd);
}

void
mtree_spec_diff_free(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	mtree_entry_free_all(sd->s1only);
	mtree_entry_free_all(sd->s2only);
	mtree_entry_free_all(sd->match);
	mtree_entry_free_all(sd->diff);
	free(sd);
}

struct mtree_entry *
mtree_spec_diff_get_spec1_only(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->s1only);
}

struct mtree_entry *
mtree_spec_diff_get_spec2_only(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->s2only);
}

struct mtree_entry *
mtree_spec_diff_get_different(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->diff);
}

struct mtree_entry *
mtree_spec_diff_get_matching(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->match);
}

/*
 * Write the spec diff in the original mtree's comm(1)-like format.
 */
int
mtree_spec_diff_write_file(struct mtree_spec_diff *sd, FILE *fp)
{
	struct mtree_writer	*writer;
	int			 ret;

	assert(sd != NULL);
	assert(fp != NULL);

	writer = mtree_writer_create();
	if (writer == NULL)
		return (-1);

	/*
	 * Only in first spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_FIRST);
	ret = mtree_writer_write_file(writer, sd->s1only, fp);
	if (ret == -1)
		goto out;
	/*
	 * Only in second spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_SECOND);
	ret = mtree_writer_write_file(writer, sd->s2only, fp);
	if (ret == -1)
		goto out;
	/*
	 * Different in each spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_DIFFER);
	ret = mtree_writer_write_file(writer, sd->diff, fp);
	if (ret == -1)
		goto out;
out:
	mtree_writer_free(writer);
	return (ret);
}

int
mtree_spec_diff_write_fd(struct mtree_spec_diff *sd, int fd)
{
	struct mtree_writer	*writer;
	int			 ret;

	assert(sd != NULL);
	assert(fd != -1);

	writer = mtree_writer_create();
	if (writer == NULL)
		return (-1);

	/*
	 * Only in first spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_FIRST);
	ret = mtree_writer_write_fd(writer, sd->s1only, fd);
	if (ret == -1)
		goto out;
	/*
	 * Only in second spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_SECOND);
	ret = mtree_writer_write_fd(writer, sd->s2only, fd);
	if (ret == -1)
		goto out;
	/*
	 * Different in each spec
	 */
	mtree_writer_set_format(writer, MTREE_FORMAT_DIFF_DIFFER);
	ret = mtree_writer_write_fd(writer, sd->diff, fd);
	if (ret == -1)
		goto out;
out:
	mtree_writer_free(writer);
	return (ret);
}
