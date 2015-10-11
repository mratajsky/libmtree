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
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

#define SET_KEYWORD(entry, keyword)	(entry)->data.keywords |=  (keyword)
#define CLR_KEYWORD(entry, keyword)	(entry)->data.keywords &= ~(keyword)

#define SET_KEYWORD_VAL(entry, p, value, keyword)		\
	do {							\
		(p) = (value);					\
		SET_KEYWORD(entry, keyword);			\
	} while (0)

#define SET_KEYWORD_STR(entry, p, value, keyword)		\
	do {							\
		free(p);					\
		(p) = value;					\
		if (value != NULL)				\
			SET_KEYWORD(entry, keyword);		\
		else						\
			CLR_KEYWORD(entry, keyword);		\
	} while (0)
#define CLR_KEYWORD_STR(entry, p, keyword)			\
	SET_KEYWORD_STR(entry, p, NULL, keyword)

#define SET_KEYWORD_DUP(entry, p, value, keyword)		\
	do {							\
		free(p);					\
		if (value != NULL) {				\
			(p) = strdup(value);			\
			if ((p) != NULL)			\
				SET_KEYWORD(entry, keyword);	\
			else					\
				CLR_KEYWORD(entry, keyword);	\
		} else {					\
			(p) = NULL;				\
			CLR_KEYWORD(entry, keyword);		\
		}						\
	} while (0)

/*
 * Create a new mtree_entry and initialize it with the given path.
 */
struct mtree_entry *
mtree_entry_create(const char *path)
{
	struct mtree_entry	*entry;
	char			*p;
	char			*n;

	assert(path != NULL);

	if (mtree_cleanup_path(path, &p, &n) != 0)
		return (NULL);
	entry = mtree_entry_create_empty();
	if (entry == NULL) {
		free(p);
		free(n);
		return (NULL);
	}
	entry->orig = strdup(path);
	if (entry->orig == NULL) {
		mtree_entry_free(entry);
		return (NULL);
	}
	entry->path = p;
	entry->name = n;

	return (entry);
}

/*
 * Create a new empty mtree_entry.
 */
struct mtree_entry *
mtree_entry_create_empty(void)
{

	return (calloc(1, sizeof(struct mtree_entry)));
}

/*
 * Free the given mtree_entry.
 */
void
mtree_entry_free(struct mtree_entry *entry)
{

	assert(entry != NULL);

	mtree_entry_free_data_items(&entry->data);
	free(entry->path);
	free(entry->name);
	free(entry->orig);
	free(entry->dirname);
	free(entry);
}

/*
 * Free the given mtree_entry list.
 *
 * The value must be the head of the list. The list can be empty.
 */
void
mtree_entry_free_all(struct mtree_entry *start)
{
	struct mtree_entry *next;

	while (start != NULL) {
		next = start->next;
		mtree_entry_free(start);
		start = next;
	}
}

/*
 * Free contents of the given mtree_entry_data.
 */
void
mtree_entry_free_data_items(struct mtree_entry_data *data)
{

	assert(data != NULL);

	if (data->device != NULL)
		mtree_device_free(data->device);
	if (data->resdevice != NULL)
		mtree_device_free(data->resdevice);

	free(data->contents);
	free(data->flags);
	free(data->gname);
	free(data->link);
	free(data->tags);
	free(data->uname);
	free(data->md5digest);
	free(data->sha1digest);
	free(data->sha256digest);
	free(data->sha384digest);
	free(data->sha512digest);
	free(data->rmd160digest);
}

/*
 * Compare values of a single keyword and return non-zero if they don't match
 * or if the keyword is only present in one of the entries.
 */
int
mtree_entry_data_compare_keyword(const struct mtree_entry_data *data1,
    const struct mtree_entry_data *data2, uint64_t keyword)
{
	int res;

	if ((data1->keywords & keyword) != (data2->keywords & keyword))
		return (1);
	else if ((data1->keywords & keyword) == 0)
		return (0);

#define CMP_VAL(a, b) ((a) < (b) ? -1 : ((a) > (b) ? 1 : 0))
#define CMP_STR(a, b) (strcmp(a, b))
	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		return (CMP_VAL(data1->cksum, data2->cksum));
	case MTREE_KEYWORD_CONTENTS:
		return (CMP_STR(data1->contents, data2->contents));
	case MTREE_KEYWORD_DEVICE:
		return (mtree_device_compare(data1->device, data2->device));
	case MTREE_KEYWORD_FLAGS:
		return (CMP_STR(data1->flags, data2->flags));
	case MTREE_KEYWORD_GID:
		return (CMP_VAL(data1->st_gid, data2->st_gid));
	case MTREE_KEYWORD_GNAME:
		return (CMP_STR(data1->gname, data2->gname));
	case MTREE_KEYWORD_INODE:
		return (CMP_VAL(data1->st_ino, data2->st_ino));
	case MTREE_KEYWORD_LINK:
		return (CMP_STR(data1->link, data2->link));
	case MTREE_KEYWORD_MD5:
	case MTREE_KEYWORD_MD5DIGEST:
		return (CMP_STR(data1->md5digest, data2->md5digest));
	case MTREE_KEYWORD_MODE:
		return (CMP_VAL(data1->st_mode, data2->st_mode));
	case MTREE_KEYWORD_NLINK:
		return (CMP_VAL(data1->st_nlink, data2->st_nlink));
	case MTREE_KEYWORD_RESDEVICE:
		return (mtree_device_compare(data1->resdevice, data2->resdevice));
	case MTREE_KEYWORD_RIPEMD160DIGEST:
	case MTREE_KEYWORD_RMD160:
	case MTREE_KEYWORD_RMD160DIGEST:
		return (CMP_STR(data1->rmd160digest, data2->rmd160digest));
	case MTREE_KEYWORD_SHA1:
	case MTREE_KEYWORD_SHA1DIGEST:
		return (CMP_STR(data1->sha1digest, data2->sha1digest));
	case MTREE_KEYWORD_SHA256:
	case MTREE_KEYWORD_SHA256DIGEST:
		return (CMP_STR(data1->sha256digest, data2->sha256digest));
	case MTREE_KEYWORD_SHA384:
	case MTREE_KEYWORD_SHA384DIGEST:
		return (CMP_STR(data1->sha384digest, data2->sha384digest));
	case MTREE_KEYWORD_SHA512:
	case MTREE_KEYWORD_SHA512DIGEST:
		return (CMP_STR(data1->sha512digest, data2->sha512digest));
	case MTREE_KEYWORD_SIZE:
		return (CMP_VAL(data1->st_size, data2->st_size));
	case MTREE_KEYWORD_TAGS:
		return (CMP_STR(data1->tags, data2->tags));
	case MTREE_KEYWORD_TIME:
		res = CMP_VAL(data1->st_mtim.tv_sec, data2->st_mtim.tv_sec);
		if (res != 0)
			return (res);
		return (CMP_VAL(data1->st_mtim.tv_nsec, data2->st_mtim.tv_nsec));
	case MTREE_KEYWORD_TYPE:
		return (CMP_VAL(data1->type, data2->type));
	case MTREE_KEYWORD_UID:
		return (CMP_VAL(data1->st_uid, data2->st_uid));
	case MTREE_KEYWORD_UNAME:
		return (CMP_STR(data1->uname, data2->uname));
	}
	return (0);
}

