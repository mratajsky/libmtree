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
#include <sys/time.h>

#include <assert.h>
#include <fts.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mtree.h"
#include "mtree_file.h"
#include "mtree_private.h"

/* Mask of all keywords read by stat(2) */
#define MTREE_KEYWORD_MASK_STAT	(MTREE_KEYWORD_DEVICE |	\
				 MTREE_KEYWORD_FLAGS |	\
				 MTREE_KEYWORD_GID |	\
				 MTREE_KEYWORD_GNAME |	\
				 MTREE_KEYWORD_INODE |	\
				 MTREE_KEYWORD_LINK |	\
				 MTREE_KEYWORD_MODE |	\
				 MTREE_KEYWORD_NLINK |	\
				 MTREE_KEYWORD_SIZE |	\
				 MTREE_KEYWORD_TIME |	\
				 MTREE_KEYWORD_TYPE |	\
				 MTREE_KEYWORD_UID |	\
				 MTREE_KEYWORD_UNAME)

mtree_entry *
mtree_entry_create(void)
{
	mtree_entry *entry;

	entry = calloc(1, sizeof(mtree_entry));

	return (entry);
}

mtree_entry *
mtree_entry_create_from_ftsent(FTSENT *ftsent, long keywords)
{
	mtree_entry *entry;

	assert (ftsent != NULL);

	entry = mtree_entry_create();
	if (entry == NULL)
		return (NULL);

	entry->path = strndup(ftsent->fts_path, ftsent->fts_pathlen);
	entry->name = strndup(ftsent->fts_name, ftsent->fts_namelen);

	mtree_entry_set_keywords(entry, keywords, 1);

	return (entry);
}

void
mtree_entry_free(mtree_entry *entry)
{

	mtree_entry_free_data_items(&entry->data);
	free(entry->path);
	free(entry->name);
	free(entry);
}

void
mtree_entry_free_all(mtree_entry *entries)
{
	mtree_entry *next;

	while (entries != NULL) {
		next = entries->next;
		mtree_entry_free(entries);
		entries = next;
	}
}

void
mtree_entry_free_data_items(mtree_entry_data *data)
{

	if (data->device != NULL)
		mtree_device_free(data->device);

	free(data->contents);
	free(data->flags);
	free(data->gname);
	free(data->link);
	free(data->uname);
	free(data->md5digest);
	free(data->rmd160digest);
	free(data->sha1digest);
	free(data->sha256digest);
	free(data->sha384digest);
	free(data->sha512digest);
}

