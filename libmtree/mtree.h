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

typedef struct _mtree_spec  mtree_spec;
typedef struct _mtree_entry mtree_entry;

/*
 * mtree entry fields
 */
#define MTREE_KEYWORD_CKSUM		0x00000001
#define MTREE_KEYWORD_CONTENTS		0x00000002
#define MTREE_KEYWORD_FLAGS		0x00000004
#define MTREE_KEYWORD_GID		0x00000008
#define MTREE_KEYWORD_GNAME		0x00000010
#define MTREE_KEYWORD_IGNORE		0x00000020
#define MTREE_KEYWORD_INODE		0x00000040
#define MTREE_KEYWORD_LINK		0x00000080
#define MTREE_KEYWORD_MD5		0x00000100
#define MTREE_KEYWORD_MD5DIGEST		0x00000200
#define MTREE_KEYWORD_MODE		0x00000400
#define MTREE_KEYWORD_NLINK		0x00000800
#define MTREE_KEYWORD_NOCHANGE		0x00001000
#define MTREE_KEYWORD_OPTIONAL		0x00002000
#define MTREE_KEYWORD_RIPEMD160DIGEST	0x00004000
#define MTREE_KEYWORD_RMD160		0x00008000
#define MTREE_KEYWORD_RMD160DIGEST	0x00010000
#define MTREE_KEYWORD_SHA1		0x00020000
#define MTREE_KEYWORD_SHA1DIGEST	0x00040000
#define MTREE_KEYWORD_SHA256		0x00080000
#define MTREE_KEYWORD_SHA256DIGEST	0x00100000
#define MTREE_KEYWORD_SHA384		0x00200000
#define MTREE_KEYWORD_SHA384DIGEST	0x00400000
#define MTREE_KEYWORD_SHA512		0x00800000
#define MTREE_KEYWORD_SHA512DIGEST	0x01000000
#define MTREE_KEYWORD_SIZE		0x02000000
#define MTREE_KEYWORD_TIME		0x04000000
#define MTREE_KEYWORD_TYPE		0x08000000
#define MTREE_KEYWORD_UID		0x10000000
#define MTREE_KEYWORD_UNAME		0x20000000

mtree_spec      *mtree_spec_create(void);
void             mtree_spec_free(mtree_spec *spec);
void             mtree_spec_reset(mtree_spec *spec);

int              mtree_spec_read_data(mtree_spec *spec, const char *data, int len);
int              mtree_spec_read_data_end(mtree_spec *spec);

int              mtree_spec_add_file(mtree_spec *spec, const char *path);
int              mtree_spec_add_directory(mtree_spec *spec, const char *path);

mtree_entry 	*mtree_entry_first(mtree_entry *entry);
mtree_entry 	*mtree_entry_last(mtree_entry *entry);
mtree_entry 	*mtree_entry_previous(mtree_entry *entry);
mtree_entry 	*mtree_entry_next(mtree_entry *entry);

#endif /* !_LIBMTREE_MTREE_H_ */
