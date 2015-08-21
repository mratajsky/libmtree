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

#include "config.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <mtree.h>
#include <mtree_file.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>

#include "local.h"

#define	MODE_MASK	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

#define	RP(path)	((path)[0] == '.' && \
			 (path)[1] == '/' ? (path) + 2 : (path))

#define RPE(entry)	(RP(mtree_entry_get_path(entry)))

#define	INDENT_NAME_LEN	8
#define MARK								\
do {									\
	if (flavor == FLAVOR_FREEBSD9) {				\
		len = printf("%s changed\n", RP(path));			\
		tab = "\t";						\
	} else {							\
		len = printf("%s: ", RP(path));				\
		if (len > INDENT_NAME_LEN) {				\
			tab = "\t";					\
			printf("\n");					\
		} else {						\
			tab = "";					\
			printf("%*s", INDENT_NAME_LEN - (int) len, "");	\
		}							\
	}								\
} while (0)

#define	LABEL if (!label++) MARK

#if HAVE_STRUCT_STAT_ST_FLAGS
/* User settable flags */
#define	UF_MASK	((UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE) & UF_SETTABLE)
/* root settable flags */
#define	SF_MASK ((SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND) & SF_SETTABLE)
/* all settable flags */
#define	CH_MASK	 (UF_MASK | SF_MASK)
/* special flags */
#define	SP_FLGS	 (SF_IMMUTABLE | SF_APPEND)

#define CHANGEFLAGS(path, flags, flags_orig)				\
do {									\
	char *sf;							\
	if (!label) {							\
		MARK;							\
		sf = convert_flags_to_string(flags_orig, "none");	\
		printf("%sflags (\"%s\"", tab, sf);			\
		free(sf);						\
	}								\
	if (lchflags(path, flags)) {					\
		label++;						\
		printf(", not modified: %s)\n", strerror(errno));	\
	} else {							\
		sf = convert_flags_to_string(flags, "none");		\
		printf(", modified to \"%s\")\n", sf);			\
		free(sf);						\
	}								\
} while (0)

/* SETFLAGS:
 * given pflags, additionally set those flags specified in eflags and
 * selected by mask (the other flags are left unchanged).
 */
#define SETFLAGS(path, eflags, fflags, pflags, mask)			\
do {									\
	uint32_t flags = (eflags & (mask)) | (pflags);			\
	if (flags != fflags)						\
		CHANGEFLAGS(path, flags, fflags);			\
} while (0)

/* CLEARFLAGS:
 * given pflags, reset the flags specified in eflags and selected by mask
 * (the other flags are left unchanged).
 */
#define CLEARFLAGS(path, eflags, fflags, pflags, mask)			\
do {									\
	uint32_t flags = (~(eflags & (mask)) & CH_MASK) & (pflags);	\
	if (flags != fflags)						\
		CHANGEFLAGS(path, flags, fflags);			\
} while (0)

#endif	/* HAVE_STRUCT_STAT_ST_FLAGS */

static int
compare(struct mtree_entry *e, struct mtree_entry *f)
{
	mtree_entry_type etype;
	const char	*tab;
	const char	*path;
	const char	*edigest;
	const char	*fdigest;
	long		 kw;
	int		 label;
	int		 len;

	kw = mtree_entry_get_keywords(e);
	/*
	 * Match entry's keywords in the file.
	 */
	mtree_entry_set_keywords(f, kw, 0);
	kw = kw & mtree_entry_get_keywords(f);

	path  = mtree_entry_get_path(e);
	tab   = NULL;
	label = 0;

	if (kw & MTREE_KEYWORD_TYPE) {
		mtree_entry_type ftype;

		etype = mtree_entry_get_type(e);
		ftype = mtree_entry_get_type(f);
		if (etype != ftype) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "\ttype expected %s found %s\n" :
			    "\ttype (%s, %s)\n",
			    mtree_entry_type_string(etype),
			    mtree_entry_type_string(ftype));
			return (label);
		}
	} else
		etype = MTREE_ENTRY_UNKNOWN;

	if (Wflag)
		goto afterpermcheck;