/*
 * Compare paths of the given entries and if they match, compare the selected
 * keyword values.
 *
 * Return non-zero if the paths don't match or if some keyword doesn't match.
 * If some of the selected keywords is only present in one of the entries, it
 * is also considered a mismatch.
 *
 * If diff is non-NULL, set it to a mask of mismatching keywords, or to zero if
 * the paths don't match.
 */
int
mtree_entry_compare(const struct mtree_entry *entry1,
    const struct mtree_entry *entry2, uint64_t keywords, uint64_t *diff)
{
	int ret;

	assert(entry1 != NULL);
	assert(entry2 != NULL);

	if (entry1->path != NULL && entry2->path != NULL) {
		ret = strcmp(entry1->path, entry2->path);
		if (ret != 0) {
			if (diff != NULL)
				*diff = 0;
			return (ret);
		}
	} else if (entry1->path != entry2->path) {
		/*
		 * One of the paths is NULL.
		 */
		if (diff != NULL)
			*diff = 0;
		return (entry1->path == NULL ? -1 : 1);
	}
	return (mtree_entry_compare_keywords(entry1, entry2, keywords, diff));
}

/*
 * Compare the selected keyword values of the given entries.
 *
 * Return non-zero if some keyword doesn't match or if some of the selected
 * keywords is only present in one of the entries.
 *
 * If diff is non-NULL, set it to a mask of mismatching keywords.
 */
int
mtree_entry_compare_keywords(const struct mtree_entry *entry1,
    const struct mtree_entry *entry2, uint64_t keywords, uint64_t *diff)
{
	uint64_t	differ;
	int		ret;
	int		i;

	assert(entry1 != NULL);
	assert(entry2 != NULL);

	differ = 0;
	for (i = 0; mtree_keywords[i].keyword != 0; i++) {
		if ((keywords & mtree_keywords[i].keyword) == 0)
			continue;
		ret = mtree_entry_data_compare_keyword(&entry1->data,
		    &entry2->data, mtree_keywords[i].keyword);
		if (ret != 0) {
			/*
			 * Build up a list of keywords with different values,
			 * but only when the user is interested.
			 */
			if (diff == NULL)
				return (ret);
			differ |= mtree_keywords[i].keyword;
		}
	}
	if (diff != NULL)
		*diff = differ;
	if (differ != 0)
		return (-1);

	return (0);
}

/*
 * Compare paths of two entries, as strcmp(3) would, with the difference that
 * files are placed before directories.
 */
static int
path_cmp(struct mtree_entry *e1, struct mtree_entry *e2)
{
	const unsigned char	*p1, *p2;
	mtree_entry_type	 t1, t2;
	unsigned char		 c1, c2;

	t1 = e1->data.type;
	t2 = e2->data.type;
	p1 = (const unsigned char *)e1->path;
	p2 = (const unsigned char *)e2->path;
	while (*p1 == *p2 && *p1 != '\0') {
		p1++;
		p2++;
	}
	if (t1 != MTREE_ENTRY_DIR || t2 != MTREE_ENTRY_DIR) {
		if (strchr((const char *)p1, '/') != NULL)
			t1 = MTREE_ENTRY_DIR;
		if (strchr((const char *)p2, '/') != NULL)
			t2 = MTREE_ENTRY_DIR;

		if (t1 == MTREE_ENTRY_DIR) {
			if (t2 != MTREE_ENTRY_DIR)
				return (1);
		} else if (t2 == MTREE_ENTRY_DIR)
			return (-1);
	}
	do {
		c1 = *p1++;
		c2 = *p2++;
		if (c1 == '/' || c2 == '/') {
			if (c1 == '/')
				c1 = '\0';
			else
				c2 = '\0';
			break;
		} else if (c1 == '\0')
			break;
	} while (c1 == c2);

	return (c1 - c2);
}

/*
 * Sort list of entries, simplified version for non-circular doubly
 * linked lists.
 *
 * The algorithm used is Mergesort, because that works really well
 * on linked lists, without requiring the O(N) extra space it needs
 * when you do it on arrays.
 */
static struct mtree_entry *
sort_entries(struct mtree_entry *head, mtree_entry_compare_fn cmp)
{
	struct mtree_entry	*p, *q, *e, *tail;
	int			 insize, nmerges, psize, qsize;
	int			 i;

	insize = 1;

	for (;;) {
		p = head;
		head = NULL;
		tail = NULL;
		/* Count number of merges we do in this pass. */
		nmerges = 0;

		while (p != NULL) {
			nmerges++;
			/* Step `insize' places along from p. */
			q = p;
			psize = 0;
			for (i = 0; i < insize; i++) {
				psize++;
				q = q->next;
				if (q == NULL)
					break;
			}
			/* If q hasn't fallen off end, we have two lists to merge. */
			qsize = insize;

			/* Now we have two lists; merge them. */
			while (psize > 0 || (qsize > 0 && q)) {
				/*
				 * Decide whether next element of merge comes
				 * from p or q.
				 */
				if (psize == 0) {
					/* p is empty; e must come from q. */
					e = q; q = q->next; qsize--;
				} else if (qsize == 0 || !q) {
					/* q is empty; e must come from p. */
					e = p; p = p->next; psize--;
				} else if (cmp(p, q) <= 0) {
					/*
					 * First element of p is lower (or same);
					 * e must come from p.
					 */
					e = p; p = p->next; psize--;
				} else {
					/*
					 * First element of q is lower;
					 * e must come from q.
					 */
					e = q; q = q->next; qsize--;
				}
				/* Add the next element to the merged list. */
				if (tail)
					tail->next = e;
				else
					head = e;
				/*
				 * Maintain reverse pointers in the doubly
				 * linked list.
				 */
				e->prev = tail;
				tail = e;
			}
			/*
			 * Now p has stepped `insize' places along, and q has too. */
			p = q;
		}
		tail->next = NULL;

		/* If we have done only one merge, we're finished. */
		if (nmerges <= 1)
			break;
		/* Otherwise repeat, merging lists twice the size */
		insize *= 2;
	}
	return (head);
}

/*
 * Sort entries using a custom comparison function and return the new head
 * of the list.
 */
struct mtree_entry *
mtree_entry_sort(struct mtree_entry *head, mtree_entry_compare_fn cmp)
{
	return sort_entries(head, cmp);
}

