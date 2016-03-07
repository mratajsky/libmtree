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
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

/*
 * Create a new mtree_spec.
 */
struct mtree_spec *
mtree_spec_create(void)
{
	struct mtree_spec *spec;

	spec = calloc(1, sizeof(struct mtree_spec));
	if (spec == NULL)
		return (NULL);
	spec->reader = mtree_reader_create();
	if (spec->reader == NULL) {
		free(spec);
		return (NULL);
	}
	spec->writer = mtree_writer_create();
	if (spec->writer == NULL) {
		mtree_reader_free(spec->reader);
		free(spec);
		return (NULL);
	}
	/*
	 * Default sets of keywords to read/write.
	 *
	 * Path reading uses to `default' set, which tries to read the most
	 * usable values without doing any expensive calculations.
	 * Spec reading reads everything included in the spec by default.
	 */
	mtree_spec_set_read_path_keywords(spec, MTREE_KEYWORD_MASK_DEFAULT);
	mtree_spec_set_read_spec_keywords(spec, MTREE_KEYWORD_MASK_ALL);
	mtree_spec_set_read_options(spec, MTREE_READ_MERGE);
	return (spec);
}

/*
 * Free the given mtree_spec.
 */
void
mtree_spec_free(struct mtree_spec * spec)
{

	assert(spec != NULL);

	if (spec->entries != NULL)
		mtree_entry_free_all(spec->entries);

	mtree_reader_free(spec->reader);
	mtree_writer_free(spec->writer);

	free(spec);
}

/*
 * Read spec data from the given FILE.
 */
int
mtree_spec_read_spec_file(struct mtree_spec *spec, FILE *fp)
{

	assert(spec != NULL);
	assert(fp != NULL);

	if (spec->reading) {
		mtree_reader_set_errno_error(spec->reader, EPERM,
		    "Reading not finalized, call mtree_spec_read_spec_data_finish()");
		return (-1);
	}
	if (mtree_reader_add_from_file(spec->reader, fp) == -1)
		return (-1);

	return (mtree_reader_finish(spec->reader, &spec->entries));
}

/*
 * Read spec data from the given file descriptor.
 */
int
mtree_spec_read_spec_fd(struct mtree_spec *spec, int fd)
{

	assert(spec != NULL);
	assert(fd != -1);

	if (spec->reading) {
		mtree_reader_set_errno_error(spec->reader, EPERM,
		    "Reading not finalized, call mtree_spec_read_spec_data_finish()");
		return (-1);
	}
	if (mtree_reader_add_from_fd(spec->reader, fd) == -1)
		return (-1);

	return (mtree_reader_finish(spec->reader, &spec->entries));
}

/*
 * Read spec data from the given buffer.
 */
int
mtree_spec_read_spec_data(struct mtree_spec *spec, const char *data, size_t len)
{

	assert(spec != NULL);

	if (len == 0)
		return (0);

	assert(data != NULL);

	spec->reading = 1;
	return (mtree_reader_add(spec->reader, data, len));
}

/*
 * Finish reading spec data when reading from buffers.
 *
 * This may parse remaining data that's stored in the reader's buffer.
 */
int
mtree_spec_read_spec_data_finish(struct mtree_spec *spec)
{
	int ret;

	assert(spec != NULL);

	ret = mtree_reader_finish(spec->reader, &spec->entries);

	spec->reading = 0;
	return (ret);
}

int
mtree_spec_read_path(struct mtree_spec *spec, const char *path)
{

	assert(spec != NULL);
	assert(path != NULL);

	return (mtree_reader_read_path(spec->reader, path, &spec->entries));
}

/*
 * Write the spec to the given FILE.
 */
int
mtree_spec_write_file(struct mtree_spec *spec, FILE *fp)
{

	assert(spec != NULL);
	assert(fp != NULL);

	mtree_writer_set_output_file(spec->writer, fp);

	return (mtree_writer_write_entries(spec->writer, spec->entries));
}

/*
 * Write the spec to the given file descriptor.
 */
