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

#ifndef _LIBMTREE_MTREE_H_
#define _LIBMTREE_MTREE_H_

#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>
#include <stddef.h>

struct mtree_cksum;
struct mtree_device;
struct mtree_digest;
struct mtree_entry;
struct mtree_spec;
struct mtree_spec_diff;
/*
 * POSIX.1b structure for a time value.  This is like a `struct timeval' but
 * has nanoseconds instead of microseconds.
 *
 * It is exactly the same as struct timespec, provided here for compatibility.
 */
struct mtree_timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * mtree_cksum:
 *
 * Calculate POSIX 1003.2 checksum.
 *
 * The typical way to use these functions is:
 *	- call mtree_cksum_create() to create an mtree_cksum,
 *	- call mtree_cksum_update() repeatedly to update the checksum with
 *	  file contents,
 *	- call mtree_cksum_get_result() to finalize the calculation and
 *	  retrieve the result,
 *	- call mtree_cksum_free() to free the structure.
 */
struct mtree_cksum	*mtree_cksum_create(uint32_t init);
void			 mtree_cksum_free(struct mtree_cksum *cksum);
void			 mtree_cksum_reset(struct mtree_cksum *cksum, uint32_t init);
void			 mtree_cksum_update(struct mtree_cksum *cksum,
			    const unsigned char *data, size_t len);
uint32_t		 mtree_cksum_get_result(struct mtree_cksum *cksum);

/*
 * Default initialization value for mtree_cksum_create().
 */
#define MTREE_CKSUM_DEFAULT_INIT	0

/*****************************************************************************/

/* Device formats. */
typedef enum {
	MTREE_DEVICE_386BSD,
	MTREE_DEVICE_4BSD,
	MTREE_DEVICE_BSDOS,
	MTREE_DEVICE_FREEBSD,
	MTREE_DEVICE_HPUX,
	MTREE_DEVICE_ISC,
	MTREE_DEVICE_LINUX,
	MTREE_DEVICE_NATIVE,
	MTREE_DEVICE_NETBSD,
	MTREE_DEVICE_OSF1,
	MTREE_DEVICE_SCO,
	MTREE_DEVICE_SOLARIS,
	MTREE_DEVICE_SUNOS,
	MTREE_DEVICE_SVR3,
	MTREE_DEVICE_SVR4,
	MTREE_DEVICE_ULTRIX
} mtree_device_format;

/* Device field constants. */
#define MTREE_DEVICE_FIELD_NUMBER		0x01
#define MTREE_DEVICE_FIELD_MAJOR		0x02
#define MTREE_DEVICE_FIELD_MINOR		0x04
#define MTREE_DEVICE_FIELD_UNIT			0x08
#define MTREE_DEVICE_FIELD_SUBUNIT		0x10

/*
 * mtree_device:
 *
 * Structure describing the "device" and "resdevice" keywords.
 */
struct mtree_device	*mtree_device_create(void);
void			 mtree_device_free(struct mtree_device *dev);
struct mtree_device	*mtree_device_copy(const struct mtree_device *dev);
void			 mtree_device_reset(struct mtree_device *dev);
int			 mtree_device_parse(struct mtree_device *dev, const char *s);
char			*mtree_device_string(struct mtree_device *dev);
mtree_device_format	 mtree_device_get_format(struct mtree_device *dev);
void			 mtree_device_set_format(struct mtree_device *dev,
			    mtree_device_format format);
dev_t			 mtree_device_get_value(struct mtree_device *dev,
			    int field);
void			 mtree_device_set_value(struct mtree_device *dev,
			    int field, dev_t value);
int			 mtree_device_get_fields(struct mtree_device *dev);
const char		*mtree_device_get_error(struct mtree_device *dev);

/*****************************************************************************/

/* Crypto digest types. */
#define MTREE_DIGEST_MD5	0x01
#define MTREE_DIGEST_SHA1	0x02
#define MTREE_DIGEST_SHA256	0x04
#define MTREE_DIGEST_SHA384	0x08
#define MTREE_DIGEST_SHA512	0x10
#define MTREE_DIGEST_RMD160	0x20

