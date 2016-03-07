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

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compat.h"
#include "mtree.h"
#include "mtree_private.h"


#ifndef TIME_T_MIN
#define TIME_T_MIN	(0 < (time_t)-1 ? (time_t)0 \
			 : ~ (time_t)0 << (sizeof(time_t) * 8 - 1))
#endif
#ifndef TIME_T_MAX
#define TIME_T_MAX	(~ (time_t)0 - TIME_T_MIN)
#endif

#define INVALID_CHAR(c)	((c) > 0x7E || \
			((c) < 0x20 && (c) != '\n' && (c) != '\t' && (c) != '\r'))

#define SKIP_TYPE(o, t)	((o & MTREE_READ_SKIP_BLOCK   && t == MTREE_ENTRY_BLOCK) ||  \
			 (o & MTREE_READ_SKIP_CHAR    && t == MTREE_ENTRY_CHAR) ||   \
			 (o & MTREE_READ_SKIP_DIR     && t == MTREE_ENTRY_DIR) ||    \
			 (o & MTREE_READ_SKIP_FIFO    && t == MTREE_ENTRY_FIFO) ||   \
			 (o & MTREE_READ_SKIP_FILE    && t == MTREE_ENTRY_FILE) ||   \
			 (o & MTREE_READ_SKIP_LINK    && t == MTREE_ENTRY_LINK) ||   \
			 (o & MTREE_READ_SKIP_SOCKET  && t == MTREE_ENTRY_SOCKET) || \
			 (o & MTREE_READ_SKIP_UNKNOWN && t == MTREE_ENTRY_UNKNOWN))

/*
 * Create a new mtree_reader.
 */
struct mtree_reader *
mtree_reader_create(void)
{
	struct mtree_reader *r;

	r = calloc(1, sizeof(struct mtree_reader));
	if (r != NULL)
		r->path_last = -1;
	return (r);
}

/*
 * Free the given mtree_reader.
 */
void
mtree_reader_free(struct mtree_reader *r)
{

	assert(r != NULL);

	mtree_reader_reset(r);
	free(r->buf);
	free(r);
}

/*
 * Reset the reader to its initial state.
 */
void
mtree_reader_reset(struct mtree_reader *r)
{

	assert(r != NULL);

	if (r->skip_trie != NULL) {
		mtree_trie_free(r->skip_trie);
		r->skip_trie = NULL;
	}
	if (r->entries != NULL) {
		mtree_entry_free_all(r->entries);
		r->entries = NULL;
	}
	if (r->loose != NULL) {
		mtree_entry_free_all(r->loose);
		r->loose = NULL;
	}
	r->parent = NULL;
	r->buflen = 0;
	r->path_last = -1;

	mtree_copy_string(&r->error, NULL);
	mtree_entry_free_data_items(&r->defaults);

	memset(&r->defaults, 0, sizeof(r->defaults));
}

/*
 * Read a single mtree keyword and either set or unset it in `data'.
 */
