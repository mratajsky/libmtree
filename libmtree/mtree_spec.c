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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree_spec.h"

#ifndef MIN
#define	MIN(a, b)	((a) > (b) ? (b) : (a))
#endif
#ifndef MAX
#define	MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

#define BUFSIZE_LIMIT 1024

struct _mtree_spec {
	char *buf;
	int   buflen;
	int   bufsize;
};

static void 	reset_spec_buf(mtree_spec *spec);
static int 	parse_line(mtree_spec * spec, const char *line);
static int 	parse_chunk(mtree_spec * spec, const char *chunk, int len);

mtree_spec *
mtree_spec_create(void)
{
	mtree_spec *spec;

	spec = malloc(sizeof(mtree_spec));

	return (spec);
}

void
mtree_spec_free(mtree_spec * spec)
{

	assert(spec != NULL);

	mtree_spec_reset(spec);
	free(spec);
}

void
mtree_spec_reset(mtree_spec *spec)
{

	assert(spec != NULL);

	reset_spec_buf(spec);
}

int
mtree_spec_read_file(mtree_spec *spec, const char *file)
{
	FILE *fp;
	int ret;
	char buf[512];

	assert(spec != NULL);
	assert(file != NULL);

	fp = fopen(file, "r");
	if (fp == NULL)
		return (-1);

	ret = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		ret = parse_chunk(spec, buf, -1);
		if (ret == -1)
			break;
	}

	if (ret == 0) {
		/* If there is no newline at the end of the file, the last
		 * line is still in the buffer */
		if (ferror(fp))
			ret = -1;
		else if (spec->buf != NULL)
			ret = parse_line(spec, spec->buf);
	}

	if (ret == 0) {
		fclose(fp);
	} else {
		int err;

		err = errno;
		fclose(fp);
		errno = err;
	}
	reset_spec_buf(spec);
	return (ret);
}

int
mtree_spec_read_fd(mtree_spec * spec, int fd)
{
	ssize_t n;
	int ret;
	char buf[512];

	assert(spec != NULL);

	ret = 0;
	for (;;) {
		n = read(fd, buf, sizeof(buf) - 1);
		if (n > 0) {
			buf[n] = '\0';
			ret = parse_chunk(spec, buf, n);
		} else if (n < 0) {
			ret = -1;
		} else /* n == 0 */
			break;

		if (ret == -1)
			break;
	}
	reset_spec_buf(spec);
	return (ret);
}

int
mtree_spec_read_data(mtree_spec * spec, const char *data, int len)
{

	assert(spec != NULL);
	assert(data != NULL);

	return (parse_chunk(spec, data, len));
}

int
mtree_spec_read_data_end(mtree_spec *spec)
{
	int ret;

	assert(spec != NULL);

	if (spec->buf != NULL) {
		/* There is at most one line left in the buffer */
		ret = parse_line(spec, spec->buf);
		reset_spec_buf(spec);
	} else
		ret = 0;

	return (ret);
}

static int
append_spec_buf(mtree_spec *spec, const char *chunk, int len)
{
	int buflen;

	assert(len >= 0);

	if (len == 0)
		return (0);

	buflen = spec->buflen + len;
	if ((buflen + 1) > BUFSIZE_LIMIT) {
		errno = ENOBUFS;
		return (-1);
	}
	if ((buflen + 1) > spec->bufsize)
		spec->bufsize = MAX(buflen + 1, MIN(BUFSIZE_LIMIT, spec->bufsize * 2));

	spec->buf = realloc(spec->buf, spec->bufsize);

	memcpy(spec->buf + spec->buflen, chunk, len);
	spec->buflen = buflen;
	spec->buf[buflen] = '\0';

	return (0);
}

static void
reset_spec_buf(mtree_spec *spec)
{

	if (spec->buf == NULL)
		return;

	free(spec->buf);
	spec->buf = NULL;
	spec->buflen = 0;
	spec->bufsize = 0;
}

/* Parse the first line of the string */
static int
parse_line(mtree_spec *spec, const char *s)
{
	return (0);
}

static int
parse_chunk(mtree_spec *spec, const char *s, int len)
{
	int ret;

	if (len == 0)
		return (0);
	if (len < 0)
		len = strlen(s);

	if (spec->buf != NULL) {
		char *nl;

		nl = memchr(s, '\n', len);
		if (nl == NULL)
			return append_spec_buf(spec, s, len);

		/* There is something in the buffer that does not include a newline
		 * and the current chunk does, combine the buffer and the first line
		 * of the chunk */
		if (append_spec_buf(spec, s, nl - s + 1) == -1)
			return (-1);

		ret = parse_line(spec, spec->buf);
		if (ret == -1)
			return (-1);

		/* A single line was passed to parse_line(), so if there wasn't
		 * an error, the whole line must have been parsed */
		assert(ret == spec->buflen);

		reset_spec_buf(spec);
	}

	for (;;) {
		ret = parse_line(spec, s);
		if (ret < 0) {
			ret = -1;
			break;
		}
		if (ret == 0) {
			/* No newline left in the chunk, append the rest to
			 * the buffer */
			if (len > 0)
				append_spec_buf(spec, s, len);
			break;
		}

		s   += ret;
		len -= ret;
	}

	return (ret);
}