/*
 * mtree_digest:
 *
 * Calculate MD5, SHA1, etc. crypto digests.
 *
 * The typical way to use these functions is:
 *	- call mtree_digest_create() to create an mtree_digest,
 *	- call mtree_digest_update() repeatedly to update the digest with
 *	  file contents,
 *	- call mtree_digest_get_result() to retrieve the result,
 *	- call mtree_digest_free() to free the structure.
 */
struct mtree_digest	*mtree_digest_create(int types);
void			 mtree_digest_free(struct mtree_digest *digest);
void			 mtree_digest_reset(struct mtree_digest *digest);
void			 mtree_digest_update(struct mtree_digest *digest,
			    const unsigned char *data, size_t len);
const char		*mtree_digest_get_result(struct mtree_digest *digest,
			    int type);
int			 mtree_digest_get_types(struct mtree_digest *digest);
int			 mtree_digest_get_available_types(void);

/*****************************************************************************/

/* Spec entry keyword constants. */
#define MTREE_KEYWORD_CKSUM			0x000000001ULL
#define MTREE_KEYWORD_CONTENTS			0x000000002ULL
#define MTREE_KEYWORD_DEVICE			0x000000004ULL
#define MTREE_KEYWORD_FLAGS			0x000000008ULL
#define MTREE_KEYWORD_GID			0x000000010ULL
#define MTREE_KEYWORD_GNAME			0x000000020ULL
#define MTREE_KEYWORD_IGNORE			0x000000040ULL
#define MTREE_KEYWORD_INODE			0x000000080ULL
#define MTREE_KEYWORD_LINK			0x000000100ULL
#define MTREE_KEYWORD_MD5			0x000000200ULL
#define MTREE_KEYWORD_MD5DIGEST			0x000000400ULL
#define MTREE_KEYWORD_MODE			0x000000800ULL
#define MTREE_KEYWORD_NLINK			0x000001000ULL
#define MTREE_KEYWORD_NOCHANGE			0x000002000ULL
#define MTREE_KEYWORD_OPTIONAL			0x000004000ULL
#define MTREE_KEYWORD_RESDEVICE			0x000008000ULL
#define MTREE_KEYWORD_RIPEMD160DIGEST		0x000010000ULL
#define MTREE_KEYWORD_RMD160			0x000020000ULL
#define MTREE_KEYWORD_RMD160DIGEST		0x000040000ULL
#define MTREE_KEYWORD_SHA1			0x000080000ULL
#define MTREE_KEYWORD_SHA1DIGEST		0x000100000ULL
#define MTREE_KEYWORD_SHA256			0x000200000ULL
#define MTREE_KEYWORD_SHA256DIGEST		0x000400000ULL
#define MTREE_KEYWORD_SHA384			0x000800000ULL
#define MTREE_KEYWORD_SHA384DIGEST		0x001000000ULL
#define MTREE_KEYWORD_SHA512			0x002000000ULL
#define MTREE_KEYWORD_SHA512DIGEST		0x004000000ULL
#define MTREE_KEYWORD_SIZE			0x008000000ULL
#define MTREE_KEYWORD_TAGS			0x010000000ULL
#define MTREE_KEYWORD_TIME			0x020000000ULL
#define MTREE_KEYWORD_TYPE			0x040000000ULL
#define MTREE_KEYWORD_UID			0x080000000ULL
#define MTREE_KEYWORD_UNAME			0x100000000ULL

/*
 * Mask of all keywords.
 */