static int
read_keyword(struct mtree_reader *r, char *s, struct mtree_entry_data *data, int set)
{
	char		*value;
	char		 name[MAXPATHLEN];
	const char	*endptr;
	uint64_t	 keyword;
	int64_t		 num;

	value = strchr(s, '=');
	if (value != NULL)
		*value++ = '\0';

	keyword = mtree_keyword_parse(s);
	if (keyword == 0) {
		/*
		 * Unsupported keyword, this is not an error.
		 */
		WARN("Ignoring unknown keyword `%s'", s);
		return (0);
	}
	if (!set || (r->spec_keywords & keyword) == 0) {
		/* Unset the keyword. */
		data->keywords &= ~keyword;
		return (0);
	}

	/*
	 * Read the keyword value.
	 *
	 * Wherever possible, the value is validated and error is indicated
	 * in case the value is invalid or missing.
	 */
	errno = 0;

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		num = mtree_atol(value, &endptr);
		if (num < 0 || num > UINT32_MAX || *endptr != '\0')
			errno = EINVAL;
		else
			data->cksum = (uint32_t)num;
		break;
	case MTREE_KEYWORD_CONTENTS:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		if (strnunvis(name, sizeof(name), value) == -1)
			errno = ENAMETOOLONG;
		else
			mtree_copy_string(&data->contents, value);
		break;
	case MTREE_KEYWORD_DEVICE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		if (data->device == NULL) {
			data->device = mtree_device_create();
			if (data->device == NULL)
				break;
		}
		if (mtree_device_parse(data->device, value) == -1) {
			mtree_reader_set_errno_error(r, errno, "`%s': %s", value,
			    mtree_device_get_error(data->device));

			/* Prevent rewriting the error later. */
			return (-1);
		}
		break;
	case MTREE_KEYWORD_FLAGS:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->flags, value);
		break;
	case MTREE_KEYWORD_GID:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		data->st_gid = mtree_atol(value, &endptr);
		if (*endptr != '\0')
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_GNAME:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->gname, value);
		break;
	case MTREE_KEYWORD_IGNORE:
		/* No value */
		break;
	case MTREE_KEYWORD_INODE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		num = mtree_atol(value, &endptr);
		if (num < 0 || *endptr != '\0')
			errno = EINVAL;
		else
			data->st_ino = (uint64_t)num;
		break;
	case MTREE_KEYWORD_LINK:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		if (strnunvis(name, sizeof(name), value) == -1)
			errno = ENAMETOOLONG;
		else
			mtree_copy_string(&data->link, name);
		break;
	case MTREE_KEYWORD_MD5:
	case MTREE_KEYWORD_MD5DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->md5digest, value);
		break;
	case MTREE_KEYWORD_MODE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		if (value[0] >= '0' && value[0] <= '9') {
			data->st_mode = (int)mtree_atol8(value, &endptr);
			if (*endptr != '\0')
				errno = EINVAL;
		} else
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_NLINK:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		data->st_nlink = mtree_atol(value, &endptr);
		if (*endptr != '\0')
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_NOCHANGE:
		/* No value */
		break;
	case MTREE_KEYWORD_OPTIONAL:
		/* No value */
		break;
	case MTREE_KEYWORD_RESDEVICE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		if (data->resdevice == NULL) {
			data->resdevice = mtree_device_create();
			if (data->resdevice == NULL)
				break;
		}
		if (mtree_device_parse(data->resdevice, value) == -1) {
			mtree_reader_set_errno_error(r, errno, "`%s': %s", value,
			    mtree_device_get_error(data->resdevice));

			/* Prevent rewriting the error later. */
			return (-1);
		}
		break;
	case MTREE_KEYWORD_RIPEMD160DIGEST:
	case MTREE_KEYWORD_RMD160:
	case MTREE_KEYWORD_RMD160DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->rmd160digest, value);
		break;
	case MTREE_KEYWORD_SHA1:
	case MTREE_KEYWORD_SHA1DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->sha1digest, value);
		break;
	case MTREE_KEYWORD_SHA256:
	case MTREE_KEYWORD_SHA256DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->sha256digest, value);
		break;
	case MTREE_KEYWORD_SHA384:
	case MTREE_KEYWORD_SHA384DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->sha384digest, value);
		break;
	case MTREE_KEYWORD_SHA512:
	case MTREE_KEYWORD_SHA512DIGEST:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->sha512digest, value);
		break;
	case MTREE_KEYWORD_SIZE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		/*
		 * It doesn't make a lot of sense to allow negative values
		 * here, but this value is related to off_t, which is signed.
		 */
		data->st_size = mtree_atol(value, &endptr);
		if (*endptr != '\0')
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_TAGS:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->tags, value);
		break;
	case MTREE_KEYWORD_TIME:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		num = mtree_atol10(value, &endptr);
		if (num < TIME_T_MIN || num > TIME_T_MAX) {
			errno = EINVAL;
			break;
		}
		if (*endptr != '\0' && *endptr != '.') {
			errno = EINVAL;
			break;
		}
		data->st_mtim.tv_sec = (time_t)num;
		if (*endptr == '.') {
			num = mtree_atol10(endptr + 1, &endptr);
			if (*endptr != '\0') {
				errno = EINVAL;
				break;
			}
			data->st_mtim.tv_nsec = (long)num;
		} else
			data->st_mtim.tv_nsec = 0;
		break;
	case MTREE_KEYWORD_TYPE:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		data->type = mtree_entry_type_parse(value);
		if (data->type == MTREE_ENTRY_UNKNOWN)
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_UID:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		data->st_uid = mtree_atol(value, &endptr);
		if (*endptr != '\0')
			errno = EINVAL;
		break;
	case MTREE_KEYWORD_UNAME:
		if (value == NULL) {
			errno = ENOENT;
			break;
		}
		mtree_copy_string(&data->uname, value);
		break;
	}

	if (errno != 0) {
		if (errno == ENOENT)
			mtree_reader_set_errno_error(r, errno,
			    "`%s': missing keyword value", s);
		else if (errno == EINVAL)
			mtree_reader_set_errno_error(r, errno,
			    "`%s': invalid keyword value: `%s'", s, value);
		else if (errno == ENAMETOOLONG)
			mtree_reader_set_errno_error(r, errno,
			    "`%s': file name too long: `%s'", s, value);
		else {
			/* These should be just memory errors. */
			mtree_reader_set_errno_error(r, errno, NULL);
		}
		return (-1);
	}

	data->keywords |= keyword;
	return (0);
}

/*
 * Skip initial spaces and tabs and put pointer to the start of the word
 * into *word. If there are no more words, set *word to NULL.
 *
 * Additionally, skip spaces and tabs after the current word and make *next
 * point to the start of the next word, or set it to NULL if there isn't one.
 */
static void
read_word(char *s, char **word, char **next)
{
	char *w, *n;

	assert(word != NULL);
	assert(next != NULL);

	w = s + strspn(s, " \t");
	if (*w != '\0') {
		n = w + strcspn(w, " \t");
		if (*n != '\0') {
			*n++ = '\0';
			n += strspn(n, " \t");
			if (*n == '\0')
				n = NULL;
		} else
			n = NULL;
	} else {
		w = NULL;
		n = NULL;
	}
	*word = w;
	*next = n;
}