/*
 * Sort entries in path order and return the new head of the list.
 */
struct mtree_entry *
mtree_entry_sort_path(struct mtree_entry *head)
{
	return sort_entries(head, path_cmp);
}

/*
 * Calculate cksum and digests and store them in the given entry, setting
 * the selected keywords.
 *
 * Digests are the wanted mtree_digest types.
 *
 * If some digest type is unavailable or the calculation fails, the
 * respective keywords are unset.
 */
static void
set_checksums(struct mtree_entry *entry, int digests, uint64_t keywords)
{
	struct mtree_cksum	*cksum;
	struct mtree_digest	*digest;
	unsigned char		 buf[16 * 1024];
	int			 fd;
	int			 n;

	/*
	 * Unset everything first to simplify error handling. The keywords
	 * will be set back once the checksums are calculated.
	 */
	if (keywords & MTREE_KEYWORD_CKSUM)
		CLR_KEYWORD(entry, MTREE_KEYWORD_CKSUM);
	if (digests & MTREE_DIGEST_MD5)
		SET_KEYWORD_STR(entry, entry->data.md5digest,
		    NULL,
		    MTREE_KEYWORD_MASK_MD5);
	if (digests & MTREE_DIGEST_SHA1)
		SET_KEYWORD_STR(entry, entry->data.sha1digest,
		    NULL,
		    MTREE_KEYWORD_MASK_SHA1);
	if (digests & MTREE_DIGEST_SHA256)
		SET_KEYWORD_STR(entry, entry->data.sha256digest,
		    NULL,
		    MTREE_KEYWORD_MASK_SHA256);
	if (digests & MTREE_DIGEST_SHA384)
		SET_KEYWORD_STR(entry, entry->data.sha384digest,
		    NULL,
		    MTREE_KEYWORD_MASK_SHA384);
	if (digests & MTREE_DIGEST_SHA512)
		SET_KEYWORD_STR(entry, entry->data.sha512digest,
		    NULL,
		    MTREE_KEYWORD_MASK_SHA512);

	/* Remove unavailable digests. */
	digests &= mtree_digest_get_available_types();
	if ((keywords & MTREE_KEYWORD_CKSUM) == 0 && digests == 0)
		return;

	if (entry->orig != NULL)
		fd = open(entry->orig, O_RDONLY);
	else
		fd = open(entry->path, O_RDONLY);
	if (fd == -1)
		return;
	cksum  = NULL;
	digest = NULL;
	if (keywords & MTREE_KEYWORD_CKSUM) {
		cksum = mtree_cksum_create(MTREE_CKSUM_DEFAULT_INIT);
		if (cksum == NULL)
			goto end;
	}
	if (digests != 0) {
		digest = mtree_digest_create(digests);
		if (digest == NULL)
			goto end;
	}

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		if (cksum != NULL)
			mtree_cksum_update(cksum, buf, n);
		if (digest != NULL)
			mtree_digest_update(digest, buf, n);
	}
	if (n == 0) {
		/*
		 * Set the keywords and their values once the calculation
		 * has succeeded.
		 */
		if (keywords & MTREE_KEYWORD_CKSUM)
			mtree_entry_set_cksum(entry,
			    mtree_cksum_get_result(cksum));
		if (digests & MTREE_DIGEST_MD5)
			mtree_entry_set_md5digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_MD5),
			    keywords);
		if (digests & MTREE_DIGEST_SHA1)
			mtree_entry_set_sha1digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_SHA1),
			    keywords);
		if (digests & MTREE_DIGEST_SHA256)
			mtree_entry_set_sha256digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_SHA256),
			    keywords);
		if (digests & MTREE_DIGEST_SHA384)
			mtree_entry_set_sha384digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_SHA384),
			    keywords);
		if (digests & MTREE_DIGEST_SHA512)
			mtree_entry_set_sha512digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_SHA512),
			    keywords);
		if (digests & MTREE_DIGEST_RMD160)
			mtree_entry_set_rmd160digest(entry,
			    mtree_digest_get_result(digest, MTREE_DIGEST_RMD160),
			    keywords);
	}
end:
	if (cksum != NULL)
		mtree_cksum_free(cksum);
	if (digest != NULL)
		mtree_digest_free(digest);
	close(fd);
}

/*
 * Add or remove the requested keywords.
 *
 * Keyword values are read from the supplied stat or from the file system.
 * The overwrite argument indicates whether to overwrite digests.
 */