#define MTREE_KEYWORD_MASK_ALL		(MTREE_KEYWORD_CKSUM |		\
					 MTREE_KEYWORD_CONTENTS |	\
					 MTREE_KEYWORD_DEVICE |		\
					 MTREE_KEYWORD_FLAGS |		\
					 MTREE_KEYWORD_GID |		\
					 MTREE_KEYWORD_GNAME |		\
					 MTREE_KEYWORD_IGNORE |		\
					 MTREE_KEYWORD_INODE |		\
					 MTREE_KEYWORD_LINK |		\
					 MTREE_KEYWORD_MASK_MD5 |	\
					 MTREE_KEYWORD_MODE |		\
					 MTREE_KEYWORD_NLINK |		\
					 MTREE_KEYWORD_NOCHANGE |	\
					 MTREE_KEYWORD_OPTIONAL |	\
					 MTREE_KEYWORD_RESDEVICE |	\
					 MTREE_KEYWORD_MASK_RMD160 |	\
					 MTREE_KEYWORD_MASK_SHA1 |	\
					 MTREE_KEYWORD_MASK_SHA256 |	\
					 MTREE_KEYWORD_MASK_SHA384 |	\
					 MTREE_KEYWORD_MASK_SHA512 |	\
					 MTREE_KEYWORD_SIZE |		\
					 MTREE_KEYWORD_TAGS |		\
					 MTREE_KEYWORD_TIME |		\
					 MTREE_KEYWORD_TYPE |		\
					 MTREE_KEYWORD_UID |		\
					 MTREE_KEYWORD_UNAME)

/*
 * Mask of "default" keywords. This value is used as the default set
 * of keywords to be read from files on a file system.
 *
 * See mtree_spec_set_read_path_keywords().
 */
#define MTREE_KEYWORD_MASK_DEFAULT	(MTREE_KEYWORD_DEVICE |		\
					 MTREE_KEYWORD_FLAGS |		\
					 MTREE_KEYWORD_GID |		\
					 MTREE_KEYWORD_LINK |		\
					 MTREE_KEYWORD_MODE |		\
					 MTREE_KEYWORD_NLINK |		\
					 MTREE_KEYWORD_SIZE |		\
					 MTREE_KEYWORD_TIME |		\
					 MTREE_KEYWORD_TYPE |		\
					 MTREE_KEYWORD_UID)

/*
 * Crypto digest keywords provide several aliases that refer to the
 * same value.
 *
 * These masks put the aliases together for each digest type.
 */
#define MTREE_KEYWORD_MASK_MD5		(MTREE_KEYWORD_MD5 |		\
					 MTREE_KEYWORD_MD5DIGEST)
#define MTREE_KEYWORD_MASK_SHA1		(MTREE_KEYWORD_SHA1 |		\
					 MTREE_KEYWORD_SHA1DIGEST)
#define MTREE_KEYWORD_MASK_SHA256	(MTREE_KEYWORD_SHA256 |		\
					 MTREE_KEYWORD_SHA256DIGEST)
#define MTREE_KEYWORD_MASK_SHA384	(MTREE_KEYWORD_SHA384 |		\
					 MTREE_KEYWORD_SHA384DIGEST)
#define MTREE_KEYWORD_MASK_SHA512	(MTREE_KEYWORD_SHA512 |		\
					 MTREE_KEYWORD_SHA512DIGEST)
#define MTREE_KEYWORD_MASK_RMD160	(MTREE_KEYWORD_RMD160 |		\
					 MTREE_KEYWORD_RMD160DIGEST |	\
					 MTREE_KEYWORD_RIPEMD160DIGEST)

/*
 * Mask of all digests.
 */
#define MTREE_KEYWORD_MASK_DIGEST	(MTREE_KEYWORD_MASK_MD5 |	\
					 MTREE_KEYWORD_MASK_RMD160 |	\
					 MTREE_KEYWORD_MASK_SHA1 |	\
					 MTREE_KEYWORD_MASK_SHA256 |	\
					 MTREE_KEYWORD_MASK_SHA384 |	\
					 MTREE_KEYWORD_MASK_SHA512)

/*
 * Masks that put together other keywords with similar meanings.
 */
#define MTREE_KEYWORD_MASK_USER		(MTREE_KEYWORD_UID |		\
					 MTREE_KEYWORD_UNAME)
#define MTREE_KEYWORD_MASK_GROUP	(MTREE_KEYWORD_GID |		\
					 MTREE_KEYWORD_GNAME)

/*
 * Mask of all keywords that can be read by stat(2).
 *
 * Calling mtree_entry_set_keywords_stat() will only work with these
 * keywords.
 */