/*
 * Read keywords until the end of the given string into `data'.
 */
static int
read_keywords(struct mtree_reader *r, char *s, struct mtree_entry_data *data,
    int set)
{
	char	*word, *next;
	int	 ret = 0;

	for (;;) {
		read_word(s, &word, &next);
		if (word == NULL)
			break;
		/* Sets reader error. */
		ret = read_keyword(r, word, data, set);
		if (ret == -1)
			break;
		if (next == NULL)
			break;
		s = next;
	}
	return (ret);
}

/*
 * Process spec commands.
 */
static int
read_command(struct mtree_reader *r, char *s)
{
	char	*cmd, *next;
	int	 ret;

	read_word(s, &cmd, &next);
	assert(cmd != NULL);

	if (!strcmp(cmd, "/set"))
		/* Sets reader error. */
		ret = read_keywords(r, next, &r->defaults, 1);
	else if (!strcmp(cmd, "/unset"))
		/* Sets reader error. */
		ret = read_keywords(r, next, &r->defaults, 0);
	else {
		WARN("Ignoring unknown command `%s'", cmd);
		ret = 0;
	}
	return (ret);
}

/*
 * Create path from the name and parent of an entry.
 */
static char *
create_v1_path(struct mtree_entry *entry)
{
	char	*path;
	char	*prefix = "";
	size_t	 len;

	len = strlen(entry->name);
	if (entry->parent == NULL) {
		if (!IS_DOT(entry->name))
			prefix = "./";
	} else
		len += strlen(entry->parent->path) + 1;

	len += strlen(prefix);
	path = malloc(len + 1);
	if (path != NULL)
		snprintf(path, len + 1, "%s%s%s%s",
		    prefix,
		    entry->parent ? entry->parent->path : "",
		    entry->parent ? "/" : "",
		    entry->name);
	return (path);
}

/*
 * Find out whether path is at the start or at the end of the line.
 */
static int
detect_format(struct mtree_reader *r, char *s)
{
	char *word, *next;
	char *p, *cp;

	/*
	 * Try to detect the format if it isn't known yet.
	 *
	 * Some basic assumptions are:
	 *  - keyword names never contain slashes
	 *  - 2.0 with path at the end must contain a slash in the path
	 *  - it is not allowed to mix 1.0/2.0 with 2.0 with path at the end
	 */
	cp = strdup(s);
	if (cp == NULL) {
		mtree_reader_set_errno_error(r, errno, NULL);
		return (-1);
	}
	read_word(cp, &word, &next);
	if (word == NULL)
		return (0);
	/*
	 * Only look for a slash before '=' as it could be a keyword with a path
	 * in the argument.
	 */
	p = strchr(word, '=');
	if (p != NULL)
		*p = '\0';
	if (strchr(word, '/') != NULL) {
		/* Surely a path at the start. */
		r->path_last = 0;
		return (0);
	}
	if (next == NULL)
		return (0);

	/* Find the last word. */
	for (;;) {
		read_word(next, &word, &next);
		if (next == NULL) {
			p = strchr(word, '=');
			if (p != NULL)
				*p = '\0';
			/*
			 * If the final word has no slash, assume that it is
			 * not a path.
			 *
			 * In theory, there could be a file with a name like
			 * "pa=th/file", but libmtree never writes entries
			 * like that and this shouldn't be the reason for doing
			 * some complicated look-aheads.
			 */
			if (strchr(word, '/') != NULL)
				r->path_last = 1;
			else
				r->path_last = 0;
			break;
		}
	}
	return (0);
}

