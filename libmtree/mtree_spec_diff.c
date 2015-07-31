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

struct mtree_spec_diff *
mtree_spec_diff_create(struct mtree_spec *spec1, struct mtree_spec *spec2)
{
	struct mtree_spec_diff	*sd;
	struct mtree_entry	*s1only;
	struct mtree_entry	*s2only;
	struct mtree_entry	*diff;
	struct mtree_entry	*e1, *e2;
	struct mtree_entry	*next;

	assert(spec1 != NULL);
	assert(spec2 != NULL);

	s1only	= mtree_entry_copy_all(spec1->entries);
	s2only	= mtree_entry_copy_all(spec2->entries);
	diff	= NULL;
	e1	= s1only;
	while (e1 != NULL) {
		next = e1->next;
		e2 = mtree_entry_find_path(s2only, e1->path);
		if (e2 != NULL) {
			long differ;

			s1only = mtree_entry_unlink(s1only, e1);
			s2only = mtree_entry_unlink(s2only, e2);
			if (mtree_entry_compare(e1, e2, &differ) != 0) {
				mtree_entry_set_keywords(e1, differ, 0);
				mtree_entry_set_keywords(e2, differ, 0);
				diff = mtree_entry_prepend(diff, e2);
				diff = mtree_entry_prepend(diff, e1);
			}
		}
		e1 = next;
	}
	sd = malloc(sizeof(struct mtree_spec_diff));
	sd->s1only = mtree_entry_reverse(s1only);
	sd->s2only = mtree_entry_reverse(s2only);
	sd->diff   = diff;
	return (sd);
}

void
mtree_spec_diff_free(struct mtree_spec_diff *sd)
{

	assert(sd != NULL);

	mtree_entry_free_all(sd->s1only);
	mtree_entry_free_all(sd->s2only);
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

int
mtree_spec_diff_write_file(struct mtree_spec_diff *sd, FILE *fp)
{
	struct mtree_writer *writer;
	int ret;

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
	struct mtree_writer *writer;
	int ret;

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
