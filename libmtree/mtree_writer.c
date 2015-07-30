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
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_private.h"

#define	INDENTNAMELEN	15
#define	INDENTLINELEN	80

mtree_writer *
mtree_writer_create(void)
{

	return calloc(1, sizeof(mtree_writer));
}

void
mtree_writer_free(mtree_writer *w)
{

	free(w);
}

static int
write_fd(mtree_writer *w, const char *s)
{
	int len;
	int n;

	len = strlen(s);
	while (len > 0) {
		if ((n = write(w->dst.fd, s, len)) == -1) {
			if (errno == EINTR ||
#ifdef EWOULDBLOCK
			    errno == EWOULDBLOCK ||
#endif
			    errno == EAGAIN)
				continue;
			return (-1);
		}
		s   += n;
		len -= n;
	}
	return (0);

}

static int
write_file(mtree_writer *w, const char *s)
{

	if (fputs(s, w->dst.fp) == EOF)
		return (-1);

	return (0);
}

/*
 * Write a part of mtree spec output using the configured writer
 */
static int
write_part(mtree_writer *w, int *offset, const char *format, ...)
{
	char buf[MAX_LINE_LENGTH];
	va_list args;
	int len;
	int ret;

	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	/*
	 * Write indentation/line continuation
	 */
	if (offset != NULL &&
	    w->options & (MTREE_WRITE_SPLIT_LONG_LINES | MTREE_WRITE_INDENT)) {
		if (*offset + strlen(buf) > INDENTLINELEN - 3) {
			char cont[INDENTLINELEN];

			if (w->options & MTREE_WRITE_SPLIT_LONG_LINES) {
				ret = w->writer(w, " \\\n");
				if (ret == -1)
					return (-1);
				*offset = 0;
			}
			/*
			 * Indent start of the new line
			 */
			if (w->options & MTREE_WRITE_INDENT) {
				snprintf(cont, sizeof(cont), "%*s",
				    INDENTNAMELEN + w->indent, "");
				ret = w->writer(w, cont);
				if (ret == -1)
					return (-1);
				*offset = INDENTNAMELEN + w->indent;
			}
		}

		*offset += len;
	}

	ret = w->writer(w, buf);
	if (ret == -1)
		return (0);

	return (len);
}

#define WRITE_KW_PREFIX		0x01
#define WRITE_KW_POSTFIX	0x02

/*
 * Write a single keyword value, optionally with a space before or after it
 */
