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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include "mtree.h"
#include "mtree_private.h"

#define	INDENTNAMELEN	15
#define	INDENTLINELEN	80

/*
 * This is the order of keywords as they are written to specfiles.
 * It mostly resembles the v2 order in the original mtree.
 */
static const long write_keywords[] = {
	MTREE_KEYWORD_TYPE,
	MTREE_KEYWORD_UNAME,
	MTREE_KEYWORD_UID,
	MTREE_KEYWORD_GNAME,
	MTREE_KEYWORD_GID,
	MTREE_KEYWORD_MODE,
	MTREE_KEYWORD_INODE,
	MTREE_KEYWORD_DEVICE,
	MTREE_KEYWORD_NLINK,
	MTREE_KEYWORD_LINK,
	MTREE_KEYWORD_SIZE,
	MTREE_KEYWORD_TIME,
	MTREE_KEYWORD_CKSUM,
	MTREE_KEYWORD_MD5,
	MTREE_KEYWORD_MD5DIGEST,
	MTREE_KEYWORD_RIPEMD160DIGEST,
	MTREE_KEYWORD_RMD160,
	MTREE_KEYWORD_RMD160DIGEST,
	MTREE_KEYWORD_SHA1,
	MTREE_KEYWORD_SHA1DIGEST,
	MTREE_KEYWORD_SHA256,
	MTREE_KEYWORD_SHA256DIGEST,
	MTREE_KEYWORD_SHA384,
	MTREE_KEYWORD_SHA384DIGEST,
	MTREE_KEYWORD_SHA512,
	MTREE_KEYWORD_SHA512DIGEST,
	MTREE_KEYWORD_FLAGS,
	MTREE_KEYWORD_CONTENTS,
	MTREE_KEYWORD_IGNORE,
	MTREE_KEYWORD_OPTIONAL,
	MTREE_KEYWORD_NOCHANGE,
	MTREE_KEYWORD_TAGS
};

/*
 * Create a new mtree_writer.
 */
struct mtree_writer *
mtree_writer_create(void)
{

	return calloc(1, sizeof(struct mtree_writer));
}

/*
 * Free the given mtree_writer.
 */
void
mtree_writer_free(struct mtree_writer *w)
{

	free(w);
}

/*
 * Write the given string to an open file descriptor.
 */
static int
write_fd(struct mtree_writer *w, const char *s)
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

/*
 * Write the given string to a file stream.
 */
static int
write_file(struct mtree_writer *w, const char *s)
{

	if (fputs(s, w->dst.fp) == EOF)
		return (-1);

	return (0);
}

/*
 * Write the given string using a user-defined writing function.
 */
static int
write_fn(struct mtree_writer *w, const char *s)
{
	int ret;

	/*
	 * The user-defined function is supposed to return 0 on success and
	 * non-zero on error.
	 *
	 * Positive value should be a valid errno and a negative one will be
	 * converted to ECANCELED, as we need to report a valid errno to the
	 * caller.
	 */
	ret = w->dst.fn.fn(s, strlen(s), w->dst.fn.data);
	if (ret != 0) {
		if (ret > 0)
			errno = ret;
		else
			errno = ECANCELED;
		ret = -1;
	}
	return (ret);
}

/*
 * Write a part of mtree spec output using the configured writer.
 */
