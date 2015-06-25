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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mtree.h"
#include "mtree_private.h"

#define	IS_DOT(nm)		((nm)[0] == '.' && (nm)[1] == '\0')
#define	IS_DOTDOT(nm)		((nm)[0] == '.' && (nm)[1] == '.' && (nm)[2] == '\0')

mtree_reader *
mtree_reader_create(void)
{

	return calloc(1, sizeof(mtree_reader));
}

void
mtree_reader_free(mtree_reader *r)
{

	assert(r != NULL);

	mtree_reader_reset(r);
	free(r->buf);
	free(r);
}

void
mtree_reader_reset(mtree_reader *r)
{
	mtree_entry *entry, *next;

	assert(r != NULL);

	entry = r->entries;
	while (entry != NULL) {
		next = entry->next;
		mtree_entry_free(entry);
		entry = next;
	}
	mtree_entry_free_data_items(&r->defaults);

	memset(&r->defaults, 0, sizeof(r->defaults));
	r->entries = NULL;
	r->parent = NULL;
	r->buflen = 0;
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
		default:
			/*
			 * Unknown escape sequence, most likely file path
			 * encoded by vis(3), leave it as it is
			 */
			if (esc) {
				c = '\\';
				sidx--;
			}
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
read_mtree_keywords(mtree_reader *r, const char *s, mtree_entry_data *data, bool set)
{
	char word[MAX_LINE_LENGTH];
	char *value, *p;
	intmax_t num;
	long keyword;
	int err;

	for (;;) {
		err = read_word(s, word, sizeof(word));
		if (err < 1)
			break;
		if (word[0] == '\0')
			break;
		s += err;

		value = strchr(word, '=');
		if (value != NULL)
			*value++ = '\0';

		keyword = mtree_parse_keyword(word);
		if (keyword == -1)
			continue;

		if (set == false || (r->keywords & keyword) == 0) {
			/* Unset a keyword */
			data->keywords &= ~keyword;
			continue;
		}

		/* Set a keyword... */
		err = 0;

		/*
		 * We use EINVAL and ENOSYS to signal certain conditions:
		 *
		 *   EINVAL - Value missing or invalid for a keyword
		 *            that needs a value. The keyword is ignored.
		 *   ENOSYS - Unsupported keyword encountered. The
		 *            keyword is ignored.
		 */
		switch (keyword) {
		case MTREE_KEYWORD_CKSUM:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->cksum = (uint32_t) num;
			break;
		case MTREE_KEYWORD_CONTENTS:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->contents, value);
			break;
		case MTREE_KEYWORD_FLAGS:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->flags, value);
			break;
		case MTREE_KEYWORD_GID:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->st_gid = (gid_t) num;
			break;
		case MTREE_KEYWORD_GNAME:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->gname, value);
			break;
		case MTREE_KEYWORD_IGNORE:
			/* No value */
			break;
		case MTREE_KEYWORD_INODE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->st_ino = (ino_t) num;
			break;
		case MTREE_KEYWORD_LINK:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			mtree_copy_string(&data->link, value);
			break;
		case MTREE_KEYWORD_MD5:
		case MTREE_KEYWORD_MD5DIGEST:
			mtree_copy_string(&data->md5digest, value);
			break;
		case MTREE_KEYWORD_MODE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			if (value[0] >= '0' && value[0] <= '9') {
				err = read_number(value, 8, &num,
				    0, 07777);
				if (!err)
					data->st_mode = num;
			} else {
				/* Symbolic mode not supported. */
				err = EINVAL;
			}
			break;
		case MTREE_KEYWORD_NLINK:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, INTMAX_MAX);
			if (!err)
				data->st_nlink = (nlink_t) num;
			break;
		case MTREE_KEYWORD_NOCHANGE:
			/* No value */
			break;
		case MTREE_KEYWORD_OPTIONAL:
			/* No value */
			break;
		case MTREE_KEYWORD_RIPEMD160DIGEST:
		case MTREE_KEYWORD_RMD160:
		case MTREE_KEYWORD_RMD160DIGEST:
			mtree_copy_string(&data->rmd160digest, value);
			break;
		case MTREE_KEYWORD_SHA1:
		case MTREE_KEYWORD_SHA1DIGEST:
			mtree_copy_string(&data->sha1digest, value);
			break;
		case MTREE_KEYWORD_SHA256:
		case MTREE_KEYWORD_SHA256DIGEST:
			mtree_copy_string(&data->sha256digest, value);
			break;
		case MTREE_KEYWORD_SHA384:
		case MTREE_KEYWORD_SHA384DIGEST:
			mtree_copy_string(&data->sha384digest, value);
			break;
		case MTREE_KEYWORD_SHA512:
		case MTREE_KEYWORD_SHA512DIGEST:
			mtree_copy_string(&data->sha512digest, value);
			break;
		case MTREE_KEYWORD_SIZE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, INTMAX_MAX);
			if (!err)
				data->st_size = num;
			break;
		case MTREE_KEYWORD_TIME:
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
			data->st_mtim.tv_sec = num;
			if (p == NULL)
				break;

			/* Nanoseconds */
			err = read_number(p, 10, &num, 0, INTMAX_MAX);
			if (err)
				break;
			data->st_mtim.tv_nsec = num;
			break;
		case MTREE_KEYWORD_TYPE:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			data->type = mtree_parse_type(value);
			break;
		case MTREE_KEYWORD_UID:
			if (value == NULL) {
				err = EINVAL;
				break;
			}
			err = read_number(value, 10, &num, 0, UINT_MAX);
			if (err == 0)
				data->st_uid = (uid_t) num;
			break;
		case MTREE_KEYWORD_UNAME:
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
			data->keywords |= keyword;
	}

	return (0);
}