static int
write_keyword(mtree_writer *w, mtree_entry_data *data, int *offset, long keyword,
    int options)
{
	char *path;
	int ret;

#define WRITE(fmt, ...)								\
	(options & WRITE_KW_PREFIX)						\
		? (options & WRITE_KW_POSTFIX)					\
			? write_part(w, offset, " " fmt " ", __VA_ARGS__)	\
			: write_part(w, offset, " " fmt, __VA_ARGS__)		\
		: (options & WRITE_KW_POSTFIX)					\
			? write_part(w, offset, fmt " ", __VA_ARGS__)		\
			: write_part(w, offset, fmt, __VA_ARGS__)

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		return WRITE("cksum=%lu", data->cksum);
	case MTREE_KEYWORD_CONTENTS:
		path = mtree_get_vispath(data->contents);
		if (path != NULL) {
			ret = WRITE("contents=%s", path);
			free(path);
		} else
			ret = -1;
		return (ret);
	case MTREE_KEYWORD_DEVICE:
		if (data->device != NULL) {
			if (mtree_device_string(data->device, &s) == 0) {
				ret = WRITE("device=%s", s);
				free(s);
			} else
				ret = -1;
		}
		return (ret);
	case MTREE_KEYWORD_FLAGS:
		return WRITE("flags=%s", data->flags);
	case MTREE_KEYWORD_GID:
		return WRITE("gid=%ju", data->st_gid);
	case MTREE_KEYWORD_GNAME:
		return WRITE("gname=%s", data->gname);
	case MTREE_KEYWORD_IGNORE:
		return WRITE("ignore", NULL);
	case MTREE_KEYWORD_INODE:
		return WRITE("inode=%ju", data->st_ino);
	case MTREE_KEYWORD_LINK:
		path = mtree_get_vispath(data->link);
		if (path != NULL) {
			ret = WRITE("link=%s", path);
			free(path);
		} else
			ret = -1;
		return (ret);
	case MTREE_KEYWORD_MD5:
		return WRITE("md5=%lu", data->md5digest);
	case MTREE_KEYWORD_MD5DIGEST:
		return WRITE("md5digest=%lu", data->md5digest);
	case MTREE_KEYWORD_MODE:
		return WRITE("mode=%#o", data->st_mode);
	case MTREE_KEYWORD_NLINK:
		return WRITE("nlink=%ju", data->st_nlink);
	case MTREE_KEYWORD_NOCHANGE:
		return WRITE("nochange", NULL);
	case MTREE_KEYWORD_OPTIONAL:
		return WRITE("optional", NULL);
	case MTREE_KEYWORD_RIPEMD160DIGEST:
		return WRITE("ripemd160digest=%s", data->rmd160digest);
	case MTREE_KEYWORD_RMD160:
		return WRITE("rmd160=%s", data->rmd160digest);
	case MTREE_KEYWORD_RMD160DIGEST:
		return WRITE("rmd160digest=%s", data->rmd160digest);
	case MTREE_KEYWORD_SHA1:
		return WRITE("sha1=%s", data->sha1digest);
	case MTREE_KEYWORD_SHA1DIGEST:
		return WRITE("sha1digest=%s", data->sha1digest);
	case MTREE_KEYWORD_SHA256:
		return WRITE("sha256=%s", data->sha256digest);
	case MTREE_KEYWORD_SHA256DIGEST:
		return WRITE("sha256digest=%s", data->sha256digest);
	case MTREE_KEYWORD_SHA384:
		return WRITE("sha384=%s", data->sha384digest);
	case MTREE_KEYWORD_SHA384DIGEST:
		return WRITE("sha384digest=%s", data->sha384digest);
	case MTREE_KEYWORD_SHA512:
		return WRITE("sha512=%s", data->sha512digest);
	case MTREE_KEYWORD_SHA512DIGEST:
		return WRITE("sha512digest=%s", data->sha512digest);
	case MTREE_KEYWORD_SIZE:
		return WRITE("size=%ju", data->st_size);
	case MTREE_KEYWORD_TAGS:
		return WRITE("tags=%s", data->tags);
	case MTREE_KEYWORD_TIME:
		return WRITE("time=%jd.%09jd",
		    data->st_mtim.tv_sec,
		    data->st_mtim.tv_nsec);
	case MTREE_KEYWORD_TYPE:
		return WRITE("type=%s", mtree_type_string(data->type));
	case MTREE_KEYWORD_UID:
		return WRITE("uid=%ju", data->st_uid);
	case MTREE_KEYWORD_UNAME:
		return WRITE("uname=%s", data->uname);
	}

	/* Ignore unknown keyword */
	return (0);

#undef WRITE
}

#define SET_MAX_UID	8000
#define SET_MAX_GID	SET_MAX_UID
#define SET_MAX_NLINK	100
#define SET_LIMIT	50

/*
 * Write /set and update the default values in the writer
 */
