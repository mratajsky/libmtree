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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libmtree/mtree.h>
#include <libmtree/mtree_file.h>

#include "mtree.h"

static struct {
	int		flavor;
	const char	name[9];
} flavors[] = {
	{ FLAVOR_MTREE,		"mtree" },
	{ FLAVOR_FREEBSD9,	"freebsd9" },
	{ FLAVOR_NETBSD6,	"netbsd6" },
};

int	flavor = FLAVOR_MTREE;

int	bflag, dflag, eflag, iflag, jflag, lflag, Lflag, mflag, Mflag, nflag,
	qflag, rflag, sflag, Sflag, tflag, uflag, xflag, Wflag;
char	fullpath[MAXPATHLEN];

long	keywords = MTREE_KEYWORD_MASK_DEFAULT;

static const char *_progname;

static void
usage(void)
{
	unsigned int i;

	fprintf(stderr,
	    "usage: %s [-bCcDdejLlMnPqrStUuWx] [-i|-m] [-E tags]\n"
	    "\t\t[-f spec] [-f spec]\n"
	    "\t\t[-I tags] [-K keywords] [-k keywords] [-N dbdir] [-p path]\n"
	    "\t\t[-R keywords] [-s seed] [-X exclude-file]\n"
	    "\t\t[-F flavor]\n",
	    _progname);

	fprintf(stderr, "\nflavors:");
	for (i = 0; i < __arraycount(flavors); i++)
		fprintf(stderr, " %s", flavors[i].name);

	fprintf(stderr, "\n");
	exit(1);
}

void
mtree_warnv(const char *fmt, va_list ap)
{
	fprintf(stderr, "%s: ", _progname);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);

	fprintf(stderr, "\n");
}

void
mtree_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mtree_warnv(fmt, ap);
	va_end(ap);
}

void
mtree_err(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	mtree_warnv(fmt, ap);
	va_end(ap);

	exit(1);
	/* NOTREACHED */
}