static int
compare_keyword(mtree_entry_data *data1, mtree_entry_data *data2, long keyword)
{

	if ((data1->keywords & keyword) != (data2->keywords & keyword))
		return (1);
	else if ((data1->keywords & keyword) == 0)
		return (0);

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		if (data1->cksum != data2->cksum)
			return (1);
		break;
	case MTREE_KEYWORD_CONTENTS:
		if (strcmp(data1->contents, data2->contents) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_FLAGS:
		if (strcmp(data1->flags, data2->flags) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_GID:
		if (data1->st_gid != data2->st_gid)
			return (1);
		break;
	case MTREE_KEYWORD_GNAME:
		if (strcmp(data1->gname, data2->gname) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_INODE:
		if (data1->st_ino != data2->st_ino)
			return (1);
		break;
	case MTREE_KEYWORD_LINK:
		if (strcmp(data1->link, data2->link) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_MD5:
	case MTREE_KEYWORD_MD5DIGEST:
		if (strcmp(data1->md5digest, data2->md5digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_MODE:
		if (data1->st_mode != data2->st_mode)
			return (1);
		break;
	case MTREE_KEYWORD_NLINK:
		if (data1->st_nlink != data2->st_nlink)
			return (1);
		break;
	case MTREE_KEYWORD_RIPEMD160DIGEST:
	case MTREE_KEYWORD_RMD160:
	case MTREE_KEYWORD_RMD160DIGEST:
		if (strcmp(data1->rmd160digest, data2->rmd160digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_SHA1:
	case MTREE_KEYWORD_SHA1DIGEST:
		if (strcmp(data1->sha1digest, data2->sha1digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_SHA256:
	case MTREE_KEYWORD_SHA256DIGEST:
		if (strcmp(data1->sha256digest, data2->sha256digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_SHA384:
	case MTREE_KEYWORD_SHA384DIGEST:
		if (strcmp(data1->sha384digest, data2->sha384digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_SHA512:
	case MTREE_KEYWORD_SHA512DIGEST:
		if (strcmp(data1->sha512digest, data2->sha512digest) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_SIZE:
		if (data1->st_size != data2->st_size)
			return (1);
		break;
	case MTREE_KEYWORD_TAGS:
		if (strcmp(data1->tags, data2->tags) != 0)
			return (1);
		break;
	case MTREE_KEYWORD_TIME:
		if (data1->st_mtim.tv_sec != data2->st_mtim.tv_sec ||
		    data1->st_mtim.tv_nsec != data2->st_mtim.tv_nsec)
			return (1);
		break;
	case MTREE_KEYWORD_TYPE:
		if (data1->type != data2->type)
			return (1);
		break;
	case MTREE_KEYWORD_UID:
		if (data1->st_uid != data2->st_uid)
			return (1);
		break;
	case MTREE_KEYWORD_UNAME:
		if (strcmp(data1->uname, data2->uname) != 0)
			return (1);
		break;
	}

	return (0);
}

int
mtree_entry_compare(mtree_entry *entry1, mtree_entry *entry2, long *diff)
{
	long differ;
	int ret;
	int i;

	assert(entry1 != NULL);
	assert(entry2 != NULL);

	/*
	 * Compare paths
	 */
	if (entry1->path != NULL && entry2->path != NULL) {
		ret = strcmp(entry1->path, entry2->path);
		if (ret != 0)
			return (ret);
	} else if (entry1->path != entry2->path) {
		/* One path is NULL */
		return (entry1->path == NULL ? -1 : 1);
	}

	/*
	 * Compare keywords, only the keywords that are present in both
	 * specs are compared
	 */
	differ = 0;
	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if ((entry1->data.keywords & mtree_keywords[i].keyword) == 0 ||
		    (entry2->data.keywords & mtree_keywords[i].keyword) == 0)
			continue;

		ret = compare_keyword(&entry1->data, &entry2->data,
		    mtree_keywords[i].keyword);
		if (ret != 0) {
			/*
			 * Build up a list of keywords with different values,
			 * but only when the user is interested
			 */
			if (diff == NULL)
				return (ret);
			differ |= mtree_keywords[i].keyword;
		}
	}

	if (differ) {
		*diff = differ;
		return (1);
	}
	return (0);
}

int
mtree_entry_compare_keyword(mtree_entry *entry1, mtree_entry *entry2, long keyword)
{

	assert(entry1 != NULL);
	assert(entry2 != NULL);

	return compare_keyword(&entry1->data, &entry2->data, keyword);
}

int
mtree_entry_data_compare_keyword(mtree_entry_data *data1, mtree_entry_data *data2,
    long keyword)
{

	assert(data1 != NULL);
	assert(data2 != NULL);

	return compare_keyword(data1, data2, keyword);
}

void
mtree_entry_set_keywords(mtree_entry *entry, long keywords, int overwrite)
{
	struct stat st;
	int ret;

	assert(entry != NULL);

	keywords &= MTREE_KEYWORD_MASK_ALL;

	/* Assign keyword that require stat(2) */
	if (keywords & MTREE_KEYWORD_MASK_STAT)
		if (stat(entry->path, &st) == -1)
			keywords &= ~MTREE_KEYWORD_MASK_STAT;

#define CAN_SET_KEYWORD(kw) \
	((keywords & (kw)) && (overwrite || (entry->data.keywords & (kw)) == 0))

	if (CAN_SET_KEYWORD(MTREE_KEYWORD_DEVICE)) {
		if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
			if (entry->data.device == NULL)
				entry->data.device = mtree_device_create();

			entry->data.device->format = MTREE_DEVICE_NATIVE;
			entry->data.device->number = st.st_rdev;
			entry->data.device->fields = MTREE_DEVICE_FIELD_NUMBER;
		} else
			keywords &= ~MTREE_KEYWORD_DEVICE;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_FLAGS)) {
		free(entry->data.flags);
#ifdef HAVE_FFLAGSTOSTR
		entry->data.flags = fflagstostr(st.st_flags);
		if (entry->data.flags != NULL &&
		    entry->data.flags[0] == '\0')
			mtree_copy_string(&entry->data.flags, "none");
#else
		entry->data.flags = NULL;
#endif
		if (entry->data.flags == NULL)
			keywords &= ~MTREE_KEYWORD_FLAGS;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_GNAME)) {
		free(entry->data.gname);
		entry->data.gname = mtree_get_uname(st.st_gid);
		if (entry->data.gname == NULL)
			keywords &= ~MTREE_KEYWORD_GNAME;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_GID))
		entry->data.st_gid = st.st_gid;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_INODE))
		entry->data.st_ino = st.st_ino;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_LINK)) {
		free(entry->data.link);
		if (S_ISLNK(st.st_mode))
			entry->data.link = mtree_get_link(entry->path);
		else
			entry->data.link = NULL;
		if (entry->data.link == NULL)
			keywords &= ~MTREE_KEYWORD_LINK;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MODE))
		entry->data.st_mode = st.st_mode & MTREE_ENTRY_MODE_MASK;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_NLINK))
		entry->data.st_nlink = st.st_nlink;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_SIZE))
		entry->data.st_size = st.st_size;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_TIME)) {
#ifdef HAVE_STRUCT_STAT_ST_MTIM
		entry->data.st_mtim = st.st_mtim;
#else
#ifdef HAVE_STRUCT_STAT_ST_MTIME
		entry->data.st_mtim.tv_sec  = st.st_mtime;
#else
		entry->data.st_mtim.tv_sec  = 0;
#endif
		entry->data.st_mtim.tv_nsec = 0;
#endif
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_TYPE)) {
		switch (st.st_mode & S_IFMT) {
		case S_IFREG:
			entry->data.type = MTREE_ENTRY_FILE;
			break;
		case S_IFDIR:
			entry->data.type = MTREE_ENTRY_DIR;
			break;
		case S_IFLNK:
			entry->data.type = MTREE_ENTRY_LINK;
			break;
		case S_IFBLK:
			entry->data.type = MTREE_ENTRY_BLOCK;
			break;
		case S_IFCHR:
			entry->data.type = MTREE_ENTRY_CHAR;
			break;
		case S_IFIFO:
			entry->data.type = MTREE_ENTRY_FIFO;
			break;
		case S_IFSOCK:
			entry->data.type = MTREE_ENTRY_SOCKET;
			break;
		}
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_UID))
		entry->data.st_uid = st.st_uid;
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_UNAME)) {
		free(entry->data.uname);
		entry->data.uname = mtree_get_uname(st.st_uid);
		if (entry->data.uname == NULL)
			keywords &= ~MTREE_KEYWORD_UNAME;
	}

	if (CAN_SET_KEYWORD(MTREE_KEYWORD_CKSUM)) {
		ret = mtree_digest_file_crc32(entry->path, &entry->data.cksum, NULL);
		if (ret != 0) {
			entry->data.cksum = 0;
			keywords &= ~MTREE_KEYWORD_CKSUM;
		}
	}

	/* Digests */
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_MD5)) {
		free(entry->data.md5digest);
		entry->data.md5digest = mtree_digest_file(MTREE_DIGEST_MD5,
		    entry->path);
		if (entry->data.md5digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_MD5;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_RMD160)) {
		free(entry->data.rmd160digest);
		entry->data.rmd160digest = mtree_digest_file(MTREE_DIGEST_RMD160,
		    entry->path);
		if (entry->data.rmd160digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_RMD160;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_SHA1)) {
		free(entry->data.sha1digest);
		entry->data.sha1digest = mtree_digest_file(MTREE_DIGEST_SHA1,
		    entry->path);
		if (entry->data.sha1digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_SHA1;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_SHA256)) {
		free(entry->data.sha256digest);
		entry->data.sha256digest = mtree_digest_file(MTREE_DIGEST_SHA256,
		    entry->path);
		if (entry->data.sha256digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_SHA256;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_SHA384)) {
		free(entry->data.sha384digest);
		entry->data.sha384digest = mtree_digest_file(MTREE_DIGEST_SHA384,
		    entry->path);
		if (entry->data.sha384digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_SHA384;
	}
	if (CAN_SET_KEYWORD(MTREE_KEYWORD_MASK_SHA512)) {
		free(entry->data.sha512digest);
		entry->data.sha512digest = mtree_digest_file(MTREE_DIGEST_SHA512,
		    entry->path);
		if (entry->data.sha512digest == NULL)
			keywords &= ~MTREE_KEYWORD_MASK_SHA512;
	}

	entry->data.keywords = keywords;

#undef CAN_SET_KEYWORD
}