static int
read_mtree_command(mtree_reader *r, const char *s)
{
	char cmd[10];
	int ret;

	ret = read_word(s, cmd, sizeof(cmd));
	if (ret < 1)
		return (ret);

	if (!strcmp(cmd, "/set"))
		ret = read_mtree_keywords(r, s + ret, &r->defaults, true);
	else if (!strcmp(cmd, "/unset"))
		ret = read_mtree_keywords(r, s + ret, &r->defaults, false);
	else
		ret = 0;

	return (ret);
}

static char *
create_path(mtree_entry *entry)
{
	char buf[MAX_LINE_LENGTH];
	size_t len;
	size_t total;

	total = 0;
	while (entry != NULL) {
		len = strlen(entry->name);
		if ((total + len + 1) >= sizeof(buf)) {
			errno = ENOBUFS;
			return (NULL);
		}
		memmove(buf + 1 + len, buf, total);
		memmove(buf + 1, entry->name, len);
		buf[0] = '/';

		total += len + 1;
		entry = entry->parent;
	}

	buf[total] = '\0';

	/* Skip the leading slash */
	return (strndup(buf + 1, total - 1));
}

static int
read_mtree_spec(mtree_reader *r, const char *s)
{
	mtree_entry *entry;
	char word[MAXPATHLEN];
	char name[MAXPATHLEN];
	char *p;
	int ret;

	ret = read_word(s, word, sizeof(word));
	if (ret < 1)
		return (ret);

	if (IS_DOTDOT(word)) {
		/* Only change the parent, keywords are ignored */
		if (r->parent == NULL || r->parent->parent == NULL) {
			errno = EINVAL;
			return (-1);
		}
		r->parent = r->parent->parent;
		return (0);
	} else {
		if (strunvis(name, word) == -1)
			return (-1);

		entry = mtree_entry_create();
		if (entry == NULL)
			return (-1);
		ret = read_mtree_keywords(r, s + ret, &entry->data, true);
		if (ret == -1) {
			mtree_entry_free(entry);
			return (-1);
		}
		mtree_entry_copy_missing_keywords(entry, &r->defaults);

		p = strchr(name, '/');
		if (p != NULL) {
			char *base;

			/*
			 * If the name contains a slash, it is a relative
			 * path.
			 *
			 * According to the specification, these are valid in
			 * the mtree-2.0 spec and do not change the working
			 * directory.
			 */
			/*
			 * XXX: find a reliable way to link it to a parent, which
			 * might be present in the current spec, another spec
			 * (when merging) or nowhere. This is a problem when
			 * writing this as v1, the parent nodes may need to be
			 * introduced artificially.
			 * Also, the path should be cleaned up, but probably not
			 * the way libarchive does it - by stripping leading ../ and
			 * actually changing the path.
			 */
			p = strdup(p);
			base = basename(p);
			if (base == NULL) {
				mtree_entry_free(entry);
				return (-1);
			}
			entry->name = strdup(base);
			entry->path = strdup(name);
			free(p);
		} else {
			/* Name */
			entry->name = strdup(name);
			entry->parent = r->parent;
			entry->path = create_path(entry);
			if (entry->path == NULL) {
				mtree_entry_free(entry);
				return (-1);
			}
			if (entry->data.type == MTREE_ENTRY_DIR)
				r->parent = entry;
		}

		r->entries = mtree_entry_append(r->entries, entry);
	}

	return (0);
}

