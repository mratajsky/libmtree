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
#include <stdint.h>
#include <stdio.h>
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
    uint64_t keywords, int options)
{
	struct mtree_spec_diff	*sd;
	struct mtree_entry	*e1, *e2;
	struct mtree_entry	*prev;
	struct mtree_trie	*s2trie;
	uint64_t		 kdiff;
	uint64_t		 kcmp;

	assert(spec1 != NULL);
	assert(spec2 != NULL);

	sd = calloc(1, sizeof(struct mtree_spec_diff));
	if (sd == NULL)
		return (NULL);
	sd->writer = mtree_writer_create();
	if (sd->writer == NULL) {
		free(sd);
		return (NULL);
	}
	s2trie = NULL;
	if (spec1->entries != NULL) {
		sd->s1only = mtree_entry_copy_all(spec1->entries);
		if (sd->s1only == NULL)
			goto err;
	}
	if (spec2->entries != NULL) {
		sd->s2only = mtree_entry_copy_all(spec2->entries);
		if (sd->s2only == NULL)
			goto err;
	}
	/*
	 * If one of the specs has no entries, it's enough to pupulate the
	 * "only" list of the other spec.
	 */
	if (sd->s1only == NULL || sd->s2only == NULL)
		return (sd);

	/*
	 * Convert the 2nd spec to a trie to avoid O^2 comparison.
	 *
	 * This has a side-effect of "merging" the list by replacing former
	 * entries with later ones with the same path.
	 */
	s2trie = mtree_trie_create(NULL);
	if (s2trie == NULL)
		goto err;
	e2 = sd->s2only;
	while (e2 != NULL) {
		if (mtree_trie_insert(s2trie, e2->path, e2) == -1)
			goto err;
		e2 = e2->next;
	}

	/*
	 * Read the 1st list from the end back to the start and for each entry
	 * lookup the path in the 2nd list's trie.
	 *
	 * Reading from the end allows prepending matching entries to either the
	 * match or diff list without having to reverse the lists later.
	 */
	e1 = mtree_entry_get_last(sd->s1only);
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
			sd->s1only = mtree_entry_unlink(sd->s1only, e1);
			sd->s2only = mtree_entry_unlink(sd->s2only, e2);

			if (options & MTREE_SPEC_DIFF_MATCH_EXTRA_KEYWORDS) {
				/*
				 * In this case only compare what they have
				 * in common.
				 */
				kcmp = e1->data.keywords & e2->data.keywords;
			} else
				kcmp = keywords;

			if (kcmp == 0 ||
			    mtree_entry_compare_keywords(e1, e2, kcmp, &kdiff) == 0) {
			    	/* Matching. */
				sd->match = mtree_entry_prepend(sd->match, e2);
				sd->match = mtree_entry_prepend(sd->match, e1);
			} else {
				/* Different. */
				mtree_entry_set_keywords(e1,
				    e1->data.keywords & kdiff,
				    MTREE_ENTRY_REMOVE_EXCLUDED);
				mtree_entry_set_keywords(e2,
				    e2->data.keywords & kdiff,
				    MTREE_ENTRY_REMOVE_EXCLUDED);
				sd->diff = mtree_entry_prepend(sd->diff, e2);
				sd->diff = mtree_entry_prepend(sd->diff, e1);
			}
		}
		e1 = prev;
	}
	mtree_trie_free(s2trie);
	return (sd);
err:
	if (s2trie != NULL)
		mtree_trie_free(s2trie);

	mtree_spec_diff_free(sd);
	return (NULL);
}

/*
 * Free the given mtree_spec_diff.
 */
void
mtree_spec_diff_free(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	mtree_writer_free(sd->writer);
	mtree_entry_free_all(sd->s1only);
	mtree_entry_free_all(sd->s2only);
	mtree_entry_free_all(sd->match);
	mtree_entry_free_all(sd->diff);
	free(sd);
}

/*
 * Return entries that are only present in the first spec.
 */
struct mtree_entry *
mtree_spec_diff_get_spec1_only(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->s1only);
}

/*
 * Return entries that are only present in the second spec.
 */
struct mtree_entry *
mtree_spec_diff_get_spec2_only(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->s2only);
}

/*
 * Return entries that are present in both specs and their keywords match.
 */
struct mtree_entry *
mtree_spec_diff_get_matching(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->match);
}

/*
 * Return entries that are present in both specs, but their keywords
 * don't match.
 */
struct mtree_entry *
mtree_spec_diff_get_different(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	return (sd->diff);
}

/*
 * Write the spec diff in the original mtree's comm(1)-like format.
 */
static int
write_diff(struct mtree_spec_diff *sd)
{
	int ret;

	/*
	 * Only in first spec.
	 */
	mtree_writer_set_format(sd->writer, MTREE_FORMAT_DIFF_FIRST);
	ret = mtree_writer_write_entries(sd->writer, sd->s1only);
	if (ret == -1)
		return (-1);
	/*
	 * Only in second spec.
	 */
	mtree_writer_set_format(sd->writer, MTREE_FORMAT_DIFF_SECOND);
	ret = mtree_writer_write_entries(sd->writer, sd->s2only);
	if (ret == -1)
		return (-1);
	/*
	 * Different in each spec.
	 */
	mtree_writer_set_format(sd->writer, MTREE_FORMAT_DIFF_DIFFER);
	ret = mtree_writer_write_entries(sd->writer, sd->diff);
	if (ret == -1)
		return (-1);

	return (ret);
}

/*
 * Write spec diff to a file.
 */
int
mtree_spec_diff_write_file(struct mtree_spec_diff *sd, FILE *fp)
{

	assert(sd != NULL);
	assert(fp != NULL);

	mtree_writer_set_output_file(sd->writer, fp);

	return (write_diff(sd));
}

/*
 * Write spec diff to a file descriptor.
 */
int
mtree_spec_diff_write_fd(struct mtree_spec_diff *sd, int fd)
{

	assert(sd != NULL);
	assert(fd != -1);

	mtree_writer_set_output_fd(sd->writer, fd);

	return (write_diff(sd));
}

/*
 * Write spec diff to a custom writer.
 */
int
mtree_spec_diff_write_writer(struct mtree_spec_diff *sd, mtree_writer_fn f,
    void *user_data)
{

	assert(sd != NULL);
	assert(f != NULL);

	mtree_writer_set_output_writer(sd->writer, f, user_data);

	return (write_diff(sd));
}