static void
set_keywords(struct mtree_entry *entry, const struct stat *st, uint64_t kset,
    uint64_t kclr, int overwrite)
{
	char	*s;
	int	 digests;

#define TRY_CLR_KEYWORD(k)	  if ((kclr & (k)) == (k)) CLR_KEYWORD(entry, k)
#define TRY_CLR_KEYWORD_STR(p, k) if ((kclr & (k)) == (k)) CLR_KEYWORD_STR(entry, p, k)

	/*
	 * Set/unset keywords that don't take a value.
	 */
	if (kset & MTREE_KEYWORD_IGNORE)
		SET_KEYWORD(entry, MTREE_KEYWORD_IGNORE);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_IGNORE);
	if (kset & MTREE_KEYWORD_NOCHANGE)
		SET_KEYWORD(entry, MTREE_KEYWORD_NOCHANGE);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_NOCHANGE);
	if (kset & MTREE_KEYWORD_OPTIONAL)
		SET_KEYWORD(entry, MTREE_KEYWORD_OPTIONAL);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_OPTIONAL);

	/*
	 * Unset kclr keywords that are not read from the file system.
	 */
	TRY_CLR_KEYWORD_STR(entry->data.contents, MTREE_KEYWORD_CONTENTS);
	TRY_CLR_KEYWORD_STR(entry->data.tags, MTREE_KEYWORD_TAGS);

	/*
	 * Set/unset stat(2) keywords.
	 *
	 * Use setters for the more complicated types. For simple values and
	 * strings, use the macros directly, mainly to avoid copying the
	 * strings.
	 */
	if (kset & MTREE_KEYWORD_TYPE)
		mtree_entry_set_type(entry,
		    mtree_entry_type_from_mode(st->st_mode));
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_TYPE);

	if (kset & MTREE_KEYWORD_DEVICE) {
#ifdef HAVE_STRUCT_STAT_ST_RDEV
		mtree_entry_set_device_number(entry, st->st_rdev);
#else
		mtree_entry_set_device(entry, NULL);
#endif
	} else if (kclr & MTREE_KEYWORD_DEVICE)
		mtree_entry_set_device(entry, NULL);

	if (kset & MTREE_KEYWORD_RESDEVICE)
		mtree_entry_set_resdevice_number(entry, st->st_dev);
	else if (kclr & MTREE_KEYWORD_RESDEVICE)
		mtree_entry_set_resdevice(entry, NULL);

	if (kset & MTREE_KEYWORD_FLAGS) {
#if defined(HAVE_FFLAGSTOSTR) && defined(HAVE_STRUCT_STAT_ST_FLAGS)
		s = fflagstostr(st->st_flags);
		if (s != NULL) {
			/*
			 * fflagstostr(3) returns a zero-length string when no
			 * flags are set; mtree uses the string "none" instead.
			 */
			if (*s != '\0')
				SET_KEYWORD_STR(entry, entry->data.flags, s,
				    MTREE_KEYWORD_FLAGS);
			else {
				SET_KEYWORD_DUP(entry, entry->data.flags, "none",
				    MTREE_KEYWORD_FLAGS);
				free(s);
			}
		} else
			CLR_KEYWORD_STR(entry, entry->data.flags,
			    MTREE_KEYWORD_FLAGS);
#else
		CLR_KEYWORD_STR(entry, entry->data.flags,
		    MTREE_KEYWORD_FLAGS);
#endif
	} else
		TRY_CLR_KEYWORD_STR(entry->data.flags, MTREE_KEYWORD_FLAGS);

	if (kset & MTREE_KEYWORD_GID)
		SET_KEYWORD_VAL(entry, entry->data.st_gid, st->st_gid,
		    MTREE_KEYWORD_GID);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_GID);
	if (kset & MTREE_KEYWORD_INODE)
		SET_KEYWORD_VAL(entry, entry->data.st_ino, st->st_ino,
		    MTREE_KEYWORD_INODE);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_INODE);
	if (kset & MTREE_KEYWORD_MODE) {
		/* Use setter to extract the mode part first. */
		mtree_entry_set_mode(entry, st->st_mode);
	} else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_MODE);
	if (kset & MTREE_KEYWORD_NLINK)
		SET_KEYWORD_VAL(entry, entry->data.st_nlink, st->st_nlink,
		    MTREE_KEYWORD_NLINK);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_NLINK);
	if (kset & MTREE_KEYWORD_SIZE)
		SET_KEYWORD_VAL(entry, entry->data.st_size, st->st_size,
		    MTREE_KEYWORD_SIZE);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_SIZE);

	if (kset & MTREE_KEYWORD_TIME) {
#ifdef HAVE_STRUCT_STAT_ST_MTIM
		entry->data.st_mtim.tv_sec  = st->st_mtim.tv_sec;
		entry->data.st_mtim.tv_nsec = st->st_mtim.tv_nsec;
#else
#ifdef HAVE_STRUCT_STAT_ST_MTIME
		entry->data.st_mtim.tv_sec  = st->st_mtime;
#else
		entry->data.st_mtim.tv_sec  = 0;
#endif
		entry->data.st_mtim.tv_nsec = 0;
#endif
		SET_KEYWORD(entry, MTREE_KEYWORD_TIME);
	} else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_TIME);

	if (kset & MTREE_KEYWORD_UID)
		SET_KEYWORD_VAL(entry, entry->data.st_uid, st->st_uid,
		    MTREE_KEYWORD_UID);
	else
		TRY_CLR_KEYWORD(MTREE_KEYWORD_UID);

	/*
	 * Set/unset non-stat keywords.
	 */
	if (kset & MTREE_KEYWORD_GNAME) {
		s = mtree_gname_from_gid(st->st_gid);
		if (s != NULL)
			SET_KEYWORD_STR(entry, entry->data.gname, s,
			    MTREE_KEYWORD_GNAME);
		else
			CLR_KEYWORD_STR(entry, entry->data.gname,
			    MTREE_KEYWORD_GNAME);
	} else
		TRY_CLR_KEYWORD_STR(entry->data.gname, MTREE_KEYWORD_GNAME);

	if (kset & MTREE_KEYWORD_LINK) {
		if (entry->orig != NULL)
			s = mtree_readlink(entry->orig);
		else
			s = mtree_readlink(entry->path);
		if (s != NULL)
			SET_KEYWORD_STR(entry, entry->data.link, s,
			    MTREE_KEYWORD_LINK);
		else
			CLR_KEYWORD_STR(entry, entry->data.link,
			    MTREE_KEYWORD_LINK);
	} else
		TRY_CLR_KEYWORD_STR(entry->data.link, MTREE_KEYWORD_LINK);

	if (kset & MTREE_KEYWORD_UNAME) {
		s = mtree_uname_from_uid(st->st_uid);
		if (s != NULL)
			SET_KEYWORD_STR(entry, entry->data.uname, s,
			    MTREE_KEYWORD_UNAME);
		else
			CLR_KEYWORD_STR(entry, entry->data.uname,
			    MTREE_KEYWORD_UNAME);
	} else
		TRY_CLR_KEYWORD_STR(entry->data.uname, MTREE_KEYWORD_UNAME);

	/*
	 * Set/unset cksum and digests.
	 *
	 * These are calculated at the same time, avoiding multiple read(2)s.
	 */
	TRY_CLR_KEYWORD(MTREE_KEYWORD_CKSUM);

	digests = 0;
	if (kset & MTREE_KEYWORD_MASK_MD5) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_MD5);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_MD5)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_MD5);
		else
			digests |= MTREE_DIGEST_MD5;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.md5digest,
		    MTREE_KEYWORD_MASK_MD5);
	if (kset & MTREE_KEYWORD_MASK_SHA1) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_SHA1);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_SHA1)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_SHA1);
		else
			digests |= MTREE_DIGEST_SHA1;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.sha1digest,
		    MTREE_KEYWORD_MASK_SHA1);
	if (kset & MTREE_KEYWORD_MASK_SHA256) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_SHA256);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_SHA256)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_SHA256);
		else
			digests |= MTREE_DIGEST_SHA256;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.sha256digest,
		    MTREE_KEYWORD_MASK_SHA256);
	if (kset & MTREE_KEYWORD_MASK_SHA384) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_SHA384);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_SHA384)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_SHA384);
		else
			digests |= MTREE_DIGEST_SHA384;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.sha384digest,
		    MTREE_KEYWORD_MASK_SHA384);
	if (kset & MTREE_KEYWORD_MASK_SHA512) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_SHA512);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_SHA512)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_SHA512);
		else
			digests |= MTREE_DIGEST_SHA512;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.sha512digest,
		    MTREE_KEYWORD_MASK_SHA512);
	if (kset & MTREE_KEYWORD_MASK_RMD160) {
		CLR_KEYWORD(entry, kclr & MTREE_KEYWORD_MASK_RMD160);
		if (!overwrite && entry->data.keywords & MTREE_KEYWORD_MASK_RMD160)
			SET_KEYWORD(entry, kset & MTREE_KEYWORD_MASK_RMD160);
		else
			digests |= MTREE_DIGEST_RMD160;
	} else
		TRY_CLR_KEYWORD_STR(entry->data.rmd160digest,
		    MTREE_KEYWORD_MASK_RMD160);

	if (digests != 0) {
		uint64_t mask = MTREE_KEYWORD_CKSUM | MTREE_KEYWORD_MASK_DIGEST;
		set_checksums(entry, digests,
		    (kset & mask) |
		    (entry->data.keywords & mask));
	}