static int
read_spec(struct mtree_reader *r, char *s)
{
	struct mtree_entry	*entry;
	char			 name[MAXPATHLEN];
	char			*slash, *file, *word, *next;
	int			 ret;
	int			 skip = 0;

	/* Try to detect the format if it isn't known yet. */
	if (r->path_last == -1 && detect_format(r, s) == -1)
		return (-1);
	if (r->path_last != 1) {
		/*
		 * Path is surely in the first field, either because we know
		 * the format or there is just one single field.
		 */
		read_word(s, &file, &next);
		assert(file != NULL);

		if (IS_DOTDOT(file)) {
			/* Only change the parent, keywords are ignored. */
			if (r->parent == NULL) {
				mtree_reader_set_errno_error(r, EINVAL,
				    "`..' not allowed, no parent directory");
				return (-1);
			}
			r->parent = r->parent->parent;
			return (0);
		}
	}
	entry = mtree_entry_create_empty();
	if (entry == NULL) {
		mtree_reader_set_errno_error(r, errno, NULL);
		return (-1);
	}

	if (r->path_last != 1) {
		if (next != NULL) {
			/* Read keyword that follows the path, sets reader error. */
			ret = read_keywords(r, next, &entry->data, 1);
			if (ret == -1) {
				mtree_entry_free(entry);
				return (-1);
			}
		}
	} else {
		/* Path is at the end, read the keywords first. */
		for (file = NULL; file == NULL;) {
			read_word(s, &word, &next);
			assert(word != NULL);

			if (next != NULL) {
				/* Sets reader error. */
				ret = read_keyword(r, word, &entry->data, 1);
				if (ret == -1) {
					mtree_entry_free(entry);
					return (-1);
				}
				s = next;
			} else
				file = word;
		}
		assert(file != NULL);

		if (IS_DOTDOT(file)) {
			mtree_reader_set_errno_error(r, EINVAL,
			    "`..' not allowed in this format");
			mtree_entry_free(entry);
			return (-1);
		}
	}
	/* Copy /set values to the entry. */
	mtree_entry_data_copy_keywords(&entry->data, &r->defaults,
	    r->defaults.keywords, 0);

	/*
	 * See if we should skip this file.
	 */
	if (SKIP_TYPE(r->options, entry->data.type)) {
		/*
		 * With directories in version 1.0 format, we'll have to worry
		 * about potentially setting the directory as the current
		 * parent, other types can be skipped immediately.
		 */
		if (entry->data.type != MTREE_ENTRY_DIR || r->path_last == 1)
			goto skip;
		else
			skip = 1;
	}

	/* Save the file path and name. */
	if (strnunvis(name, sizeof(name), file) == -1) {
		mtree_reader_set_errno_prefix(r, ENAMETOOLONG, "`%s'", file);
		mtree_entry_free(entry);
		return (-1);
	}
	if ((slash = strchr(name, '/')) != NULL) {
		/*
		 * If the name contains a slash, it is a relative path.
		 * These do not change the working directory.
		 */
		if (skip)
			goto skip;
		ret = mtree_cleanup_path(name, &entry->path, &entry->name);
		if (ret == -1) {
			mtree_reader_set_errno_error(r, errno, NULL);
			mtree_entry_free(entry);
			return (-1);
		}
	} else {
		entry->name = strdup(name);
		if (entry->name == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			mtree_entry_free(entry);
			return (-1);
		}
		entry->parent = r->parent;
		entry->path = create_v1_path(entry);
		if (entry->path == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			mtree_entry_free(entry);
			return (-1);
		}
	}

	/*
	 * We may want to skip this entry if a filter told us to skip
	 * children of a directory, which is a parent of this entry.
	 *
	 * There is also no need to worry about changing the parent if
	 * this happens.
	 */
	if (!skip && r->skip_trie != NULL) {
		char *tok;
		char *endptr;

		for (tok = strtok_r(name, "/", &endptr); tok != NULL;
		     tok = strtok_r(NULL, "/", &endptr)) {
			if (mtree_trie_find(r->skip_trie, tok) != NULL) {
				mtree_entry_free(entry);
				return (0);
			}
		}
	}

	/*
	 * Mark the current entry as the current directory. This only applies
	 * to classic (1.0) entries and must be done after keywords are read.
	 */
	if (slash == NULL && entry->data.type == MTREE_ENTRY_DIR)
		r->parent = entry;
	if (skip)
		goto skip;

	if (r->filter != NULL) {
		int result;

		/* Apply filter. */
		result = r->filter(entry, r->filter_data);
		if (result & MTREE_ENTRY_SKIP_CHILDREN) {
			if (entry->data.type == MTREE_ENTRY_DIR) {
				struct mtree_entry *found, *start;

				if (r->skip_trie == NULL) {
					r->skip_trie = mtree_trie_create(NULL);
					if (r->skip_trie == NULL) {
						mtree_reader_set_errno_error(r,
						    errno, NULL);
						return (-1);
					}
				}
				if (mtree_trie_insert(r->skip_trie, entry->path,
				    TRIE_ITEM) == -1) {
					mtree_reader_set_errno_error(r, errno,
					    NULL);
					return (-1);
				}

				strcpy(name, entry->path);
				strcat(name, "/");
				/*
				 * Remove children that are already in the
				 * list.
				 */
				start = r->entries;
				for (;;) {
					found = mtree_entry_find_prefix(start,
					    name);
					if (found == NULL)
						break;

					start = found->next;
					r->entries = mtree_entry_unlink(
					    r->entries, found);
					mtree_entry_free(found);
				}
			}
		}
		if (result & MTREE_ENTRY_SKIP)
			goto skip;
	}

	r->entries = mtree_entry_prepend(r->entries, entry);
	return (0);
skip:
	/*
	 * Skipping directory which is the current parent would make its files
	 * end up in a wrong directory.
	 *
	 * Keep such entries in a list of `loose' entries instead, these will
	 * be deleted when all the work is done.
	 */
	if (entry == r->parent)
		r->loose = mtree_entry_prepend(r->loose, entry);
	else
		mtree_entry_free(entry);
	return (0);
}