#define MTREE_KEYWORD_MASK_STAT		(MTREE_KEYWORD_DEVICE |		\
					 MTREE_KEYWORD_FLAGS |		\
					 MTREE_KEYWORD_GID |		\
					 MTREE_KEYWORD_INODE |		\
					 MTREE_KEYWORD_LINK |		\
					 MTREE_KEYWORD_MODE |		\
					 MTREE_KEYWORD_NLINK |		\
					 MTREE_KEYWORD_SIZE |		\
					 MTREE_KEYWORD_TIME |		\
					 MTREE_KEYWORD_TYPE |		\
					 MTREE_KEYWORD_UID)

/* Spec entry types. */
typedef enum {
	MTREE_ENTRY_BLOCK,
	MTREE_ENTRY_CHAR,
	MTREE_ENTRY_DIR,
	MTREE_ENTRY_FIFO,
	MTREE_ENTRY_FILE,
	MTREE_ENTRY_LINK,
	MTREE_ENTRY_SOCKET,
	MTREE_ENTRY_UNKNOWN
} mtree_entry_type;

/*
 * mtree_entry:
 *
 * Structure representing a file entry in an mtree spec.
 *	- file name (basename)
 *	- file path (includes the file name)
 *	- keywords
 *	- values of keywords (not all keywords take values)
 *
 * The structure also includes pointers to the next and previous entries. It
 * is a doubly-linked list.
 */
struct mtree_entry	*mtree_entry_create(const char *path);
void			 mtree_entry_free(struct mtree_entry *entry);
void			 mtree_entry_free_all(struct mtree_entry *start);
struct mtree_entry	*mtree_entry_copy(const struct mtree_entry *entry);
struct mtree_entry	*mtree_entry_copy_all(const struct mtree_entry *start);
void			 mtree_entry_copy_keywords(struct mtree_entry *entry,
			    const struct mtree_entry *from, uint64_t keywords,
			    int overwrite);
const char		*mtree_entry_get_path(struct mtree_entry *entry);
const char		*mtree_entry_get_name(struct mtree_entry *entry);
const char		*mtree_entry_get_dirname(struct mtree_entry *entry);
uint64_t		 mtree_entry_get_keywords(struct mtree_entry *entry);

#define MTREE_ENTRY_OVERWRITE		0x01
#define MTREE_ENTRY_REMOVE_EXCLUDED	0x02
void			 mtree_entry_set_keywords(struct mtree_entry *entry,
			    uint64_t keywords, int options);
void			 mtree_entry_set_keywords_stat(struct mtree_entry *entry,
			    const struct stat *st, uint64_t keywords,
			    int options);
int			 mtree_entry_compare(const struct mtree_entry *entry1,
			    const struct mtree_entry *entry2, uint64_t keywords,
			    uint64_t *diff);
int			 mtree_entry_compare_keywords(
			    const struct mtree_entry *entry1,
			    const struct mtree_entry *entry2, uint64_t keywords,
			    uint64_t *diff);
/*
 * Various list functions.
 */
size_t			 mtree_entry_count(struct mtree_entry *start);
struct mtree_entry 	*mtree_entry_get_first(struct mtree_entry *entry);
struct mtree_entry 	*mtree_entry_get_last(struct mtree_entry *entry);
struct mtree_entry 	*mtree_entry_get_previous(struct mtree_entry *entry);
struct mtree_entry 	*mtree_entry_get_next(struct mtree_entry *entry);
/*
 * All of these return the potentially changed head of the list.
 */
struct mtree_entry 	*mtree_entry_append(struct mtree_entry *head,
			    struct mtree_entry *entry);
struct mtree_entry 	*mtree_entry_prepend(struct mtree_entry *head,
			    struct mtree_entry *entry);
struct mtree_entry	*mtree_entry_insert_before(struct mtree_entry *head,
			    struct mtree_entry *sibling, struct mtree_entry *entry);
struct mtree_entry	*mtree_entry_insert_after(struct mtree_entry *head,
			    struct mtree_entry *sibling, struct mtree_entry *entry);