static void
set_keyword_defaults(mtree_writer *w, mtree_entry *entry)
{
	mtree_entry	*parent;
	mtree_entry_type t[100];
	uintmax_t	 u[SET_MAX_UID];
	uintmax_t	 g[SET_MAX_GID];
	uintmax_t	 m[MTREE_ENTRY_MODE_MASK + 1];
	uintmax_t	 n[SET_MAX_NLINK];
	uintmax_t	 u_set = 0, g_set = 0, m_set = 0,
			 n_set = 0, t_set = 0;
	uint8_t		 u_max = 0, g_max = 0, m_max = 0,
			 t_max = 0, n_max = 0;
	long		 keywords = 0;
	int		 count = 0;

#define UPDATE(keyword, p, field)				\
	do {							\
		if (++p[entry->data.field] > p##_max) {		\
			p##_max = p[entry->data.field];		\
			p##_set = entry->data.field;		\
		}						\
	} while (0)

	if (entry->next == NULL ||
	    entry->next->data.type == MTREE_ENTRY_DIR)
		return;

	memset(u, 0, sizeof(u));
	memset(g, 0, sizeof(g));
	memset(m, 0, sizeof(m));
	memset(t, 0, sizeof(t));
	memset(n, 0, sizeof(n));
	entry  = entry->next;
	parent = entry->parent;
	while (entry != NULL) {
		if (entry->parent != parent)
			break;
		/*
		 * Remember number of occurrences of values for the
		 * selected keywords
		 */
		if (entry->data.keywords & MTREE_KEYWORD_UID &&
		    entry->data.st_uid < SET_MAX_UID)
			UPDATE(MTREE_KEYWORD_UID, u, st_uid);
		if (entry->data.keywords & MTREE_KEYWORD_GID &&
		    entry->data.st_gid < SET_MAX_GID)
			UPDATE(MTREE_KEYWORD_GID, g, st_gid);
		if (entry->data.keywords & MTREE_KEYWORD_NLINK &&
		    entry->data.st_nlink < SET_MAX_NLINK)
			UPDATE(MTREE_KEYWORD_NLINK, n, st_nlink);

		if (entry->data.keywords & MTREE_KEYWORD_MODE)
			UPDATE(MTREE_KEYWORD_MODE, m, st_mode);
		if (entry->data.keywords & MTREE_KEYWORD_TYPE)
			UPDATE(MTREE_KEYWORD_TYPE, t, type);
		count++;
		entry = entry->next;
	}
	/* Writing /set for a single entry doesn't look very smart */
	if (count < 2)
		return;

#define SET(keyword, p, field)						\
	do {								\
		if (p[p##_set] > 0 &&					\
		    ((w->defaults.keywords & keyword) == 0 ||		\
		     (p##_set != w->defaults.field &&			\
		      p[p##_set] > p[w->defaults.field]))) {		\
			keywords |= keyword;				\
			w->defaults.field = p##_set;			\
		}							\
	} while (0)

	/*
	 * Store the newly acquired defaults in writer
	 */
	SET(MTREE_KEYWORD_UID, u, st_uid);
	SET(MTREE_KEYWORD_GID, g, st_gid);
	SET(MTREE_KEYWORD_MODE, m, st_mode);
	SET(MTREE_KEYWORD_NLINK, n, st_nlink);
	SET(MTREE_KEYWORD_TYPE, t, type);

	if (keywords != 0) {
		int offset = 0;
		int i;

		write_part(w, &offset, "/set");

		/* Write the keywords */
		for (i = 0; mtree_keywords[i].keyword != -1; i++)
			if (keywords & mtree_keywords[i].keyword)
				write_keyword(w, &w->defaults, &offset,
					mtree_keywords[i].keyword,
					WRITE_KW_PREFIX);
		write_part(w, NULL, "\n");

		w->defaults.keywords |= keywords;
	}

#undef SET
#undef UPDATE
}

static int
write_entries_diff(mtree_writer *w, mtree_entry *entries)
{
	mtree_entry *entry;
	char *path;
	int i;

	entry = entries;
	while (entry != NULL) {
		/*
		 * The diff formats provide different indentation level
		 */
		switch (w->format) {
		case MTREE_FORMAT_DIFF_SECOND:
			write_part(w, NULL, "\t");
			break;
		case MTREE_FORMAT_DIFF_DIFFER:
			write_part(w, NULL, "\t\t");
			break;
		case MTREE_FORMAT_DIFF_FIRST:
		default:
			break;
		}

		path = mtree_get_vispath(entry->path);
		/*
		 * The diff format includes path followed by type without
		 * the keyword prefix
		 */
		write_part(w, NULL, "%s", path);
		if (entry->data.type != MTREE_ENTRY_UNKNOWN)
			write_part(w, NULL, " %s",
			    mtree_type_string(entry->data.type));
		free(path);

		/*
		 * Write keywords
		 */
		for (i = 0; mtree_keywords[i].keyword != -1; i++) {
			/*
			 * Type is included separately, skip it here
			 */
			if (mtree_keywords[i].keyword == MTREE_KEYWORD_TYPE)
				continue;
			if ((entry->data.keywords & mtree_keywords[i].keyword) == 0)
				continue;
			write_keyword(w, &entry->data, NULL,
			    mtree_keywords[i].keyword,
			    WRITE_KW_PREFIX);
		}
		write_part(w, NULL, "\n");

		entry = entry->next;
	}
	return (0);
}

/*
 * Write the spec entries
 */
static int
write_entries(mtree_writer *w, mtree_entry *entries)
{
	mtree_entry *entry;
	mtree_entry *dir;
	char *path;
	long last_unset = 0;
	long unset;
	int kw_options;
	int offset;
	int i;

	if (w->format == MTREE_FORMAT_DIFF_FIRST ||
	    w->format == MTREE_FORMAT_DIFF_SECOND ||
	    w->format == MTREE_FORMAT_DIFF_DIFFER)
		return (write_entries_diff(w, entries));

	if (w->format == MTREE_FORMAT_2_0_PATH_LAST)
		kw_options = WRITE_KW_POSTFIX;
	else
		kw_options = WRITE_KW_PREFIX;

	w->indent = 0;

	dir   = NULL;
	entry = entries;
	while (entry != NULL) {
		offset = 0;

		if (dir != NULL && entry->parent != NULL) {
			mtree_entry *parent;
			/*
			 * Go up to the level of current entry's parent
			 */
			parent = dir;
			while (parent && parent != entry->parent) {
				if (w->options & MTREE_WRITE_DIR_COMMENTS)
					write_part(w, NULL, "%*s# %s\n",
					    w->indent, "", parent->path);
				if (w->format == MTREE_FORMAT_1_0) {
					write_part(w, NULL, "%*s..\n", w->indent, "");
					if (w->options & MTREE_WRITE_DIR_BLANK_LINES &&
					    entry->parent != NULL)
						write_part(w, NULL, "\n");
				}
				if (w->options & MTREE_WRITE_INDENT_LEVEL)
					w->indent -= 4;

				parent = parent->parent;
			}
		}

		if (entry->data.type == MTREE_ENTRY_DIR) {
			if (entry->parent != NULL) {
				if (w->options & MTREE_WRITE_INDENT_LEVEL)
					w->indent += 4;
				if (w->options & MTREE_WRITE_DIR_BLANK_LINES)
					write_part(w, NULL, "\n");
			}
			if (w->options & MTREE_WRITE_DIR_COMMENTS)
				write_part(w, NULL, "# %s\n", entry->path);

			dir = entry;
		}

		if (w->options & MTREE_WRITE_USE_SET) {
			/*
			 * If any keywords are currently set and not present
			 * in the current entry, they must be /unset
			 */
			unset = w->defaults.keywords & ~entry->data.keywords;
			if (unset != 0) {
				write_part(w, NULL, "/unset");
				for (i = 0; mtree_keywords[i].keyword != -1; i++)
					if (unset & mtree_keywords[i].keyword)
						write_part(w, NULL, " %s",
						    mtree_keywords[i].name);
				write_part(w, NULL, "\n");
				w->defaults.keywords &= entry->data.keywords;
			}

			/*
			 * Look for the most frequent keyword value if we have
			 * just entered a directory or if the previous entry
			 * caused /unset to be written
			 */
			if (last_unset != 0 ||
			    entry->data.type == MTREE_ENTRY_DIR)
				set_keyword_defaults (w, entry);

			last_unset = unset;
		}

		/*
		 * Indent current level
		 */
		if (w->options & MTREE_WRITE_INDENT) {
			if (entry->data.type == MTREE_ENTRY_DIR)
				offset = write_part(w, NULL, "%*s", w->indent, "");
			else
				offset = write_part(w, NULL, "%*s", w->indent + 4, "");
		}

		if (w->format == MTREE_FORMAT_1_0)
			path = mtree_get_vispath(entry->name);
		else
			path = mtree_get_vispath(entry->path);

		if (w->format != MTREE_FORMAT_2_0_PATH_LAST)
			offset += write_part(w, NULL, "%s", path);

		if (w->options & MTREE_WRITE_INDENT) {
			if (offset > (INDENTNAMELEN + w->indent))
				offset = INDENTLINELEN;
			else
				offset += write_part(w, NULL, "%*s",
				    (INDENTNAMELEN + w->indent) - offset, "");
		}

		/*
		 * Write keywords
		 */
		for (i = 0; mtree_keywords[i].keyword != -1; i++) {
			if ((entry->data.keywords & mtree_keywords[i].keyword) == 0)
				continue;
			if (mtree_entry_data_compare_keyword(&w->defaults,
				&entry->data, mtree_keywords[i].keyword) == 0)
				continue;
			write_keyword(w, &entry->data, &offset,
			    mtree_keywords[i].keyword,
			    kw_options);
		}

		if (w->format == MTREE_FORMAT_2_0_PATH_LAST)
			write_part(w, NULL, "%s", path);

		free(path);
		write_part(w, NULL, "\n");

		entry = entry->next;
	}
	return (0);
}

int
mtree_writer_write_file(mtree_writer *w, mtree_entry *entries, FILE *fp)
{

	assert(w != NULL);
	assert(fp != NULL);

	w->dst.fp = fp;
	w->writer = write_file;
	return (write_entries(w, entries));
}

int
mtree_writer_write_fd(mtree_writer *w, mtree_entry *entries, int fd)
{

	assert(w != NULL);
	assert(fd != -1);

	w->dst.fd = fd;
	w->writer = write_fd;
	return (write_entries(w, entries));
}

void
mtree_writer_set_format(mtree_writer *w, mtree_format format)
{

	assert(w != NULL);

	w->format = format;
}

void
mtree_writer_set_options(mtree_writer *w, int options)
{

	assert(w != NULL);

	w->options = options;
}