static int
parse_line(mtree_reader *r, const char *s)
{
	int ret;

	switch (*s) {
	case '\0':              /* empty line */
	case '#':               /* comment */
		ret = 0;
		break;
	case '/':               /* special commands */
		ret = read_mtree_command(r, s);
		break;
	default:                /* specification */
		ret = read_mtree_spec(r, s);
		break;
	}

	return (ret);
}

int
mtree_reader_add(mtree_reader *r, const char *s, int len)
{
	char buf[MAX_LINE_LENGTH];
	int ret;
	int esc;
	int sidx, bidx;
	bool done;

	assert(r != NULL);
	assert(s != NULL);

	if (len < 0)
		len = strlen(s);
	if (len == 0)
		return (0);

	ret = 0;
	while (len) {
		sidx = 0;
		if (r->buflen)
			esc = r->buf[r->buflen - 1] == '\\';
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

		if (done == false || r->buflen) {
			int nlen;

			nlen = r->buflen + bidx;
			if (done == false) {
				if ((nlen + 1) > MAX_LINE_LENGTH) {
					errno = ENOBUFS;
					return (-1);
				}
				if (!r->buf)
					r->buf = malloc(MAX_LINE_LENGTH);
			}
			memcpy(r->buf + r->buflen, buf, bidx + 1);
			if (done == true) {
				/* Buffer has a full line */
				ret = parse_line(r, r->buf);
				r->buflen = 0;
				if (ret == -1)
					break;
			} else {
				r->buflen += bidx;
			}
		} else if (done == true) {
			/* The whole line is in buf */
			if (bidx) {
				ret = parse_line(r, buf);
				if (ret == -1)
					break;
			}
		}

		s   += sidx;
		len -= sidx;
	}

	return (ret);
}

int
mtree_reader_finish(mtree_reader *r, mtree_entry **entries)
{
	int ret;

	assert(r != NULL);
	assert(entries != NULL);

	ret = 0;
	if (r->buflen > 0) {
		/* When reading the final line, do not require it to be terminated
	 	 * by a newline. */
		if (r->buflen > 1) {
			if (r->buf[r->buflen - 2] == '\\') {
				/* Surely an incomplete line */
				errno = EINVAL;
				ret = -1;
			} else
				ret = parse_line(r, r->buf);
		}
		r->buflen = 0;
	}

	if (ret == 0) {
		/* Entries that have been read are transferred to the caller */
		*entries = r->entries;
		r->entries = NULL;
	}

	mtree_reader_reset(r);
	return (ret);
}

void
mtree_reader_set_options(mtree_reader *r, int options)
{

	assert(r != NULL);

	r->options = options;
}

void
mtree_reader_set_keywords(mtree_reader *r, long keywords)
{

	assert(r != NULL);

	r->keywords = keywords;
}