struct mtree_entry	*mtree_entry_unlink(struct mtree_entry *head,
			    struct mtree_entry *entry);
struct mtree_entry	*mtree_entry_reverse(struct mtree_entry *head);
struct mtree_entry	*mtree_entry_find(struct mtree_entry *start,
			    const char *path);
struct mtree_entry	*mtree_entry_find_prefix(struct mtree_entry *start,
			    const char *path_prefix);
/*
 * List merging.
 */
#define MTREE_ENTRY_MERGE_DIFFERENT_TYPES	0x01
struct mtree_entry	*mtree_entry_merge(struct mtree_entry *head1,
			    struct mtree_entry *head2,
			    int options, struct mtree_entry **mismerged);
struct mtree_entry	*mtree_entry_merge_fast(struct mtree_entry *head1_merged,
			    struct mtree_entry *head2,
			    int options, struct mtree_entry **mismerged);
/*
 * mtree_entry_compare_fn:
 *
 * Custom comparison function for mtree_entry_sort():
 *	return negative value if entry1 < entry2
 *	return positive value if entry1 > entry2
 *	return 0 if entry1 == entry2
 */
typedef int (*mtree_entry_compare_fn)(struct mtree_entry *, struct mtree_entry *);

struct mtree_entry	*mtree_entry_sort(struct mtree_entry *head,
			    mtree_entry_compare_fn cmp);
struct mtree_entry	*mtree_entry_sort_path(struct mtree_entry *head);

/*
 * Getters for keyword values.
 */
uint32_t		 mtree_entry_get_cksum(struct mtree_entry *entry);
const char		*mtree_entry_get_contents(struct mtree_entry *entry);
struct mtree_device	*mtree_entry_get_device(struct mtree_entry *entry);
const char		*mtree_entry_get_flags(struct mtree_entry *entry);
int64_t			 mtree_entry_get_gid(struct mtree_entry *entry);
const char		*mtree_entry_get_gname(struct mtree_entry *entry);
uint64_t		 mtree_entry_get_inode(struct mtree_entry *entry);
const char		*mtree_entry_get_link(struct mtree_entry *entry);
const char		*mtree_entry_get_md5digest(struct mtree_entry *entry);
int			 mtree_entry_get_mode(struct mtree_entry *entry);
int64_t			 mtree_entry_get_nlink(struct mtree_entry *entry);
struct mtree_device	*mtree_entry_get_resdevice(struct mtree_entry *entry);
const char		*mtree_entry_get_rmd160digest(struct mtree_entry *entry);
const char		*mtree_entry_get_sha1digest(struct mtree_entry *entry);
const char		*mtree_entry_get_sha256digest(struct mtree_entry *entry);
const char		*mtree_entry_get_sha384digest(struct mtree_entry *entry);
const char		*mtree_entry_get_sha512digest(struct mtree_entry *entry);
int64_t			 mtree_entry_get_size(struct mtree_entry *entry);
const char		*mtree_entry_get_tags(struct mtree_entry *entry);
struct mtree_timespec	*mtree_entry_get_time(struct mtree_entry *entry);
mtree_entry_type	 mtree_entry_get_type(struct mtree_entry *entry);
int64_t			 mtree_entry_get_uid(struct mtree_entry *entry);
const char		*mtree_entry_get_uname(struct mtree_entry *entry);

/*
 * Setters for keyword values.
 */
void			 mtree_entry_set_cksum(struct mtree_entry *entry,
			    uint32_t cksum);
void			 mtree_entry_set_contents(struct mtree_entry *entry,
			    const char *contents);
void			 mtree_entry_set_device(struct mtree_entry *entry,
			    const struct mtree_device *dev);
void			 mtree_entry_set_device_number(struct mtree_entry *entry,
			    dev_t number);
void			 mtree_entry_set_flags(struct mtree_entry *entry,
			    const char *flags);
void			 mtree_entry_set_gid(struct mtree_entry *entry,
			    int64_t gid);
void			 mtree_entry_set_gname(struct mtree_entry *entry,
			    const char *gname);