static void
copy_keyword(mtree_entry_data *data, mtree_entry_data *from, long keyword)
{

	switch (keyword) {
	case MTREE_KEYWORD_CKSUM:
		data->cksum = from->cksum;
		break;
	case MTREE_KEYWORD_CONTENTS:
		mtree_copy_string(&data->contents, from->contents);
		break;
	case MTREE_KEYWORD_DEVICE:
		if (data->device == NULL)
			data->device = mtree_device_create();

		mtree_device_copy_data(data->device, from->device);
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
		data->st_mode = from->st_mode & MTREE_ENTRY_MODE_MASK;
		break;
	case MTREE_KEYWORD_NLINK:
		data->st_nlink = from->st_nlink;
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
		// XXX delimit and compare each keyword??
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

mtree_entry *
mtree_entry_copy(mtree_entry *entry)
{
	mtree_entry *copy;
	int i;

	assert(entry != NULL);

	copy = mtree_entry_create();
	if (entry->path != NULL)
		copy->path = strdup(entry->path);
	if (entry->name != NULL)
		copy->name = strdup(entry->name);

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if ((entry->data.keywords & mtree_keywords[i].keyword) == 0)
			continue;
		copy_keyword(&copy->data, &entry->data,
		    mtree_keywords[i].keyword);
	}
	copy->data.keywords = entry->data.keywords;
	return (copy);
}

mtree_entry *
mtree_entry_copy_all(mtree_entry *entry)
{
	mtree_entry *entries;

	entries = NULL;
	while (entry != NULL) {
		entries = mtree_entry_prepend(entries, mtree_entry_copy(entry));
		entry = entry->next;
	}
	return (entries);
}

void
mtree_entry_copy_missing_keywords(mtree_entry *entry, mtree_entry_data *from)
{
	int i;

	assert(entry != NULL);
	assert(from != NULL);

	for (i = 0; mtree_keywords[i].name != NULL; i++) {
		if (entry->data.keywords & mtree_keywords[i].keyword)
			continue;
		if ((from->keywords & mtree_keywords[i].keyword) == 0)
			continue;
		copy_keyword(&entry->data, from, mtree_keywords[i].keyword);
	}
}

const char *
mtree_entry_get_name(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->name);
}

