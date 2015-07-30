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
#include <inttypes.h>

#include "compat.h"
#include "mtree.h"

#define MAX_LINE_LENGTH			1024

#define MTREE_ENTRY_MODE_MASK		(S_ISUID | S_ISGID | S_ISVTX |	\
					 S_IRWXU | S_IRWXG | S_IRWXO)

struct mtree_device;
typedef struct _mtree_entry_data	mtree_entry_data;
typedef struct _mtree_keyword_map	mtree_keyword_map;
typedef struct _mtree_reader		mtree_reader;
typedef struct _mtree_writer		mtree_writer;

/*
 * mtree_device
 *
 */
struct mtree_device {
	mtree_device_format	 format;	/* device entry format */
	int			 fields;	/* bit array of filled fields */
	dev_t			 number;	/* device number */
	dev_t			 major;		/* major number */
	dev_t			 minor;		/* minor number */
	dev_t			 unit;		/* unit number (for bsdos) */
	dev_t			 subunit;	/* subunit number (for bsdos) */
	char			*errstr;	/* current error message */
};

struct _mtree_entry_data {
	mtree_entry_type type;
	uint32_t	 cksum;
	long		 keywords;
	char		*contents;
	struct mtree_device	*device;
	char		*flags;
	char		*gname;
	char		*link;
	char		*tags;
	char		*uname;
	char		*md5digest;
	char		*rmd160digest;
	char		*sha1digest;
	char		*sha256digest;
	char		*sha384digest;
	char		*sha512digest;
	/* stat(2) data */
	gid_t		st_gid;
	ino_t		st_ino;
	mode_t		st_mode;
	struct timespec st_mtim;
	nlink_t		st_nlink;
	off_t		st_size;
	uid_t		st_uid;
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
	long		 keywords;
	int		 options;
};

typedef int (*writer_func)(mtree_writer *, const char *);
struct _mtree_writer {
	mtree_entry 	*entries;
	union {
		FILE	*fp;
		int	 fd;
	} dst;
	mtree_entry_data defaults;
	mtree_format	 format;
	int		 options;
	int		 indent;
	writer_func	 writer;
};

struct _mtree_spec_diff {
	mtree_entry	*s1only;
	mtree_entry	*s2only;
	mtree_entry	*diff;
};

struct _mtree_spec {
	mtree_entry 	*entries;
	mtree_reader 	*reader;
	mtree_writer 	*writer;
	long		 read_keywords;
	int		 read_options;
};

struct _mtree_keyword_map {
	char		*name;
	long		 keyword;
};

extern const mtree_keyword_map mtree_keywords[];

/* mtree_device.c */
void			 mtree_device_copy_data(struct mtree_device *dev,
			    struct mtree_device *from);
int			 mtree_device_string(struct mtree_device *dev, char **s);
int			 mtree_device_parse(struct mtree_device *dev, const char *s);
const char		*mtree_device_error(struct mtree_device *dev);

/* mtree_entry.c */
mtree_entry	*mtree_entry_create(void);
mtree_entry	*mtree_entry_create_from_ftsent(FTSENT *ftsent, long keywords);
void		 mtree_entry_free(mtree_entry *entry);
void		 mtree_entry_free_all(mtree_entry *entries);
void		 mtree_entry_free_data_items(mtree_entry_data *data);
mtree_entry	*mtree_entry_copy(mtree_entry *entry);
mtree_entry	*mtree_entry_copy_all(mtree_entry *entry);
void		 mtree_entry_copy_missing_keywords(mtree_entry *entry, mtree_entry_data *from);
int		 mtree_entry_compare(mtree_entry *entry1, mtree_entry *entry2,
		    long *diff);
int		 mtree_entry_compare_keyword(mtree_entry *entry1, mtree_entry *entry2,
		    long keyword);
int		 mtree_entry_data_compare_keyword(mtree_entry_data *data1, mtree_entry_data *data2,
		    long keyword);
mtree_entry 	*mtree_entry_prepend(mtree_entry *entry, mtree_entry *child);
mtree_entry 	*mtree_entry_append(mtree_entry *entry, mtree_entry *child);
mtree_entry	*mtree_entry_reverse(mtree_entry *entry);
mtree_entry	*mtree_entry_unlink(mtree_entry *head, mtree_entry *entry);

/* mtree_reader.c */
mtree_reader	*mtree_reader_create(void);
void		 mtree_reader_free(mtree_reader *r);
void		 mtree_reader_reset(mtree_reader *r);
int		 mtree_reader_add(mtree_reader *r, const char *s, int len);
int		 mtree_reader_finish(mtree_reader *r, mtree_entry **entries);
void		 mtree_reader_set_options(mtree_reader *r, int options);
void		 mtree_reader_set_keywords(mtree_reader *r, long keywords);

/* mtree_writer.c */
mtree_writer	*mtree_writer_create(void);
void		 mtree_writer_free(mtree_writer *w);
int		 mtree_writer_write_file(mtree_writer *w, mtree_entry *entries,
		    FILE *fp);
int		 mtree_writer_write_fd(mtree_writer *w, mtree_entry *entries,
		    int fd);
void		 mtree_writer_set_format(mtree_writer *w, mtree_format format);
void		 mtree_writer_set_options(mtree_writer *w, int options);

/* mtree_utils.c */
int		 mtree_copy_string(char **dst, const char *src);
char		*mtree_get_gname(gid_t gid);
char		*mtree_get_link(const char *path);
char		*mtree_get_uname(uid_t uid);
char		*mtree_get_vispath(const char *path);

#endif /* !_LIBMTREE_MTREE_PRIVATE_H_ */
