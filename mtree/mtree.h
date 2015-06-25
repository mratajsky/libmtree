/*-
 * Copyright (c) 2015 Michal Ratajsky <michal@FreeBSD.org>
 * Copyright (c) 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef _MTREE_H_
#define _MTREE_H_

#include <sys/param.h>

#include <libmtree/mtree.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "compat.h"

#define	MISMATCHEXIT	2

#ifdef __FreeBSD__
# define KEY_DIGEST	"digest"
#else
# define KEY_DIGEST
#endif

#define	MD5KEY		"md5"		KEY_DIGEST
#ifdef __FreeBSD__
# define RMD160KEY	"ripemd160"	KEY_DIGEST
#else
# define RMD160KEY	"rmd160"	KEY_DIGEST
#endif
#define	SHA1KEY		"sha1"		KEY_DIGEST
#define	SHA256KEY	"sha256"	KEY_DIGEST
#define	SHA384KEY	"sha384"
#define	SHA512KEY	"sha512"

__BEGIN_DECLS

enum {
	FLAVOR_MTREE,
	FLAVOR_FREEBSD9,
	FLAVOR_NETBSD6
};
extern int flavor;

extern int bflag, dflag, eflag, iflag, jflag, lflag, Lflag, mflag, Mflag, nflag,
	   qflag, rflag, sflag, Sflag, tflag, uflag, xflag, Wflag;

extern long keywords;

void		 mtree_warnv(const char *fmt, va_list ap);
void		 mtree_warn(const char *fmt, ...);
void		 mtree_err(const char *fmt, ...);

int		 read_excludes(const char *file);
int		 check_excludes(const char *fname, const char *path);
int		 setup_getid(const char *dir);
void		 load_only(const char *fname);
bool		 find_only(const char *path);
mtree_spec	*read_spec(FILE *fp);
int		 read_write_spec(FILE *fr, FILE *fw, int path_last);
int		 write_spec_tree(FILE *fp, const char *tree);
int		 compare_spec(FILE *f1, FILE *f2, FILE *fw);
int		 verify_spec(FILE *fp);
long		 parse_keyword(const char *name);
const char 	*file_type_string(mode_t mode);

__END_DECLS

#endif /* _MTREE_H_ */
