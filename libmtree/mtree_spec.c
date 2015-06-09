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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

#define MAX_LINE_LENGTH 	1024

struct _mtree_spec {
	char *buf;
	int   buflen;
};

static int 	parse_line(mtree_spec * spec, const char *line);
static int 	parse_finish(mtree_spec *spec);
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

	if (spec->buf != NULL)
		free(spec->buf);
	free(spec);
}

void
mtree_spec_reset(mtree_spec *spec)
{

	assert(spec != NULL);

	spec->buflen = 0;
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
		else
			ret = parse_finish(spec);
	}

	if (ret == 0) {
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
		} else { /* n == 0 */
			/* Read what remains in the internal buffer */
			ret = parse_finish(spec);
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

	return (parse_chunk(spec, data, len));
}

int
mtree_spec_read_data_end(mtree_spec *spec)
{
	int ret;

	assert(spec != NULL);

	if (spec->buf != NULL) {
		/* Read what remains in the internal buffer */
		ret = parse_finish(spec);
	} else
		ret = 0;

	return (ret);
}

static int
parse_line(mtree_spec *spec, const char *s)
{
	return (0);
}

static int
parse_finish(mtree_spec *spec)
{

	if (spec->buflen == 0)
		return (0);

	/* When reading the final line, do not require it to be terminated
	 * by a newline. */
	if (spec->buflen > 1) {
		if (spec->buf[spec->buflen - 2] == '\\') {
			/* Surely an incomplete line */
			errno = EINVAL;
			return (-1);
		}
		parse_line(spec, spec->buf);
	}
	spec->buflen = 0;
	return (0);
}

static int
parse_chunk(mtree_spec *spec, const char *s, int len)
{
	char buf[MAX_LINE_LENGTH];
	int esc;
	int sidx, bidx;
	bool done;

	if (len < 0)
		len = strlen(s);
	if (len == 0)
		return (0);

	while (len) {
		sidx = 0;
		if (spec->buflen)
			esc = spec->buf[spec->buflen - 1] == '\\';
		else {
			/* Eat blank characters at the start of the line */
			while (sidx < len && (s[sidx] == ' ' || s[sidx] == '\t'))
				sidx++;
			if (sidx == len)
				break;
			esc = 0;
		}

		bidx = 0;
		done = false;
		while (sidx < len) {
			switch (s[sidx]) {
			case '\n':
				/* Eat newlines as well as escaped newlines, keep
				 * reading after an escaped one */
				if (esc) {
					bidx--;
					esc = 0;
				} else
					done = true;
				break;
			case '\\':
				buf[bidx++] = s[sidx];
				esc ^= 1;
				break;
			default:
				buf[bidx++] = s[sidx];
				esc = 0;
				break;
			}

			if (bidx == MAX_LINE_LENGTH) {
				errno = ENOBUFS;
				return (-1);
			}
			sidx++;
			if (done == true)
				break;
		}
		buf[bidx] = '\0';

		if (done == false || spec->buflen) {
			int nlen;

			nlen = spec->buflen + bidx;
			if (done == false) {
				if ((nlen + 1) > MAX_LINE_LENGTH) {
					errno = ENOBUFS;
					return (-1);
				}
				if (!spec->buf)
					spec->buf = malloc(MAX_LINE_LENGTH);
			}
			memcpy(spec->buf + spec->buflen, buf, bidx + 1);
			if (done == true) {
				/* Buffer has a full line */
				parse_line(spec, spec->buf);
				spec->buflen = 0;
			} else {
				spec->buflen += bidx;
			}
		} else if (done == true) {
			/* The whole line is in buf */
			if (bidx)
				parse_line(spec, buf);
		}

		s   += sidx;
		len -= sidx;
	}

	return (0);
}
