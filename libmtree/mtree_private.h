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

#ifndef _LIBMTREE_MTREE_PRIVATE_H_
#define _LIBMTREE_MTREE_PRIVATE_H_

#include <sys/types.h>
#include <sys/stat.h>

#include <fts.h>
#include <stdint.h>

#include "mtree.h"

typedef struct _mtree_entry_data	mtree_entry_data;
typedef struct _mtree_keyword_map	mtree_keyword_map;
typedef struct _mtree_reader		mtree_reader;

struct _mtree_entry_data {
	uint32_t	 cksum;
	long		 keywords;
	char		*contents;
	char		*flags;
	char		*gname;
	char		*link;
	char		*uname;
	char		*md5digest;
	char		*rmd160digest;
	char		*sha1digest;
	char		*sha256digest;
	char		*sha384digest;
	char		*sha512digest;
	struct {
		gid_t	st_gid;
		ino_t	st_ino;
		mode_t	st_mode;
		struct timespec st_mtim;
		nlink_t	st_nlink;
		off_t	st_size;
		uid_t	st_uid;
	} stat;
};

struct _mtree_entry {
	mtree_entry 	*prev;
	mtree_entry 	*next;
	mtree_entry 	*parent;
	char            *path;
	char            *name;
	mtree_entry_data data;
};

struct _mtree_reader {
	mtree_entry 	*entries;
	mtree_entry 	*parent;
	mtree_entry_data defaults;
	char 		*buf;
	int   		 buflen;
};

struct _mtree_spec {
	mtree_entry 	*entries;
	mtree_reader 	*reader;
};

struct _mtree_keyword_map {
	char		*name;
	long		 keyword;
};

extern const mtree_keyword_map mtree_keywords[];

/* mtree_entry.c */
mtree_entry	*mtree_entry_create(void);
mtree_entry	*mtree_entry_create_from_file(const char *path);
mtree_entry	*mtree_entry_create_from_ftsent(FTSENT *ftsent);
void		 mtree_entry_free(mtree_entry *entry);
void		 mtree_entry_free_all(mtree_entry *entries);
void		 mtree_entry_free_data_items(mtree_entry_data *data);
void		 mtree_entry_copy_missing_keywords(mtree_entry *entry, mtree_entry_data *from);
mtree_entry 	*mtree_entry_prepend(mtree_entry *entry, mtree_entry *child);
mtree_entry 	*mtree_entry_append(mtree_entry *entry, mtree_entry *child);

/* mtree_crc.c */
int 		 mtree_crc(int fd, uint32_t *crc_val, uint32_t *crc_total);

/* mtree_reader.c */
mtree_reader	*mtree_reader_create(void);
void		 mtree_reader_free(mtree_reader *r);
void		 mtree_reader_reset(mtree_reader *r);
int		 mtree_reader_add(mtree_reader *r, const char *s, int len);
int		 mtree_reader_finish(mtree_reader *r, mtree_entry **entries);

/* mtree_utils.c */
long		 mtree_str_to_keyword(const char *s);
int32_t 	 mtree_str_to_type(const char *s);
const char 	*mtree_keyword_to_str(long keyword);
int		 mtree_copy_string(char **dst, const char *src);

#endif /* !_LIBMTREE_MTREE_PRIVATE_H_ */