int
mtree_spec_write_fd(struct mtree_spec *spec, int fd)
{

	assert(spec != NULL);
	assert(fd != -1);

	mtree_writer_set_output_fd(spec->writer, fd);

	return (mtree_writer_write_entries(spec->writer, spec->entries));
}

/*
 * Write spec to the given user-defined writer.
 */
int
mtree_spec_write_writer(struct mtree_spec *spec, mtree_writer_fn f,
    void *user_data)
{

	assert(spec != NULL);
	assert(f != NULL);

	mtree_writer_set_output_writer(spec->writer, f, user_data);

	return (mtree_writer_write_entries(spec->writer, spec->entries));
}

/*
 * Get entries of the spec.
 */
struct mtree_entry *
mtree_spec_get_entries(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (spec->entries);
}

/*
 * Get entries of the spec and remove them from the spec.
 */
struct mtree_entry *
mtree_spec_take_entries(struct mtree_spec *spec)
{
	struct mtree_entry *entries;

	assert(spec != NULL);

	entries = spec->entries;
	spec->entries = NULL;

	return (entries);
}

/*
 * Assign entries to the spec. The spec claims ownership.
 */
void
mtree_spec_set_entries(struct mtree_spec *spec, struct mtree_entry *entries)
{

	assert(spec != NULL);

	if (spec->entries != NULL)
		mtree_entry_free_all(spec->entries);

	spec->entries = entries;
}

/*
 * Assign entries to the spec, copying them first.
 */
int
mtree_spec_copy_entries(struct mtree_spec *spec, const struct mtree_entry *entries)
{
	struct mtree_entry *copy;

	assert(spec != NULL);

	if (entries != NULL) {
		copy = mtree_entry_copy_all(entries);
		if (copy == NULL)
			return (-1);
	} else
		copy = NULL;

	mtree_spec_set_entries(spec, copy);
	return (0);
}

/*
 * Get the last error that occured in the reader.
 */
const char *
mtree_spec_get_read_error(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_reader_get_error(spec->reader));
}

/*
 * Get reading options.
 */
int
mtree_spec_get_read_options(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_reader_get_options(spec->reader));
}

/*
 * Set reading options.
 */
void
mtree_spec_set_read_options(struct mtree_spec *spec, int options)
{

	assert(spec != NULL);

	mtree_reader_set_options(spec->reader, options);
}

/*
 * Set reading filter.
 */
void
mtree_spec_set_read_filter(struct mtree_spec *spec, mtree_entry_filter_fn f,
    void *user_data)
{

	assert(spec != NULL);

	mtree_reader_set_filter(spec->reader, f, user_data);
}

/*
 * Get keywords to be read from path.
 */
uint64_t
mtree_spec_get_read_path_keywords(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_reader_get_path_keywords(spec->reader));
}

/*
 * Set keywords to be read from path.
 */
void
mtree_spec_set_read_path_keywords(struct mtree_spec *spec, uint64_t keywords)
{

	assert(spec != NULL);

	mtree_reader_set_path_keywords(spec->reader, keywords);
}

/*
 * Get keywords to be read from spec files.
 */
uint64_t
mtree_spec_get_read_spec_keywords(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_reader_get_spec_keywords(spec->reader));
}

/*
 * Set keywords to be read from spec files.
 */
void
mtree_spec_set_read_spec_keywords(struct mtree_spec *spec, uint64_t keywords)
{

	assert(spec != NULL);

	mtree_reader_set_spec_keywords(spec->reader, keywords);
}

/*
 * Get writing format.
 */
mtree_format
mtree_spec_get_write_format(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_writer_get_format(spec->writer));
}

/*
 * Set writing format.
 */
void
mtree_spec_set_write_format(struct mtree_spec *spec, mtree_format format)
{

	assert(spec != NULL);

	mtree_writer_set_format(spec->writer, format);
}

/*
 * Get writing options.
 */
int
mtree_spec_get_write_options(struct mtree_spec *spec)
{

	assert(spec != NULL);

	return (mtree_writer_get_options(spec->writer));
}

/*
 * Set writing options.
 */
void
mtree_spec_set_write_options(struct mtree_spec *spec, int options)
{

	assert(spec != NULL);

	mtree_writer_set_options(spec->writer, options);
}