int
main(int argc, char **argv)
{
	FILE	*spec1, *spec2;
	char	*dir, *p;
	char	*specfile1, *specfile2;
	size_t  i;
	int	ch, status;
	int	cflag, Cflag, Dflag, Pflag, Uflag, wflag;

	_progname = argv[0];

	cflag = Cflag = Dflag = Uflag = wflag = 0;
	dir = NULL;
	spec1 = NULL;
	spec2 = NULL;
	specfile1 = NULL;
	specfile2 = NULL;

	while ((ch = getopt(argc, argv,
	    "bcCdDeE:f:F:I:ijk:K:lLmMnN:O:p:PqrR:s:StuUwWxX:")) != -1) {
		switch (ch) {
		case 'b':
			bflag = 1;
			break;
		case 'c':
			cflag = 1;
			break;
		case 'C':
			Cflag = 1;
			break;
		case 'd':
			dflag = 1; // TODO
			break;
		case 'D':
			Dflag = 1;
			break;
		case 'E':
			// TODO
			// parsetags(&excludetags, optarg);
			break;
		case 'e':
			eflag = 1; // TODO
			break;
		case 'f':
			if (specfile1 == NULL)
				specfile1 = optarg;
			else if (specfile2 == NULL)
				specfile2 = optarg;
			else
				usage();
			break;
		case 'F':
			for (i = 0; i < __arraycount(flavors); i++)
				if (strcmp(optarg, flavors[i].name) == 0) {
					flavor = flavors[i].flavor;
					break;
				}
			if (i == __arraycount(flavors))
				usage();
			break;
		case 'i':
			iflag = 1; // TODO
			break;
		case 'I':
			// TODO
			// parsetags(&includetags, optarg);
			break;
		case 'j':
			jflag = 1;
			break;
		case 'k':
			keywords = MTREE_KEYWORD_TYPE;
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keywords |= parse_keyword(p);
			break;
		case 'K':
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keywords |= parse_keyword(p);
			break;
		case 'l':
			lflag = 1;
			break;
		case 'L':
			Lflag = 1;
			break;
		case 'm':
			mflag = 1; // TODO
			break;
		case 'M':
			Mflag = 1; // TODO
			break;
		case 'n':
			nflag = 1;
			break;
		case 'N':
			if (!setup_getid(optarg))
				mtree_err("Unable to use user and group databases in `%s'",
				    optarg);
			break;
		case 'O':
			// TODO
			load_only(optarg);
			break;
		case 'p':
			dir = optarg;
			break;
		case 'P':
			/* Disables -L */
			Pflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'R':
			while ((p = strsep(&optarg, " \t,")) != NULL)
				if (*p != '\0')
					keywords &= ~parse_keyword(p);
			break;
		case 's':
			sflag = 1; // TODO
			break;
		case 'S':
			Sflag = 1; // TODO
			break;
		case 't':
			tflag = 1; // TODO
			break;
		case 'u':
			uflag = 1; // TODO
			break;
		case 'U':
			Uflag = uflag = 1; // TODO
			break;
		case 'w':
			wflag = 1; // TODO
			break;
		case 'W':
			Wflag = 1; // TODO
			break;
		case 'x':
			xflag = 1;
			break;
		case 'X':
			// TODO
			if (read_excludes(optarg) == -1)
				mtree_err("%s: %s", optarg, strerror(errno));
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (flavor == FLAVOR_FREEBSD9) {
		if (cflag && iflag) {
			mtree_warn("-c and -i passed, replacing -i with -j for "
			    "FreeBSD compatibility");
			iflag = 0;
			jflag = 1;
		}
		if (dflag && !bflag) {
			mtree_warn("Adding -b to -d for FreeBSD compatibility");
			bflag = 1;
		}
		if (uflag && !iflag) {
			mtree_warn("Adding -i to -%c for FreeBSD compatibility",
			    Uflag ? 'U' : 'u');
			iflag = 1;
		}
		if (uflag && !tflag) {
			mtree_warn("Adding -t to -%c for FreeBSD compatibility",
			    Uflag ? 'U' : 'u');
			tflag = 1;
		}
		if (wflag)
			mtree_warn("The -w flag is a no-op");
	} else {
		if (wflag)
			usage();
	}

	if (Pflag)
		Lflag = 0;

	if (specfile2 && (cflag || Cflag || Dflag)){
		printf("specfile1=%s\n",specfile1);
		printf("specfile2=%s\n",specfile2);
		mtree_err("Double -f, -c, -C and -D flags are mutually exclusive");
	}

	if (dir && specfile2)
		mtree_err("Double -f and -p flags are mutually exclusive");

	if (dir && chdir(dir))
		mtree_err("%s: %s", dir, strerror(errno));

	if ((cflag || sflag) && !getcwd(fullpath, sizeof(fullpath)))
		mtree_err("%s", strerror(errno));

	if ((cflag && Cflag) || (cflag && Dflag) || (Cflag && Dflag))
		mtree_err("-c, -C and -D flags are mutually exclusive");

	if (iflag && mflag)
		mtree_err("-i and -m flags are mutually exclusive");

	if (lflag && uflag)
		mtree_err("-l and -u flags are mutually exclusive");

	if (cflag) {
		/* Write a spec for `dir' to stdout */
		if (write_spec_tree(stdout, fullpath) != 0)
			mtree_err("Failed to write spec: %s", strerror(errno));
		exit(0);
	}

	if (specfile1 != NULL) {
		spec1 = fopen(specfile1, "r");
		if (spec1 == NULL)
			mtree_err("%s: %s", specfile1, strerror(errno));
	} else
		spec1 = stdin;

	if (Cflag || Dflag) {
		if (read_write_spec(spec1, stdout, Dflag) != 0)
			mtree_err("%s", strerror(errno));
		exit(0);
	}

	if (specfile2 != NULL) {
		spec2 = fopen(specfile2, "r");
		if (spec2 == NULL)
			mtree_err("%s: %s", specfile2, strerror(errno));

		status = compare_spec(spec1, spec2, stdout);
	} else
		status = verify_spec(spec1);

	if (Uflag && (status == MISMATCHEXIT))
		status = 0;

	exit(status);
}
