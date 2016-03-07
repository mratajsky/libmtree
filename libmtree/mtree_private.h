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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "compat.h"

#define	IS_DOT(nm)		((nm)[0] == '.' && (nm)[1] == '\0')
#define	IS_DOTDOT(nm)		((nm)[0] == '.' && (nm)[1] == '.' && (nm)[2] == '\0')

#define MAX_ERRSTR_LENGTH	1024
#define MAX_LINE_LENGTH		4096
#define MODE_MASK		(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

/*
 * Universal trie item, used when only the presence of a key matters.
 */
#define TRIE_ITEM		((void *) (size_t) 1)

#ifdef MTREE_WARN
#define WARN(...) do {			\
	fprintf(stderr, __VA_ARGS__);	\
	fprintf(stderr, "\n");		\
} while (0)
#else
#define WARN(...) do { } while (0)
#endif

struct mtree_cksum;
struct mtree_device;
struct mtree_entry;
struct mtree_entry_data;
struct mtree_spec;
struct mtree_spec_diff;
struct mtree_timespec;
struct mtree_trie;
struct mtree_trie_node;
struct mtree_reader;
struct mtree_writer;

/*
 * struct mtree_cksum
 */
struct mtree_cksum {
	uint32_t		 init;		/* initializer */
	uint32_t		 crc;		/* current checksum */
	size_t			 len;		/* current length of input */
	int			 done;
};

/*
 * struct mtree_device
 */
struct mtree_device {
	mtree_device_format	 format;	/* device entry format */
	int			 fields;	/* bit array of filled fields */
	dev_t			 number;	/* device number */
	dev_t			 major;		/* major number */
	dev_t			 minor;		/* minor number */
	dev_t			 unit;		/* unit number (for bsdos) */
	dev_t			 subunit;	/* subunit number (for bsdos) */
	int			 err;
	char			*errstr;	/* current error message */
};

/*
 * struct mtree_entry_data
 */
struct mtree_entry_data {
	uint64_t		 keywords;
	mtree_entry_type	 type;		/* keyword values */
	uint32_t		 cksum;
	char			*contents;
	struct mtree_device	*device;
	struct mtree_device	*resdevice;
	char			*flags;
	char			*gname;
	char			*link;
	char			*tags;
	char			*uname;
	char			*md5digest;
	char			*rmd160digest;
	char			*sha1digest;
	char			*sha256digest;
	char			*sha384digest;
	char			*sha512digest;
	int64_t			 st_gid;	/* stat(2) values */
	uint64_t		 st_ino;
	int			 st_mode;
	struct mtree_timespec	 st_mtim;
	int64_t			 st_nlink;
	int64_t			 st_size;
	int64_t			 st_uid;
};

/*
 * Entry flags, for internal use onyl.
 */
#define __MTREE_ENTRY_VIRTUAL		0x01	/* artificially created entry */
#define __MTREE_ENTRY_SKIP		0x02	/* skip the entry */
#define __MTREE_ENTRY_SKIP_CHILDREN	0x04	/* skip children of the entry */

/*
 * struct mtree_entry
 */
struct mtree_entry {
	struct mtree_entry	*prev;
	struct mtree_entry	*next;
	struct mtree_entry	*parent;
	struct mtree_entry_data  data;
	char			*path;
	char			*name;
	char			*orig;
	char			*dirname;
	int			 flags;
};

/*
 * struct mtree_reader
 */
struct mtree_reader {
	struct mtree_entry 	*entries;
	struct mtree_entry 	*parent;
	struct mtree_entry	*loose;
	struct mtree_entry_data  defaults;
	char			*buf;
	int			 buflen;
	int			 path_last;
	dev_t			 base_dev;
	char			*error;
	uint64_t		 path_keywords;
	uint64_t		 spec_keywords;
	int			 options;
	mtree_entry_filter_fn	 filter;
	void			*filter_data;
	struct mtree_trie	*skip_trie;
};

typedef int (*writer_fn)(struct mtree_writer *, const char *);
/*
 * struct mtree_writer
 */
struct mtree_writer {
	union {
		FILE		*fp;
		int		 fd;
		struct {
			mtree_writer_fn  fn;
			void		*data;
		} fn;
	} dst;
	struct mtree_entry_data  defaults;
	mtree_format		 format;
	int			 options;
	int			 indent;
	uint64_t		 keywords;
	writer_fn		 writer;
};

/*
 * struct mtree_spec_diff
 */
struct mtree_spec_diff {
	struct mtree_entry	*s1only;
	struct mtree_entry	*s2only;
	struct mtree_entry	*diff;
	struct mtree_entry	*match;
	struct mtree_writer	*writer;
};

/*
 * struct mtree_spec
 */
struct mtree_spec {
	struct mtree_entry 	*entries;
	struct mtree_reader 	*reader;
	struct mtree_writer 	*writer;
	int			 reading;
	uint64_t		 read_keywords;
	int			 read_options;
	mtree_entry_filter_fn	 read_filter;
	void			*read_filter_data;
};

