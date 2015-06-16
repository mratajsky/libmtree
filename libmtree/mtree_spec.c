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

#include <assert.h>
#include <errno.h>
#include <fts.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

mtree_spec *
mtree_spec_create(void)
{
	mtree_spec *spec;

	spec = calloc(1, sizeof(mtree_spec));
	spec->reader = mtree_reader_create();

	return (spec);
}

void
mtree_spec_free(mtree_spec * spec)
{

	assert(spec != NULL);

	mtree_reader_free(spec->reader);
	free(spec);
}

void
mtree_spec_reset(mtree_spec *spec)
{

	assert(spec != NULL);

	mtree_entry_free_all(spec->entries);
	spec->entries = NULL;

	mtree_reader_reset(spec->reader);
}

int
mtree_spec_read_file(mtree_spec *spec, const char *file)
{
	mtree_entry *entries;
	FILE *fp;
	char buf[512];
	int ret;

	assert(spec != NULL);
	assert(file != NULL);

	fp = fopen(file, "r");
	if (fp == NULL)
		return (-1);

	ret = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		ret = mtree_reader_add(spec->reader, buf, -1);
		if (ret == -1)
			break;
	}

	if (ret == 0) {
		/* If there is no newline at the end of the file, the last
		 * line is still in the buffer */
		if (ferror(fp))
			ret = -1;
		else
			ret = mtree_reader_finish(spec->reader, &entries);
	}

	if (ret == 0) {
		spec->entries = mtree_entry_append(spec->entries, entries);
		fclose(fp);
	} else {
		int err;

		err = errno;
		fclose(fp);
		errno = err;
	}
	return (ret);
}

int
mtree_spec_read_fd(mtree_spec * spec, int fd)
{
	mtree_entry *entries;
	char buf[512];
	ssize_t n;
	int ret;

	assert(spec != NULL);

	ret = 0;
	for (;;) {
		n = read(fd, buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			ret = mtree_reader_add(spec->reader, buf, n);
		} else if (n < 0) {
			ret = -1;
		} else { /* n == 0 */
			ret = mtree_reader_finish(spec->reader, &entries);
			if (ret == 0)
				spec->entries = mtree_entry_append(spec->entries,
				    entries);
			break;
		}
		if (ret == -1)
			break;
	}
	return (ret);
}

int
mtree_spec_read_data(mtree_spec * spec, const char *data, int len)
{

	assert(spec != NULL);
	assert(data != NULL);

	// XXX probably mark that we are reading until _end is called
	return (mtree_reader_add(spec->reader, data, len));
}

int
mtree_spec_read_data_end(mtree_spec *spec)
{
	mtree_entry *entries;
	int ret;

	assert(spec != NULL);

	ret = mtree_reader_finish(spec->reader, &entries);
	if (ret == 0)
		spec->entries = mtree_entry_append(spec->entries, entries);

	return (ret);
}

int
mtree_spec_add_file(mtree_spec *spec, const char *path)
{
	mtree_entry *entry;

	assert (spec != NULL);
	assert (path != NULL);

	entry = mtree_entry_create_from_file (path);
	if (entry == NULL)
		return (-1);

	spec->entries = mtree_entry_append(spec->entries, entry);
	return (0);
}

int
mtree_spec_add_directory(mtree_spec *spec, const char *path)
{
	FTS *fts;
	FTSENT *ftsent;
	mtree_entry *first, *entry;
	char *argv[2];
	struct stat st;
	int ret;

	assert (spec != NULL);
	assert (path != NULL);

	/* Make sure the supplied path is a directory */
	if (stat(path, &st) == -1)
		return (-1);
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return (-1);
	}

	argv[0] = (char *) path;
	argv[1] = NULL;
	fts = fts_open(argv, FTS_PHYSICAL, NULL);
	if (fts == NULL)
		return (-1);

	first = NULL;
	ret = 0;
	while (ret == 0) {
		ftsent = fts_read(fts);
		if (ftsent == NULL)
			break;

		switch (ftsent->fts_info) {
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			errno = ftsent->fts_errno;
			ret = -1;
			break;
		case FTS_DP:
		case FTS_DC:
		case FTS_DOT:
			break;
		default:
			entry = mtree_entry_create_from_ftsent(ftsent);
			if (entry == NULL) {
				ret = -1;
				break;
			}
			first = mtree_entry_append(first, entry);
			break;
		}
	}

	// XXX probably add option whether to stop or skip on error
	if (ret == 0) {
		spec->entries = mtree_entry_append(spec->entries, first);
	} else {
		int err;

		err = errno;
		fts_close(fts);
		errno = err;
	}
	return (ret);
}