#undef TRY_CLR_KEYWORD
#undef TRY_CLR_KEYWORD_STR
}

/*
 * Add, remove keywords, or change the entry to include the given set
 * of keywords.
 *
 * Keyword values are read from the file system. This doesn't set
 * keywords that take arbitrary values, such as "contents" or "tags".
 */
void
mtree_entry_set_keywords(struct mtree_entry *entry, uint64_t keywords, int options)
{
	struct stat	st;
	uint64_t	kstat;
	uint64_t	kset;
	uint64_t	kclr;
	int		ret;

	assert(entry != NULL);

	if (options & MTREE_ENTRY_REMOVE_EXCLUDED)
		kclr = entry->data.keywords & ~keywords;
	else
		kclr = 0;
	if (options & MTREE_ENTRY_OVERWRITE)
		kset = keywords;
	else
		kset = keywords & ~entry->data.keywords;

	kstat = keywords & MTREE_KEYWORD_MASK_STAT;
	if ((options & MTREE_ENTRY_OVERWRITE) == 0)
		kstat &= ~entry->data.keywords;

	if (kset & MTREE_KEYWORD_MASK_STAT) {
		/*
		 * There is some stat field that should be updated.
		 *
		 * If lstat(2) fails, make sure to unset all stat keywords that
		 * should have been updated.
		 */
		if (entry->orig != NULL)
			ret = lstat(entry->orig, &st);
		else
			ret = lstat(entry->path, &st);
		if (ret == -1) {
			kclr |= kset & MTREE_KEYWORD_MASK_STAT;
			kset &= ~MTREE_KEYWORD_MASK_STAT;
		}
	}
	set_keywords(entry, &st, kset, kclr, options & MTREE_ENTRY_OVERWRITE);
}

/*
 * Add, remove keywords, or change the entry to include the given set
 * of keywords.
 *
 * This only works with MTREE_KEYWORD_MASK_STAT keywords and uses the
 * given stat structure.
 */
void
mtree_entry_set_keywords_stat(struct mtree_entry *entry, const struct stat *st,
    uint64_t keywords, int options)
{
	uint64_t kset;
	uint64_t kclr;

	assert(entry != NULL);
	assert(st != NULL);

	keywords &= MTREE_KEYWORD_MASK_STAT;

	if (options & MTREE_ENTRY_REMOVE_EXCLUDED)
		kclr = entry->data.keywords & MTREE_KEYWORD_MASK_STAT & ~keywords;
	else
		kclr = 0;
	if (options & MTREE_ENTRY_OVERWRITE)
		kset = keywords;
	else
		kset = keywords & ~entry->data.keywords;

	/* Overwrite is unused here. */
	set_keywords(entry, st, kset, kclr, 0);
}

/*
 * Copy a single keyword from one mtree_entry_data structure to another.
 */
static void
copy_keyword(struct mtree_entry_data *data, const struct mtree_entry_data *from,
    uint64_t keyword)
{

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		data->cksum = from->cksum;
		break;
	case MTREE_KEYWORD_CONTENTS:
		mtree_copy_string(&data->contents, from->contents);
		break;
	case MTREE_KEYWORD_DEVICE:
		if (from->device != NULL) {
			if (data->device != NULL)
				mtree_device_copy_data(data->device,
				    from->device);
			else
				data->device = mtree_device_copy(data->device);
		} else if (data->device != NULL) {
			mtree_device_free(data->device);
			data->device = NULL;
		}
		break;
	case MTREE_KEYWORD_FLAGS:
		mtree_copy_string(&data->flags, from->flags);
		break;
	case MTREE_KEYWORD_GID:
		data->st_gid = from->st_gid;
		break;
	case MTREE_KEYWORD_GNAME:
		mtree_copy_string(&data->gname, from->gname);
		break;
	case MTREE_KEYWORD_IGNORE:
		/* No value */
		break;
	case MTREE_KEYWORD_INODE:
		data->st_ino = from->st_ino;
		break;
	case MTREE_KEYWORD_LINK:
		mtree_copy_string(&data->link, from->link);
		break;
	case MTREE_KEYWORD_MD5:
	case MTREE_KEYWORD_MD5DIGEST:
		mtree_copy_string(&data->md5digest, from->md5digest);
		break;
	case MTREE_KEYWORD_MODE:
		data->st_mode = from->st_mode & MODE_MASK;
		break;
	case MTREE_KEYWORD_NLINK:
		data->st_nlink = from->st_nlink;
		break;
	case MTREE_KEYWORD_NOCHANGE:
	case MTREE_KEYWORD_OPTIONAL:
		/* No value */
		break;
	case MTREE_KEYWORD_RESDEVICE:
		if (from->resdevice != NULL) {
			if (data->resdevice != NULL)
				mtree_device_copy_data(data->resdevice,
				    from->resdevice);
			else
				data->resdevice = mtree_device_copy(data->resdevice);
		} else if (data->resdevice != NULL) {
			mtree_device_free(data->resdevice);
			data->resdevice = NULL;
		}
		break;
	case MTREE_KEYWORD_RIPEMD160DIGEST:
	case MTREE_KEYWORD_RMD160:
	case MTREE_KEYWORD_RMD160DIGEST:
		mtree_copy_string(&data->rmd160digest, from->rmd160digest);
		break;
	case MTREE_KEYWORD_SHA1:
	case MTREE_KEYWORD_SHA1DIGEST:
		mtree_copy_string(&data->sha1digest, from->sha1digest);
		break;
	case MTREE_KEYWORD_SHA256:
	case MTREE_KEYWORD_SHA256DIGEST:
		mtree_copy_string(&data->sha256digest, from->sha256digest);
		break;
	case MTREE_KEYWORD_SHA384:
	case MTREE_KEYWORD_SHA384DIGEST:
		mtree_copy_string(&data->sha384digest, from->sha384digest);
		break;
	case MTREE_KEYWORD_SHA512:
	case MTREE_KEYWORD_SHA512DIGEST:
		mtree_copy_string(&data->sha512digest, from->sha512digest);
		break;
	case MTREE_KEYWORD_SIZE:
		data->st_size = from->st_size;
		break;
	case MTREE_KEYWORD_TAGS:
		mtree_copy_string(&data->tags, from->tags);
		break;
	case MTREE_KEYWORD_TIME:
		data->st_mtim = from->st_mtim;
		break;
	case MTREE_KEYWORD_TYPE:
		data->type = from->type;
		break;
	case MTREE_KEYWORD_UID:
		data->st_uid = from->st_uid;
		break;
	case MTREE_KEYWORD_UNAME:
		mtree_copy_string(&data->uname, from->uname);
		break;
	default:
		/* Invalid keyword */
		return;
	}

	data->keywords |= keyword;
}