const char *
mtree_entry_get_path(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->path);
}

long
mtree_entry_get_keywords(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.keywords);
}

mtree_entry *
mtree_entry_get_parent(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->parent);
}

mtree_entry *
mtree_entry_get_first(mtree_entry *entry)
{

	assert(entry != NULL);

	while (entry->prev != NULL)
		entry = entry->prev;

	return (entry);
}

mtree_entry *
mtree_entry_get_last(mtree_entry *entry)
{

	assert(entry != NULL);

	while (entry->next != NULL)
		entry = entry->next;

	return (entry);
}

mtree_entry *
mtree_entry_get_previous(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->prev);
}

mtree_entry *
mtree_entry_get_next(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->next);
}

mtree_entry_type
mtree_entry_get_type(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.type);
}

uint32_t
mtree_entry_get_cksum(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.cksum);
}

gid_t
mtree_entry_get_gid(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.st_gid);
}

const char *
mtree_entry_get_gname(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.gname);
}

const char *
mtree_entry_get_link(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.link);
}

const char *
mtree_entry_get_md5digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.md5digest);
}

mode_t
mtree_entry_get_mode(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.st_mode);
}

nlink_t
mtree_entry_get_nlink(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.st_nlink);
}

const char *
mtree_entry_get_rmd160digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.rmd160digest);
}

const char *
mtree_entry_get_sha1digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha1digest);
}

const char *
mtree_entry_get_sha256digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha256digest);
}

const char *
mtree_entry_get_sha384digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha384digest);
}

const char *
mtree_entry_get_sha512digest(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.sha512digest);
}

off_t
mtree_entry_get_size(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.st_size);
}

struct timespec *
mtree_entry_get_time(mtree_entry *entry)
{

	assert(entry != NULL);

	return (&entry->data.st_mtim);
}

uid_t
mtree_entry_get_uid(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.st_uid);
}

const char *
mtree_entry_get_uname(mtree_entry *entry)
{

	assert(entry != NULL);

	return (entry->data.uname);
}

mtree_entry *
mtree_entry_find_path(mtree_entry *entry, const char *path)
{

	assert(entry != NULL);
	assert(path != NULL);

	while (entry != NULL) {
		if (strcmp(entry->path, path) == 0)
			return (entry);

		entry = entry->next;
	}
	return (NULL);
}

mtree_entry *
mtree_entry_prepend(mtree_entry *entry, mtree_entry *child)
{

	assert(child != NULL);

	child->next = entry;
	if (entry != NULL) {
		child->prev = entry->prev;
		if (entry->prev != NULL)
			entry->prev->next = child;
		entry->prev = child;
	} else
		child->prev = NULL;

	return (child);
}

mtree_entry *
mtree_entry_append(mtree_entry *entry, mtree_entry *child)
{

	assert(child != NULL);

	if (entry != NULL) {
		mtree_entry *last;

		last = mtree_entry_get_last(entry);
		last->next  = child;
		child->prev = last;
	} else {
		child->prev = NULL;
		entry = child;
	}
	return (entry);
}

mtree_entry *
mtree_entry_reverse(mtree_entry *entry)
{
	mtree_entry *last;

	last = NULL;
	while (entry != NULL) {
		last = entry;
		entry = last->next;
		last->next = last->prev;
		last->prev = entry;
	}
	return (last);
}

mtree_entry *
mtree_entry_unlink(mtree_entry *head, mtree_entry *entry)
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
