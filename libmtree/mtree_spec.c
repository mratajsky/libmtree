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
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

#define MAX_LINE_LENGTH 	1024

struct _mtree_spec {
	mtree_entry 	*entries;
	mtree_entry_data defaults;
	char 		*buf;
	int   		 buflen;
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
read_word(const char *s, char *buf, int bufsz)
{
	int sidx, bidx, qidx;
	int esc, qlvl;
	bool done;

	sidx = 0;
	while (s[sidx] == ' ' || s[sidx] == '\t')
		sidx++;

	esc  = 0;
	bidx = 0;
	qidx = -1;
	qlvl = 0;
	done = false;
	do {
		char c = s[sidx++];

		switch (c) {
		case '\t':              /* end of the word */
		case ' ':
		case '#':		/* comment */
			if (!esc && qlvl == 0)
				done = true;
			break;
		case '\0':
			if (qlvl > 0) {
				errno = EINVAL;
				return (-1);
			}
			done = true;
			break;
		case '\\':
			esc ^= 1;
			if (esc == 1)
				continue;
			break;
		case '`':		/* quote */
		case '\'':
		case '"':
			if (esc)
				break;
			if (qlvl == 0) {
				qlvl++;
				qidx = sidx;
			} else if (c == buf[qidx]) {
				qlvl--;
				if (qlvl > 0) {
					do {
						qidx--;
					} while (buf[qidx] != '`' &&
					    buf[qidx] != '\'' &&
					    buf[qidx] != '"');
				} else
					qidx = -1;
			} else {
				qlvl++;
				qidx = sidx;
			}
			break;
		case 'a':
			if (esc)
				c = '\a';
			break;
		case 'b':
			if (esc)
				c = '\b';
			break;
		case 'f':
			if (esc)
				c = '\f';
			break;
		case 'r':
			if (esc)
				c = '\r';
			break;
		case 't':
			if (esc)
				c = '\t';
			break;
		case 'v':
			if (esc)
				c = '\v';
			break;
		}

		if (!done) {
			buf[bidx++] = c;
			esc = 0;
		}
	} while (bidx < bufsz && !done);

	if (bidx == bufsz) {
		errno = ENOBUFS;
		return (-1);
	}
	buf[bidx] = '\0';

	/* The last read character is not a part of the word */
	return (sidx - 1);
}

static int
read_number(const char *tok, u_int base, intmax_t *res, intmax_t min,
    intmax_t max)
{
	char *end;
	intmax_t val;

	val = strtoimax(tok, &end, base);
	if (end == tok || end[0] != '\0')
		return (EINVAL);
	if (val < min || val > max)
		return (EDOM);
	*res = val;
	return (0);
}

static int
read_mtree_keywords(const char *s, mtree_entry_data *data, bool set)
{
	char keyword[MAX_LINE_LENGTH];
	char *value, *p;
	intmax_t num;
	int32_t field;
	int32_t type;
#ifdef HAVE_STRTOFFLAGS
	u_long flset, flclr;
#endif
	int err;

	for (;;) {
		err = read_word(s, keyword, sizeof(keyword));
		if (err < 1)
			break;
		if (keyword[0] == '\0')
			break;
		s += err;

		value = strchr(keyword, '=');
		if (value != NULL)
			*value++ = '\0';

		field = mtree_str_to_field(keyword);
		if (field == -1)
			continue;
		if (set == false) {
			/* Unset a field */
			data->fields &= ~field;
			continue;
		}

		/* Set a field... */
		err = 0;

		/*
		 * We use EINVAL and ENOSYS to signal certain conditions:
		 *
		 *   EINVAL - Value missing or invalid for a keyword
		 *            that needs a value. The keyword is ignored.
		 *   ENOSYS - Unsupported keyword encountered. The
		 *            keyword is ignored.
		 */
		switch (field) {
		case MTREE_F_CKSUM:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->cksum = (uint32_t) num;
			break;
		case MTREE_F_CONTENTS:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->contents, value);
			break;
#ifdef HAVE_STRTOFFLAGS
		case MTREE_F_FLAGS:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			flset = flclr = 0;
			if (strtofflags(&value, &flset, &flclr) == 0) {
				data->stat.st_flags &= ~flclr;
				data->stat.st_flags |= flset;
			} else
				err = errno;
			break;
#endif
		case MTREE_F_GID:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->stat.st_gid = (gid_t) num;
			break;
		case MTREE_F_GNAME:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->gname, value);
			break;
		case MTREE_F_IGNORE:
			/* No value */
			break;
		case MTREE_F_INODE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->stat.st_ino = (ino_t) num;
			break;
		case MTREE_F_LINK:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->link, value);
			break;
		case MTREE_F_MD5:
		case MTREE_F_MD5DIGEST:
			mtree_copy_string(&data->md5digest, value);
			break;
		case MTREE_F_MODE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			if (value[0] >= '0' && value[0] <= '9') {
				err = read_number(value, 8, &num,
				    0, 07777);
				if (!err) {
					data->stat.st_mode &= S_IFMT;
					data->stat.st_mode |= num;
				}
			} else {
				/* Symbolic mode not supported. */
				err = EINVAL;
				break;
			}
			break;
		case MTREE_F_NLINK:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, INTMAX_MAX);
			if (!err)
				data->stat.st_nlink = (nlink_t) num;
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
			mtree_copy_string(&data->rmd160digest, value);
			break;
		case MTREE_F_SHA1:
		case MTREE_F_SHA1DIGEST:
			mtree_copy_string(&data->sha1digest, value);
			break;
		case MTREE_F_SHA256:
		case MTREE_F_SHA256DIGEST:
			mtree_copy_string(&data->sha256digest, value);
			break;
		case MTREE_F_SHA384:
		case MTREE_F_SHA384DIGEST:
			mtree_copy_string(&data->sha384digest, value);
			break;
		case MTREE_F_SHA512:
		case MTREE_F_SHA512DIGEST:
			mtree_copy_string(&data->sha512digest, value);
			break;
		case MTREE_F_SIZE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, INTMAX_MAX);
			if (!err)
				data->stat.st_size = num;
			break;
		case MTREE_F_TIME:
			if (value == NULL) {
				err = EINVAL;
				break;
			}

			/* Seconds */
			p = strchr(value, '.');
			if (p != NULL)
				*p++ = '\0';
			err = read_number(value, 10, &num, 0, INTMAX_MAX);
			if (err)
				break;
			data->stat.st_mtim.tv_sec = num;
			if (p == NULL)
				break;

			/* Nanoseconds */
			err = read_number(p, 10, &num, 0, INTMAX_MAX);
			if (err)
				break;
			data->stat.st_mtim.tv_nsec = num;
			break;
		case MTREE_F_TYPE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			type = mtree_str_to_type(value);
			if (type != -1) {
				data->stat.st_mode &= ~S_IFMT;
				data->stat.st_mode |= type;
			} else
				err = EINVAL;
			break;
		case MTREE_F_UID:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->stat.st_uid = (uid_t) num;
			break;
		case MTREE_F_UNAME:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->uname, value);
			break;
		default:
			err = ENOSYS;
			break;
		}

		if (err == 0)
			data->fields |= field;
	}

	return (0);
}

