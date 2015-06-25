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
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <inttypes.h>
#include <libmtree/mtree.h>
#include <libmtree/mtree_file.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>

#include "mtree.h"

#define	MODE_MASK	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXU|S_IRWXG|S_IRWXO)

#define	RP(p)		((p)->fts_path[0] == '.' &&	\
			 (p)->fts_path[1] == '/' ?	\
			 (p)->fts_path + 2 : (p)->fts_path)

#define	INDENT_NAME_LEN	8
#define MARK								\
do {									\
	if (flavor == FLAVOR_FREEBSD9) {				\
		len = printf("%s changed\n", RP(p));			\
		tab = "\t";						\
	} else {							\
		len = printf("%s: ", RP(p));				\
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

static int
compare(mtree_entry *entry, FTSENT *p)
{
	mtree_entry_type type;
	const char *tab;
	const char *edigest;
	char *fdigest;
	long keywords;
	bool typeerr;
	int label;
	int len;

	tab     = NULL;
	label   = 0;
	type    = mtree_entry_get_type(entry);
	typeerr = false;
	switch (type) {
	case MTREE_ENTRY_BLOCK:
		if (!S_ISBLK(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_CHAR:
		if (!S_ISCHR(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_DIR:
		if (!S_ISDIR(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_FIFO:
		if (!S_ISFIFO(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_FILE:
		if (!S_ISREG(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_LINK:
		if (!S_ISLNK(p->fts_statp->st_mode))
			typeerr = true;
		break;
	case MTREE_ENTRY_SOCKET:
		if (!S_ISSOCK(p->fts_statp->st_mode))
			typeerr = true;
		break;
	default:
		break;
	}

	if (typeerr) {
		LABEL;
		printf(flavor == FLAVOR_FREEBSD9 ?
			"\ttype expected %s found %s\n" : "\ttype (%s, %s)\n",
			mtree_type_string(type),
			file_type_string(p->fts_statp->st_mode));
		return (label);
	}

	keywords = mtree_entry_get_keywords(entry);

	/* Set the uid/gid first, then set the mode */
	if (keywords & MTREE_KEYWORD_UID) {
		uid_t uid;

		uid = mtree_entry_get_uid(entry);
		if (uid != p->fts_statp->st_uid) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%suser expected %ju found %ju\n" :
			    "%suser (%ju, %ju)\n",
			    tab,
			    (uintmax_t) uid,
			    (uintmax_t) p->fts_statp->st_uid);
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_GID) {
		gid_t gid;

		gid = mtree_entry_get_gid(entry);
		if (gid != p->fts_statp->st_gid) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%sgid expected %ju found %ju\n" :
			    "%sgid (%ju, %ju)\n",
			    tab,
			    (uintmax_t) gid,
			    (uintmax_t) p->fts_statp->st_gid);
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_MODE) {
		mode_t emode, fmode;

		emode = mtree_entry_get_mode(entry);
		fmode = p->fts_statp->st_mode & MODE_MASK;
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
			    "%spermissions expcted %#o found %#o\n" :
			    "%spermissions (%#o, %#o)\n",
			    tab, emode, fmode);
			tab = "\t";
		}
	skip:	;
	}

	if (keywords & MTREE_KEYWORD_NLINK && type != MTREE_ENTRY_DIR) {
		nlink_t nlink;

		nlink = mtree_entry_get_nlink(entry);
		if (nlink != p->fts_statp->st_nlink) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%slink count expected %ju found %ju\n" :
			    "%slink count (%ju, %ju)\n",
			    tab,
			    (uintmax_t) nlink,
			    (uintmax_t) p->fts_statp->st_nlink);
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_SIZE) {
		off_t size;

		size = mtree_entry_get_size(entry);
		if (size != p->fts_statp->st_size) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%ssize expected %ju found %ju\n" :
			    "%ssize (%ju, %ju)\n",
			    tab,
			    (uintmax_t) size,
			    (uintmax_t) p->fts_statp->st_size);
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_TIME) {
		struct timespec *ets;
		struct timeval tv[2];

		ets = mtree_entry_get_time(entry);
		/*
		 * Since utimes(2) only takes a timeval, there's no point in
		 * comparing the low bits of the timespec nanosecond field.  This
		 * will only result in mismatches that we can never fix.
		 */
		tv[0].tv_sec  = ets->tv_sec;
		tv[0].tv_usec = ets->tv_nsec / 1000;
#ifdef HAVE_STRUCT_STAT_ST_MTIM
		tv[1].tv_sec  = p->fts_statp->st_mtim.tv_sec;
		tv[1].tv_usec = p->fts_statp->st_mtim.tv_nsec / 1000;
#else
#ifdef HAVE_STRUCT_STAT_ST_MTIME
		tv[1].tv_sec  = p->fts_statp->st_mtime;
#else
		tv[1].tv_sec  = 0;
#endif
		tv[1].tv_usec = 0;
#endif

		if (tv[0].tv_sec != tv[1].tv_sec) {
			time_t et = tv[0].tv_sec;
			time_t ft = tv[1].tv_sec;

			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%smodification time expected %.24s found " :
			    "%smodification time (%.24s, ",
			    tab, ctime(&et));
			printf("%.24s%s\n", ctime(&ft),
			    flavor == FLAVOR_FREEBSD9 ? "" : ")");
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_CKSUM) {
		uint32_t ecrc, fcrc;
		int ret;

		ret = mtree_digest_file_crc32(p->fts_accpath, &fcrc, NULL);
		if (ret != 0) {
			LABEL;
			printf("%scksum: %s: %s\n",
			    tab, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			ecrc = mtree_entry_get_cksum(entry);
			if (ecrc != fcrc) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%scksum expected %u found %u\n" :
				    "%scksum (%u, %u)\n",
				    tab, ecrc, fcrc);
			}
			tab = "\t";
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_MD5) {
		fdigest = mtree_digest_file(MTREE_DIGEST_MD5, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, MD5KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_md5digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, MD5KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_RMD160) {
		fdigest = mtree_digest_file(MTREE_DIGEST_RMD160, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, RMD160KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_rmd160digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, RMD160KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_SHA1) {
		fdigest = mtree_digest_file(MTREE_DIGEST_SHA1, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, SHA1KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_sha1digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, SHA1KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_SHA256) {
		fdigest = mtree_digest_file(MTREE_DIGEST_SHA256, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, SHA256KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_sha256digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, SHA256KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_SHA384) {
		fdigest = mtree_digest_file(MTREE_DIGEST_SHA384, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, SHA1KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_sha384digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, SHA384KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_MASK_SHA512) {
		fdigest = mtree_digest_file(MTREE_DIGEST_SHA512, p->fts_accpath);
		if (fdigest == NULL) {
			LABEL;
			printf("%s%s: %s: %s\n",
			    tab, SHA512KEY, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			edigest = mtree_entry_get_sha512digest(entry);
			if (strcmp(edigest, fdigest) != 0) {
				LABEL;
				printf(flavor == FLAVOR_FREEBSD9 ?
				    "%s%s expected %s found %s\n" :
				    "%s%s (0x%s, 0x%s)\n",
				    tab, SHA512KEY, edigest, fdigest);
			}
			tab = "\t";
			free(fdigest);
		}
	}

	if (keywords & MTREE_KEYWORD_LINK) {
		char linkbuf[MAXPATHLEN];
		const char *link;

		len = readlink(p->fts_accpath, linkbuf, sizeof(linkbuf) - 1);
		if (len == -1)
			mtree_err("%s: %s", p->fts_accpath, strerror(errno));
		linkbuf[len] = '\0';

		link = mtree_entry_get_link(entry);
		if (strcmp(link, linkbuf) != 0) {
			LABEL;
			printf(flavor == FLAVOR_FREEBSD9 ?
			    "%slink ref expected %s found %s" :
			    "%slink ref (%s, %s", tab, linkbuf, link);

			printf("%s\n", flavor == FLAVOR_FREEBSD9 ? "" : ")");
		}
	}
	return (label);
}

int
verify_spec(FILE *fp)
{
	FTS *t;
	FTSENT *p;
	mtree_spec *spec;
	mtree_entry *entry, *level, *parent;
	char *argv[2];
	int ftsoptions;
	int specdepth;
	int ret;

	spec = read_spec(fp);
	if (spec == NULL)
		return (-1);

	argv[0] = ".";
	argv[1] = NULL;

	if (Lflag)
		ftsoptions = FTS_LOGICAL;
	else
		ftsoptions = FTS_PHYSICAL;
	if (xflag)
		ftsoptions |= FTS_XDEV;

	if ((t = fts_open(argv, ftsoptions, NULL)) == NULL)
		mtree_err("fts_open: %s", strerror(errno));

	level = mtree_spec_get_entries(spec);
	specdepth = ret = 0;
	while ((p = fts_read(t)) != NULL) {
		switch(p->fts_info) {
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			mtree_warn("%s: %s", RP(p), strerror(p->fts_errno));
			continue;
		case FTS_D:
		case FTS_SL:
			break;
		case FTS_DP:
			while (specdepth > p->fts_level) {
				mtree_entry *prev;
				/*
				 * Go up a level and back to find the first
				 * entry with the current parent
				 */
				prev   = mtree_entry_get_parent(level);
				parent = mtree_entry_get_parent(prev);
				while (prev != NULL) {
					if (mtree_entry_get_parent(prev) == parent)
						level = prev;
					prev = mtree_entry_get_previous(prev);
				}
				specdepth--;
			}
			continue;
		default:
			if (dflag)
				continue;
		}

		if (specdepth != p->fts_level)
			goto extra;

		entry = level;
		parent = mtree_entry_get_parent(entry);
		while (entry != NULL) {
			const char *name;

			if (mtree_entry_get_parent(entry) != parent) {
				entry = mtree_entry_get_next(entry);
				continue;
			}

			name = mtree_entry_get_name(entry);
			if (strcmp(name, p->fts_name) == 0) {
				long keywords;

				keywords = mtree_entry_get_keywords(entry);
				if ((keywords & MTREE_KEYWORD_NOCHANGE) == 0) {
					if (compare(entry, p) != 0)
						ret = MISMATCHEXIT;
				}
				if ((keywords & MTREE_KEYWORD_IGNORE) == 0) {
					mtree_entry_type type;

					type = mtree_entry_get_type(entry);
					if (type == MTREE_ENTRY_DIR &&
					    p->fts_info == FTS_D) {
						mtree_entry *child;

						/*
						 * If the directory is not empty,
						 * descend into it
						 */
						child = mtree_entry_get_next(entry);
						if (child != NULL &&
						    mtree_entry_get_parent(child) == entry) {
							level = child;
							specdepth++;
						}
					}
				} else
					fts_set(t, p, FTS_SKIP);
				break;
			}
			entry = mtree_entry_get_next(entry);
		}
		if (entry)
			continue;
 extra:
		if (!eflag && !(dflag && p->fts_info == FTS_SL)) {
			printf("extra: %s\n", RP(p));
		}
		fts_set(t, p, FTS_SKIP);
	}
	fts_close(t);
	return (ret);
}