static int
write_part(struct mtree_writer *w, int *offset, const char *format, ...)
{
	char	buf[MAX_LINE_LENGTH];
	va_list args;
	int	len;
	int	ret;

	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	if (len < 0)
		return (len);
	/*
	 * Write indentation/line continuation.
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
			 * Indent start of the new line.
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
 * Write a single keyword value, optionally with a space before or after it.
 */
static int
write_keyword(struct mtree_writer *w, struct mtree_entry_data *data, int *offset,
    long keyword, int options)
{
	char	*s;
	int	 ret;

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
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("cksum=%lu", data->cksum);
	case MTREE_KEYWORD_CONTENTS:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		s = mtree_vispath(data->contents,
		    w->options & MTREE_WRITE_ENCODE_CSTYLE);
		if (s != NULL) {
			ret = WRITE("contents=%s", s);
			free(s);
		} else
			ret = -1;
		return (ret);
	case MTREE_KEYWORD_DEVICE:
		/* Types: block, char */
		if (data->type != MTREE_ENTRY_BLOCK &&
		    data->type != MTREE_ENTRY_CHAR)
			return (0);
		s = mtree_device_string(data->device);
		if (s != NULL) {
			ret = WRITE("device=%s", s);
			free(s);
		} else
			ret = -1;
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
		/* Types: link */
		if (data->type != MTREE_ENTRY_LINK)
			return (0);
		s = mtree_vispath(data->link,
		    w->options & MTREE_WRITE_ENCODE_CSTYLE);
		if (s != NULL) {
			ret = WRITE("link=%s", s);
			free(s);
		} else
			ret = -1;
		return (ret);
	case MTREE_KEYWORD_MD5:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("md5=%s", data->md5digest);
	case MTREE_KEYWORD_MD5DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("md5digest=%s", data->md5digest);
	case MTREE_KEYWORD_MODE:
		return WRITE("mode=%#o", data->st_mode);
	case MTREE_KEYWORD_NLINK:
		return WRITE("nlink=%ju", data->st_nlink);
	case MTREE_KEYWORD_NOCHANGE:
		return WRITE("nochange", NULL);
	case MTREE_KEYWORD_OPTIONAL:
		return WRITE("optional", NULL);
	case MTREE_KEYWORD_RIPEMD160DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("ripemd160digest=%s", data->rmd160digest);
	case MTREE_KEYWORD_RMD160:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("rmd160=%s", data->rmd160digest);
	case MTREE_KEYWORD_RMD160DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("rmd160digest=%s", data->rmd160digest);
	case MTREE_KEYWORD_SHA1:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha1=%s", data->sha1digest);
	case MTREE_KEYWORD_SHA1DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha1digest=%s", data->sha1digest);
	case MTREE_KEYWORD_SHA256:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha256=%s", data->sha256digest);
	case MTREE_KEYWORD_SHA256DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha256digest=%s", data->sha256digest);
	case MTREE_KEYWORD_SHA384:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha384=%s", data->sha384digest);
	case MTREE_KEYWORD_SHA384DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha384digest=%s", data->sha384digest);
	case MTREE_KEYWORD_SHA512:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha512=%s", data->sha512digest);
	case MTREE_KEYWORD_SHA512DIGEST:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("sha512digest=%s", data->sha512digest);
	case MTREE_KEYWORD_SIZE:
		/* Types: file */
		if (data->type != MTREE_ENTRY_FILE)
			return (0);
		return WRITE("size=%ju", data->st_size);
	case MTREE_KEYWORD_TAGS:
		return WRITE("tags=%s", data->tags);
	case MTREE_KEYWORD_TIME:
		return WRITE("time=%jd.%09jd",
		    data->st_mtim.tv_sec,
		    data->st_mtim.tv_nsec);
	case MTREE_KEYWORD_TYPE:
		return WRITE("type=%s", mtree_entry_type_string(data->type));
	case MTREE_KEYWORD_UID:
		return WRITE("uid=%ju", data->st_uid);
	case MTREE_KEYWORD_UNAME:
		return WRITE("uname=%s", data->uname);
	}

	/* Ignore unknown keyword. */
	return (0);

#undef WRITE
}

#define SET_MAX_UID	8000
#define SET_MAX_GID	SET_MAX_UID
#define SET_MAX_NLINK	100
#define SET_LIMIT	50

/*
 * Write /set and update the default values in the writer.
 */
static int
set_keyword_defaults(struct mtree_writer *w, struct mtree_entry *entry)
{
	struct mtree_entry	*parent;
	mtree_entry_type	 t[100];
	int64_t			 u[SET_MAX_UID];
	int64_t			 g[SET_MAX_GID];
	int64_t 		 m[MODE_MASK + 1];
	int64_t 		 n[SET_MAX_NLINK];
	int64_t			 u_set = 0, g_set = 0, m_set = 0,
				 n_set = 0, t_set = 0;
	uint8_t			 u_max = 0, g_max = 0, m_max = 0,
				 t_max = 0, n_max = 0;
	uint64_t		 keywords = 0;
	int			 count = 0;

#define UPDATE(keyword, p, field)				\
	do {							\
		if (++p[entry->data.field] > p##_max) {		\
			p##_max = p[entry->data.field];		\
			p##_set = entry->data.field;		\
		}						\
	} while (0)

	if (entry->next == NULL ||
	    entry->next->data.type == MTREE_ENTRY_DIR)
		return (0);

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
		return (0);

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
		int	offset;
		size_t	i;

		offset = 0;
		if (write_part(w, &offset, "/set") < 0)
			return (-1);

		/* Write the keywords */
		for (i = 0; i < __arraycount(write_keywords); i++) {
			if (keywords & write_keywords[i])
				if (write_keyword(w, &w->defaults, &offset,
				    write_keywords[i],
				    WRITE_KW_PREFIX) < 0)
					return (-1);
		}
		if (write_part(w, NULL, "\n") < 0)
			return (-1);

		w->defaults.keywords |= keywords;
	}
	return (0);