static int
parse_line(struct mtree_reader *r, char *s)
{
	char	*p, *ep;
	int	 ret, esc;

	/* First get rid of comment part and blanks at the end of the line. */
	for (p = s; (p = strchr(p, '#')) != NULL; p++) {
		esc = 0;
		/* Make sure the comment char is not escaped. */
		if (p != s) {
			ep = p - 1;
			do {
				if (*ep != '\\')
					break;
				esc++;
			} while (ep-- != s);
		}
		if ((esc % 2) == 0) {
			*p = '\0';
			break;
		}
	}

	switch (*s) {
	case '\0':              /* empty line */
		ret = 0;
		break;
	case '/':               /* special commands */
		ret = read_command(r, s);
		break;
	default:                /* specification */
		ret = read_spec(r, s);
		break;
	}
	return (ret);
}

static int
finish_entries(struct mtree_reader *r, struct mtree_entry **entries)
{

	if (r->entries == NULL)
		return (0);

	r->entries = mtree_entry_reverse(r->entries);
	if (r->options & MTREE_READ_MERGE) {
		struct mtree_entry	*merged;
		struct mtree_entry	*mismerged = NULL;
		int			 options = 0;

		if (r->options & MTREE_READ_MERGE_DIFFERENT_TYPES)
			options = MTREE_ENTRY_MERGE_DIFFERENT_TYPES;
		merged = mtree_entry_merge(*entries, r->entries, options,
			&mismerged);
		if (merged != NULL)
			r->entries = merged;
		else {
			if (errno == EEXIST) {
				if (mismerged != NULL)
					mtree_reader_set_errno_error(r, errno,
					    "Merge failed: %s is specified with "
					    "multiple different types (%s and %s)",
					    mismerged->path,
					    mtree_entry_type_string(mismerged->data.type),
					    mtree_entry_type_string(mismerged->next->data.type));
				else
					mtree_reader_set_errno_error(r, errno,
					    "Merge failed: spec contains duplicate "
					    "entries with different types");
			}
			return (-1);
		}
	} else
		r->entries = mtree_entry_append(*entries, r->entries);

	if (r->options & MTREE_READ_SORT)
		r->entries = mtree_entry_sort_path(r->entries);

	*entries = r->entries;
	r->entries = NULL;
	return (0);
}

static int
read_path_file(struct mtree_reader *r, struct mtree_entry *entry, int *skip,
    int *skip_children)
{
	struct stat	st, *stp;
	int		ret;

	*skip = 0;
	*skip_children = 0;
	if (entry->data.type == MTREE_ENTRY_UNKNOWN ||
	    (entry->data.type == MTREE_ENTRY_DIR &&
	    (r->options & MTREE_READ_PATH_DONT_CROSS_MOUNT) != 0)) {
		/*
		 * We need to stat() to determine the type, even when the user
		 * hasn't requested any stat keywords.
		 */
		stp = &st;
		if (r->options & MTREE_READ_PATH_FOLLOW_SYMLINKS) {
			if (entry->orig != NULL)
				ret = stat(entry->orig, stp);
			else
				ret = stat(entry->path, stp);
		} else {
			if (entry->orig != NULL)
				ret = lstat(entry->orig, stp);
			else
				ret = lstat(entry->path, stp);
		}
		if (ret == -1) {
			if ((r->options & MTREE_READ_PATH_SKIP_ON_ERROR) == 0) {
				mtree_reader_set_errno_prefix(r, errno, "`%s'",
				    entry->orig);
				return (-1);
			}
			*skip = 1;
			*skip_children = 1;
			return (0);
		}
		entry->data.type = mtree_entry_type_from_mode(stp->st_mode & S_IFMT);
	} else
		stp = NULL;

	/*
	 * Watch out for crossing mount point.
	 */
	if (entry->data.type == MTREE_ENTRY_DIR &&
	    (r->options & MTREE_READ_PATH_DONT_CROSS_MOUNT) != 0 &&
	    r->base_dev != stp->st_dev) {
		*skip = 1;
		*skip_children = 1;
		return (0);
	}
	if (SKIP_TYPE(r->options, entry->data.type)) {
		*skip = 1;
		return (0);
	}
	if (r->path_keywords & MTREE_KEYWORD_TYPE) {
		/* Already have the type, no need to convert again. */
		entry->data.keywords |= MTREE_KEYWORD_TYPE;
	}
	if (stp != NULL) {
		/* Set stat keywords. */
		mtree_entry_set_keywords_stat(entry, stp, r->path_keywords, 0);
		/*
		 * Set remaining keywords. Force skipping stat here
		 * in case some stat field failed to be set. Entry
		 * would pointlessly call stat() again in such case.
		 */
		mtree_entry_set_keywords(entry,
		    r->path_keywords & ~MTREE_KEYWORD_MASK_STAT, 0);
	} else
		mtree_entry_set_keywords(entry, r->path_keywords, 0);

	if (r->filter != NULL) {
		int result;

		result = r->filter(entry, r->filter_data);
		if ((result & MTREE_ENTRY_SKIP_CHILDREN) != 0)
			*skip_children = 1;
		if ((result & MTREE_ENTRY_SKIP) != 0)
			*skip = 1;
	}
	return (0);
}

/*
 * Read directory structure and store entries in `entries', which must initially
 * point to an empty list.
 */