static int
read_mtree_command(mtree_spec *spec, const char *s)
{
	char cmd[10];
	int ret;

	ret = read_word(s, cmd, sizeof(cmd));
	if (ret < 1)
		return (ret);

	if (!strcmp(cmd, "/set"))
		ret = read_mtree_keywords(s + ret, &spec->defaults, true);
	else if (!strcmp(cmd, "/unset"))
		ret = read_mtree_keywords(s + ret, &spec->defaults, false);
	else
		ret = 0;

	return (ret);
}

static int
parse_line(mtree_spec *spec, const char *s)
{
	int ret;

	switch (*s) {
	case '\0':              /* empty line */
	case '#':               /* comment */
		ret = 0;
		break;
	case '/':               /* special commands */
		ret = read_mtree_command(spec, s);
		break;
	default:                /* specification */
		break;
	}

	return (ret);
}

static int
parse_finish(mtree_spec *spec)
{
	int ret;

	if (spec->buflen == 0)
		return (0);

	/* When reading the final line, do not require it to be terminated
	 * by a newline. */
	ret = 0;
	if (spec->buflen > 1) {
		if (spec->buf[spec->buflen - 2] == '\\') {
			/* Surely an incomplete line */
			errno = EINVAL;
			ret = -1;
		} else
			ret = parse_line(spec, spec->buf);
	}
	spec->buflen = 0;
	return (ret);
}

static int
parse_chunk(mtree_spec *spec, const char *s, int len)
{
	char buf[MAX_LINE_LENGTH];
	int ret;
	int esc;
	int sidx, bidx;
	bool done;

	if (len < 0)
		len = strlen(s);
	if (len == 0)
		return (0);

	ret = 0;
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
				ret = parse_line(spec, spec->buf);
				spec->buflen = 0;
				if (ret == -1)
					break;
			} else {
				spec->buflen += bidx;
			}
		} else if (done == true) {
			/* The whole line is in buf */
			if (bidx) {
				ret = parse_line(spec, buf);
				if (ret == -1)
					break;
			}
		}

		s   += sidx;
		len -= sidx;
	}

	return (ret);
}