#if HAVE_STRUCT_STAT_ST_FLAGS
	if (!uflag && (iflag || mflag)) {
		if (keywords & MTREE_KEYWORD_FLAGS) {
			const char	*eflags;
			const char	*fflags;
			uint32_t 	 n_eflags;
			uint32_t 	 n_fflags;

			eflags = mtree_entry_get_flags(e);
			fflags = mtree_entry_get_flags(f);
			if (convert_string_to_flags(eflags, &n_eflags) == -1) {
				LABEL;
				printf("%sinvalid flags %s\n",
				    tab,
				    eflags);
			} else if (convert_string_to_flags(fflags, &n_fflags) == -1) {
				LABEL;
				printf("%sinvalid flags %s\n",
				    tab,
				    eflags);
			} else if (iflag) {
				SETFLAGS(path, n_eflags, n_fflags,
				    n_fflags, SP_FLGS);
			} else if (mflag) {
				CLEARFLAGS(path, n_eflags, n_fflags,
				    n_fflags, SP_FLGS);
			}
		}
		return (label);
	}
#endif

	if (kw & MTREE_KEYWORD_MASK_USER) {
		uid_t	euid;
		uid_t	fuid;
		int	modify = 0;

		/*
		 * Verify the uname if uid is not given, otherwise only verify
		 * the uid
		 */
		if ((kw & MTREE_KEYWORD_UID) == 0) {
			const char *euname;
			const char *funame;

			/* Compare unames */
			euname = mtree_entry_get_uname(e);
			funame = mtree_entry_get_uname(f);
			if (strcmp(euname, funame) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%suser expected %s found %s" :
				    "%suser (%s, %s",
				    tab,
				    funame, euname);
				if (uflag) {
					if (convert_uname_to_uid(euname, &euid) == -1)
						printf(", not modified: unknown user %s",
						    euname);
					else
						modify = 1;
				}
				if (!modify)
					printf("%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				tab = "\t";
			}
		} else {
			euid = mtree_entry_get_uid(e);
			fuid = mtree_entry_get_uid(f);

			if (euid != fuid) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%suser expected %ju found %ju" :
				    "%suser (%ju, %ju",
				    tab,
				    (uintmax_t) euid,
				    (uintmax_t) fuid);
				if (uflag)
					modify = 1;
				else
					printf("%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				tab = "\t";
			}
		}
		if (modify) {
			if (lchown(path, euid, -1) != 0)
				printf(", not modified: %s%s\n",
				    strerror(errno),
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			else
				printf(", modified%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
		}
	}

	if (kw & MTREE_KEYWORD_MASK_GROUP) {
		gid_t	egid;
		gid_t	fgid;
		int	modify = 0;

		/*
		 * Verify the gname if gid is not given, otherwise only verify
		 * the gid
		 */
		if ((kw & MTREE_KEYWORD_GID) == 0) {
			const char *egname;
			const char *fgname;

			/* Compare gnames */
			egname = mtree_entry_get_gname(e);
			fgname = mtree_entry_get_gname(f);
			if (strcmp(egname, fgname) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%sgroup expected %s found %s" :
				    "%sgroup (%s, %s",
				    tab,
				    fgname, egname);
				if (uflag) {
					if (convert_gname_to_gid(egname, &egid) == -1)
						printf(", not modified: unknown group %s",
						    egname);
					else
						modify = 1;
				}
				if (!modify)
					printf("%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				tab = "\t";
			}
		} else {
			egid = mtree_entry_get_gid(e);
			fgid = mtree_entry_get_gid(f);

			if (egid != fgid) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%sgroup expected %ju found %ju" :
				    "%sgroup (%ju, %ju",
				    tab,
				    (uintmax_t) egid,
				    (uintmax_t) fgid);
				if (uflag)
					modify = 1;
				else
					printf("%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				tab = "\t";
			}
		}
		if (modify) {
			if (lchown(path, -1, egid) != 0)
				printf(", not modified: %s%s\n",
				    strerror(errno),
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			else
				printf(", modified%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
		}
	}

	if (kw & MTREE_KEYWORD_MODE) {
		mode_t emode;
		mode_t fmode;

		emode = mtree_entry_get_mode(e) & MODE_MASK;
		fmode = mtree_entry_get_mode(f) & MODE_MASK;
		if (emode != fmode) {
			if (lflag) {
				/*
				 * If none of the suid/sgid/etc bits are set,
				 * then if the mode is a subset of the target,
				 * skip
				 */
				if (!((emode & ~(S_IRWXU|S_IRWXG|S_IRWXO)) ||
				    (fmode & ~(S_IRWXU|S_IRWXG|S_IRWXO))))
					if ((fmode | emode) == emode)
						goto skip;
			}

			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%spermissions expected %#o found %#o" :
			    "%spermissions (%#o, %#o",
			    tab, emode, fmode);
			if (uflag) {
				int ret;
#ifdef HAVE_LCHMOD
				ret = lchmod(path, emode);
#else
				/* Set permissions on the real file if we don't
				 * have lchmod */
				ret = chmod(path, emode);
#endif
				if (ret == -1)
					printf(", not modified: %s%s\n",
					    strerror(errno),
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				else
					printf(", modified%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			} else
				printf("%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			tab = "\t";
		}
	skip:	;
	}

	if (kw & MTREE_KEYWORD_NLINK && etype != MTREE_ENTRY_DIR) {
		nlink_t enlink;
		nlink_t fnlink;

		enlink = mtree_entry_get_nlink(e);
		fnlink = mtree_entry_get_nlink(f);
		if (enlink != fnlink) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%slink count expected %ju found %ju\n" :
			    "%slink count (%ju, %ju)\n",
			    tab,
			    (uintmax_t) enlink,
			    (uintmax_t) fnlink);
			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_INODE) {
		ino_t eino;
		ino_t fino;

		eino = mtree_entry_get_inode(e);
		fino = mtree_entry_get_inode(f);
		if (eino != fino) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%sinode expected %ju found %ju\n" :
			    "%sinode (%ju, %ju)\n",
			    tab,
			    (uintmax_t) eino,
			    (uintmax_t) fino);
			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_SIZE) {
		off_t esize;
		off_t fsize;

		esize = mtree_entry_get_size(e);
		fsize = mtree_entry_get_size(f);
		if (esize != fsize) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%ssize expected %ju found %ju\n" :
			    "%ssize (%ju, %ju)\n",
			    tab,
			    (uintmax_t) esize,
			    (uintmax_t) fsize);
			tab = "\t";
		}
	}

#if HAVE_STRUCT_STAT_ST_FLAGS
	/*
	 * XXX
	 * since lchflags(2) will reset file times, the utimes() above
	 * may have been useless!  oh well, we'd rather have correct
	 * flags, rather than times?
	 */
	if (keywords & MTREE_KEYWORD_FLAGS) {
		const char	*eflags;
		const char	*fflags;
		uint32_t 	 n_eflags;
		uint32_t 	 n_fflags;

		eflags = mtree_entry_get_flags(e);
		fflags = mtree_entry_get_flags(f);
		if (convert_string_to_flags(eflags, &n_eflags) == -1) {
			LABEL;
			printf("%sinvalid flags %s\n",
			    tab,
			    eflags);
			tab = "\t";
		} else if (convert_string_to_flags(fflags, &n_fflags) == -1) {
			LABEL;
			printf("%sinvalid flags %s\n",
			    tab,
			    fflags);
			tab = "\t";
		} else if (n_eflags != n_fflags || mflag || iflag) {
			if (n_eflags != n_fflags) {
				char *efs, *ffs;

				efs = convert_flags_to_string(n_eflags, "none");
				ffs = convert_flags_to_string(n_fflags, "none");
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%sflags expected \"%s\" found \"%s\"" :
				    "%sflags (\"%s\" is not \"%s\"",
				    tab,
				    efs, ffs);
				free(efs);
				free(ffs);
			}
			if (uflag) {
				if (iflag)
					SETFLAGS(path, n_eflags, n_fflags,
					    0, CH_MASK);
				else if (mflag)
					CLEARFLAGS(path, n_eflags, n_fflags,
					    0, SP_FLGS);
				else
					SETFLAGS(path, n_eflags, n_fflags,
					    0, (~SP_FLGS & CH_MASK));
			} else
				printf("%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			tab = "\t";
		}
	}
#endif	/* HAVE_STRUCT_STAT_ST_FLAGS */

	if (kw & MTREE_KEYWORD_TIME) {
		struct mtree_timespec ets;
		struct mtree_timespec fts;

		ets = *mtree_entry_get_time(e);
		fts = *mtree_entry_get_time(f);
#ifndef HAVE_STRUCT_STAT_ST_MTIM
		ets.tv_nsec = 0;
		fts.tv_nsec = 0;
#endif
		if (ets.tv_sec != fts.tv_sec || ets.tv_nsec != fts.tv_nsec) {
			time_t et = ets.tv_sec;
			time_t ft = fts.tv_sec;
			/*
			 * XXX
			 * Doesn't display micro/nanosecond differences.
			 */
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%smodification time expected %.24s found " :
			    "%smodification time (%.24s, ",
			    tab, ctime(&et));
			printf("%.24s", ctime(&ft));
			if (tflag) {
#ifdef HAVE_UTIMENSAT
				struct timespec ts[2];
				/*
				 * utimensat(2) supports nanosecond precision, use
				 * it if possible
				 */
				ts[0].tv_sec  = ets.tv_sec;
				ts[0].tv_nsec = ets.tv_nsec;
				ts[1] = ts[0];
				if (utimensat(-1, path, ts,
				    AT_SYMLINK_NOFOLLOW) != 0)
					printf(", not modified: %s%s\n",
					    strerror(errno),
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				else
					printf(", modified%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
#else
				struct timeval tv[2];
				/*
				 * Use utimes(2) and timevals with microsecond
				 * precision if utimensat(2) is not available
				 */
				tv[0].tv_sec  = ets.tv_sec;
				tv[0].tv_usec = ets.tv_nsec / 1000;
				tv[1] = tv[0];
				if (utimes(path, tv) != 0)
					printf(", not modified: %s%s\n",
					    strerror(errno),
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				else
					printf(", modified%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
#endif
			} else
				printf("%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");

			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_CKSUM) {
		uint32_t ecrc;
		uint32_t fcrc;

		ecrc = mtree_entry_get_cksum(e);
		fcrc = mtree_entry_get_cksum(f);
		if (ecrc != fcrc) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%scksum expected %u found %u\n" :
			    "%scksum (%u, %u)\n",
			    tab,
			    ecrc, fcrc);
			tab = "\t";
		}
	}

	/*
	 * From this point, no more permission checking or changing
	 * occurs, only checking of stuff like checksums and symlinks.
	 */
afterpermcheck:
	if (kw & MTREE_KEYWORD_MASK_MD5) {
		edigest = mtree_entry_get_md5digest(e);
		fdigest = mtree_entry_get_md5digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    MD5KEY, edigest, fdigest);
			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_MASK_RMD160) {
		edigest = mtree_entry_get_rmd160digest(e);
		fdigest = mtree_entry_get_rmd160digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    RMD160KEY, edigest, fdigest);
			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_MASK_SHA1) {
		edigest = mtree_entry_get_sha1digest(e);
		fdigest = mtree_entry_get_sha1digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    SHA1KEY, edigest, fdigest);
			tab = "\t";
		}
	}
	if (kw & MTREE_KEYWORD_MASK_SHA256) {
		edigest = mtree_entry_get_sha256digest(e);
		fdigest = mtree_entry_get_sha256digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    SHA256KEY, edigest, fdigest);
			tab = "\t";
		}
	}
	if (kw & MTREE_KEYWORD_MASK_SHA384) {
		edigest = mtree_entry_get_sha384digest(e);
		fdigest = mtree_entry_get_sha384digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    SHA384KEY, edigest, fdigest);
			tab = "\t";
		}
	}
	if (kw & MTREE_KEYWORD_MASK_SHA512) {
		edigest = mtree_entry_get_sha512digest(e);
		fdigest = mtree_entry_get_sha512digest(f);
		if (strcmp(edigest, fdigest) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%s%s expected %s found %s\n" :
			    "%s%s (0x%s, 0x%s)\n",
			    tab,
			    SHA512KEY, edigest, fdigest);
			tab = "\t";
		}
	}

	if (kw & MTREE_KEYWORD_LINK) {
		const char *elink;
		const char *flink;

		elink = mtree_entry_get_link(e);
		flink = mtree_entry_get_link(f);
		if (strcmp(elink, flink) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%slink ref expected %s found %s" :
			    "%slink ref (%s, %s",
			    tab, elink, flink);
			if (uflag) {
				if ((unlink(path) == -1) ||
				    (symlink(elink, path) == -1))
					printf(", not modified: %s%s\n",
					    strerror(errno),
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
				else
					printf(", modified%s\n",
					    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			} else
				printf("%s\n",
				    flavor == FLAVOR_FREEBSD9 ? "" : ")");
		}
	}
	return (label);
}