#undef SET
#undef UPDATE
}

#define WRITE(w, offset, ...)				\
	if (write_part(w, offset, __VA_ARGS__) < 0) {	\
		ret = -1;				\
		break;					\
	}
#define WRITE_RET(w, offset, ...)			\
	if (write_part(w, offset, __VA_ARGS__) < 0) {	\
		return (-1);				\
	}

/*
 * Write spec entries in one of the "diff" formats.
 */
static int
write_entries_diff(struct mtree_writer *w, struct mtree_entry *entries)
{
	struct mtree_entry	*entry;
	char			*path;
	size_t			 i;
	int			 ret = 0;

	entry = entries;
	while (entry != NULL) {
		/*
		 * The diff formats provide different indentation level.
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

		path = mtree_vispath(entry->path,
		    w->options & MTREE_WRITE_ENCODE_CSTYLE);
		if (path == NULL) {
			ret = -1;
			break;
		}
		/*
		 * The diff format includes path followed by type without
		 * the keyword prefix.
		 */
		WRITE(w, NULL, "%s", path);
		free(path);
		if (entry->data.type != MTREE_ENTRY_UNKNOWN)
			WRITE(w, NULL, " %s",
			    mtree_entry_type_string(entry->data.type));
		/*
		 * Write keywords.
		 */
		for (i = 0; i < __arraycount(write_keywords); i++) {
			/*
			 * Type is included separately, skip it here.
			 */
			if (write_keywords[i] == MTREE_KEYWORD_TYPE)
				continue;
			if ((entry->data.keywords & write_keywords[i]) == 0)
				continue;
			if (write_keyword(w, &entry->data, NULL, write_keywords[i],
			    WRITE_KW_PREFIX) < 0)
				return (-1);
		}
		WRITE(w, NULL, "\n");

		entry = entry->next;
	}
	return (ret);
}

static int
common_dir_len(const char *p1, const char *p2)
{
	const char	*p = p1;
	int		 n = 0;

	while (*p1 == *p2 && *p1 != '\0') {
		p1++;
		p2++;
		if (*p1 == '/' || *p1 == '\0')
			n = p1 - p;
	}
	return (n);
}

/*
 * Write spec entries.
 */