/*
 * Copy the given entry.
 *
 * The created copy will be a standalone entry with both the prev and next
 * pointers set to NULL.
 */
struct mtree_entry *
mtree_entry_copy(const struct mtree_entry *entry)
{
	struct mtree_entry *copy;

	assert(entry != NULL);

	copy = mtree_entry_create_empty();
	if (copy == NULL)
		return (NULL);
	if (entry->path != NULL &&
	    (copy->path = strdup(entry->path)) == NULL) {
		mtree_entry_free(copy);
		return (NULL);
	}
	if (entry->orig != NULL &&
	    (copy->orig = strdup(entry->orig)) == NULL) {
		mtree_entry_free(copy);
		return (NULL);
	}
	if (entry->name != NULL &&
	    (copy->name = strdup(entry->name)) == NULL) {
		mtree_entry_free(copy);
		return (NULL);
	}
	mtree_entry_data_copy_keywords(&copy->data, &entry->data,
	    entry->data.keywords, 1);
	return (copy);
}

/*
 * Copy list of entries, starting with the given entry and moving forward
 * in the list.
 */
struct mtree_entry *
mtree_entry_copy_all(const struct mtree_entry *start)
{
	struct mtree_entry *copy;
	struct mtree_entry *entries;

	entries = NULL;
	while (start != NULL) {
		copy = mtree_entry_copy(start);
		if (copy == NULL) {
			mtree_entry_free_all(entries);
			return (NULL);
		}
		entries = mtree_entry_prepend(entries, copy);
		start = start->next;
	}
	return (mtree_entry_reverse(entries));
}

/*
 * Copy keywords from one entry to another.
 *
 * Keywords that are already present in the target entry are not overwritten
 * unless overwrite is set to non-zero.
 */
void
mtree_entry_copy_keywords(struct mtree_entry *entry,
    const struct mtree_entry *from, uint64_t keywords, int overwrite)
{
	assert(entry != NULL);
	assert(from != NULL);

	mtree_entry_data_copy_keywords(&entry->data, &from->data, keywords,
	    overwrite);
}

void
mtree_entry_data_copy_keywords(struct mtree_entry_data *data,
    const struct mtree_entry_data *from, uint64_t keywords, int overwrite)
{
	int i;

	assert(data != NULL);
	assert(from != NULL);

	keywords &= from->keywords;
	if (overwrite == 0)
		keywords &= ~data->keywords;

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if ((keywords & mtree_keywords[i].keyword) == 0)
			continue;
		copy_keyword(data, from, mtree_keywords[i].keyword);
	}
}

/*
 * Merge entries.
 */
static struct mtree_entry *
merge_entries(struct mtree_entry *merged, struct mtree_entry *head,
    int options, struct mtree_entry **mismerged)
{
	struct mtree_entry	*list;
	struct mtree_entry	*entry, *next, *found;
	struct mtree_trie	*trie;

	trie = mtree_trie_create();
	if (trie == NULL)
		return (NULL);
	/*
	 * Use the list of already merged entries as the start of the list
	 * of fully merged entries. Its entries still need to be added to the
	 * lookup trie as there may be their duplicates in the non-merged lists.
	 */
	list = merged;
	while (merged != NULL) {
		if (mtree_trie_insert(trie, merged->path, merged) == -1) {
			mtree_trie_free(trie, NULL);
			return (NULL);
		}
		merged = merged->next;
	}

	entry = head;
	while (entry != NULL) {
		next = entry->next;
		found = mtree_trie_find(trie, entry->path);
		if (found != NULL) {
			if ((options & MTREE_ENTRY_MERGE_DIFFERENT_TYPES) == 0 &&
			    found->data.type != entry->data.type) {
				if (mismerged != NULL) {
					/*
					 * Create a copy of the two entries that
					 * are not mergeable.
					 */
					list = mtree_entry_copy(entry);
					if (list != NULL) {
						entry = mtree_entry_copy(found);
						if (entry != NULL)
							list = mtree_entry_prepend(
							    list, entry);
						else {
							mtree_entry_free_all(list);
							list = NULL;
						}
					}
					*mismerged = list;
				}
				errno = EEXIST;
				mtree_trie_free(trie, NULL);
				return (NULL);
			}
			/*
			 * Copy keywords from the current entry to the previous
			 * one and remove the current entry.
			 *
			 * Skip error checking here as there is no way to recover.
			 */
			mtree_entry_data_copy_keywords(&found->data,
			    &entry->data, entry->data.keywords, 1);

			head = mtree_entry_unlink(head, entry);
			mtree_entry_free(entry);
		} else if (mtree_trie_insert(trie, entry->path, entry) == -1) {
			mtree_trie_free(trie, NULL);
			return (NULL);
		}
		entry = next;
	}
	mtree_trie_free(trie, NULL);

	return (mtree_entry_append(list, head));
}

/*
 * Merge entries according to the given options.
 *
 * If some two entries fail to be merged, they are stored in mismerged.
 */
struct mtree_entry *
mtree_entry_merge(struct mtree_entry *head1,
    struct mtree_entry *head2, int options, struct mtree_entry **mismerged)
{
	struct mtree_entry *merged;

	merged = merge_entries(NULL, head1, options, mismerged);
	if (head1 != NULL && merged == NULL)
		return (NULL);
	return (merge_entries(head1, head2, options, mismerged));
}

/*
 * Merge entries in two lists, assuming the first list is already merged.
 */
struct mtree_entry *
mtree_entry_merge_fast(struct mtree_entry *head1_merged,
    struct mtree_entry *head2, int options, struct mtree_entry **mismerged)
{

	return (merge_entries(head1_merged, head2, options, mismerged));
}

/*
 * Count number of entries in the linked list starting from the given entry
 * and counting forward.
 */
size_t
mtree_entry_count(struct mtree_entry *start)
{
	size_t count;

	count = 0;
	while (start != NULL) {
		count++;
		start = start->next;
	}
	return (count);
}

/*
 * Find entry with the given path using linear search.
 */
struct mtree_entry *
mtree_entry_find(struct mtree_entry *start, const char *path)
{

	assert(path != NULL);

	while (start != NULL) {
		if (strcmp(start->path, path) == 0)
			return (start);

		start = start->next;
	}
	return (NULL);
}

/*
 * Find entry with the given path using linear search.
 */
struct mtree_entry *
mtree_entry_find_prefix(struct mtree_entry *start, const char *path_prefix)
{
	size_t len;

	assert(path_prefix != NULL);

	len = strlen(path_prefix);
	while (start != NULL) {
		if (strncmp(start->path, path_prefix, len) == 0)
			return (start);

		start = start->next;
	}
	return (NULL);
}

/*
 * Append entry at the end of the list and return the new head of the list.
 */