typedef void (*mtree_trie_free_fn)(void *);
/*
 * struct mtree_trie
 */
struct mtree_trie {
	struct mtree_trie_node	*top;
	mtree_trie_free_fn	 free_fn;
};

/*
 * struct mtree_keyword_map
 * Assists conversion between keyword names and constants
 */
struct mtree_keyword_map {
	char			*name;
	uint64_t		 keyword;
};

extern const struct mtree_keyword_map mtree_keywords[];

/* mtree_device.c */
int			 mtree_device_compare(const struct mtree_device *dev1,
			    const struct mtree_device *dev2);
void			 mtree_device_copy_data(struct mtree_device *dev,
			    const struct mtree_device *from);

/* mtree_entry.c */
struct mtree_entry	*mtree_entry_create_empty(void);
int			 mtree_entry_data_compare_keyword(
			    const struct mtree_entry_data *data1,
			    const struct mtree_entry_data *data2,
			    uint64_t keyword);
void			 mtree_entry_data_copy_keywords(
			    struct mtree_entry_data *data,
			    const struct mtree_entry_data *from,
			    uint64_t keywords, int overwrite);
void			 mtree_entry_free_data_items(struct mtree_entry_data *data);

/* mtree_reader.c */
struct mtree_reader	*mtree_reader_create(void);
void			 mtree_reader_free(struct mtree_reader *r);
void			 mtree_reader_reset(struct mtree_reader *r);
int			 mtree_reader_read_path(struct mtree_reader *r, const char *path,
			    struct mtree_entry **entries);
int			 mtree_reader_add(struct mtree_reader *r, const char *s,
			    ssize_t len);
int			 mtree_reader_add_from_file(struct mtree_reader *r, FILE *fp);
int			 mtree_reader_add_from_fd(struct mtree_reader *r, int fd);
int			 mtree_reader_finish(struct mtree_reader *r,
			    struct mtree_entry **entries);

int			 mtree_reader_get_options(struct mtree_reader *r);
void			 mtree_reader_set_options(struct mtree_reader *r, int options);
void			 mtree_reader_set_filter(struct mtree_reader *r,
			    mtree_entry_filter_fn f, void *user_data);

uint64_t		 mtree_reader_get_spec_keywords(struct mtree_reader *r);
void			 mtree_reader_set_spec_keywords(struct mtree_reader *r,
			    uint64_t keywords);
uint64_t		 mtree_reader_get_path_keywords(struct mtree_reader *r);
void			 mtree_reader_set_path_keywords(struct mtree_reader *r,
			    uint64_t keywords);

const char		*mtree_reader_get_error(struct mtree_reader *r);
void			 mtree_reader_set_errno_error(struct mtree_reader *r,
			    int err, const char *format, ...);
void			 mtree_reader_set_errno_prefix(struct mtree_reader *r,
			    int err, const char *prefix, ...);

/* mtree_writer.c */
struct mtree_writer	*mtree_writer_create(void);
void			 mtree_writer_free(struct mtree_writer *w);
const char		*mtree_writer_get_error(struct mtree_writer *w);
mtree_format		 mtree_writer_get_format(struct mtree_writer *w);
void			 mtree_writer_set_format(struct mtree_writer *w,
			    mtree_format format);
int			 mtree_writer_get_options(struct mtree_writer *w);
void			 mtree_writer_set_options(struct mtree_writer *w, int options);
void			 mtree_writer_set_output_file(struct mtree_writer *w, FILE *fp);
void			 mtree_writer_set_output_fd(struct mtree_writer *w, int fd);
void			 mtree_writer_set_output_writer(struct mtree_writer *w,
			    mtree_writer_fn f, void *user_data);
int			 mtree_writer_write_entries(struct mtree_writer *w,
			    struct mtree_entry *entries);

/* mtree_trie.c */
struct mtree_trie	*mtree_trie_create(mtree_trie_free_fn f);
void			 mtree_trie_free(struct mtree_trie *trie);
int			 mtree_trie_insert(struct mtree_trie *trie, const char *key,
			    void *item);
void			*mtree_trie_find(struct mtree_trie *trie, const char *key);
size_t			 mtree_trie_count(struct mtree_trie *trie);

/* mtree_utils.c */
int64_t			 mtree_atol(const char *p, const char **endptr);
int64_t			 mtree_atol8(const char *p, const char **endptr);
int64_t			 mtree_atol10(const char *p, const char **endptr);
int64_t			 mtree_atol16(const char *p, const char **endptr);
int			 mtree_cleanup_path(const char *path, char **ppart,
			    char **npart);
char			*mtree_concat_path(const char *d, const char *f);
int			 mtree_copy_string(char **dst, const char *src);
char			*mtree_gname_from_gid(gid_t gid);
char			*mtree_uname_from_uid(uid_t uid);
char			*mtree_readlink(const char *path);
char			*mtree_vispath(const char *path, int style);

#endif /* !_LIBMTREE_MTREE_PRIVATE_H_ */