void			 mtree_entry_set_inode(struct mtree_entry *entry,
			    uint64_t ino);
void			 mtree_entry_set_link(struct mtree_entry *entry,
			    const char *link);
void			 mtree_entry_set_md5digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_mode(struct mtree_entry *entry,
			    int mode);
void			 mtree_entry_set_nlink(struct mtree_entry *entry,
			    int64_t nlink);
void			 mtree_entry_set_resdevice(struct mtree_entry *entry,
			    const struct mtree_device *dev);
void			 mtree_entry_set_resdevice_number(struct mtree_entry *entry,
			    dev_t number);
void			 mtree_entry_set_rmd160digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_sha1digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_sha256digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_sha384digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_sha512digest(struct mtree_entry *entry,
			    const char *digest, uint64_t keywords);
void			 mtree_entry_set_size(struct mtree_entry *entry,
			    int64_t size);
void			 mtree_entry_set_tags(struct mtree_entry *entry,
			    const char *tags);
void			 mtree_entry_set_time(struct mtree_entry *entry,
			    const struct mtree_timespec *ts);
void			 mtree_entry_set_type(struct mtree_entry *entry,
			    mtree_entry_type type);
void			 mtree_entry_set_uid(struct mtree_entry *entry,
			    int64_t uid);
void			 mtree_entry_set_uname(struct mtree_entry *entry,
			    const char *uname);

/*****************************************************************************/

/*
 * Writing format
 */
typedef enum {
	MTREE_FORMAT_DEFAULT,
	MTREE_FORMAT_1_0,		/* mtree -c format */
	MTREE_FORMAT_2_0,		/* mtree -C format */
	MTREE_FORMAT_2_0_PATH_LAST,	/* mtree -D format */
	MTREE_FORMAT_DIFF_FIRST,	/* mtree -f -f format, used by spec diff */
	MTREE_FORMAT_DIFF_SECOND,
	MTREE_FORMAT_DIFF_DIFFER
} mtree_format;

typedef int (*mtree_entry_filter_fn)(struct mtree_entry *, void *);

/*
 * Return values for entry filters.
 */
#define MTREE_ENTRY_KEEP		0x01	/* keep current entry */
#define MTREE_ENTRY_SKIP		0x02	/* skip current entry */
#define MTREE_ENTRY_SKIP_CHILDREN	0x04	/* skip children of current directory */

/*
 * mtree_spec:
 *
 * Structure representing a collection of mtree entries.
 */
struct mtree_spec	*mtree_spec_create(void);
void			 mtree_spec_free(struct mtree_spec *spec);
struct mtree_entry	*mtree_spec_get_entries(struct mtree_spec *spec);
struct mtree_entry	*mtree_spec_take_entries(struct mtree_spec *spec);
void			 mtree_spec_set_entries(struct mtree_spec *spec,
			    struct mtree_entry *entries);
int			 mtree_spec_copy_entries(struct mtree_spec *spec,
			    const struct mtree_entry *entries);
/*
 * Universal reading options.
 *
 * These options are applicable for both path and spec reading.
 */
#define MTREE_READ_SKIP_BLOCK			0x0001
#define MTREE_READ_SKIP_CHAR			0x0002
#define MTREE_READ_SKIP_DIR			0x0004
#define MTREE_READ_SKIP_FIFO			0x0008
#define MTREE_READ_SKIP_FILE			0x0010
#define MTREE_READ_SKIP_LINK			0x0020
#define MTREE_READ_SKIP_SOCKET			0x0040
#define MTREE_READ_SKIP_UNKNOWN			0x0080
#define MTREE_READ_SKIP_ALL			0x00FF
#define MTREE_READ_SORT				0x0100
#define MTREE_READ_MERGE			0x0200
#define MTREE_READ_MERGE_DIFFERENT_TYPES	0x0400
/*
 * Path reading options.
 */
#define MTREE_READ_PATH_SKIP_ON_ERROR		0x1000
#define MTREE_READ_PATH_FOLLOW_SYMLINKS		0x2000
#define MTREE_READ_PATH_DONT_CROSS_MOUNT	0x4000