struct mtree_entry *
mtree_entry_append(struct mtree_entry *head, struct mtree_entry *entry)
{
	struct mtree_entry *last;

	if (entry == NULL)
		return (head);
	if (head == NULL) {
		entry->prev = NULL;
		return (entry);
	}
	last = mtree_entry_get_last(head);
	last->next  = entry;
	entry->prev = last;

	return (head);
}

/*
 * Prepend entry before the given head element of the list and return the new
 * head of the list.
 */
struct mtree_entry *
mtree_entry_prepend(struct mtree_entry *head, struct mtree_entry *entry)
{

	if (entry == NULL)
		return (head);

	entry->next = head;
	if (head != NULL) {
		entry->prev = head->prev;
		if (head->prev != NULL)
			head->prev->next = entry;
		head->prev = entry;
	} else
		entry->prev = NULL;

	return (entry);
}

/*
 * Insert entry before the giving sibling and return the new head of
 * the list.
 *
 * If sibling is NULL, prepend before the first element of the list.
 */
struct mtree_entry *
mtree_entry_insert_before(struct mtree_entry *head, struct mtree_entry *sibling,
    struct mtree_entry *entry)
{

	if (entry == NULL)
		return (head);
	if (sibling == NULL)
		return (mtree_entry_prepend(head, entry));

	entry->next = sibling;
	entry->prev = sibling->prev;
	sibling->prev = entry;
	if (entry->prev == NULL)
		return (entry);

	entry->prev->next = entry;
	return (head);
}

/*
 * Insert entry after the giving sibling and return the new head of
 * the list.
 *
 * If sibling is NULL, append to the end of the list.
 */
struct mtree_entry *
mtree_entry_insert_after(struct mtree_entry *head, struct mtree_entry *sibling,
    struct mtree_entry *entry)
{

	if (entry == NULL)
		return (head);
	if (sibling == NULL)
		return (mtree_entry_append(head, entry));

	entry->prev = sibling;
	entry->next = sibling->next;
	sibling->next = entry;
	if (entry->next != NULL)
		entry->next->prev = entry;

	return (head);
}

/*
 * Reverse order of entries in the list and return the new head of the list.
 */
struct mtree_entry *
mtree_entry_reverse(struct mtree_entry *head)
{
	struct mtree_entry *entry;
	struct mtree_entry *last;

	entry = head;
	last = NULL;
	while (entry != NULL) {
		last = entry;
		entry = last->next;
		last->next = last->prev;
		last->prev = entry;
	}
	return (last);
}

/*
 * Unlink the given entry from the list and return the new head of the list.
 *
 * The entry become a standalone entry with prev and next set to NULL.
 */
struct mtree_entry *
mtree_entry_unlink(struct mtree_entry *head, struct mtree_entry *entry)
{

	assert(head != NULL);
	assert(entry != NULL);

	if (entry->next != NULL)
		entry->next->prev = entry->prev;
	if (entry->prev != NULL)
		entry->prev->next = entry->next;
	if (entry == head)
		head = head->next;

	entry->prev = entry->next = NULL;

	return (head);
}

/*
 * Get file name of the given entry.
 */
const char *
mtree_entry_get_name(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->name);
}

/*
 * Get file path of the given entry.
 */
const char *
mtree_entry_get_path(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->path);
}

/*
 * Get file path of the given entry without the last component.
 */
const char *
mtree_entry_get_dirname(struct mtree_entry *entry)
{
	const char *slash;

	assert(entry != NULL);

	if (entry->dirname == NULL) {
		if ((slash = strrchr(entry->path, '/')) != NULL)
			entry->dirname = strndup(entry->path,
			    slash - entry->path);
		else
			entry->dirname = strdup(".");
	}
	return (entry->dirname);
}

/*
 * Get keywords of the given entry.
 */
uint64_t
mtree_entry_get_keywords(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.keywords);
}

/*
 * Find the first (head) entry in a list which includes the given entry.
 */
struct mtree_entry *
mtree_entry_get_first(struct mtree_entry *entry)
{

	if (entry != NULL) {
		while (entry->prev != NULL)
			entry = entry->prev;
	}
	return (entry);
}

/*
 * Find the last entry in a list which includes the given entry.
 */
struct mtree_entry *
mtree_entry_get_last(struct mtree_entry *entry)
{

	if (entry != NULL) {
		while (entry->next != NULL)
			entry = entry->next;
	}
	return (entry);
}

/*
 * Get the previous entry in the list.
 */
struct mtree_entry *
mtree_entry_get_previous(struct mtree_entry *entry)
{

	return (entry ? entry->prev : NULL);
}

/*
 * Get the next entry in the list.
 */
struct mtree_entry *
mtree_entry_get_next(struct mtree_entry *entry)
{

	return (entry ? entry->next : NULL);
}

/*
 * Keyword value getters.
 */
uint32_t
mtree_entry_get_cksum(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_CKSUM) == 0)
		return (0);
	return (entry->data.cksum);
}

const char *
mtree_entry_get_contents(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.contents);
}

struct mtree_device *
mtree_entry_get_device(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.device);
}

const char *
mtree_entry_get_flags(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.flags);
}

int64_t
mtree_entry_get_gid(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_GID) == 0)
		return (0);
	return (entry->data.st_gid);
}

const char *
mtree_entry_get_gname(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.gname);
}

uint64_t
mtree_entry_get_inode(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_INODE) == 0)
		return (0);
	return (entry->data.st_ino);
}

const char *
mtree_entry_get_link(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.link);
}

const char *
mtree_entry_get_md5digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.md5digest);
}

int
mtree_entry_get_mode(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_MODE) == 0)
		return (0);
	return (entry->data.st_mode);
}

int64_t
mtree_entry_get_nlink(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_NLINK) == 0)
		return (0);
	return (entry->data.st_nlink);
}

struct mtree_device *
mtree_entry_get_resdevice(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.resdevice);
}

const char *
mtree_entry_get_rmd160digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.rmd160digest);
}

const char *
mtree_entry_get_sha1digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha1digest);
}

const char *
mtree_entry_get_sha256digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha256digest);
}


const char *
mtree_entry_get_sha384digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha384digest);
}

const char *
mtree_entry_get_sha512digest(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha512digest);
}

int64_t
mtree_entry_get_size(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_SIZE) == 0)
		return (0);
	return (entry->data.st_size);
}

const char *
mtree_entry_get_tags(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.tags);
}

struct mtree_timespec *
mtree_entry_get_time(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_TIME) == 0)
		return (NULL);
	return (&entry->data.st_mtim);
}

mtree_entry_type
mtree_entry_get_type(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_TYPE) == 0)
		return (MTREE_ENTRY_UNKNOWN);
	return (entry->data.type);
}