static void
miss(struct mtree_entry *entry)
{
	struct stat	 st;
	const char	*path;
	const char	*typename;
	const char	*s;
	long		 kw;
	mtree_entry_type type;
	mode_t		 mode;
	uid_t		 uid;
	gid_t		 gid;
	int		 mode_set;

	kw = mtree_entry_get_keywords(entry);
	if (kw & MTREE_KEYWORD_OPTIONAL)
		return;
	type = mtree_entry_get_type(entry);
	if (dflag && type != MTREE_ENTRY_DIR)
		return;

	path = mtree_entry_get_path(entry);

	/* Don't print missing message if file exists as a symbolic link and
	 * the -q flag is set */
	if (qflag && stat(path, &st) == 0 && S_ISDIR(st.st_mode))
		return;

	printf("%s missing", path);
	if (!uflag) {
		printf("\n");
		return;
	}

	switch (type) {
	case MTREE_ENTRY_BLOCK:
	case MTREE_ENTRY_CHAR:
		typename = "device";
		break;
	case MTREE_ENTRY_DIR:
		typename = "directory";
		break;
	case MTREE_ENTRY_LINK:
		typename = "symlink";
		break;
	default:
		printf("\n");
		return;
	}

	if (!Wflag && type != MTREE_ENTRY_LINK) {
		if (kw & MTREE_KEYWORD_MASK_USER) {
			if ((kw & MTREE_KEYWORD_UID) == 0) {
				/*
				 * Only uname is specified, see if there
				 * is a matching user in the system
				 */
				s = mtree_entry_get_uname(entry);
				if (convert_uname_to_uid(s, &uid) == -1) {
					printf(" (%s not created: unknown user %s)\n",
					    typename, s);
					return;
				}
			} else
				uid = mtree_entry_get_uid(entry);
		} else {
			printf(" (%s not created: user not specified)\n",
			    typename);
			return;
		}
		if (kw & MTREE_KEYWORD_MASK_GROUP) {
			if ((kw & MTREE_KEYWORD_GID) == 0) {
				/*
				 * Only gname is specified, see if there
				 * is a matching group in the system
				 */
				s = mtree_entry_get_gname(entry);
				if (convert_gname_to_gid(s, &gid) == -1) {
					printf(" (%s not created: unknown group %s)\n",
					    typename, s);
					return;
				}
			} else
				gid = mtree_entry_get_gid(entry);
		} else {
			printf(" (%s not created: group not specified)\n", typename);
			return;
		}

		if (kw & MTREE_KEYWORD_MODE)
			mode = mtree_entry_get_mode(entry);
		else {
			printf(" (%s not created: mode not specified)\n", typename);
			return;
		}
	}

	/*
	 * Devices and directories have permissions set while they are
	 * created, setting mode_set to 1 indicates this
	 */
	mode_set = 0;

	switch (type) {
	case MTREE_ENTRY_BLOCK:
	case MTREE_ENTRY_CHAR:
		if (Wflag)
			return;
		if ((kw & MTREE_KEYWORD_DEVICE) == 0) {
			printf(" (%s not created: device not specified)\n",
			    typename);
			return;
		} else {
			struct mtree_device	*dev;
			mode_t			 dev_mode;
			dev_t			 dev_number;

			dev = mtree_entry_get_device(entry);
			if ((mtree_device_get_fields(dev) &
			    MTREE_DEVICE_FIELD_NUMBER) == 0) {
				printf(" (%s not created: device number unknown)\n",
				    typename);
				return;
			}
			if (type == MTREE_ENTRY_BLOCK)
				dev_mode = mode | S_IFBLK;
			else
				dev_mode = mode | S_IFCHR;
			dev_number = mtree_device_get_value(dev,
			    MTREE_DEVICE_FIELD_NUMBER);

			if (mknod(path, dev_mode, dev_number) == -1) {
				printf(" (%s not created: %s)\n",
				    typename, strerror(errno));
				return;
			}
			mode_set = 1;
		}
		break;
	case MTREE_ENTRY_LINK:
		if ((kw & MTREE_KEYWORD_LINK) == 0) {
			printf(" (%s not created: link not specified)\n",
			    typename);
			return;
		} else {
			s = mtree_entry_get_link(entry);
			if (symlink(s, path) != 0) {
				printf(" (%s not created: %s)\n",
				    typename, strerror(errno));
				return;
			}
		}
		break;
	case MTREE_ENTRY_DIR:
		if (mkdir(path, mode) != 0) {
			printf(" (%s not created: %s)\n",
			    typename, strerror(errno));
			return;
		}
		mode_set = 1;
		break;
	default:
		mtree_err("\ncan't create %s", typename);
		break;
	}

	printf(" (created)\n");
	if (Wflag)
		return;

	/*
	 * Set uid/gid
	 */
	if ((kw & MTREE_KEYWORD_MASK_USER) &&
	    (kw & MTREE_KEYWORD_MASK_GROUP)) {
		if (lchown(path, uid, gid) != 0) {
			printf("%s: user/group/mode not modified: %s\n",
			    path, strerror(errno));
			printf("%s: warning: file mode %snot set\n", path,
#ifdef HAVE_LCHFLAGS
			    (kw & MTREE_KEYWORD_FLAGS) ? "and file flags " :
#endif
			    "");
			return;
		}
	}

	/*
	 * Set permissions
	 */
	if ((kw & MTREE_KEYWORD_MODE) && !mode_set) {
		int ret;
#ifdef HAVE_LCHMOD
		ret = lchmod(path, mode);
#else
		/* Set permissions on the real file if we don't
		 * have lchmod */
		ret = chmod(path, mode);
#endif
		if (ret != 0) {
			printf("%s: permissions not set: %s\n",
			    path, strerror(errno));
			return;
		}
	}

#if HAVE_LCHFLAGS
	/*
	 * Set file flags
	 */
	if (kw & MTREE_KEYWORD_FLAGS) {
		uint32_t flags;

		s = mtree_entry_get_flags(entry);
		if (convert_string_to_flags(s, &flags) == 0) {
			if (!iflag)
				flags &= ~SP_FLGS;
			if (lchflags(path, flags) != 0)
				printf("%s: file flags not set: %s\n",
				    path, strerror(errno));
		} else {
			printf("%s: file flags not set: invalid value %s\n",
			    path, s);
			return;
		}
	}
#endif	/* HAVE_LCHFLAGS */
}