int			 mtree_spec_read_path(struct mtree_spec *spec,
			    const char *path);
int			 mtree_spec_read_spec_data(struct mtree_spec *spec,
			    const char *data, size_t len);
int			 mtree_spec_read_spec_data_finish(struct mtree_spec *spec);

const char		*mtree_spec_get_read_error(struct mtree_spec *spec);
int			 mtree_spec_get_read_options(struct mtree_spec *spec);
void			 mtree_spec_set_read_options(struct mtree_spec *spec,
			    int options);
void			 mtree_spec_set_read_filter(struct mtree_spec *spec,
			    mtree_entry_filter_fn f, void *user_data);
uint64_t		 mtree_spec_get_read_path_keywords(struct mtree_spec *spec);
void			 mtree_spec_set_read_path_keywords(struct mtree_spec *spec,
			    uint64_t keywords);
uint64_t		 mtree_spec_get_read_spec_keywords(struct mtree_spec *spec);
void			 mtree_spec_set_read_spec_keywords(struct mtree_spec *spec,
			    uint64_t keywords);
/*
 * Writing options.
 */
#define MTREE_WRITE_USE_SET			0x02
#define MTREE_WRITE_INDENT			0x04
#define MTREE_WRITE_INDENT_LEVEL		0x08
#define MTREE_WRITE_SPLIT_LONG_LINES		0x10
#define MTREE_WRITE_DIR_COMMENTS		0x20
#define MTREE_WRITE_DIR_BLANK_LINES		0x40
#define MTREE_WRITE_ENCODE_CSTYLE		0x80  /* use C-style path encoding */

typedef int (*mtree_writer_fn)(const char *, size_t, void *);

int			 mtree_spec_write_writer(struct mtree_spec *spec,
			    mtree_writer_fn f, void *user_data);

mtree_format		 mtree_spec_get_write_format(struct mtree_spec *spec);
int			 mtree_spec_get_write_options(struct mtree_spec *spec);
void			 mtree_spec_set_write_filter(struct mtree_spec *spec,
			    mtree_entry_filter_fn f, void *user_data);
void			 mtree_spec_set_write_format(struct mtree_spec *spec,
			    mtree_format format);
void			 mtree_spec_set_write_options(struct mtree_spec *spec,
			    int options);

/*****************************************************************************/

/*
 * Consider entries to be matching if their common keywords match, but one
 * of the entries has some extra keywords.
 *
 * By default such entries are considered to be different.
 */
#define MTREE_SPEC_DIFF_MATCH_EXTRA_KEYWORDS	0x100

/*
 * mtree_spec_diff:
 *
 * Result of comparison of mtree specs.
 */
struct mtree_spec_diff	*mtree_spec_diff_create(struct mtree_spec *spec1,
			    struct mtree_spec *spec2, uint64_t keywords, int options);
void			 mtree_spec_diff_free(struct mtree_spec_diff *sd);
struct mtree_entry	*mtree_spec_diff_get_spec1_only(struct mtree_spec_diff *sd);
struct mtree_entry	*mtree_spec_diff_get_spec2_only(struct mtree_spec_diff *sd);
struct mtree_entry	*mtree_spec_diff_get_matching(struct mtree_spec_diff *sd);
struct mtree_entry	*mtree_spec_diff_get_different(struct mtree_spec_diff *sd);
int			 mtree_spec_diff_write_writer(struct mtree_spec_diff *sd,
			    mtree_writer_fn f, void *user_data);

/*****************************************************************************/

/*
 * Some convenience function.
 */
uint64_t		 mtree_keyword_parse(const char *keyword);
const char		*mtree_keyword_string(uint64_t keyword);
mtree_entry_type	 mtree_entry_type_parse(const char *type);
mtree_entry_type	 mtree_entry_type_from_mode(int mode);
const char		*mtree_entry_type_string(mtree_entry_type type);

#ifdef __cplusplus
}
#endif

#endif /* !_LIBMTREE_MTREE_H_ */