int64_t
mtree_entry_get_uid(struct mtree_entry *entry)
{

	assert(entry != NULL);

	if ((entry->data.keywords & MTREE_KEYWORD_UID) == 0)
		return (0);
	return (entry->data.st_uid);
}

const char *
mtree_entry_get_uname(struct mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.uname);
}

/*
 * Keyword value setters.
 */
void
mtree_entry_set_cksum(struct mtree_entry *entry, uint32_t cksum)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.cksum,
	    cksum,
	    MTREE_KEYWORD_CKSUM);
}

void
mtree_entry_set_contents(struct mtree_entry *entry, const char *contents)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.contents,
	    contents,
	    MTREE_KEYWORD_CONTENTS);
}

void
mtree_entry_set_device(struct mtree_entry *entry, const struct mtree_device *dev)
{

	assert(entry != NULL);

	if (dev != NULL) {
		if (entry->data.device == NULL) {
			entry->data.device = mtree_device_copy(dev);
			if (entry->data.device == NULL)
				return;
		} else
			mtree_device_copy_data(entry->data.device, dev);

		SET_KEYWORD(entry, MTREE_KEYWORD_DEVICE);
	} else {
		if (entry->data.device != NULL) {
			mtree_device_free(entry->data.device);
			entry->data.device = NULL;
		}
		CLR_KEYWORD(entry, MTREE_KEYWORD_DEVICE);
	}
}

void
mtree_entry_set_device_number(struct mtree_entry *entry, dev_t number)
{

	assert(entry != NULL);

	if (entry->data.device == NULL) {
		entry->data.device = mtree_device_create();
		if (entry->data.device == NULL)
			return;
	}
	mtree_device_reset(entry->data.device);
	mtree_device_set_value(entry->data.device, MTREE_DEVICE_FIELD_NUMBER,
	    number);

	SET_KEYWORD(entry, MTREE_KEYWORD_DEVICE);
}

void
mtree_entry_set_flags(struct mtree_entry *entry, const char *flags)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.flags,
	    flags,
	    MTREE_KEYWORD_FLAGS);
}

void
mtree_entry_set_gid(struct mtree_entry *entry, int64_t gid)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_gid,
	    gid,
	    MTREE_KEYWORD_GID);
}

void
mtree_entry_set_gname(struct mtree_entry *entry, const char *gname)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.gname,
	    gname,
	    MTREE_KEYWORD_GNAME);
}

void
mtree_entry_set_inode(struct mtree_entry *entry, uint64_t ino)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_ino,
	    ino,
	    MTREE_KEYWORD_INODE);
}

void
mtree_entry_set_link(struct mtree_entry *entry, const char *link)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.link,
	    link,
	    MTREE_KEYWORD_LINK);
}

void
mtree_entry_set_md5digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_MD5);
	SET_KEYWORD_DUP(entry, entry->data.md5digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_MD5);
}

void
mtree_entry_set_mode(struct mtree_entry *entry, int mode)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_mode,
	    mode & MODE_MASK,
	    MTREE_KEYWORD_MODE);
}

void
mtree_entry_set_nlink(struct mtree_entry *entry, int64_t nlink)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_nlink,
	    nlink,
	    MTREE_KEYWORD_NLINK);
}

void
mtree_entry_set_resdevice(struct mtree_entry *entry, const struct mtree_device *dev)
{

	assert(entry != NULL);

	if (dev != NULL) {
		if (entry->data.resdevice == NULL) {
			entry->data.resdevice = mtree_device_copy(dev);
			if (entry->data.resdevice == NULL)
				return;
		} else
			mtree_device_copy_data(entry->data.resdevice, dev);

		SET_KEYWORD(entry, MTREE_KEYWORD_RESDEVICE);
	} else {
		if (entry->data.resdevice != NULL) {
			mtree_device_free(entry->data.resdevice);
			entry->data.resdevice = NULL;
		}
		CLR_KEYWORD(entry, MTREE_KEYWORD_RESDEVICE);
	}
}

void
mtree_entry_set_resdevice_number(struct mtree_entry *entry, dev_t number)
{

	assert(entry != NULL);

	if (entry->data.resdevice == NULL) {
		entry->data.resdevice = mtree_device_create();
		if (entry->data.resdevice == NULL)
			return;
	}
	mtree_device_reset(entry->data.resdevice);
	mtree_device_set_value(entry->data.resdevice, MTREE_DEVICE_FIELD_NUMBER,
	    number);

	SET_KEYWORD(entry, MTREE_KEYWORD_RESDEVICE);
}

void
mtree_entry_set_rmd160digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_RMD160);
	SET_KEYWORD_DUP(entry, entry->data.rmd160digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_RMD160);
}

void
mtree_entry_set_sha1digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_SHA1);
	SET_KEYWORD_DUP(entry, entry->data.sha1digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_SHA1);
}

void
mtree_entry_set_sha256digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_SHA256);
	SET_KEYWORD_DUP(entry, entry->data.sha256digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_SHA256);
}

void
mtree_entry_set_sha384digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_SHA384);
	SET_KEYWORD_DUP(entry, entry->data.sha384digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_SHA384);
}

void
mtree_entry_set_sha512digest(struct mtree_entry *entry, const char *digest,
    uint64_t keywords)
{

	assert(entry != NULL);

	CLR_KEYWORD(entry, MTREE_KEYWORD_MASK_SHA512);
	SET_KEYWORD_DUP(entry, entry->data.sha512digest,
	    digest,
	    keywords & MTREE_KEYWORD_MASK_SHA512);
}

void
mtree_entry_set_size(struct mtree_entry *entry, int64_t size)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_size,
	    size,
	    MTREE_KEYWORD_SIZE);
}

void
mtree_entry_set_tags(struct mtree_entry *entry, const char *tags)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.tags,
	    tags,
	    MTREE_KEYWORD_TAGS);
}

void
mtree_entry_set_time(struct mtree_entry *entry, const struct mtree_timespec *ts)
{

	assert(entry != NULL);

	entry->data.st_mtim.tv_sec  = ts->tv_sec;
	entry->data.st_mtim.tv_nsec = ts->tv_nsec;

	SET_KEYWORD(entry, MTREE_KEYWORD_TIME);
}

void
mtree_entry_set_type(struct mtree_entry *entry, mtree_entry_type type)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.type,
	    type,
	    MTREE_KEYWORD_TYPE);
}

void
mtree_entry_set_uid(struct mtree_entry *entry, int64_t uid)
{

	assert(entry != NULL);

	SET_KEYWORD_VAL(entry, entry->data.st_uid,
	    uid,
	    MTREE_KEYWORD_UID);
}

void
mtree_entry_set_uname(struct mtree_entry *entry, const char *uname)
{

	assert(entry != NULL);

	SET_KEYWORD_DUP(entry, entry->data.uname,
	    uname,
	    MTREE_KEYWORD_UNAME);
}