static int
read_path(struct mtree_reader *r, const char *path, struct mtree_entry **entries,
    struct mtree_entry *parent)
{
	DIR			*dirp;
	struct dirent		*dp;
	struct dirent		*result;
	struct mtree_entry	*entry;
	struct mtree_entry	*dirs;
	struct mtree_entry	*dot;
	int			 err;
	int			 len;
	int			 ret;
	int			 skip;
	int			 skip_children;

	if (parent == NULL) {
		struct stat st;

		if ((r->options & MTREE_READ_PATH_FOLLOW_SYMLINKS) != 0)
			ret = stat(path, &st);
		else
			ret = lstat(path, &st);
		if (ret == -1) {
			mtree_reader_set_errno_prefix(r, errno, "`%s'", path);
			return (-1);
		}

		entry = mtree_entry_create(path);
		if (entry == NULL)
			return (-1);

		entry->data.type = mtree_entry_type_from_mode(st.st_mode & S_IFMT);

		/*
		 * If the path doesn't point to a directory, simply store
		 * the single entry.
		 */
		if (entry->data.type != MTREE_ENTRY_DIR) {
			ret = read_path_file(r, entry, &skip, &skip_children);
			if (ret == -1 || skip) {
				if (ret == -1)
					mtree_reader_set_errno_prefix(r, errno,
					    "`%s'", path);
				mtree_entry_free(entry);
				return (ret);
			}
			*entries = entry;
			return (0);
		}
		mtree_entry_free(entry);

		r->base_dev = st.st_dev;
	}

	dirs = dot = NULL;
	dp = NULL;

	/*
	 * Read the directory structure.
	 */
	ret = 0;
	if ((dirp = opendir(path)) == NULL) {
		if ((r->options & MTREE_READ_PATH_SKIP_ON_ERROR) == 0) {
			mtree_reader_set_errno_prefix(r, errno, "`%s'", path);
			ret = -1;
			goto end;
		}
		return (0);
	}
#if defined(HAVE_FPATHCONF) && defined(HAVE_DIRFD) && defined(_PC_NAME_MAX)
	len = fpathconf(dirfd(dirp), _PC_NAME_MAX);
#else
	len = -1;
#endif
	if (len == -1) {
#if defined(NAME_MAX)
		len = (NAME_MAX > 255) ? NAME_MAX : 255;
#else
		len = 255;
#endif
	}
	/* POSIX.1 requires that d_name is the last field in a struct dirent. */
	dp = malloc(offsetof(struct dirent, d_name) + len + 1);
	if (dp == NULL) {
		mtree_reader_set_errno_error(r, errno, NULL);
		ret = -1;
		goto end;
	}

	while ((err = readdir_r(dirp, dp, &result)) == 0 && result != NULL) {
		if (IS_DOTDOT(dp->d_name))
			continue;
		/*
		 * Dot is read only in the initial directory.
		 */
		if (parent != NULL && IS_DOT(dp->d_name))
			continue;

		entry = mtree_entry_create_empty();
		if (entry == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			ret = -1;
			break;
		}
		entry->name = strdup(dp->d_name);
		if (entry->name == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			ret = -1;
			mtree_entry_free(entry);
			break;
		}
		if (IS_DOT(dp->d_name))
			entry->orig = strdup(path);
		else
			entry->orig = mtree_concat_path(path, dp->d_name);
		if (entry->orig == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			ret = -1;
			mtree_entry_free(entry);
			break;
		}
		entry->parent = parent;
		entry->path = create_v1_path(entry);
		if (entry->path == NULL) {
			mtree_reader_set_errno_error(r, errno, NULL);
			ret = -1;
			mtree_entry_free(entry);
			break;
		}

#ifdef HAVE_DIRENT_D_TYPE
		switch (dp->d_type) {
		case DT_BLK:
			entry->data.type = MTREE_ENTRY_BLOCK;
			break;
		case DT_CHR:
			entry->data.type = MTREE_ENTRY_CHAR;
			break;
		case DT_DIR:
			entry->data.type = MTREE_ENTRY_DIR;
			break;
		case DT_FIFO:
			entry->data.type = MTREE_ENTRY_FIFO;
			break;
		case DT_LNK:
			entry->data.type = MTREE_ENTRY_LINK;
			break;
		case DT_REG:
			entry->data.type = MTREE_ENTRY_FILE;
			break;
		case DT_SOCK:
			entry->data.type = MTREE_ENTRY_SOCKET;
			break;
		default:
			break;
		}
#endif
		ret = read_path_file(r, entry, &skip, &skip_children);
		if (ret == -1)
			break;
		if (IS_DOT(dp->d_name)) {
			if (skip)
				mtree_entry_free(entry);
			else
				dot = entry;
			if (skip_children) {
				/*
				 * This is a bit artificial, but when the user
				 * asks to skip children below ".", remove
				 * all the entries, except for the dot itself
				 * (unless the dot is skipped as well).
				 */
				mtree_entry_free_all(*entries);
				mtree_entry_free_all(dirs);
				*entries = NULL;
				dirs = NULL;
				break;
			}
			continue;
		}
		if (skip && skip_children) {
			mtree_entry_free(entry);
			continue;
		}
		if (entry->data.type == MTREE_ENTRY_DIR) {
			if (skip)
				entry->flags |= __MTREE_ENTRY_SKIP;
			if (skip_children)
				entry->flags |= __MTREE_ENTRY_SKIP_CHILDREN;
			dirs = mtree_entry_prepend(dirs, entry);
		} else if (!skip)
			*entries = mtree_entry_prepend(*entries, entry);
	}
	if (err > 0)
		errno = ret = err;
end:
	if (dp != NULL)
		free(dp);