static int
verify_filter(struct mtree_entry *entry, void *user_data)
{
	int ret;

	/*
	 * Apply the default filter and skip the entry immediately if
	 * the default filter says so
	 */
	ret = filter_spec(entry, NULL);
	if (ret & MTREE_ENTRY_SKIP)
		return (ret);

	if (mtree_entry_get_type(entry) == MTREE_ENTRY_DIR) {
		struct mtree_entry	*s1;
		const char	*path;
		const char	*p1;
		size_t		 path_len;
		size_t		 p1_len;

		s1 = (struct mtree_entry *) user_data;
		path = mtree_entry_get_path(entry);
		/*
		 * Content of this directory shouldn't be reported if the
		 * directory itself is not present in the spec.
		 *
		 * Doing this early in a filter prevents the reader from
		 * reading a potentially large directory structure only to
		 * throw it away later.
		 */
		while (s1 != NULL) {
			path_len = strlen(path);
			p1 = mtree_entry_get_path(s1);
			if (strncmp(p1, path, path_len) == 0) {
				/*
				 * Some entry is present in this directory, make
				 * sure not to skip its children.
				 */
				p1_len = strlen(p1);
				if (p1_len == path_len ||
				    (p1_len > path_len && p1[path_len] == '/'))
					return (MTREE_ENTRY_KEEP);
			}
			s1 = mtree_entry_get_next(s1);
		}
		return (MTREE_ENTRY_KEEP | MTREE_ENTRY_SKIP_CHILDREN);
	}
	return (MTREE_ENTRY_KEEP);
}