static int
write_entries(struct mtree_writer *w, struct mtree_entry *entries)
{
	struct mtree_entry	*entry;
	struct mtree_entry	*dir;
	struct mtree_entry	*parent;
	char			*path;
	uint64_t		 last_unset;
	uint64_t		 unset;
	size_t			 i;
	int			 kw_options;
	int			 has_virtual = 0;
	int			 offset;
	int			 inc;
	int			 ret = 0;

	if (w->format == MTREE_FORMAT_DIFF_FIRST ||
	    w->format == MTREE_FORMAT_DIFF_SECOND ||
	    w->format == MTREE_FORMAT_DIFF_DIFFER)
		return (write_entries_diff(w, entries));

	if (w->format == MTREE_FORMAT_2_0_PATH_LAST)
		kw_options = WRITE_KW_POSTFIX;
	else
		kw_options = WRITE_KW_PREFIX;

	if (w->format == MTREE_FORMAT_1_0) {
		const char	*dirname;
		char		 dirpath[MAXPATHLEN];
		char		 fwdpath[MAXPATHLEN];
		char		*endptr;
		char		*tok;
		size_t		 n;

		dir = NULL;
		entry = entries;
		while (entry != NULL) {
			if (IS_DOT(entry->path))
				goto skip;
			/*
			 * Find the common path of the current directory and
			 * the current entry's dirname and move up the dir
			 * pointer until it matches the common directory.
			 *
			 * A similar thing will be done later when writing
			 * the entries to count the number of `..' to write.
			 */
			dirname = mtree_entry_get_dirname(entry);
			if (dir != NULL) {
				n = common_dir_len(dirname, dir->path);
				if (n < strlen(dir->path)) {
					while (dir != NULL &&
					       dir->path[n] != '\0' &&
					       !IS_DOT(dir->path))
						dir = dir->parent;
				}
			} else
				n = 0;
			/*
			 * Don't continue if we haven't moved from the current
			 * directory.
			 */
			if (dirname[n] == '\0')
				goto skip;

			if (dir != NULL)
				strcpy(dirpath, dir->path);
			else {
				dirpath[0] = '.';
				dirpath[1] = '\0';
			}

			/*
			 * Now we need to go forward in the hierarchy until we
			 * reach the current entry's directory.
			 *
			 * A new entry is created for each path component and
			 * inserted before the current entry. This allows
			 * staircasing the directory structure one element at
			 * a time.
			 */
			strcpy(fwdpath, dirname + n + ((dirname[n] == '/') ? 1 : 0));
			parent = dir;
			for (tok = strtok_r(fwdpath, "/", &endptr); tok != NULL;
			     tok = strtok_r(NULL, "/", &endptr)) {
			     	if (!IS_DOT(tok)) {
					if (*dirpath != '\0')
						strcat(dirpath, "/");
					strcat(dirpath, tok);
				}

				dir = mtree_entry_create_empty();
				if (dir == NULL)
					goto end;
				dir->name = strdup(tok);
				if (dir->name == NULL) {
					mtree_entry_free(dir);
					goto end;
				}
				dir->path = strdup(dirpath);
				if (dir->path == NULL) {
					mtree_entry_free(dir);
					goto end;
				}
				dir->flags  = __MTREE_ENTRY_VIRTUAL;
				dir->parent = parent;
				mtree_entry_set_type(dir, MTREE_ENTRY_DIR);

				parent  = dir;
				entries = mtree_entry_insert_before(entries,
				    entry, dir);
				has_virtual = 1;
			}
skip:
			entry->parent = dir;
			if (entry->data.type == MTREE_ENTRY_DIR)
				dir = entry;

			entry = entry->next;
		}
	}
#undef WRITE
#define WRITE(w, offset, ...)				\
	if (write_part(w, offset, __VA_ARGS__) < 0) {	\
		ret = -1;				\
		goto end;				\
	}
	w->indent = 0;

	dir	= NULL;
	path    = NULL;
	entry	= entries;
	last_unset = 0;
	while (entry != NULL) {
		offset = 0;
		if (dir != NULL && entry->parent != NULL) {
			/*
			 * Go up to the level of current entry's parent.
			 */
			parent = dir;
			while (parent != NULL && parent != entry->parent) {
				if (w->options & MTREE_WRITE_DIR_COMMENTS)
					WRITE(w, NULL, "%*s# %s\n",
					    w->indent, "", parent->path);
				if (w->format == MTREE_FORMAT_1_0) {
					WRITE(w, NULL, "%*s..\n", w->indent, "");
					if (w->options & MTREE_WRITE_DIR_BLANK_LINES &&
					    entry->parent != NULL)
						WRITE(w, NULL, "\n");
				}
				if (w->options & MTREE_WRITE_INDENT_LEVEL)
					w->indent -= 4;

				parent = parent->parent;
			}
			dir = parent;
		}

		if (entry->data.type == MTREE_ENTRY_DIR) {
			if (entry->parent != NULL) {
				if (w->options & MTREE_WRITE_INDENT_LEVEL)
					w->indent += 4;
				if (w->options & MTREE_WRITE_DIR_BLANK_LINES)
					WRITE(w, NULL, "\n");
			}
			if (w->options & MTREE_WRITE_DIR_COMMENTS)
				WRITE(w, NULL, "# %s\n", entry->path);

			dir = entry;
		}

		if (w->options & MTREE_WRITE_USE_SET) {
			/*
			 * If any keywords are currently set and not present
			 * in the current entry, they must be /unset.
			 */
			unset = w->defaults.keywords & ~entry->data.keywords;
			if (unset != 0) {
				WRITE(w, NULL, "/unset");
				for (i = 0; mtree_keywords[i].name != NULL; i++)
					if (unset & mtree_keywords[i].keyword)
						WRITE(w, NULL, " %s",
						    mtree_keywords[i].name);
				WRITE(w, NULL, "\n");
				w->defaults.keywords &= entry->data.keywords;
			}

			/*
			 * Look for the most frequent keyword value if we have
			 * just entered a directory or if the previous entry
			 * caused /unset to be written.
			 */
			if (last_unset != 0 ||
			    entry->data.type == MTREE_ENTRY_DIR)
				set_keyword_defaults (w, entry);

			last_unset = unset;
		}

		/*
		 * Indent current level.
		 */
		if (w->options & MTREE_WRITE_INDENT) {
			if (entry->data.type == MTREE_ENTRY_DIR)
				inc = write_part(w, NULL, "%*s", w->indent, "");
			else
				inc = write_part(w, NULL, "%*s", w->indent + 4, "");
			if (inc < 0) {
				ret = -1;
				goto end;
			}
			offset = inc;
		}
		if (w->format == MTREE_FORMAT_1_0)
			path = mtree_vispath(entry->name,
			    w->options & MTREE_WRITE_ENCODE_CSTYLE);
		else
			path = mtree_vispath(entry->path,
			    w->options & MTREE_WRITE_ENCODE_CSTYLE);
		if (path == NULL) {
			ret = -1;
			break;
		}
		if (w->format != MTREE_FORMAT_2_0_PATH_LAST) {
			inc = write_part(w, NULL, "%s", path);
			if (inc < 0) {
				ret = -1;
				goto end;
			}
			offset += inc;
		}

		if (w->options & MTREE_WRITE_INDENT) {
			if (offset > (INDENTNAMELEN + w->indent))
				offset = INDENTLINELEN;
			else {
				inc = write_part(w, NULL, "%*s",
				    (INDENTNAMELEN + w->indent) - offset, "");
				if (inc < 0) {
					ret = -1;
					goto end;
				}
				offset += inc;
			}
		}

		/*
		 * Write keywords.
		 */
		for (i = 0; i < __arraycount(write_keywords); i++) {
			if ((entry->data.keywords & write_keywords[i]) == 0)
				continue;
			if (mtree_entry_data_compare_keyword(&w->defaults,
				&entry->data, write_keywords[i]) == 0)
				continue;
			if (write_keyword(w, &entry->data, &offset,
			    write_keywords[i],
			    kw_options) == -1) {
				ret = -1;
				goto end;
			}
		}

		if (w->format == MTREE_FORMAT_2_0_PATH_LAST)
			WRITE(w, NULL, "%s", path);

		free(path);
		WRITE(w, NULL, "\n");

		entry = entry->next;
	}
end:
	if (has_virtual) {
		struct mtree_entry *next;

		entry = entries;
		while (entry != NULL) {
			next = entry->next;
			if (entry->flags & __MTREE_ENTRY_VIRTUAL) {
				entries = mtree_entry_unlink(entries, entry);
				mtree_entry_free(entry);
			}
			entry = next;
		}
	}
	return (ret);
}
#undef WRITE
#undef WRITE_RET