	if (ret == 0) {
		if (dot != NULL) {
			/* Put the initial dot at the (reversed) start. */
			*entries = mtree_entry_append(*entries, dot);
		}
		closedir(dirp);

		/* Directories are processed after files. */
		entry = dirs;
		while (entry != NULL) {
			struct mtree_entry *next = entry->next;

			if ((entry->flags & __MTREE_ENTRY_SKIP) == 0) {
				dirs = mtree_entry_unlink(dirs, entry);
				*entries = mtree_entry_prepend(*entries, entry);
			}
			if ((entry->flags & __MTREE_ENTRY_SKIP_CHILDREN) == 0)
				ret = read_path(r, entry->path, entries, entry);

			if (ret == -1)
				break;

			entry = next;
		}
	} else {
		/* Fatal error, clean up and make our way back to the caller. */
		mtree_entry_free_all(*entries);
		*entries = NULL;

		err = errno;
		closedir(dirp);
		errno = err;
	}
	mtree_entry_free_all(dirs);

	return (ret);
}

int
mtree_reader_read_path(struct mtree_reader *r, const char *path,
    struct mtree_entry **entries)
{
	int ret;

	r->entries = NULL;

	/* Sets reader error. */
	ret = read_path(r, path, &r->entries, NULL);
	if (ret == -1)
		return (-1);

	ret = finish_entries(r, entries);

	mtree_reader_reset(r);
	return (ret);
}