int
verify_spec(FILE *fp)
{
	struct mtree_spec	*spec1;
	struct mtree_spec	*spec2;
	struct mtree_spec_diff	*sd;
	struct mtree_entry	*entry;
	uint64_t		 kw;
	int			 options;
	int			 ret = 0;

	spec1 = read_spec(fp);
	if (spec1 == NULL)
		return (-1);
	spec2 = create_spec();
	if (spec2 == NULL) {
		mtree_spec_free(spec1);
		return (-1);
	}
	mtree_spec_set_read_filter(spec2, verify_filter,
	    mtree_spec_get_entries(spec1));
	mtree_spec_set_read_path_keywords(spec2, MTREE_KEYWORD_TYPE);
	if (mtree_spec_read_path(spec2, ".") != 0) {
		mtree_spec_free(spec1);
		mtree_spec_free(spec2);
		return (-1);
	}
	/*
	 * Create a spec diff, but avoid comparing keywords until later.
	 */
	options = 0;
	sd = mtree_spec_diff_create(spec1, spec2, 0, 0);
	if (sd == NULL) {
		mtree_spec_free(spec1);
		mtree_spec_free(spec2);
		return (-1);
	}

	entry = mtree_spec_diff_get_matching(sd);
	while (entry != NULL) {
		struct mtree_entry *e = entry;
		struct mtree_entry *f = mtree_entry_get_next(e);

		kw = mtree_entry_get_keywords(e);
		if ((kw & MTREE_KEYWORD_NOCHANGE) == 0)
			if (compare(e, f) != 0)
				ret = MISMATCHEXIT;

		entry = mtree_entry_get_next(f);
	}

	if (!eflag) {
		/*
		 * Display entries that are present in the file system, but
		 * not in the spec
		 */
		entry = mtree_spec_diff_get_spec2_only(sd);
		while (entry != NULL) {
			if (dflag) {
				mtree_entry_set_keywords(entry, MTREE_KEYWORD_TYPE, 0);
				if (mtree_entry_get_type(entry) == MTREE_ENTRY_LINK) {
					entry = mtree_entry_get_next(entry);
					continue;
				}
			}
			printf("extra: %s", RPE(entry));
			if (rflag) {
				const char	*path;
				int		 ret;
				/*
				 * Remove the entry if it's a file or an
				 * empty directory
				 */
				path = mtree_entry_get_path(entry);
				if (mtree_entry_get_type(entry) == MTREE_ENTRY_DIR)
					ret = rmdir(path);
				else
					ret = unlink(path);
				if (ret == 0)
					printf(", removed");
				else
					printf(", not removed: %s", strerror(errno));
			}
			putchar('\n');
			entry = mtree_entry_get_next(entry);
		}
	}

	entry = mtree_spec_diff_get_spec1_only(sd);
	while (entry != NULL) {
		miss(entry);
		entry = mtree_entry_get_next(entry);
	}

	/*
	TODO: use mtree_cksum and ~ the result after each file
	if (sflag)
		mtree_warn("%s checksum: %u", fullpath, crc_total);
	*/
	return (ret);
}