mtree_format
mtree_writer_get_format(struct mtree_writer *w)
{

	assert(w != NULL);

	return (w->format);
}

void
mtree_writer_set_format(struct mtree_writer *w, mtree_format format)
{

	assert(w != NULL);

	w->format = format;
}

int
mtree_writer_get_options(struct mtree_writer *w)
{

	assert(w != NULL);

	return (w->options);
}

void
mtree_writer_set_options(struct mtree_writer *w, int options)
{

	assert(w != NULL);

	w->options = options;
}

void
mtree_writer_set_output_file(struct mtree_writer *w, FILE *fp)
{

	assert(w != NULL);

	w->dst.fp = fp;
	w->writer = write_file;
}

void
mtree_writer_set_output_fd(struct mtree_writer *w, int fd)
{

	assert(w != NULL);

	w->dst.fd = fd;
	w->writer = write_fd;
}

void
mtree_writer_set_output_writer(struct mtree_writer *w, mtree_writer_fn f,
    void *user_data)
{

	assert(w != NULL);
	assert(f != NULL);

	w->dst.fn.fn = f;
	w->dst.fn.data = user_data;
	w->writer = write_fn;
}

int
mtree_writer_write_entries(struct mtree_writer *w, struct mtree_entry *entries)
{

	return (write_entries(w, entries));
}