int
mtree_reader_add(struct mtree_reader *r, const char *s, ssize_t len)
{
	char	buf[MAX_LINE_LENGTH];
	int	ret;
	int	esc;
	int	sidx, bidx, lines;
	int	done;

	assert(r != NULL);
	assert(s != NULL);

	if (len < 0)
		len = strlen(s);
	if (len == 0)
		return (0);

	ret = 0;
	while (len > 0) {
		sidx = 0;
		if (r->buflen > 0)
			esc = r->buf[r->buflen - 1] == '\\';
		else {
			/* Eat blank characters at the start of the line. */
			while (sidx < len && (s[sidx] == ' ' || s[sidx] == '\t'))
				sidx++;
			if (sidx == len)
				break;
			esc = 0;
		}

		bidx = 0;
		done = 0;
		lines = 0;
		while (sidx < len) {
			if (INVALID_CHAR((unsigned char)s[sidx])) {
				// mtree_reader_set_error(r, EINVAL,
				    // "Invalid character in input '%d'",s[sidx]);
				// return (-1);
			}

			switch (s[sidx]) {
			case '\n':
				/* Eat newlines as well as escaped newlines, keep
				 * reading after an escaped one. */
				if (esc) {
					bidx--;
					esc = 0;
				} else
					done = 1;
				lines++;
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
				mtree_reader_set_errno_error(r, ENOBUFS, NULL);
				return (-1);
			}
			sidx++;
			if (done == 1)
				break;
		}
		buf[bidx] = '\0';

		if (done == 0 || r->buflen > 0) {
			int nlen;

			nlen = r->buflen + bidx;
			if (done == 0) {
				if ((nlen + 1) > MAX_LINE_LENGTH) {
					mtree_reader_set_errno_error(r,
					    ENOBUFS, NULL);
					return (-1);
				}
				if (!r->buf) {
					r->buf = malloc(MAX_LINE_LENGTH);
					if (r->buf == NULL) {
						mtree_reader_set_errno_error(r,
						    errno, NULL);
						return (-1);
					}
				}
			}
			memcpy(r->buf + r->buflen, buf, bidx + 1);
			if (done == 1) {
				/* Buffer has a full line. */
				ret = parse_line(r, r->buf);
				r->buflen = 0;
				if (ret == -1)
					break;
			} else
				r->buflen += bidx;
		} else if (done == 1) {
			/* The whole line is in buf. */
			if (bidx) {
				/* Sets reader error. */
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
mtree_reader_add_from_file(struct mtree_reader *r, FILE *fp)
{
	char	 buf[MAX_LINE_LENGTH];
	char	*s;
	int	 ret = 0;

	assert(r != NULL);
	assert(fp != NULL);

	for (;;) {
		errno = 0;
		s = fgets(buf, sizeof(buf), fp);
		if (s == NULL) {
			if (ferror(fp)) {
				mtree_reader_set_errno_error(r, errno, NULL);
				ret = -1;
			}
			break;
		}
		/* Sets reader error. */
		ret = mtree_reader_add(r, buf, -1);
		if (ret == -1)
			break;
	}
	if (ret == -1)
		mtree_reader_reset(r);

	return (ret);
}

/*
 * Read bytes from the given file descriptor and process them with the
 * spec parser.
 */
int
mtree_reader_add_from_fd(struct mtree_reader *r, int fd)
{
	char	 buf[MAX_LINE_LENGTH];
	ssize_t  n;
	int	 ret = 0;

	assert(r != NULL);
	assert(fd != -1);

	for (;;) {
		n = read(fd, buf, sizeof(buf));
		if (n == 0)
			break;
		if (n > 0) {
			/* Sets reader error. */
			ret = mtree_reader_add(r, buf, n);
		} else {
			if (errno == EINTR ||
#ifdef EWOULDBLOCK
			    errno == EWOULDBLOCK ||
#endif
			    errno == EAGAIN)
				continue;
			mtree_reader_set_errno_error(r, errno, NULL);
			ret = -1;
		}
		if (ret == -1)
			break;
	}
	if (ret == -1)
		mtree_reader_reset(r);

	return (ret);
}

int
mtree_reader_finish(struct mtree_reader *r, struct mtree_entry **entries)
{
	int ret;

	assert(r != NULL);
	assert(entries != NULL);

	ret = 0;
	if (r->buflen > 0) {
		/*
		 * When reading the final line, do not require it to be terminated
	 	 * by a newline.
	 	 */
	 	// XXX what it has let's say "a" at the end? it's a valid entry!!
		if (r->buflen > 1) {
			if (r->buf[r->buflen - 2] == '\\') {
				/* Surely an incomplete line. */
				errno = EINVAL;
				ret = -1;
			} else
				ret = parse_line(r, r->buf);
		}
		r->buflen = 0;
	}
	if (ret == 0) {
		/* Sets reader error. */
		ret = finish_entries(r, entries);
	}

	mtree_reader_reset(r);
	return (ret);
}

const char *
mtree_reader_get_error(struct mtree_reader *r)
{

	assert(r != NULL);

	return (r->error);
}

/*
 * Set errno and reader error to the given string or strerror.
 */
void
mtree_reader_set_errno_error(struct mtree_reader *r, int err, const char *format, ...)
{
	va_list args;
	char	buf[MAX_ERRSTR_LENGTH];

	assert(r != NULL);

	if (format != NULL) {
		errno = err;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);

		mtree_copy_string(&r->error, buf);
	} else
		mtree_reader_set_errno_prefix(r, err, NULL);
}

/*
 * Set errno and reader error to a string in the format "prefix: strerror".
 */
void
mtree_reader_set_errno_prefix(struct mtree_reader *r, int err, const char *prefix, ...)
{
	va_list  args;
	char	 buf[MAX_ERRSTR_LENGTH];
	char	 errstr[MAX_ERRSTR_LENGTH];
	char	*errstrp;
	int	 len;

	assert(r != NULL);

	errno = err;
#ifdef STRERROR_R_CHAR_P
	errstrp = strerror_r(errno, errstr, sizeof(errstr));
#else
	strerror_r(errno, errstr, sizeof(errstr));
	errstrp = errstr;
#endif
	if (prefix != NULL) {
		va_start(args, prefix);
		len = vsnprintf(buf, sizeof(buf), prefix, args);
		va_end(args);
		if (len < 1) {
			if (len == 0)
				mtree_copy_string(&r->error, errstrp);
			else
				mtree_copy_string(&r->error, NULL);
			return;
		}
		len += strlen(errstr) + 3;

		r->error = realloc(r->error, len);
		if (r->error == NULL)
			return;

		snprintf(r->error, len, "%s: %s", buf, errstrp);
	} else
		mtree_copy_string(&r->error, errstrp);
}

/*
 * Get reading options.
 */
int
mtree_reader_get_options(struct mtree_reader *r)
{

	assert(r != NULL);

	return (r->options);
}

/*
 * Set reading options.
 */
void
mtree_reader_set_options(struct mtree_reader *r, int options)
{

	assert(r != NULL);

	r->options = options;
}

/*
 * Set reading filter.
 */
void
mtree_reader_set_filter(struct mtree_reader *r, mtree_entry_filter_fn f,
    void *user_data)
{

	assert(r != NULL);

	r->filter = f;
	r->filter_data = user_data;
}

/*
 * Get keywords to be read from spec files.
 */
uint64_t
mtree_reader_get_spec_keywords(struct mtree_reader *r)
{

	assert(r != NULL);

	return (r->spec_keywords);
}

/*
 * Set keywords to be read from spec files.
 */
void
mtree_reader_set_spec_keywords(struct mtree_reader *r, uint64_t keywords)
{

	assert(r != NULL);

	r->spec_keywords = keywords;
}

/*
 * Get keywords to be read from path.
 */
uint64_t
mtree_reader_get_path_keywords(struct mtree_reader *r)
{

	assert(r != NULL);

	return (r->path_keywords);
}

/*
 * Set keywords to be read from path.
 */
void
mtree_reader_set_path_keywords(struct mtree_reader *r, uint64_t keywords)
{

	assert(r != NULL);

	r->path_keywords = keywords;
}
